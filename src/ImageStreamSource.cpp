#include "ImageStreamSource.h"
#include "Types.h"
#include <QDebug>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QFileInfo>
#include <QImage>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

// ============================================================
// ImageStreamSource implementation
// ============================================================

ImageStreamSource::ImageStreamSource(int cameraId, QObject* parent)
    : QObject(parent)
    , mCameraId(cameraId)
    , mSnapshotIntervalMs(1000)
    , mFetchTimer(nullptr)
    , mRunning(0)
    , mFrameCount(0)
{
}

ImageStreamSource::~ImageStreamSource()
{
    stopStream();
}

void ImageStreamSource::setSnapshotUrl(const QString& url)
{
    QMutexLocker locker(&mMutex);
    mSnapshotUrl = url;
}

void ImageStreamSource::setSnapshotInterval(int ms)
{
    mSnapshotIntervalMs = qMax(100, ms);
}

void ImageStreamSource::startStream()
{
    if (mRunning.loadRelaxed() != 0) return;
    mRunning.storeRelaxed(1);

    if (!mFetchTimer) {
        mFetchTimer = new QTimer(this);
        mFetchTimer->setTimerType(Qt::PreciseTimer);
        connect(mFetchTimer, &QTimer::timeout, this, &ImageStreamSource::fetchOne);
    }

    // Stagger initial fetch by camera ID (250ms each) so 8 threads
    // don't all open RTSP connections simultaneously.
    int delay = mCameraId * 250;  // 0, 250, 500, ..., 1750 ms
    QTimer::singleShot(delay, this, [this]() {
        if (mRunning.loadRelaxed() == 0) return;
        fetchOne();
        if (mFetchTimer && !mFetchTimer->isActive())
            mFetchTimer->start(mSnapshotIntervalMs);
    });

    mLastFpsTime   = QDateTime::currentMSecsSinceEpoch();
    mFpsFrameCount = 0;
    mFrameCount    = 0;

    qDebug() << "ImageStreamSource[" << mCameraId << "]: Started, interval:" << mSnapshotIntervalMs << "ms";
}

void ImageStreamSource::stopStream()
{
    mRunning.storeRelaxed(0);
    if (mFetchTimer) {
        mFetchTimer->stop();
    }
    if (mRtspCapture.isOpened()) {
        mRtspCapture.release();
    }
    qDebug() << "ImageStreamSource[" << mCameraId << "]: Stopped";
}

void ImageStreamSource::requestStop()
{
    mUserStopped = true;
    stopStream();
}

void ImageStreamSource::requestStart(const QString& url)
{
    mUserStopped = false;
    setSnapshotUrl(url);
    startStream();
    qDebug() << "ImageStreamSource[" << mCameraId << "]: User-requested start, url:" << url;
}

void ImageStreamSource::fetchOne()
{
    if (mRunning.loadRelaxed() == 0) return;

    // Prevent re-entrant calls: if >>frame blocks (timeout up to 5s),
    // the next timer tick would call fetchOne again before we return.
    if (mFetchInProgress) return;
    mFetchInProgress = true;

    QString url;
    {
        QMutexLocker locker(&mMutex);
        url = mSnapshotUrl;
    }

    if (!url.isEmpty()) {
        if (url.startsWith("http://", Qt::CaseInsensitive) ||
            url.startsWith("https://", Qt::CaseInsensitive)) {
            fetchViaHttp();
        } else if (url.startsWith("rtsp://", Qt::CaseInsensitive)) {
            fetchViaRtsp();
        } else {
            fetchViaFile();
        }
    }

    mFetchInProgress = false;
}

// ── HTTP fetch ──────────────────────────────────────────────

