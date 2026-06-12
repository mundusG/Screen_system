#include "RKNNInferenceEngine.h"
#include <QDateTime>
#include <QFile>
#include <QDebug>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <opencv2/imgproc.hpp>

// ============================================================
// RKNNInferenceWorker implementation
// ============================================================

RKNNInferenceWorker::RKNNInferenceWorker(int cameraId, QObject* parent)
    : QObject(parent)
    , mCameraId(cameraId)
    , mCtx(0)
    , mModelLoaded(false)
    , mInputWidth(640)
    , mInputHeight(640)
    , mNumOutputs(0)
    , mConfThreshold(0.5f)
    , mNmsThreshold(0.45f)
{
}

RKNNInferenceWorker::~RKNNInferenceWorker()
{
    if (mCtx) {
        rknn_destroy(mCtx);
        mCtx = 0;
    }
}

bool RKNNInferenceWorker::loadModel(const QString& modelPath, int inputWidth, int inputHeight)
{
    QMutexLocker locker(&mMutex);

    if (modelPath.isEmpty()) {
        qWarning() << "RKNNWorker[" << mCameraId << "]: Empty model path";
        return false;
    }

    QFile file(modelPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "RKNNWorker[" << mCameraId << "]: Cannot open model file:" << modelPath;
        return false;
    }
    QByteArray modelData = file.readAll();
    file.close();

    if (modelData.isEmpty()) {
        qWarning() << "RKNNWorker[" << mCameraId << "]: Model file is empty:" << modelPath;
        return false;
    }

    // Destroy previous context if exists
    if (mCtx) {
        rknn_destroy(mCtx);
        mCtx = 0;
    }

    int ret = rknn_init(&mCtx, modelData.data(), modelData.size(), 0, nullptr);
    if (ret < 0) {
        qWarning() << "RKNNWorker[" << mCameraId << "]: rknn_init failed, ret=" << ret;
        mModelLoaded = false;
        return false;
    }

    // Query input/output counts
    rknn_input_output_num io_num;
    ret = rknn_query(mCtx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret < 0) {
        qWarning() << "RKNNWorker[" << mCameraId << "]: rknn_query IO num failed, ret=" << ret;
        rknn_destroy(mCtx);
        mCtx = 0;
        mModelLoaded = false;
        return false;
    }

    mNumOutputs = io_num.n_output;
    mInputWidth = inputWidth;
    mInputHeight = inputHeight;
    mModelLoaded = true;

    qDebug() << "RKNNWorker[" << mCameraId << "]: Model loaded from" << modelPath
             << "inputs:" << io_num.n_input << "outputs:" << io_num.n_output
             << "input size:" << inputWidth << "x" << inputHeight;

    return true;
}

void RKNNInferenceWorker::setConfidenceThreshold(float thresh)
{
    QMutexLocker locker(&mMutex);
    mConfThreshold = thresh;
}

void RKNNInferenceWorker::setNmsThreshold(float thresh)
{
    QMutexLocker locker(&mMutex);
    mNmsThreshold = thresh;
}

