#include "InferenceEngine.h"
#include <QDebug>
#include <QDateTime>
#include <algorithm>
#include <numeric>
#include <cmath>

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

    // Debug: check input blob stats
    {
        static int frameCount = 0;
        frameCount++;
        if (frameCount <= 3) {
            const float* blobData = blob.ptr<float>();
            float minV = 1e9f, maxV = -1e9f;
            for (int i = 0; i < std::min(10000, (int)blob.total()); ++i) {
                minV = std::min(minV, blobData[i]);
                maxV = std::max(maxV, blobData[i]);
            }
            qWarning() << "InferenceWorker[" << mCameraId << "]: Frame" << frameCount
                       << "Input blob shape=[" << blob.size[0] << "," << blob.size[1] << "," << blob.size[2] << "," << blob.size[3] << "]"
                       << "value_range=[" << minV << "," << maxV << "]";
        }
    }

    // Forward pass
    std::vector<cv::Mat> outputs;
    {
        QMutexLocker locker(&mMutex);
        try {
            mNet.setInput(blob);
            auto outNames = mNet.getUnconnectedOutLayersNames();
            mNet.forward(outputs, outNames);
        }
        catch (const cv::Exception& e) {
            qWarning() << "InferenceWorker[" << mCameraId << "]: Forward pass error:" << e.what();
            emit error(QString("Camera %1: Inference failed - %2").arg(mCameraId).arg(e.what()));
            return;
        }
    }

    // Postprocess
    QVector<Detection> detections = postprocess(outputs, cv::Size(frame.image.cols, frame.image.rows));

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
    result.frameWidth      = frame.image.cols;
    result.frameHeight     = frame.image.rows;

    emit inferenceFinished(result);
}

void InferenceWorker::reloadModel(const QString& modelPath, int inputWidth, int inputHeight)
{
    loadModel(modelPath, inputWidth, inputHeight);
}