void ImageStreamSource::fetchViaHttp()
{
    // Skip if a request is already in flight
    if (mHttpPending) return;

    // Lazily create QNetworkAccessManager in this thread
    if (!mNetworkManager) {
        mNetworkManager = new QNetworkAccessManager(this);
        connect(mNetworkManager, &QNetworkAccessManager::finished,
                this, &ImageStreamSource::onHttpReplyFinished);
    }

    QString url;
    {
        QMutexLocker locker(&mMutex);
        url = mSnapshotUrl;
    }

    QUrl qurl(url);
    QNetworkRequest request(qurl);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("Accept", "image/*");
    request.setTransferTimeout(qMax(2000, mSnapshotIntervalMs / 2));

    mHttpPending = true;
    mNetworkManager->get(request);
}

void ImageStreamSource::onHttpReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    mHttpPending = false;

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "ImageStreamSource[" << mCameraId
                    << "]: HTTP error:" << reply->errorString();
        emit error(QString("Camera %1: HTTP %2").arg(mCameraId).arg(reply->errorString()));
        return;
    }

    QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        qWarning() << "ImageStreamSource[" << mCameraId << "]: Empty HTTP response";
        return;
    }

    // Decode JPEG/PNG to cv::Mat via QImage (robust, supports many formats)
    QImage qimg;
    if (!qimg.loadFromData(data)) {
        qWarning() << "ImageStreamSource[" << mCameraId << "]: Failed to decode image";
        return;
    }

    // Convert QImage (RGB) to cv::Mat (BGR)
    qimg = qimg.convertToFormat(QImage::Format_RGB888);
    cv::Mat mat(qimg.height(), qimg.width(), CV_8UC3,
                const_cast<uchar*>(qimg.bits()), qimg.bytesPerLine());
    cv::Mat bgr;
    cv::cvtColor(mat, bgr, cv::COLOR_RGB2BGR);

    emitFrame(bgr.clone());
}

// ── RTSP: persistent connection, read 1 frame per tick ───────
// Keeps the cv::VideoCapture open across ticks — no per-tick TCP
// connect / RTSP handshake / TEARDOWN. This eliminates ephemeral
// port TIME_WAIT exhaustion and server-side connection floods.
//
// OPENCV_FFMPEG_CAPTURE_OPTIONS is set globally in main.cpp before
// any threads start (timeout=5000000 prevents indefinite block).
// Reading at 1 fps keeps H.264 decoder load trivial, avoiding the
// persistent-connection state corruption that plagued 30 fps mode.

void ImageStreamSource::fetchViaRtsp()
{
    // Back off after 2 consecutive failures: retry only every 5th tick.
    // Uses its own counter (not mFrameCount) because mFrameCount only
    // increments on SUCCESS — would never skip during a failure streak.
    if (mRtspConsecutiveFailures > 2) {
        mRtspBackoffCounter++;
        if (mRtspBackoffCounter % 5 != 0) return;
    } else {
        mRtspBackoffCounter = 0;
    }

    // (Re)open if needed
    if (!mRtspCapture.isOpened()) {
        QString url;
        {
            QMutexLocker locker(&mMutex);
            url = mSnapshotUrl;
        }
        if (url.isEmpty()) return;

        bool ok = false;
        try {
            ok = mRtspCapture.open(url.toStdString(), cv::CAP_FFMPEG);
        } catch (...) {}

        if (!ok || !mRtspCapture.isOpened()) {
            mRtspConsecutiveFailures++;
            mRtspCapture.release();
            return;
        }
        mRtspConsecutiveFailures = 0;
    }

    // Drain the internal FFmpeg decode buffer to get the FRESHEST frame.
    // Between our 1fps reads the 30fps stream accumulates ~29 decoded
    // frames.  A plain "cap >> frame" returns the oldest buffered frame
    // (up to 1s stale).  grab() is non-blocking — it only dequeues
    // already-decoded frames.  We drain everything, then retrieve() the
    // last one (≤33ms old).
    bool gotFrame = false;
    int drained = 0;
    while (drained < 60 && mRtspCapture.grab()) {  // 60 = 2s worth at 30fps
        gotFrame = true;
        drained++;
    }

    cv::Mat frame;
    if (gotFrame) {
        try {
            mRtspCapture.retrieve(frame);
        } catch (...) {}
    }

    if (frame.empty()) {
        mRtspConsecutiveFailures++;
        if (mRtspConsecutiveFailures > 3) {
            mRtspCapture.release();  // reconnect next tick
        }
        return;
    }

    mRtspConsecutiveFailures = 0;
    emitFrame(frame);
}