void RKNNInferenceWorker::runInference(const FrameData& frame)
{
    if (!mModelLoaded) return;
    if (frame.image.empty()) return;

    qint64 t0 = QDateTime::currentMSecsSinceEpoch();

    // Preprocess: letterbox resize to input size, BGR -> RGB
    cv::Mat inputImg = preprocess(frame.image);

    // Set input
    rknn_input inputs[1];
    memset(inputs, 0, sizeof(inputs));
    inputs[0].index = 0;
    inputs[0].type = RKNN_TENSOR_UINT8;
    inputs[0].fmt = RKNN_TENSOR_NHWC;
    inputs[0].buf = inputImg.data;
    inputs[0].size = inputImg.total() * inputImg.elemSize();
    inputs[0].pass_through = 0;

    int ret = rknn_inputs_set(mCtx, 1, inputs);
    if (ret < 0) {
        emit error(QString("RKNNWorker[%1]: rknn_inputs_set failed, ret=%2").arg(mCameraId).arg(ret));
        return;
    }

    // Run inference
    ret = rknn_run(mCtx, nullptr);
    if (ret < 0) {
        emit error(QString("RKNNWorker[%1]: rknn_run failed, ret=%2").arg(mCameraId).arg(ret));
        return;
    }

    // Get outputs (request float format)
    QVector<rknn_output> outputs(mNumOutputs);
    memset(outputs.data(), 0, sizeof(rknn_output) * mNumOutputs);
    for (int i = 0; i < mNumOutputs; i++) {
        outputs[i].want_float = 1;
    }

    ret = rknn_outputs_get(mCtx, mNumOutputs, outputs.data(), nullptr);
    if (ret < 0) {
        emit error(QString("RKNNWorker[%1]: rknn_outputs_get failed, ret=%2").arg(mCameraId).arg(ret));
        return;
    }

    // Postprocess
    cv::Size originalSize(frame.image.cols, frame.image.rows);
    QVector<Detection> detections = postprocess(outputs.data(), mNumOutputs, originalSize);

    // Release outputs
    rknn_outputs_release(mCtx, mNumOutputs, outputs.data());

    // Apply confidence filter
    float confThresh;
    {
        QMutexLocker locker(&mMutex);
        confThresh = mConfThreshold;
    }
    for (auto& det : detections) {
        det.filtered = (det.confidence < confThresh);
    }

    qint64 t1 = QDateTime::currentMSecsSinceEpoch();

    // Build result
    InferenceResult result;
    result.cameraId        = mCameraId;
    result.timestamp       = frame.timestamp;
    result.frameIndex      = frame.frameIndex;
    result.detections      = detections;
    result.inferenceTimeMs = static_cast<float>(t1 - t0);
    result.frameWidth      = frame.image.cols;
    result.frameHeight     = frame.image.rows;

    emit inferenceFinished(result);
}

cv::Mat RKNNInferenceWorker::preprocess(const cv::Mat& frame)
{
    // Letterbox resize preserving aspect ratio, then convert BGR -> RGB
    float scale = std::min(
        static_cast<float>(mInputWidth) / frame.cols,
        static_cast<float>(mInputHeight) / frame.rows);

    int newW = static_cast<int>(frame.cols * scale);
    int newH = static_cast<int>(frame.rows * scale);

    cv::Mat resized;
    cv::resize(frame, resized, cv::Size(newW, newH));

    // Letterbox padding (gray 114)
    int padW = mInputWidth - newW;
    int padH = mInputHeight - newH;
    int padLeft = padW / 2;
    int padTop = padH / 2;

    cv::Mat letterbox;
    cv::copyMakeBorder(resized, letterbox,
                       padTop, padH - padTop,
                       padLeft, padW - padLeft,
                       cv::BORDER_CONSTANT, cv::Scalar(114, 114, 114));

    // BGR -> RGB (RKNN expects RGB for NHWC uint8 input)
    cv::Mat rgb;
    cv::cvtColor(letterbox, rgb, cv::COLOR_BGR2RGB);

    return rgb;
}

QVector<Detection> RKNNInferenceWorker::postprocess(rknn_output* outputs, int numOutputs, const cv::Size& originalSize)
{
    QVector<Detection> detections;

    float nmsThresh;
    {
        QMutexLocker locker(&mMutex);
        nmsThresh = mNmsThreshold;
    }

    // Letterbox scale and padding
    float scale = std::min(
        static_cast<float>(mInputWidth) / originalSize.width,
        static_cast<float>(mInputHeight) / originalSize.height);
    int padLeft = (mInputWidth - static_cast<int>(originalSize.width * scale)) / 2;
    int padTop = (mInputHeight - static_cast<int>(originalSize.height * scale)) / 2;

    // Find the largest output tensor (YOLOv8: [1, 4+numClasses, 8400])
    int mainIdx = 0;
    int maxSize = 0;
    for (int i = 0; i < numOutputs; i++) {
        int size = outputs[i].size / sizeof(float);
        if (size > maxSize) {
            maxSize = size;
            mainIdx = i;
        }
    }

    float* outputData = static_cast<float*>(outputs[mainIdx].buf);
    int totalElements = outputs[mainIdx].size / sizeof(float);

    // Determine format: YOLOv8 [1, 4+numClasses, numDetections]
    // We need to figure out numChannels and numDetections
    // Common: 8400 detections for 640x640 input
    int numDetections = 8400;
    int numChannels = totalElements / numDetections;

    if (numChannels < 5) {
        qWarning() << "RKNNWorker[" << mCameraId << "]: Invalid output shape, channels=" << numChannels;
        return detections;
    }

    int numClasses = numChannels - 4;

    float confThresh;
    {
        QMutexLocker locker(&mMutex);
        confThresh = mConfThreshold;
    }

    for (int i = 0; i < numDetections; ++i) {
        // YOLOv8 layout: [channels][detections] -> channel * numDetections + i
        float cx = outputData[0 * numDetections + i];
        float cy = outputData[1 * numDetections + i];
        float w  = outputData[2 * numDetections + i];
        float h  = outputData[3 * numDetections + i];

        // Find best class
        float maxLogit = -1e9f;
        int bestClass = 0;
        for (int c = 0; c < numClasses; ++c) {
            float logit = outputData[(4 + c) * numDetections + i];
            if (logit > maxLogit) {
                maxLogit = logit;
                bestClass = c;
            }
        }

        // Sigmoid activation
        float confidence = 1.0f / (1.0f + std::exp(-maxLogit));

        if (confidence < confThresh) continue;

        // Scale back to original image coordinates
        float origCx = (cx - padLeft) / scale;
        float origCy = (cy - padTop) / scale;
        float origW = w / scale;
        float origH = h / scale;

        Detection det;
        det.classId = bestClass;
        det.confidence = confidence;
        det.bbox = BoundingBox(origCx, origCy, origW, origH);
        det.filtered = false;
        detections.append(det);
    }

    detections = applyNMS(detections);
    return detections;
}

