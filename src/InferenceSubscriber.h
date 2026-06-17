#ifndef INFERENCESUBSCRIBER_H
#define INFERENCESUBSCRIBER_H

#include <QObject>
#include <QVector>
#include <QMap>
#include <QTimer>
#include "Types.h"
#include "MQTTClient.h"

/// Subscribes to MQTT topics and decodes inference results from JSON
/// Used by display device to receive detections from remote inference engines.
///
/// Rate-limiting: incoming MQTT messages are decoded and stored per camera,
/// then emitted in batches on a fixed-interval timer (default 200ms).
/// This prevents the main-thread event queue from being flooded when the
/// bridge publishes at high frame rates, which causes burst/silence display.
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

    /// Set throttle interval for batching inference results (0 = no throttle)
    void setThrottleInterval(int ms);

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
    void flushResults();

private:
    /// Decode JSON payload to InferenceResult
    bool decodeInferenceResult(const QByteArray& json, InferenceResult& result);

    /// Decode channel discovery JSON
    bool decodeChannelDiscovery(const QByteArray& json, QVector<ChannelInfo>& channels);

    MQTTClient* mMqttClient;
    QStringList mSubscribedTopics;
    QString mDiscoveryTopic;

    // Rate-limiting: store latest result per camera, emit in batches
    QTimer* mThrottleTimer;
    int mThrottleIntervalMs = 200;
    QMap<int, InferenceResult> mLatestResults;
};

#endif // INFERENCESUBSCRIBER_H
