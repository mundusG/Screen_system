#include "InferenceEngine.h"
#include <QDebug>
#include <QDateTime>
#include <algorithm>

// ============================================================
// InferenceWorker implementation
// ============================================================

InferenceWorker::InferenceWorker(int cameraId, QObject* parent)
    : QObject(parent)
    , mCameraId(cameraId)
    , mModelLoaded(false)
    , mInputWidth(640)
    , mInputHeight(640)
    , mConfThreshold(0.5f)
    , mNmsThreshold(0.45f)
{
}

InferenceWorker::~InferenceWorker()
{
    // OpenCV net cleanup
}

bool InferenceWorker::loadModel(const QString& modelPath, int inputWidth, int inputHeight)
{
    QMutexLocker locker(&mMutex);

    if (modelPath.isEmpty()) {
        qWarning() << "InferenceWorker[" << mCameraId << "]: Empty model path";
        return false;
    }

    try {
        mNet = cv::dnn::readNetFromONNX(modelPath.toStdString());
        mModelLoaded  = true;
        mInputWidth   = inputWidth;
        mInputHeight  = inputHeight;

        qDebug() << "InferenceWorker[" << mCameraId << "]: Model loaded from" << modelPath
                 << "input size:" << inputWidth << "x" << inputHeight;
    }
    catch (const cv::Exception& e) {
        qWarning() << "InferenceWorker[" << mCameraId << "]: Failed to load model:" << e.what();
        mModelLoaded = false;
        return false;
    }

    return true;
}

bool InferenceWorker::isModelLoaded() const
{
    QMutexLocker locker(&mMutex);
    return mModelLoaded;
}

void InferenceWorker::setConfidenceThreshold(float threshold)
{
    QMutexLocker locker(&mMutex);
    mConfThreshold = qBound(0.0f, threshold, 1.0f);
}

void InferenceWorker::setNmsThreshold(float threshold)
{
    QMutexLocker locker(&mMutex);
    mNmsThreshold = qBound(0.0f, threshold, 1.0f);
}

void InferenceWorker::runInference(const FrameData& frame)
{
    qint64 t0 = QDateTime::currentMSecsSinceEpoch();

    // Quick check
    {
        QMutexLocker locker(&mMutex);
        if (!mModelLoaded || frame.image.empty()) {
            return;
        }
    }

    // Preprocess
    cv::Mat blob = preprocess(frame.image);
    if (blob.empty()) {
        emit error(QString("Camera %1: Preprocessing failed").arg(mCameraId));
        return;
    }

    // Forward pass
    cv::Mat output;
    {
        QMutexLocker locker(&mMutex);
        try {
            mNet.setInput(blob);
            output = mNet.forward();
        }
        catch (const cv::Exception& e) {
            qWarning() << "InferenceWorker[" << mCameraId << "]: Forward pass error:" << e.what();
            emit error(QString("Camera %1: Inference failed - %2").arg(mCameraId).arg(e.what()));
            return;
        }
    }

    // Postprocess
    QVector<Detection> detections = postprocess(output, cv::Size(frame.image.cols, frame.image.rows));

    // Apply confidence threshold (mark low-confidence as filtered)
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

    emit inferenceFinished(result);
}

void InferenceWorker::reloadModel(const QString& modelPath, int inputWidth, int inputHeight)
{
    loadModel(modelPath, inputWidth, inputHeight);
}

cv::Mat InferenceWorker::preprocess(const cv::Mat& frame)
{
    // Resize with letterbox to preserve aspect ratio
    cv::Mat resized, letterbox;
    float scale = std::min(
        static_cast<float>(mInputWidth)  / frame.cols,
        static_cast<float>(mInputHeight) / frame.rows);

    int newW = static_cast<int>(frame.cols * scale);
    int newH = static_cast<int>(frame.rows * scale);

    cv::resize(frame, resized, cv::Size(newW, newH));

    // Letterbox padding (gray 114)
    int padW = mInputWidth  - newW;
    int padH = mInputHeight - newH;
    int padLeft = padW / 2;
    int padTop  = padH / 2;

    cv::copyMakeBorder(resized, letterbox,
                       padTop, padH - padTop,
                       padLeft, padW - padLeft,
                       cv::BORDER_CONSTANT, cv::Scalar(114, 114, 114));

    // Convert to blob: normalize to [0,1] with BGR order, no mean subtraction
    cv::Mat blob = cv::dnn::blobFromImage(
        letterbox, 1.0/255.0, cv::Size(mInputWidth, mInputHeight),
        cv::Scalar(), true, false);  // swapRB=true (BGR->RGB), crop=false

    return blob;
}

