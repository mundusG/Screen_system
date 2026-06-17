#ifndef IMAGESTREAMSOURCE_H
#define IMAGESTREAMSOURCE_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QAtomicInt>
#include <QMutex>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>

class QNetworkAccessManager;
class QNetworkReply;
struct FrameData;

/// Worker object that periodically fetches a single image frame from a URL.
/// Supports HTTP(S) snapshot URLs, RTSP streams (persistent single-frame grab),
/// and local file paths. Lives in its own QThread.
class ImageStreamSource : public QObject
{
    Q_OBJECT

public:
    explicit ImageStreamSource(int cameraId, QObject* parent = nullptr);
    ~ImageStreamSource() override;

    void setSnapshotUrl(const QString& url);
    void setSnapshotInterval(int ms);
    int cameraId() const { return mCameraId; }

signals:
    void displayFrameReady(const FrameData& frame);
    void error(const QString& message);
    void fpsUpdated(int cameraId, double fps);

public slots:
    void startStream();
    void stopStream();
    void requestStart(const QString& url);
    void requestStop();
    void fetchOne();

private slots:
    void onHttpReplyFinished(QNetworkReply* reply);

private:
    void fetchViaHttp();
    void fetchViaRtsp();
    void fetchViaFile();
    void emitFrame(const cv::Mat& image);

    int  mCameraId;
    QString mSnapshotUrl;
    int  mSnapshotIntervalMs;

    QTimer* mFetchTimer;
    QAtomicInt mRunning;
    bool mUserStopped = false;

    // HTTP mode
    QNetworkAccessManager* mNetworkManager = nullptr;
    bool mHttpPending = false;

    // RTSP mode — persistent connection, read 1 frame per tick.
    // Avoids per-tick TCP connect/RTSP-handshake/TEARDOWN that exhausts
    // ephemeral ports (TIME_WAIT) and overwhelms the RTSP server.
    cv::VideoCapture mRtspCapture;
    int mRtspConsecutiveFailures = 0;
    int mRtspBackoffCounter = 0;   // counts ticks for backoff (resets on success)

    // Guard against re-entrant fetchOne() when >>frame blocks past interval
    bool mFetchInProgress = false;

    // FPS
    qint64 mLastFpsTime = 0;
    int mFpsFrameCount = 0;
    int mFrameCount = 0;

    mutable QMutex mMutex;
};

/// Thread wrapper for ImageStreamSource (same pattern as CameraThread)
class ImageStreamThread : public QObject
{
    Q_OBJECT

public:
    explicit ImageStreamThread(int cameraId, QObject* parent = nullptr);
    ~ImageStreamThread() override;

    void setSnapshotUrl(const QString& url);
    void setSnapshotInterval(int ms);
    int  cameraId() const;
    void requestStop();
    void requestStart(const QString& url);

signals:
    void displayFrameReady(const FrameData& frame);
    void error(const QString& message);
    void fpsUpdated(int cameraId, double fps);

private:
    QThread*           mThread;
    ImageStreamSource* mSource;
};

#endif // IMAGESTREAMSOURCE_H
