#ifndef MQTTCLIENT_H
#define MQTTCLIENT_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QMutex>
#include <mqtt/async_client.h>

class MQTTClient : public QObject
{
    Q_OBJECT

public:
    explicit MQTTClient(QObject* parent = nullptr);
    ~MQTTClient() override;

    /// Connect to MQTT broker
    bool connectToBroker(const QString& brokerUrl,
                         const QString& clientId,
                         const QString& username = QString(),
                         const QString& password = QString());

    /// Disconnect from broker
    void disconnect();

    /// Check if connected
    bool isConnected() const;

    /// Publish message to topic
    bool publish(const QString& topic, const QByteArray& payload, int qos = 1);

    /// Subscribe to topic
    bool subscribe(const QString& topic, int qos = 1);

    /// Unsubscribe from topic
    bool unsubscribe(const QString& topic);

signals:
    /// Emitted when connection is established
    void connected();

    /// Emitted when connection is lost
    void disconnected();

    /// Emitted when a message arrives
    void messageReceived(const QString& topic, const QByteArray& payload);

    /// Emitted on errors
    void error(const QString& message);

private:
    class Callback;
    friend class Callback;

    mqtt::async_client* mClient;
    Callback* mCallback;
    QString mBrokerUrl;
    QString mClientId;
    mutable QMutex mMutex;
    bool mConnected;
};

/// Internal callback handler for Paho MQTT events
class MQTTClient::Callback : public mqtt::callback
{
public:
    explicit Callback(MQTTClient* parent);

    void connected(const std::string& cause) override;
    void connection_lost(const std::string& cause) override;
    void message_arrived(mqtt::const_message_ptr msg) override;
    void delivery_complete(mqtt::delivery_token_ptr token) override;

private:
    MQTTClient* mParent;
};

#endif // MQTTCLIENT_H