// ── Local file read ─────────────────────────────────────────

void ImageStreamSource::fetchViaFile()
{
    QString path;
    {
        QMutexLocker locker(&mMutex);
        path = mSnapshotUrl;
    }

    if (!QFileInfo::exists(path)) {
        emit error(QString("Camera %1: File not found: %2").arg(mCameraId).arg(path));
        return;
    }

    cv::Mat frame = cv::imread(path.toStdString(), cv::IMREAD_COLOR);
    if (frame.empty()) {
        qWarning() << "ImageStreamSource[" << mCameraId << "]: Failed to read:" << path;
        return;
    }

    emitFrame(frame);
}

// ── Common emit ─────────────────────────────────────────────

void ImageStreamSource::emitFrame(const cv::Mat& image)
{
    if (image.empty()) return;

    mFrameCount++;

    FrameData data;
    data.cameraId   = mCameraId;
    data.image      = image;
    data.timestamp  = QDateTime::currentMSecsSinceEpoch();
    data.frameIndex = mFrameCount;

    emit displayFrameReady(data);

    // FPS calculation (every 5 seconds, since we fetch infrequently)
    mFpsFrameCount++;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 elapsed = now - mLastFpsTime;
    if (elapsed >= 5000) {
        double fps = (mFpsFrameCount * 1000.0) / elapsed;
        mLastFpsTime = now;
        mFpsFrameCount = 0;
        emit fpsUpdated(mCameraId, fps);
    }
}

// ============================================================
// ImageStreamThread implementation
// ============================================================

ImageStreamThread::ImageStreamThread(int cameraId, QObject* parent)
    : QObject(parent)
{
    mThread = new QThread(this);
    mSource = new ImageStreamSource(cameraId);  // no parent — will be moved to thread

    mSource->moveToThread(mThread);

    // Wire signals through thread boundary
    connect(mSource, &ImageStreamSource::displayFrameReady,
            this, &ImageStreamThread::displayFrameReady,
            Qt::QueuedConnection);
    connect(mSource, &ImageStreamSource::error,
            this, &ImageStreamThread::error,
            Qt::QueuedConnection);
    connect(mSource, &ImageStreamSource::fpsUpdated,
            this, &ImageStreamThread::fpsUpdated,
            Qt::QueuedConnection);

    // Start/stop from main thread
    connect(mThread, &QThread::started,
            mSource, &ImageStreamSource::startStream);
    connect(mThread, &QThread::finished,
            mSource, &ImageStreamSource::stopStream);

    mThread->setObjectName(QString("ImageStreamThread_%1").arg(cameraId));
    mThread->start();
}

ImageStreamThread::~ImageStreamThread()
{
    mThread->quit();
    mThread->wait(3000);
    if (mThread->isRunning()) {
        mThread->terminate();
        mThread->wait();
    }
    delete mSource;
}

void ImageStreamThread::setSnapshotUrl(const QString& url)
{
    mSource->setSnapshotUrl(url);
}

void ImageStreamThread::setSnapshotInterval(int ms)
{
    mSource->setSnapshotInterval(ms);
}

int ImageStreamThread::cameraId() const
{
    return mSource->cameraId();
}

void ImageStreamThread::requestStop()
{
    QMetaObject::invokeMethod(mSource, "requestStop", Qt::QueuedConnection);
}

void ImageStreamThread::requestStart(const QString& url)
{
    QMetaObject::invokeMethod(mSource, "requestStart", Qt::QueuedConnection,
                              Q_ARG(QString, url));
}
