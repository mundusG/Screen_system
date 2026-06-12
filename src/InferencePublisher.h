#ifndef INFERENCEPUBLISHER_H
#define INFERENCEPUBLISHER_H

#include <QObject>
#include "Types.h"
#include "MQTTClient.h"

/// Publishes inference results to MQTT topics in JSON format
/// Used by inference devices (Rockchip edge) to send detections to display device
class InferencePublisher : public QObject
{
    Q_OBJECT

public:
    explicit InferencePublisher(QObject* parent = nullptr);
    ~InferencePublisher() override;

    /// Connect to MQTT broker
    bool connectToBroker(const QString& brokerUrl,
                         const QString& clientId,
                         const QString& username = QString(),
                         const QString& password = QString());

    /// Disconnect from broker
    void disconnect();

    /// Check if connected
    bool isConnected() const;

signals:
    /// Emitted on errors
    void error(const QString& message);

public slots:
    /// Publish inference result to MQTT topic
    void onInferenceFinished(const InferenceResult& result);

private:
    /// Encode InferenceResult to JSON payload
    QByteArray encodeInferenceResult(const InferenceResult& result);

    /// Build topic name for camera
    QString buildTopicName(int cameraId) const;

    MQTTClient* mMqttClient;
    QString mTopicPrefix;
};

#endif // INFERENCEPUBLISHER_H