QVector<Detection> RKNNInferenceWorker::applyNMS(QVector<Detection>& detections)
{
    if (detections.size() <= 1) return detections;

    float nmsThresh;
    {
        QMutexLocker locker(&mMutex);
        nmsThresh = mNmsThreshold;
    }

    // Sort by confidence descending
    std::sort(detections.begin(), detections.end(),
              [](const Detection& a, const Detection& b) {
                  return a.confidence > b.confidence;
              });

    QVector<Detection> result;
    QVector<bool> suppressed(detections.size(), false);

    for (int i = 0; i < detections.size(); ++i) {
        if (suppressed[i]) continue;
        result.append(detections[i]);

        for (int j = i + 1; j < detections.size(); ++j) {
            if (suppressed[j]) continue;
            if (detections[i].classId != detections[j].classId) continue;

            float iou = detections[i].bbox.iou(detections[j].bbox);
            if (iou > nmsThresh) {
                suppressed[j] = true;
            }
        }
    }

    return result;
}

// ============================================================
// RKNNInferenceEngine (thread wrapper)
// ============================================================

RKNNInferenceEngine::RKNNInferenceEngine(int cameraId, QObject* parent)
    : QObject(parent)
    , mThread(new QThread(this))
    , mWorker(new RKNNInferenceWorker(cameraId))
{
    mWorker->moveToThread(mThread);

    connect(this, &RKNNInferenceEngine::requestInference,
            mWorker, &RKNNInferenceWorker::runInference, Qt::QueuedConnection);
    connect(mWorker, &RKNNInferenceWorker::inferenceFinished,
            this, &RKNNInferenceEngine::inferenceFinished, Qt::QueuedConnection);
    connect(mWorker, &RKNNInferenceWorker::error,
            this, &RKNNInferenceEngine::error, Qt::QueuedConnection);

    connect(mThread, &QThread::finished, mWorker, &QObject::deleteLater);

    mThread->start();
}

RKNNInferenceEngine::~RKNNInferenceEngine()
{
    mThread->quit();
    mThread->wait();
}

bool RKNNInferenceEngine::loadModel(const QString& modelPath, int inputWidth, int inputHeight)
{
    bool result = false;
    QMetaObject::invokeMethod(mWorker, [&]() {
        result = mWorker->loadModel(modelPath, inputWidth, inputHeight);
    }, Qt::BlockingQueuedConnection);
    return result;
}

void RKNNInferenceEngine::setConfidenceThreshold(float thresh)
{
    QMetaObject::invokeMethod(mWorker, [=]() {
        mWorker->setConfidenceThreshold(thresh);
    }, Qt::QueuedConnection);
}

void RKNNInferenceEngine::setNmsThreshold(float thresh)
{
    QMetaObject::invokeMethod(mWorker, [=]() {
        mWorker->setNmsThreshold(thresh);
    }, Qt::QueuedConnection);
}
