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

    /// Disconnect from broker
    void disconnect();

    /// Check if connected
    bool isConnected() const;

signals:
    /// Emitted when inference result is received and decoded
    void inferenceFinished(const InferenceResult& result);

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

    MQTTClient* mMqttClient;
    QStringList mSubscribedTopics;
};

#endif // INFERENCESUBSCRIBER_H