QVector<Detection> InferenceWorker::postprocess(const cv::Mat& output, const cv::Size& originalSize)
{
    // YOLOv8 ONNX output format: [1, N, 8400] where N = 4 + num_classes
    // Transposed: each row is [x, y, w, h, class_0_conf, class_1_conf, ...]
    //
    // We handle the common [1, 84, 8400] shape and the transposed [1, 8400, 84] shape

    QVector<Detection> detections;
    float confThresh, nmsThresh;
    {
        QMutexLocker locker(&mMutex);
        confThresh = mConfThreshold;
        nmsThresh  = mNmsThreshold;
    }

    // Get output dimensions
    const int dims = output.dims;
    int rows, cols, numClasses;

    if (dims == 3) {
        // Shape: [1, N, 8400] or [1, 8400, N]
        int dim0 = output.size[0];  // batch (1)
        int dim1 = output.size[1];  // N or 8400
        int dim2 = output.size[2];  // 8400 or N

        if (dim1 > dim2) {
            // [1, 8400, N] — transposed format
            rows       = dim1;  // 8400
            cols       = dim2;  // N
        } else {
            // [1, N, 8400] — standard format
            rows       = dim2;  // 8400
            cols       = dim1;  // N
        }
    } else if (dims == 2) {
        // Shape: [8400, N]
        rows = output.size[0];
        cols = output.size[1];
    } else {
        qWarning() << "InferenceWorker[" << mCameraId
                   << "]: Unexpected output dimensions:" << dims;
        return detections;
    }

    numClasses = cols - 4;  // first 4 are bbox coords
    if (numClasses < 1) {
        qWarning() << "InferenceWorker[" << mCameraId
                   << "]: Invalid output cols:" << cols << "(need at least 5)";
        return detections;
    }

    float scale = std::min(
        static_cast<float>(mInputWidth)  / originalSize.width,
        static_cast<float>(mInputHeight) / originalSize.height);
    int padLeft = (mInputWidth  - static_cast<int>(originalSize.width  * scale)) / 2;
    int padTop  = (mInputHeight - static_cast<int>(originalSize.height * scale)) / 2;

    // Parse detections
    const float* data = output.ptr<float>();

    for (int r = 0; r < rows; ++r) {
        const float* row = data + r * cols;

        // Find best class
        float maxConf = 0.0f;
        int   bestClass = 0;
        for (int c = 4; c < cols; ++c) {
            if (row[c] > maxConf) {
                maxConf   = row[c];
                bestClass = c - 4;
            }
        }

        if (maxConf < confThresh) continue;

        // YOLOv8 output: cx, cy, w, h (normalized to model input size)
        float cx = row[0];
        float cy = row[1];
        float w  = row[2];
        float h  = row[3];

        // Remove padding and scale back to original size
        float origCx = (cx - padLeft) / scale;
        float origCy = (cy - padTop)  / scale;
        float origW  = w / scale;
        float origH  = h / scale;

        // Clamp to image bounds
        origCx = std::max(0.0f, std::min(origCx, static_cast<float>(originalSize.width)));
        origCy = std::max(0.0f, std::min(origCy, static_cast<float>(originalSize.height)));
        origW  = std::max(1.0f, std::min(origW, static_cast<float>(originalSize.width)));
        origH  = std::max(1.0f, std::min(origH, static_cast<float>(originalSize.height)));

        Detection det;
        det.classId    = bestClass;
        det.confidence = maxConf;
        det.bbox       = BoundingBox(origCx, origCy, origW, origH);
        det.filtered   = false;

        detections.append(det);
    }

    // Apply NMS
    detections = applyNMS(detections);

    return detections;
}

QVector<Detection> InferenceWorker::applyNMS(QVector<Detection>& detections)
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

    QVector<Detection> kept;
    QVector<bool> suppressed(detections.size(), false);

    for (int i = 0; i < detections.size(); ++i) {
        if (suppressed[i]) continue;

        kept.append(detections[i]);

        // Suppress overlapping boxes of the same class
        for (int j = i + 1; j < detections.size(); ++j) {
            if (suppressed[j]) continue;
            if (detections[i].classId != detections[j].classId) continue;

            float iou = detections[i].bbox.iou(detections[j].bbox);
            if (iou > nmsThresh) {
                suppressed[j] = true;
            }
        }
    }

    return kept;
}

// ============================================================
// InferenceEngine (thread wrapper) implementation
// ============================================================

InferenceEngine::InferenceEngine(int cameraId, QObject* parent)
    : QObject(parent)
{
    mThread = new QThread(this);
    mWorker = new InferenceWorker(cameraId);  // no parent — will be moved

    mWorker->moveToThread(mThread);

    // Wire signals
    connect(mWorker, &InferenceWorker::inferenceFinished,
            this, &InferenceEngine::inferenceFinished,
            Qt::QueuedConnection);
    connect(mWorker, &InferenceWorker::error,
            this, &InferenceEngine::error,
            Qt::QueuedConnection);

    // Internal request queuing
    connect(this, &InferenceEngine::requestInference,
            mWorker, &InferenceWorker::runInference,
            Qt::QueuedConnection);
    connect(this, &InferenceEngine::requestReloadModel,
            mWorker, &InferenceWorker::reloadModel,
            Qt::QueuedConnection);

    mThread->setObjectName(QString("InferenceThread_%1").arg(cameraId));
    mThread->start();
}

InferenceEngine::~InferenceEngine()
{
    mThread->quit();
    mThread->wait(5000);
    if (mThread->isRunning()) {
        mThread->terminate();
        mThread->wait();
    }
    delete mWorker;
}

bool InferenceEngine::loadModel(const QString& modelPath, int inputWidth, int inputHeight)
{
    return mWorker->loadModel(modelPath, inputWidth, inputHeight);
}

bool InferenceEngine::isModelLoaded() const
{
    return mWorker->isModelLoaded();
}

void InferenceEngine::setConfidenceThreshold(float threshold)
{
    mWorker->setConfidenceThreshold(threshold);
}

void InferenceEngine::setNmsThreshold(float threshold)
{
    mWorker->setNmsThreshold(threshold);
}

int InferenceEngine::cameraId() const
{
    return mWorker->cameraId();
}
