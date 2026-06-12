#include "InferencePublisher.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

InferencePublisher::InferencePublisher(QObject* parent)
    : QObject(parent)
    , mTopicPrefix("inference/camera")
{
    mMqttClient = new MQTTClient(this);

    connect(mMqttClient, &MQTTClient::error,
            this, &InferencePublisher::error);
}

InferencePublisher::~InferencePublisher()
{
    disconnect();
}

bool InferencePublisher::connectToBroker(const QString& brokerUrl,
                                          const QString& clientId,
                                          const QString& username,
                                          const QString& password)
{
    return mMqttClient->connectToBroker(brokerUrl, clientId, username, password);
}

void InferencePublisher::disconnect()
{
    mMqttClient->disconnect();
}

bool InferencePublisher::isConnected() const
{
    return mMqttClient->isConnected();
}

void InferencePublisher::onInferenceFinished(const InferenceResult& result)
{
    if (!mMqttClient->isConnected()) {
        qWarning() << "InferencePublisher: Not connected, cannot publish result";
        return;
    }

    QByteArray payload = encodeInferenceResult(result);
    if (payload.isEmpty()) {
        qWarning() << "InferencePublisher: Failed to encode inference result";
        return;
    }

    QString topic = buildTopicName(result.cameraId);
    if (!mMqttClient->publish(topic, payload)) {
        qWarning() << "InferencePublisher: Failed to publish to" << topic;
    }
}

QByteArray InferencePublisher::encodeInferenceResult(const InferenceResult& result)
{
    QJsonObject obj;
    obj["camera_id"] = result.cameraId;
    obj["timestamp"] = static_cast<qint64>(result.timestamp);
    obj["frame_index"] = static_cast<qint64>(result.frameIndex);
    obj["inference_time_ms"] = static_cast<double>(result.inferenceTimeMs);
    obj["normalized"] = true;

    float fw = static_cast<float>(result.frameWidth > 0 ? result.frameWidth : 1);
    float fh = static_cast<float>(result.frameHeight > 0 ? result.frameHeight : 1);

    QJsonArray detsArray;
    for (const Detection& det : result.detections) {
        if (det.filtered) continue;

        QJsonObject detObj;
        detObj["class_id"] = det.classId;
        detObj["confidence"] = static_cast<double>(det.confidence);

        QJsonObject bboxObj;
        bboxObj["cx"] = static_cast<double>(det.bbox.x / fw);
        bboxObj["cy"] = static_cast<double>(det.bbox.y / fh);
        bboxObj["w"] = static_cast<double>(det.bbox.width / fw);
        bboxObj["h"] = static_cast<double>(det.bbox.height / fh);
        detObj["bbox"] = bboxObj;

        detsArray.append(detObj);
    }
    obj["detections"] = detsArray;

    QJsonDocument doc(obj);
    return doc.toJson(QJsonDocument::Compact);
}

QString InferencePublisher::buildTopicName(int cameraId) const
{
    return QString("%1/%2/detections").arg(mTopicPrefix).arg(cameraId);
}
