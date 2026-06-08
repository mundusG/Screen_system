#ifndef CAMERACAPTURE_H
#define CAMERACAPTURE_H

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QMutex>
#include <QAtomicInt>
#include <opencv2/videoio.hpp>
#include "Types.h"

/// Worker object that continuously captures frames from a camera source.
/// Lives in its own QThread. Emits display frames at full rate and
/// inference frames at the configured interval (default 1 FPS).
class CameraCapture : public QObject
{
    Q_OBJECT

public:
    explicit CameraCapture(int cameraId, QObject* parent = nullptr);
    ~CameraCapture() override;

    /// Open the camera source
    bool open(const QString& source);

    /// Close the camera source
    void close();

    /// Check if camera is open
    bool isOpen() const;

    /// Set inference interval (ms between inference frames)
    void setInferenceInterval(int ms);

    /// Camera ID
    int cameraId() const { return mCameraId; }

signals:
    /// Emitted for every captured frame (high FPS for display ~25-30fps)
    void displayFrameReady(const FrameData& frame);

    /// Emitted at inference rate (1 FPS by default)
    void inferenceFrameReady(const FrameData& frame);

    /// Emitted on capture errors
    void error(const QString& message);

    /// Emitted when FPS is calculated
    void fpsUpdated(int cameraId, double fps);

public slots:
    /// Start capture loop
    void startCapture();

    /// Stop capture loop
    void stopCapture();

    /// Trigger a single capture
    void captureOne();

private:
    void captureLoop();

    int         mCameraId;
    QString     mSource;
    cv::VideoCapture mCapture;
    QTimer*     mCaptureTimer;
    int         mInferenceIntervalMs;
    int         mFrameCount;
    QAtomicInt  mRunning;

    // FPS calculation
    qint64      mLastFpsTime;
    int         mFpsFrameCount;

    mutable QMutex mMutex;
};

/// Thread wrapper for CameraCapture
class CameraThread : public QObject
{
    Q_OBJECT

public:
    explicit CameraThread(int cameraId, QObject* parent = nullptr);
    ~CameraThread() override;

    bool open(const QString& source);
    void close();
    bool isOpen() const;
    void setInferenceInterval(int ms);
    int  cameraId() const;

    CameraCapture* capture() const { return mCapture; }

signals:
    void displayFrameReady(const FrameData& frame);
    void inferenceFrameReady(const FrameData& frame);
    void error(const QString& message);
    void fpsUpdated(int cameraId, double fps);

private:
    QThread*         mThread;
    CameraCapture*   mCapture;
};

#endif // CAMERACAPTURE_H
