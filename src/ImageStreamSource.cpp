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
#include <QProcess>

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

// ── RTSP: one-shot ffmpeg subprocess per frame ──────────────
// Each frame grab spawns a fresh ffmpeg process: connect, grab
// one MJPEG frame, exit. Complete process isolation — even if
// ffmpeg or the RTSP server misbehaves, the main app is safe.

void ImageStreamSource::fetchViaRtsp()
{
    if (mRtspFailCount > 3) {
        mRtspBackoffCounter++;
        if (mRtspBackoffCounter % 5 != 0) return;
    } else {
        mRtspBackoffCounter = 0;
    }

    QString url;
    {
        QMutexLocker locker(&mMutex);
        url = mSnapshotUrl;
    }
    if (url.isEmpty()) return;

    // Use ffmpeg to grab one MJPEG frame from RTSP.
    // -map 0:v:0  explicitly selects first video stream
    // -an         disable audio (prevent stream mapping issues)
    // -f mjpeg    single MJPEG frame to stdout
    QProcess proc;
    proc.start(QStringLiteral("ffmpeg"), QStringList()
        << QStringLiteral("-loglevel") << QStringLiteral("error")
        << QStringLiteral("-rtsp_transport") << QStringLiteral("tcp")
        << QStringLiteral("-i") << url
        << QStringLiteral("-map") << QStringLiteral("0:v:0")
        << QStringLiteral("-an")
        << QStringLiteral("-vframes") << QStringLiteral("1")
        << QStringLiteral("-c:v") << QStringLiteral("mjpeg")
        << QStringLiteral("-f") << QStringLiteral("mjpeg")
        << QStringLiteral("-"));

    // Cap wait: if ffmpeg reliably fails, backoff already limits retries.
    // A valid RTSP grab typically completes in 2-4s (TCP connect + handshake).
    int timeout = qMin(qMax(5000, mSnapshotIntervalMs * 2), 8000);
    if (!proc.waitForFinished(timeout)) {
        proc.kill();
        proc.waitForFinished(1000);
        mRtspFailCount++;
        qWarning() << "ImageStreamSource[" << mCameraId << "]: ffmpeg timeout";
        return;
    }

    if (proc.exitCode() != 0) {
        QByteArray err = proc.readAllStandardError();
        if (!err.isEmpty())
            qWarning() << "ImageStreamSource[" << mCameraId << "]: ffmpeg error:" << err;
        mRtspFailCount++;
        return;
    }

    QByteArray data = proc.readAllStandardOutput();
    if (data.isEmpty()) {
        mRtspFailCount++;
        return;
    }

    cv::Mat raw(1, data.size(), CV_8UC1, const_cast<char*>(data.constData()));
    cv::Mat frame = cv::imdecode(raw, cv::IMREAD_COLOR);
    if (frame.empty()) {
        qWarning() << "ImageStreamSource[" << mCameraId << "]: Failed to decode MJPEG";
        mRtspFailCount++;
        return;
    }

    mRtspFailCount = 0;
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
