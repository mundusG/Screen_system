#include "ConfigManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDebug>

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent)
{
}

bool ConfigManager::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "ConfigManager: Cannot open config file:" << filePath;
        return false;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "ConfigManager: JSON parse error:" << parseError.errorString();
        return false;
    }

    QJsonObject root = doc.object();

    // Read system-level settings
    QJsonObject systemObj = root["system"].toObject();
    QString systemMode = systemObj["mode"].toString("local_inference");

    // Read MQTT settings
    QJsonObject mqttObj = root["mqtt"].toObject();
    QString mqttBroker = mqttObj["broker"].toString();
    QString mqttClientId = mqttObj["client_id"].toString();
    QString mqttUsername = mqttObj["username"].toString();
    QString mqttPassword = mqttObj["password"].toString();
    QString discoveryTopic = mqttObj["discovery_topic"].toString("inference/bridge/+/channels");

    QVector<MqttSourceConfig> mqttSources;
    const QJsonArray mqttSourcesArray = root["mqtt_sources"].toArray();
    for (int i = 0; i < mqttSourcesArray.size(); ++i) {
        const QJsonObject sourceObj = mqttSourcesArray[i].toObject();
        MqttSourceConfig source;
        source.id = sourceObj["id"].toString(QString("mqtt_source_%1").arg(i + 1));
        source.enabled = sourceObj["enabled"].toBool(true);
        source.broker = sourceObj["broker"].toString();
        source.clientId = sourceObj["client_id"].toString();
        source.username = sourceObj["username"].toString();
        source.password = sourceObj["password"].toString();
        source.discoveryTopic = sourceObj["discovery_topic"].toString(discoveryTopic);
        source.cameraIdMin = sourceObj["camera_id_min"].toInt(0);
        source.cameraIdMax = sourceObj["camera_id_max"].toInt(7);

        if (source.cameraIdMin > source.cameraIdMax
            || (source.enabled && (source.broker.isEmpty() || source.clientId.isEmpty()))) {
            qWarning() << "ConfigManager: Ignoring invalid MQTT source" << source.id;
            continue;
        }
        mqttSources.append(source);
    }

    // Keep existing single-broker configurations operational.
    if (mqttSources.isEmpty()) {
        MqttSourceConfig source;
        source.id = QStringLiteral("legacy");
        source.broker = mqttBroker;
        source.clientId = mqttClientId;
        source.username = mqttUsername;
        source.password = mqttPassword;
        source.discoveryTopic = discoveryTopic;
        mqttSources.append(source);
    }

    QJsonArray cameras = root["cameras"].toArray();

    QMutexLocker locker(&mMutex);
    mConfigs.clear();
    mSystemMode = systemMode;
    mMqttBroker = mqttBroker;
    mMqttClientId = mqttClientId;
    mMqttUsername = mqttUsername;
    mMqttPassword = mqttPassword;
    mMqttSources = mqttSources;

    // Read services settings
    QJsonObject servicesObj = root["services"].toObject();
    QJsonObject bridgeObj = servicesObj["nn_bridge"].toObject();
    mBridgeEnabled = bridgeObj["enabled"].toBool(mSystemMode == "mqtt_subscribe");
    mBridgeScript  = bridgeObj["script"].toString("rk3576/nn_bridge.py");
    mBridgeConfig  = bridgeObj["config"].toString("rk3576/bridge_config.json");
    mBridgePython  = bridgeObj["python"].toString("python3");
    mCheckMosquitto = servicesObj["check_mosquitto"].toBool(true);

    // Read discovery topic
    mDiscoveryTopic = discoveryTopic;

    for (int i = 0; i < cameras.size(); ++i) {
        QJsonObject camObj = cameras[i].toObject();
        mConfigs.append(parseCameraJson(camObj, i));
    }

    qDebug() << "ConfigManager: Loaded" << mConfigs.size() << "camera configs, mode:" << systemMode
             << "MQTT sources:" << mMqttSources.size();
    emit allConfigsChanged();
    return true;
}

