#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QVector>
#include <QString>
#include <QJsonObject>
#include <QMutex>
#include "Types.h"

/// Manages per-camera configuration, loads/saves from JSON
class ConfigManager : public QObject
{
    Q_OBJECT

public:
    explicit ConfigManager(QObject* parent = nullptr);
    ~ConfigManager() override = default;

    /// Load configuration from JSON file
    bool loadFromFile(const QString& filePath);

    /// Save current configuration to JSON file
    bool saveToFile(const QString& filePath) const;

    /// Get camera count
    int cameraCount() const;

    /// Get all camera configs (thread-safe copy)
    QVector<CameraConfig> allConfigs() const;

    /// Get config for a specific camera
    CameraConfig cameraConfig(int cameraId) const;

    /// Update config for a specific camera
    void setCameraConfig(int cameraId, const CameraConfig& config);

    /// Set confidence threshold for a specific camera
    void setConfidenceThreshold(int cameraId, float threshold);

    /// Set smoothing alpha for a specific camera
    void setSmoothingAlpha(int cameraId, float alpha);

    /// Set model path for a specific camera
    void setModelPath(int cameraId, const QString& path);

    /// Set camera source (RTSP URL, device path)
    void setCameraSource(int cameraId, const QString& source);

    /// Set camera enabled/disabled
    void setCameraEnabled(int cameraId, bool enabled);

    /// Get class color for a given camera and class ID
    QColor classColor(int cameraId, int classId) const;

    /// Resolve config file path by searching standard locations
    static QString resolveConfigPath();

    /// Resolve a model path: absolute paths returned as-is;
    /// relative paths searched under SCREEN_SYSTEM_MODEL_DIR, install share, exe dir, CWD
    static QString resolveModelPath(const QString& modelPath);

    /// Data directory for installed layouts (share/ScreenInferenceSystem)
    static QString dataDir();

signals:
    void configChanged(int cameraId);
    void allConfigsChanged();

private:
    CameraConfig parseCameraJson(const QJsonObject& obj, int defaultId) const;
    QJsonObject   cameraToJson(const CameraConfig& cfg) const;

    mutable QMutex      mMutex;
    QVector<CameraConfig> mConfigs;
};

#endif // CONFIGMANAGER_H
