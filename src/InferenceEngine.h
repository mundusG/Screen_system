#ifndef INFERENCEENGINE_H
#define INFERENCEENGINE_H

#include <QObject>
#include <QThread>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QAtomicInt>
#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include "Types.h"

/// YOLO inference worker using OpenCV DNN with ONNX models.
/// Each instance handles one camera's model. Lives in its own QThread.
class InferenceWorker : public QObject
{
    Q_OBJECT

public:
    explicit InferenceWorker(int cameraId, QObject* parent = nullptr);
    ~InferenceWorker() override;

    /// Load YOLO model from ONNX file
    bool loadModel(const QString& modelPath, int inputWidth, int inputHeight);

    /// Check if model is loaded
    bool isModelLoaded() const;

    /// Set confidence threshold (detections below are marked filtered)
    void setConfidenceThreshold(float threshold);

    /// Set NMS threshold
    void setNmsThreshold(float threshold);

    /// Get camera ID
    int cameraId() const { return mCameraId; }

signals:
    /// Emitted when inference completes
    void inferenceFinished(const InferenceResult& result);

    /// Emitted on errors
    void error(const QString& message);

public slots:
    /// Run inference on a frame (called from main thread via queued connection)
    void runInference(const FrameData& frame);

    /// Update model path at runtime
    void reloadModel(const QString& modelPath, int inputWidth, int inputHeight);

private:
    /// Preprocess: convert to blob, normalize, resize
    cv::Mat preprocess(const cv::Mat& frame);

    /// Postprocess: decode YOLO output to detections
    QVector<Detection> postprocess(const std::vector<cv::Mat>& outputs, const cv::Size& originalSize);

    /// NMS filter
    QVector<Detection> applyNMS(QVector<Detection>& detections);

    int     mCameraId;
    cv::dnn::Net mNet;
    bool    mModelLoaded;

    int     mInputWidth;
    int     mInputHeight;
    float   mConfThreshold;
    float   mNmsThreshold;

    // Class names (can be loaded from model metadata or config)
    QStringList mClassNames;

    mutable QMutex mMutex;
};

/// Thread wrapper for InferenceWorker
class InferenceEngine : public QObject
{
    Q_OBJECT

public:
    explicit InferenceEngine(int cameraId, QObject* parent = nullptr);
    ~InferenceEngine() override;

    bool loadModel(const QString& modelPath, int inputWidth, int inputHeight);
    bool isModelLoaded() const;
    void setConfidenceThreshold(float threshold);
    void setNmsThreshold(float threshold);
    int  cameraId() const;

    InferenceWorker* worker() const { return mWorker; }

signals:
    void inferenceFinished(const InferenceResult& result);
    void error(const QString& message);

    // Internal signals to queue across thread boundary
    void requestInference(const FrameData& frame);
    void requestReloadModel(const QString& modelPath, int inputWidth, int inputHeight);

private:
    QThread*          mThread;
    InferenceWorker*  mWorker;
};

#endif // INFERENCEENGINE_H
