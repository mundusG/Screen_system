#include "InferenceSubscriber.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

InferenceSubscriber::InferenceSubscriber(QObject* parent)
    : QObject(parent)
{
    mMqttClient = new MQTTClient(this);

    connect(mMqttClient, &MQTTClient::connected,
            this, &InferenceSubscriber::onMqttConnected);
    connect(mMqttClient, &MQTTClient::disconnected,
            this, &InferenceSubscriber::onMqttDisconnected);
    connect(mMqttClient, &MQTTClient::messageReceived,
            this, &InferenceSubscriber::onMessageReceived);
    connect(mMqttClient, &MQTTClient::error,
            this, &InferenceSubscriber::onMqttError);
}

InferenceSubscriber::~InferenceSubscriber()
{
    disconnect();
}

bool InferenceSubscriber::connectAndSubscribe(const QString& brokerUrl,
                                               const QString& clientId,
                                               const QStringList& topics,
                                               const QString& username,
                                               const QString& password)
{
    mSubscribedTopics = topics;

    if (!mMqttClient->connectToBroker(brokerUrl, clientId, username, password)) {
        return false;
    }

    // Topics will be subscribed when connection is established (via onMqttConnected)
    qDebug() << "InferenceSubscriber: MQTT connection initiated, will subscribe to" << topics.size() << "topics on connect";
    return true;
}

void InferenceSubscriber::disconnect()
{
    mSubscribedTopics.clear();
    mMqttClient->disconnect();
}

bool InferenceSubscriber::isConnected() const
{
    return mMqttClient->isConnected();
}

void InferenceSubscriber::onMessageReceived(const QString& topic, const QByteArray& payload)
{
    qDebug() << "InferenceSubscriber: Received message from" << topic << "size:" << payload.size();

    InferenceResult result;
    if (decodeInferenceResult(payload, result)) {
        qDebug() << "InferenceSubscriber: Decoded result for camera" << result.cameraId
                 << "detections:" << result.detections.size();
        emit inferenceFinished(result);
    } else {
        qWarning() << "InferenceSubscriber: Failed to decode message from" << topic;
    }
}

void InferenceSubscriber::onMqttConnected()
{
    qDebug() << "InferenceSubscriber: MQTT connected";

    // Subscribe to all topics now that connection is established
    for (const QString& topic : mSubscribedTopics) {
        if (!mMqttClient->subscribe(topic)) {
            qWarning() << "InferenceSubscriber: Failed to subscribe to" << topic;
        } else {
            qDebug() << "InferenceSubscriber: Subscribed to" << topic;
        }
    }
}

void InferenceSubscriber::onMqttDisconnected()
{
    qWarning() << "InferenceSubscriber: MQTT disconnected";
}

void InferenceSubscriber::onMqttError(const QString& message)
{
    qWarning() << "InferenceSubscriber: MQTT error:" << message;
    emit error(message);
}

bool InferenceSubscriber::decodeInferenceResult(const QByteArray& json, InferenceResult& result)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "InferenceSubscriber: JSON parse error:" << parseError.errorString();
        return false;
    }

    if (!doc.isObject()) {
        qWarning() << "InferenceSubscriber: JSON root is not an object";
        return false;
    }

    QJsonObject obj = doc.object();

    // Extract fields
    result.cameraId = obj["camera_id"].toInt(-1);
    result.timestamp = obj["timestamp"].toVariant().toLongLong();
    result.frameIndex = obj["frame_index"].toVariant().toLongLong();
    result.inferenceTimeMs = static_cast<float>(obj["inference_time_ms"].toDouble());

    if (result.cameraId < 0) {
        qWarning() << "InferenceSubscriber: Invalid camera_id";
        return false;
    }

    // Extract detections array
    bool normalized = obj["normalized"].toBool(false);
    QJsonArray detsArray = obj["detections"].toArray();
    result.detections.clear();
    result.detections.reserve(detsArray.size());

    for (const QJsonValue& detVal : detsArray) {
        QJsonObject detObj = detVal.toObject();

        Detection det;
        det.classId = detObj["class_id"].toInt();
        det.confidence = static_cast<float>(detObj["confidence"].toDouble());
        det.filtered = false;
        det.normalized = normalized;

        QJsonObject bboxObj = detObj["bbox"].toObject();
        float x = static_cast<float>(bboxObj["cx"].toDouble());
        float y = static_cast<float>(bboxObj["cy"].toDouble());
        float width = static_cast<float>(bboxObj["w"].toDouble());
        float height = static_cast<float>(bboxObj["h"].toDouble());
        det.bbox = BoundingBox(x, y, width, height);
        det.className = detObj["class_name"].toString();

        result.detections.append(det);
    }

    return true;
}