bool ConfigManager::saveToFile(const QString& filePath) const
{
    QMutexLocker locker(&mMutex);

    QJsonObject root;
    QJsonArray cameras;
    for (const auto& cfg : mConfigs) {
        cameras.append(cameraToJson(cfg));
    }
    root["cameras"] = cameras;

    QJsonDocument doc(root);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "ConfigManager: Cannot write config file:" << filePath;
        return false;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

int ConfigManager::cameraCount() const
{
    QMutexLocker locker(&mMutex);
    return mConfigs.size();
}

QVector<CameraConfig> ConfigManager::allConfigs() const
{
    QMutexLocker locker(&mMutex);
    return mConfigs;
}

CameraConfig ConfigManager::cameraConfig(int cameraId) const
{
    QMutexLocker locker(&mMutex);
    for (const auto& cfg : mConfigs) {
        if (cfg.cameraId == cameraId) {
            return cfg;
        }
    }
    // Return default if not found
    CameraConfig def;
    def.cameraId = cameraId;
    return def;
}

void ConfigManager::setCameraConfig(int cameraId, const CameraConfig& config)
{
    {
        QMutexLocker locker(&mMutex);
        for (int i = 0; i < mConfigs.size(); ++i) {
            if (mConfigs[i].cameraId == cameraId) {
                mConfigs[i] = config;
                break;
            }
        }
    }
    emit configChanged(cameraId);
}

void ConfigManager::setConfidenceThreshold(int cameraId, float threshold)
{
    {
        QMutexLocker locker(&mMutex);
        for (auto& cfg : mConfigs) {
            if (cfg.cameraId == cameraId) {
                cfg.confidenceThreshold = qBound(0.0f, threshold, 1.0f);
                break;
            }
        }
    }
    emit configChanged(cameraId);
}

void ConfigManager::setSmoothingAlpha(int cameraId, float alpha)
{
    {
        QMutexLocker locker(&mMutex);
        for (auto& cfg : mConfigs) {
            if (cfg.cameraId == cameraId) {
                cfg.smoothingAlpha = qBound(0.0f, alpha, 1.0f);
                break;
            }
        }
    }
    emit configChanged(cameraId);
}

void ConfigManager::setModelPath(int cameraId, const QString& path)
{
    {
        QMutexLocker locker(&mMutex);
        for (auto& cfg : mConfigs) {
            if (cfg.cameraId == cameraId) {
                cfg.modelPath = path;
                break;
            }
        }
    }
    emit configChanged(cameraId);
}

void ConfigManager::setCameraSource(int cameraId, const QString& source)
{
    {
        QMutexLocker locker(&mMutex);
        for (auto& cfg : mConfigs) {
            if (cfg.cameraId == cameraId) {
                cfg.source = source;
                break;
            }
        }
    }
    emit configChanged(cameraId);
}

void ConfigManager::setCameraEnabled(int cameraId, bool enabled)
{
    {
        QMutexLocker locker(&mMutex);
        for (auto& cfg : mConfigs) {
            if (cfg.cameraId == cameraId) {
                cfg.enabled = enabled;
                break;
            }
        }
    }
    emit configChanged(cameraId);
}

QColor ConfigManager::classColor(int cameraId, int classId) const
{
    QMutexLocker locker(&mMutex);
    for (const auto& cfg : mConfigs) {
        if (cfg.cameraId == cameraId) {
            if (cfg.classColors.contains(classId)) {
                return QColor(cfg.classColors[classId]);
            }
            // Fallback colors by class ID
            static const QColor fallbacks[] = {
                QColor("#00FF00"), // class 0: green
                QColor("#FF0000"), // class 1: red
                QColor("#FFFF00"), // class 2: yellow
                QColor("#00FFFF"), // class 3: cyan
                QColor("#FF00FF"), // class 4: magenta
                QColor("#FFA500"), // class 5: orange
                QColor("#800080"), // class 6: purple
                QColor("#FFFFFF"), // class 7: white
            };
            constexpr int N = sizeof(fallbacks) / sizeof(fallbacks[0]);
            return fallbacks[classId % N];
        }
    }
    return QColor("#00FF00"); // default green
}

QString ConfigManager::systemMode() const
{
    QMutexLocker locker(&mMutex);
    return mSystemMode;
}

QString ConfigManager::mqttBroker() const
{
    QMutexLocker locker(&mMutex);
    return mMqttBroker;
}

QString ConfigManager::mqttClientId() const
{
    QMutexLocker locker(&mMutex);
    return mMqttClientId;
}

QString ConfigManager::mqttUsername() const
{
    QMutexLocker locker(&mMutex);
    return mMqttUsername;
}

QString ConfigManager::mqttPassword() const
{
    QMutexLocker locker(&mMutex);
    return mMqttPassword;
}

bool ConfigManager::bridgeEnabled() const
{
    QMutexLocker locker(&mMutex);
    return mBridgeEnabled;
}

QString ConfigManager::bridgeScript() const
{
    QMutexLocker locker(&mMutex);
    return mBridgeScript;
}

QString ConfigManager::bridgeConfig() const
{
    QMutexLocker locker(&mMutex);
    return mBridgeConfig;
}

QString ConfigManager::bridgePython() const
{
    QMutexLocker locker(&mMutex);
    return mBridgePython;
}

bool ConfigManager::checkMosquitto() const
{
    QMutexLocker locker(&mMutex);
    return mCheckMosquitto;
}

QString ConfigManager::discoveryTopic() const
{
    QMutexLocker locker(&mMutex);
    return mDiscoveryTopic;
}

QVector<MqttSourceConfig> ConfigManager::mqttSources() const
{
    QMutexLocker locker(&mMutex);
    return mMqttSources;
}

QString ConfigManager::resolveConfigPath()
{
    // 1. Environment variable override
    const QString envPath = qEnvironmentVariable("SCREEN_SYSTEM_CONFIG");
    if (!envPath.isEmpty() && QFile::exists(envPath))
        return envPath;

    // 2. XDG config directory
    const QString xdgConfig = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                              + "/default_config.json";
    if (QFile::exists(xdgConfig))
        return xdgConfig;

    // 3. Beside executable (development build)
    const QString exeDir = QCoreApplication::applicationDirPath();
    const QString devPath = exeDir + "/config/default_config.json";
    if (QFile::exists(devPath))
        return devPath;

    // 4. Installed layout: <prefix>/bin/<exe> → <prefix>/share/ScreenInferenceSystem/config/
    const QString instPath = exeDir + "/../share/ScreenInferenceSystem/config/default_config.json";
    if (QFile::exists(instPath))
        return instPath;

    // Return the development path as default so saveToFile works even if nothing exists yet
    return devPath;
}

QString ConfigManager::resolveModelPath(const QString& modelPath)
{
    if (modelPath.isEmpty())
        return QString();

    // Already absolute → use as-is
    if (QDir::isAbsolutePath(modelPath))
        return modelPath;

    // 1. Environment variable
    const QString modelDir = qEnvironmentVariable("SCREEN_SYSTEM_MODEL_DIR");
    if (!modelDir.isEmpty()) {
        const QString envPath = QDir(modelDir).absoluteFilePath(modelPath);
        if (QFile::exists(envPath))
            return envPath;
    }

    // 2. Beside executable: <exe>/models/<path>
    const QString exeDir = QCoreApplication::applicationDirPath();
    const QString devPath = exeDir + "/models/" + modelPath;
    if (QFile::exists(devPath))
        return devPath;

    // 3. Project root models: <exe>/../models/<path>
    const QString projPath = exeDir + "/../models/" + modelPath;
    if (QFile::exists(projPath))
        return projPath;

    // 3. Installed share: <exe>/../share/ScreenInferenceSystem/models/<path>
    const QString instPath = exeDir + "/../share/ScreenInferenceSystem/models/" + modelPath;
    if (QFile::exists(instPath))
        return instPath;

    // 4. Bare filename fallback — look beside executable
    const QString barePath = exeDir + "/" + QFileInfo(modelPath).fileName();
    if (QFile::exists(barePath))
        return barePath;

    // 5. CWD fallback
    const QString cwdPath = QDir::currentPath() + "/" + modelPath;
    if (QFile::exists(cwdPath))
        return cwdPath;

    // Return the dev path as best guess (caller will get a warning if it fails to load)
    return devPath;
}

QString ConfigManager::dataDir()
{
    const QString exeDir = QCoreApplication::applicationDirPath();
    const QString instShare = exeDir + "/../share/ScreenInferenceSystem";
    if (QFile::exists(instShare))
        return instShare;
    return exeDir;
}

CameraConfig ConfigManager::parseCameraJson(const QJsonObject& obj, int defaultId) const
{
    CameraConfig cfg;
    cfg.cameraId             = obj["cameraId"].toInt(defaultId);
    cfg.source               = obj["source"].toString("");
    cfg.modelPath            = obj["modelPath"].toString("");
    cfg.name                 = obj["name"].toString(QString("Camera %1").arg(cfg.cameraId));
    cfg.enabled              = obj["enabled"].toBool(true);
    cfg.mode                 = obj["mode"].toString("local_inference");
    cfg.mqttTopic            = obj["mqtt_topic"].toString("");
    cfg.confidenceThreshold  = static_cast<float>(obj["confidenceThreshold"].toDouble(0.5));
    cfg.nmsThreshold         = static_cast<float>(obj["nmsThreshold"].toDouble(0.45));
    cfg.inputWidth           = obj["inputWidth"].toInt(640);
    cfg.inputHeight          = obj["inputHeight"].toInt(640);
    cfg.smoothingAlpha       = static_cast<float>(obj["smoothingAlpha"].toDouble(0.3));
    cfg.trackMaxLost         = obj["trackMaxLost"].toInt(5);
    cfg.inferenceIntervalMs  = obj["inferenceIntervalMs"].toInt(1000);
    cfg.snapshotUrl          = obj["snapshot_url"].toString("");
    cfg.snapshotIntervalMs   = obj["snapshot_interval_ms"].toInt(1000);

    // Parse class colors
    QJsonObject colors = obj["classColors"].toObject();
    for (auto it = colors.begin(); it != colors.end(); ++it) {
        int classId = it.key().toInt();
        cfg.classColors[classId] = it.value().toString();
    }

    // Ensure class 0 and 1 have their default colors
    if (!cfg.classColors.contains(0)) cfg.classColors[0] = "#00FF00";
    if (!cfg.classColors.contains(1)) cfg.classColors[1] = "#FF0000";

    return cfg;
}

QJsonObject ConfigManager::cameraToJson(const CameraConfig& cfg) const
{
    QJsonObject obj;
    obj["cameraId"]             = cfg.cameraId;
    obj["source"]               = cfg.source;
    obj["modelPath"]            = cfg.modelPath;
    obj["name"]                 = cfg.name;
    obj["enabled"]              = cfg.enabled;
    obj["mode"]                 = cfg.mode;
    obj["mqtt_topic"]           = cfg.mqttTopic;
    obj["confidenceThreshold"]  = static_cast<double>(cfg.confidenceThreshold);
    obj["nmsThreshold"]         = static_cast<double>(cfg.nmsThreshold);
    obj["inputWidth"]           = cfg.inputWidth;
    obj["inputHeight"]          = cfg.inputHeight;
    obj["smoothingAlpha"]       = static_cast<double>(cfg.smoothingAlpha);
    obj["trackMaxLost"]         = cfg.trackMaxLost;
    obj["inferenceIntervalMs"]  = cfg.inferenceIntervalMs;
    obj["snapshot_url"]         = cfg.snapshotUrl;
    obj["snapshot_interval_ms"] = cfg.snapshotIntervalMs;

    QJsonObject colors;
    for (auto it = cfg.classColors.begin(); it != cfg.classColors.end(); ++it) {
        colors[QString::number(it.key())] = it.value();
    }
    obj["classColors"] = colors;

    return obj;
}
