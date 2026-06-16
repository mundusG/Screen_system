#ifndef INFERENCESUBSCRIBER_H
#define INFERENCESUBSCRIBER_H

#include <QObject>
#include <QVector>
#include "Types.h"
#include "MQTTClient.h"

/// Subscribes to MQTT topics and decodes inference results from JSON
/// Used by display device to receive detections from remote inference engines
class InferenceSubscriber : public QObject
{
    Q_OBJECT

public:
    explicit InferenceSubscriber(QObject* parent = nullptr);
    ~InferenceSubscriber() override;

    /// Connect to MQTT broker and subscribe to topics
    bool connectAndSubscribe(const QString& brokerUrl,
                             const QString& clientId,
                             const QStringList& topics,
                             const QString& username = QString(),
                             const QString& password = QString());

    /// Set the discovery topic to subscribe to for channel auto-discovery
    void setDiscoveryTopic(const QString& topic);

    /// Subscribe to an additional topic at runtime
    bool subscribeTopic(const QString& topic);

    /// Disconnect from broker
    void disconnect();

    /// Check if connected
    bool isConnected() const;

signals:
    /// Emitted when inference result is received and decoded
    void inferenceFinished(const InferenceResult& result);

    /// Emitted when channel discovery message is received
    void channelsDiscovered(const QVector<ChannelInfo>& channels);

    /// Emitted on errors
    void error(const QString& message);

private slots:
    void onMessageReceived(const QString& topic, const QByteArray& payload);
    void onMqttConnected();
    void onMqttDisconnected();
    void onMqttError(const QString& message);

private:
    /// Decode JSON payload to InferenceResult
    bool decodeInferenceResult(const QByteArray& json, InferenceResult& result);

    /// Decode channel discovery JSON
    bool decodeChannelDiscovery(const QByteArray& json, QVector<ChannelInfo>& channels);

    MQTTClient* mMqttClient;
    QStringList mSubscribedTopics;
    QString mDiscoveryTopic;
};

#endif // INFERENCESUBSCRIBER_H
