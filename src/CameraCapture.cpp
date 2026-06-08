#include "CameraCapture.h"
#include <QDebug>
#include <QDateTime>

// ============================================================
// CameraCapture implementation
// ============================================================

CameraCapture::CameraCapture(int cameraId, QObject* parent)
    : QObject(parent)
    , mCameraId(cameraId)
    , mCaptureTimer(nullptr)
    , mInferenceIntervalMs(1000)
    , mFrameCount(0)
    , mRunning(0)
    , mLastFpsTime(0)
    , mFpsFrameCount(0)
{
    // Timer will be created in the worker thread via startCapture()
}

CameraCapture::~CameraCapture()
{
    stopCapture();
    close();
}

bool CameraCapture::open(const QString& source)
{
    QMutexLocker locker(&mMutex);
    mSource = source;

    // Try to open as integer (USB camera index) first, then as string (RTSP/file)
    bool ok = false;
    int deviceIndex = source.toInt(&ok);

    if (ok) {
        // USB/CSI camera by index
        mCapture.open(deviceIndex, cv::CAP_V4L2);  // V4L2 for Linux
        if (!mCapture.isOpened()) {
            // Fallback: try generic API
            mCapture.open(deviceIndex);
        }
    } else {
        // RTSP stream, video file, or device path
        mCapture.open(source.toStdString(), cv::CAP_FFMPEG);
        if (!mCapture.isOpened()) {
            // Try as device path
            mCapture.open(source.toStdString(), cv::CAP_V4L2);
        }
        if (!mCapture.isOpened()) {
            // Final fallback
            mCapture.open(source.toStdString());
        }
    }

    if (!mCapture.isOpened()) {
        qWarning() << "CameraCapture[" << mCameraId << "]: Failed to open source:" << source;
        return false;
    }

    // Configure capture properties
    mCapture.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    mCapture.set(cv::CAP_PROP_FRAME_WIDTH,  1920);
    mCapture.set(cv::CAP_PROP_FRAME_HEIGHT, 1080);
    mCapture.set(cv::CAP_PROP_FPS, 30);
    mCapture.set(cv::CAP_PROP_BUFFERSIZE, 3);  // reduce latency

    qDebug() << "CameraCapture[" << mCameraId << "]: Opened source:" << source
             << "resolution:" << mCapture.get(cv::CAP_PROP_FRAME_WIDTH) << "x"
             << mCapture.get(cv::CAP_PROP_FRAME_HEIGHT)
             << "fps:" << mCapture.get(cv::CAP_PROP_FPS);
    return true;
}

void CameraCapture::close()
{
    QMutexLocker locker(&mMutex);
    if (mCapture.isOpened()) {
        mCapture.release();
        qDebug() << "CameraCapture[" << mCameraId << "]: Closed";
    }
}

bool CameraCapture::isOpen() const
{
    QMutexLocker locker(&mMutex);
    return mCapture.isOpened();
}

void CameraCapture::setInferenceInterval(int ms)
{
    mInferenceIntervalMs = qMax(100, ms);  // minimum 100ms
}

void CameraCapture::startCapture()
{
    if (mRunning.loadRelaxed() != 0) return;
    mRunning.storeRelaxed(1);

    // Create timer in this thread context
    if (!mCaptureTimer) {
        mCaptureTimer = new QTimer(this);
        mCaptureTimer->setTimerType(Qt::PreciseTimer);
        connect(mCaptureTimer, &QTimer::timeout, this, &CameraCapture::captureOne);
    }

    // Capture at ~30 FPS (33ms interval)
    mCaptureTimer->start(33);
    mFrameCount    = 0;
    mLastFpsTime   = QDateTime::currentMSecsSinceEpoch();
    mFpsFrameCount = 0;

    qDebug() << "CameraCapture[" << mCameraId << "]: Capture started";
}

void CameraCapture::stopCapture()
{
    mRunning.storeRelaxed(0);
    if (mCaptureTimer) {
        mCaptureTimer->stop();
    }
    qDebug() << "CameraCapture[" << mCameraId << "]: Capture stopped";
}

void CameraCapture::captureOne()
{
    if (mRunning.loadRelaxed() == 0) return;

    cv::Mat frame;
    {
        QMutexLocker locker(&mMutex);
        if (!mCapture.isOpened()) return;

        // Grab-decode in one call
        mCapture >> frame;
    }

    if (frame.empty()) {
        // Try to reconnect
        qWarning() << "CameraCapture[" << mCameraId << "]: Empty frame, attempting reconnect...";
        emit error(QString("Camera %1: Empty frame").arg(mCameraId));

        QMutexLocker locker(&mMutex);
        QString src = mSource;
        locker.unlock();
        close();
        open(src);
        return;
    }

    mFrameCount++;

    // Build frame data
    FrameData data;
    data.cameraId  = mCameraId;
    data.image     = frame.clone();  // deep copy for thread safety
    data.timestamp = QDateTime::currentMSecsSinceEpoch();
    data.frameIndex = mFrameCount;

    // Emit display frame every time
    emit displayFrameReady(data);

    // Emit inference frame at configured interval
    // Default: every 1000ms / 33ms ≈ every 30th frame
    int framesPerInference = mInferenceIntervalMs / 33;
    if (framesPerInference < 1) framesPerInference = 1;

    if (mFrameCount % framesPerInference == 0) {
        emit inferenceFrameReady(data);
    }

    // FPS calculation (every 2 seconds)
    mFpsFrameCount++;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 elapsed = now - mLastFpsTime;
    if (elapsed >= 2000) {
        double fps = (mFpsFrameCount * 1000.0) / elapsed;
        mLastFpsTime = now;
        mFpsFrameCount = 0;
        emit fpsUpdated(mCameraId, fps);
    }
}

// ============================================================
// CameraThread implementation
// ============================================================

CameraThread::CameraThread(int cameraId, QObject* parent)
    : QObject(parent)
{
    mThread  = new QThread(this);
    mCapture = new CameraCapture(cameraId);  // no parent — will be moved to thread

    // Move capture to worker thread
    mCapture->moveToThread(mThread);

    // Wire signals through the thread boundary
    connect(mCapture, &CameraCapture::displayFrameReady,
            this, &CameraThread::displayFrameReady,
            Qt::QueuedConnection);
    connect(mCapture, &CameraCapture::inferenceFrameReady,
            this, &CameraThread::inferenceFrameReady,
            Qt::QueuedConnection);
    connect(mCapture, &CameraCapture::error,
            this, &CameraThread::error,
            Qt::QueuedConnection);
    connect(mCapture, &CameraCapture::fpsUpdated,
            this, &CameraThread::fpsUpdated,
            Qt::QueuedConnection);

    // Start/stop from main thread
    connect(mThread, &QThread::started,
            mCapture, &CameraCapture::startCapture);
    connect(mThread, &QThread::finished,
            mCapture, &CameraCapture::stopCapture);

    mThread->setObjectName(QString("CameraThread_%1").arg(cameraId));
    mThread->start();
}

CameraThread::~CameraThread()
{
    mThread->quit();
    mThread->wait(3000);
    if (mThread->isRunning()) {
        mThread->terminate();
        mThread->wait();
    }
    delete mCapture;
}

bool CameraThread::open(const QString& source)
{
    return mCapture->open(source);
}

void CameraThread::close()
{
    mCapture->close();
}

bool CameraThread::isOpen() const
{
    return mCapture->isOpen();
}

void CameraThread::setInferenceInterval(int ms)
{
    mCapture->setInferenceInterval(ms);
}

int CameraThread::cameraId() const
{
    return mCapture->cameraId();
}
