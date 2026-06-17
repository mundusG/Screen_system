#include "InferenceSubscriber.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

static bool matchesMqttPattern(const QString& pattern, const QString& topic)
{
    QStringList pat = pattern.split('/');
    QStringList top = topic.split('/');
    if (pat.size() != top.size()) return false;
    for (int i = 0; i < pat.size(); ++i) {
        if (pat[i] == QLatin1String("+")) continue;
        if (pat[i] != top[i]) return false;
    }
    return true;
}

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

    // Add discovery topic if set
    if (!mDiscoveryTopic.isEmpty() && !mSubscribedTopics.contains(mDiscoveryTopic)) {
        mSubscribedTopics.prepend(mDiscoveryTopic);
    }

    if (!mMqttClient->connectToBroker(brokerUrl, clientId, username, password)) {
        return false;
    }

    qDebug() << "InferenceSubscriber: MQTT connection initiated, will subscribe to" << topics.size() << "topics on connect";
    return true;
}

void InferenceSubscriber::setDiscoveryTopic(const QString& topic)
{
    mDiscoveryTopic = topic;
}

bool InferenceSubscriber::subscribeTopic(const QString& topic)
{
    if (!mMqttClient->isConnected()) {
        mSubscribedTopics.append(topic);
        return true;
    }
    if (mMqttClient->subscribe(topic)) {
        mSubscribedTopics.append(topic);
        qDebug() << "InferenceSubscriber: Dynamically subscribed to" << topic;
        return true;
    }
    qWarning() << "InferenceSubscriber: Failed to subscribe to" << topic;
    return false;
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
    // Channel discovery message
    if (!mDiscoveryTopic.isEmpty() && matchesMqttPattern(mDiscoveryTopic, topic)) {
        QVector<ChannelInfo> channels;
        if (decodeChannelDiscovery(payload, channels)) {
            qDebug() << "InferenceSubscriber: Discovered" << channels.size() << "channel(s)";
            emit channelsDiscovered(channels);
        } else {
            qWarning() << "InferenceSubscriber: Failed to decode channel discovery";
        }
        return;
    }

    // Inference result message
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

bool InferenceSubscriber::decodeChannelDiscovery(const QByteArray& json, QVector<ChannelInfo>& channels)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return false;

    QJsonArray arr = doc.object()["channels"].toArray();
    if (arr.isEmpty())
        return false;

    channels.clear();
    channels.reserve(arr.size());
    for (const QJsonValue& val : arr) {
        QJsonObject obj = val.toObject();
        ChannelInfo info;
        info.cameraId       = obj["camera_id"].toInt();
        info.chid            = obj["chid"].toInt();
        info.name            = obj["name"].toString(QString("camera_%1").arg(info.cameraId));
        info.previewUrl      = obj["preview_url"].toString();
        info.inferenceTopic  = obj["inference_topic"].toString();
        channels.append(info);
    }
    return true;
}