cv::Mat InferenceWorker::preprocess(const cv::Mat& frame)
{
    // Debug input
    static int callCount = 0;
    callCount++;
    if (callCount <= 3) {
        qWarning() << "InferenceWorker[" << mCameraId << "]: Preprocess call" << callCount
                   << "input frame size:" << frame.cols << "x" << frame.rows
                   << "channels:" << frame.channels()
                   << "type:" << frame.type();
    }

    // Resize with letterbox to preserve aspect ratio
    cv::Mat resized, letterbox;
    float scale = std::min(
        static_cast<float>(mInputWidth)  / frame.cols,
        static_cast<float>(mInputHeight) / frame.rows);

    int newW = static_cast<int>(frame.cols * scale);
    int newH = static_cast<int>(frame.rows * scale);

    if (callCount <= 3) {
        qWarning() << "  scale=" << scale << "newW=" << newW << "newH=" << newH;
    }

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

QVector<Detection> InferenceWorker::postprocess(const std::vector<cv::Mat>& outputs, const cv::Size& originalSize)
{
    QVector<Detection> detections;
    float confThresh, nmsThresh;
    {
        QMutexLocker locker(&mMutex);
        confThresh = mConfThreshold;
        nmsThresh  = mNmsThreshold;
    }

    float scale = std::min(
        static_cast<float>(mInputWidth)  / originalSize.width,
        static_cast<float>(mInputHeight) / originalSize.height);
    int padLeft = (mInputWidth  - static_cast<int>(originalSize.width  * scale)) / 2;
    int padTop  = (mInputHeight - static_cast<int>(originalSize.height * scale)) / 2;

    // Detect output format
    bool isRawFeatureMap = (outputs.size() >= 3 && outputs[0].dims == 4);

    if (isRawFeatureMap) {
        static bool warned = false;
        if (!warned) {
            qWarning() << "InferenceWorker[" << mCameraId
                       << "]: Raw feature map output detected. Anchor-based decode may be inaccurate."
                       << "Please re-export model with decode included:"
                       << "YOLOv5: python export.py --weights best.pt --include onnx"
                       << "YOLOv8: yolo export model=best.pt format=onnx";
            warned = true;
        }
        return detections;
    }

    // Debug: print output shape once
    {
        static bool logged = false;
        if (!logged) {
            qWarning() << "InferenceWorker[" << mCameraId << "]: Model output:";
            for (size_t i = 0; i < outputs.size(); ++i) {
                QString s = QString("  output[%1] shape: [").arg(i);
                for (int d = 0; d < outputs[i].dims; ++d) {
                    if (d > 0) s += ", ";
                    s += QString::number(outputs[i].size[d]);
                }
                s += "]";
                qWarning().noquote() << s;
            }
            logged = true;
        }
    }

    // Find the main output tensor
    cv::Mat output;
    for (const auto& o : outputs) {
        if (static_cast<int>(o.total()) > static_cast<int>(output.total())) output = o;
    }

    // Debug: print original output info
    {
        static bool logged = false;
        if (!logged) {
            QString s = QString("InferenceWorker[%1]: Original output dims=%2, shape=[").arg(mCameraId).arg(output.dims);
            for (int d = 0; d < output.dims; ++d) {
                if (d > 0) s += ",";
                s += QString::number(output.size[d]);
            }
            s += "], total=" + QString::number(output.total());
            qWarning().noquote() << s;

            // Check data range
            const float* rawPtr = output.ptr<float>();
            float minV = 1e9f, maxV = -1e9f;
            int nonZero = 0;
            for (size_t i = 0; i < std::min(static_cast<size_t>(100000), output.total()); ++i) {
                float v = rawPtr[i];
                minV = std::min(minV, v);
                maxV = std::max(maxV, v);
                if (std::abs(v) > 0.001f) nonZero++;
            }
            qWarning() << "  Value range (first 100k):" << minV << "to" << maxV << "nonZero=" << nonZero;
            logged = true;
        }
    }

    // Squeeze batch dimensions
    cv::Mat out = output;
    while (out.dims > 3 && out.size[0] == 1) {
        std::vector<int> newShape;
        for (int i = 1; i < out.dims; ++i)
            newShape.push_back(out.size[i]);
        out = out.reshape(1, newShape);
    }

    // Debug: print raw output stats before transpose
    {
        static bool logged = false;
        if (!logged && out.dims == 3) {
            const float* rawData = out.ptr<float>();
            int dim1 = out.size[1], dim2 = out.size[2];
            float minVal = 1e9f, maxVal = -1e9f;
            int nonZeroCount = 0;
            for (int i = 0; i < dim1 * dim2; ++i) {
                float v = rawData[i];
                minVal = std::min(minVal, v);
                maxVal = std::max(maxVal, v);
                if (std::abs(v) > 0.001f) nonZeroCount++;
            }
            qWarning() << "InferenceWorker[" << mCameraId << "]: Raw output before transpose:"
                       << "shape=[" << dim1 << "," << dim2 << "]"
                       << "value_range=[" << minVal << "," << maxVal << "]"
                       << "non_zero=" << nonZeroCount << "/" << (dim1*dim2);
            logged = true;
        }
    }

    // Handle YOLOv8 output: [1, 4+numClasses, 8400]
    // Each column is a detection: [bbox(4), class_scores(120)]
    if (out.dims == 3 && out.size[0] == 1) {
        int numChannels = out.size[1];  // 124
        int numDetections = out.size[2]; // 8400

        if (numChannels < 5) {
            qWarning() << "InferenceWorker[" << mCameraId << "]: Invalid output channels:" << numChannels;
            return detections;
        }

        int numClasses = numChannels - 4;

        // Debug: verify 3D Mat data access
        {
            static bool logged = false;
            if (!logged) {
                const float* rawPtr = out.ptr<float>();
                // NCHW layout: index = batch * (C*H*W) + channel * (H*W) + spatial_index
                // For [1, 124, 8400]: index = 0 + channel * 8400 + col
                qWarning() << "  Testing 3D Mat access for det 0:";
                qWarning() << "    Via .at<float>(0,4,0):" << out.at<float>(0, 4, 0);
                qWarning() << "    Via raw ptr [4*8400 + 0]:" << rawPtr[4 * 8400 + 0];
                qWarning() << "    Via raw ptr [4*8400 + 1]:" << rawPtr[4 * 8400 + 1];
                qWarning() << "    Via raw ptr [4*8400 + 100]:" << rawPtr[4 * 8400 + 100];
                logged = true;
            }
        }

        // Process each detection (column)
        int passedThresh = 0;
        float maxConfSeen = 0.0f;
        for (int i = 0; i < numDetections; ++i) {
            // Extract bbox and class scores from column i
            float cx = out.at<float>(0, 0, i);
            float cy = out.at<float>(0, 1, i);
            float w  = out.at<float>(0, 2, i);
            float h  = out.at<float>(0, 3, i);

            // Find max class score (logit)
            float maxLogit = -1e9f;
            int bestClass = 0;
            for (int c = 0; c < numClasses; ++c) {
                float logit = out.at<float>(0, 4 + c, i);
                if (logit > maxLogit) {
                    maxLogit = logit;
                    bestClass = c;
                }
            }

            // Debug: log first detection's class scores
            static bool loggedScores = false;
            if (!loggedScores && i == 0) {
                QString s = "  Det 0 class scores [0:10]: [";
                for (int c = 0; c < std::min(numClasses, 10); ++c) {
                    if (c > 0) s += ", ";
                    s += QString::number(out.at<float>(0, 4 + c, i), 'f', 3);
                }
                s += "]";
                qWarning().noquote() << s;
                loggedScores = true;
            }

            // Apply sigmoid
            float confidence = 1.0f / (1.0f + std::exp(-maxLogit));

            // Debug: log first few detections
            static int logCount = 0;
            if (logCount < 5) {
                qWarning() << "  Det" << i << ": maxLogit=" << maxLogit << "confidence=" << confidence << "bestClass=" << bestClass;
                logCount++;
            }

            maxConfSeen = std::max(maxConfSeen, confidence);

            if (confidence < confThresh) continue;
            passedThresh++;

            // Scale back to original image
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

        int detBeforeNMS = detections.size();
        detections = applyNMS(detections);

        // Debug log
        {
            static int frameCount = 0;
            frameCount++;
            if (frameCount % 30 == 1) {
                qWarning() << "InferenceWorker[" << mCameraId << "]:"
                           << "detections=" << numDetections
                           << "classes=" << numClasses
                           << "passed_thresh=" << passedThresh
                           << "max_conf=" << maxConfSeen
                           << "before_nms=" << detBeforeNMS
                           << "after_nms=" << detections.size();
            }
        }

        return detections;
    }

    // Fallback: unsupported format
    qWarning() << "InferenceWorker[" << mCameraId << "]: Unsupported output format, dims=" << out.dims;
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
