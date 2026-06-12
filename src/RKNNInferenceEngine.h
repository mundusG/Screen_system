#ifndef RKNNINFERENCEENGINE_H
#define RKNNINFERENCEENGINE_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QVector>
#include <opencv2/core.hpp>
#include "Types.h"
#include "rknn_api.h"

class RKNNInferenceWorker : public QObject
{
    Q_OBJECT

public:
    explicit RKNNInferenceWorker(int cameraId, QObject* parent = nullptr);
    ~RKNNInferenceWorker() override;

    bool loadModel(const QString& modelPath, int inputWidth, int inputHeight);
    void setConfidenceThreshold(float thresh);
    void setNmsThreshold(float thresh);

signals:
    void inferenceFinished(const InferenceResult& result);
    void error(const QString& message);

public slots:
    void runInference(const FrameData& frame);

private:
    cv::Mat preprocess(const cv::Mat& frame);
    QVector<Detection> postprocess(rknn_output* outputs, int numOutputs, const cv::Size& originalSize);
    QVector<Detection> applyNMS(QVector<Detection>& detections);

    int             mCameraId;
    rknn_context    mCtx;
    bool            mModelLoaded;
    int             mInputWidth;
    int             mInputHeight;
    int             mNumOutputs;
    float           mConfThreshold;
    float           mNmsThreshold;
    mutable QMutex  mMutex;
};

class RKNNInferenceEngine : public QObject
{
    Q_OBJECT

public:
    explicit RKNNInferenceEngine(int cameraId, QObject* parent = nullptr);
    ~RKNNInferenceEngine() override;

    bool loadModel(const QString& modelPath, int inputWidth, int inputHeight);
    void setConfidenceThreshold(float thresh);
    void setNmsThreshold(float thresh);

signals:
    void requestInference(const FrameData& frame);
    void inferenceFinished(const InferenceResult& result);
    void error(const QString& message);

private:
    QThread*              mThread;
    RKNNInferenceWorker*  mWorker;
};

#endif // RKNNINFERENCEENGINE_H
