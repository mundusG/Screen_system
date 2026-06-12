#include "MQTTClient.h"
#include <QDebug>
#include <QMutexLocker>

MQTTClient::MQTTClient(QObject* parent)
    : QObject(parent)
    , mClient(nullptr)
    , mCallback(nullptr)
    , mConnected(false)
{
}

MQTTClient::~MQTTClient()
{
    disconnect();
    delete mClient;
    delete mCallback;
}

bool MQTTClient::connectToBroker(const QString& brokerUrl,
                                  const QString& clientId,
                                  const QString& username,
                                  const QString& password)
{
    QMutexLocker locker(&mMutex);

    if (mClient) {
        qWarning() << "MQTTClient: Already connected or connecting";
        return false;
    }

    mBrokerUrl = brokerUrl;
    mClientId = clientId;

    try {
        mClient = new mqtt::async_client(brokerUrl.toStdString(), clientId.toStdString());
        mCallback = new Callback(this);
        mClient->set_callback(*mCallback);

        mqtt::connect_options connOpts;
        connOpts.set_keep_alive_interval(20);
        connOpts.set_clean_session(true);
        connOpts.set_automatic_reconnect(true);

        if (!username.isEmpty()) {
            connOpts.set_user_name(username.toStdString());
        }
        if (!password.isEmpty()) {
            connOpts.set_password(password.toStdString());
        }

        qDebug() << "MQTTClient: Connecting to" << brokerUrl << "as" << clientId;
        auto tok = mClient->connect(connOpts);
        tok->wait();

        mConnected = true;
        qDebug() << "MQTTClient: Connected successfully";
        emit connected();
        return true;
    }
    catch (const mqtt::exception& e) {
        qWarning() << "MQTTClient: Connection failed:" << e.what();
        delete mClient;
        delete mCallback;
        mClient = nullptr;
        mCallback = nullptr;
        emit error(QString("Connection failed: %1").arg(e.what()));
        return false;
    }
}

void MQTTClient::disconnect()
{
    QMutexLocker locker(&mMutex);

    if (!mClient) return;

    try {
        if (mClient->is_connected()) {
            qDebug() << "MQTTClient: Disconnecting...";
            auto tok = mClient->disconnect();
            tok->wait_for(std::chrono::seconds(5));
        }
        mConnected = false;
    }
    catch (const mqtt::exception& e) {
        qWarning() << "MQTTClient: Disconnect error:" << e.what();
    }

    delete mClient;
    delete mCallback;
    mClient = nullptr;
    mCallback = nullptr;

    emit disconnected();
}

bool MQTTClient::isConnected() const
{
    QMutexLocker locker(&mMutex);
    return mConnected && mClient && mClient->is_connected();
}

bool MQTTClient::publish(const QString& topic, const QByteArray& payload, int qos)
{
    QMutexLocker locker(&mMutex);

    if (!mClient || !mClient->is_connected()) {
        qWarning() << "MQTTClient: Cannot publish, not connected";
        return false;
    }

    try {
        mqtt::message_ptr pubmsg = mqtt::make_message(
            topic.toStdString(),
            payload.constData(),
            payload.size()
        );
        pubmsg->set_qos(qos);
        mClient->publish(pubmsg);
        return true;
    }
    catch (const mqtt::exception& e) {
        qWarning() << "MQTTClient: Publish failed:" << e.what();
        emit error(QString("Publish failed: %1").arg(e.what()));
        return false;
    }
}

bool MQTTClient::subscribe(const QString& topic, int qos)
{
    QMutexLocker locker(&mMutex);

    if (!mClient || !mClient->is_connected()) {
        qWarning() << "MQTTClient: Cannot subscribe, not connected";
        return false;
    }

    try {
        qDebug() << "MQTTClient: Subscribing to" << topic;
        auto tok = mClient->subscribe(topic.toStdString(), qos);
        tok->wait();
        qDebug() << "MQTTClient: Subscribed to" << topic;
        return true;
    }
    catch (const mqtt::exception& e) {
        qWarning() << "MQTTClient: Subscribe failed:" << e.what();
        emit error(QString("Subscribe failed: %1").arg(e.what()));
        return false;
    }
}

bool MQTTClient::unsubscribe(const QString& topic)
{
    QMutexLocker locker(&mMutex);

    if (!mClient || !mClient->is_connected()) {
        return false;
    }

    try {
        auto tok = mClient->unsubscribe(topic.toStdString());
        tok->wait();
        return true;
    }
    catch (const mqtt::exception& e) {
        qWarning() << "MQTTClient: Unsubscribe failed:" << e.what();
        return false;
    }
}

// ============================================================
// Callback implementation
// ============================================================

MQTTClient::Callback::Callback(MQTTClient* parent)
    : mParent(parent)
{
}

void MQTTClient::Callback::connected(const std::string& cause)
{
    qDebug() << "MQTTClient::Callback: Connected -" << QString::fromStdString(cause);
    QMutexLocker locker(&mParent->mMutex);
    mParent->mConnected = true;
    emit mParent->connected();
}

void MQTTClient::Callback::connection_lost(const std::string& cause)
{
    qWarning() << "MQTTClient::Callback: Connection lost -" << QString::fromStdString(cause);
    QMutexLocker locker(&mParent->mMutex);
    mParent->mConnected = false;
    emit mParent->disconnected();
}

void MQTTClient::Callback::message_arrived(mqtt::const_message_ptr msg)
{
    QString topic = QString::fromStdString(msg->get_topic());
    QByteArray payload(msg->get_payload_str().c_str(), static_cast<int>(msg->get_payload_str().size()));

    emit mParent->messageReceived(topic, payload);
}

void MQTTClient::Callback::delivery_complete(mqtt::delivery_token_ptr token)
{
    // Optional: track message delivery confirmation
}
