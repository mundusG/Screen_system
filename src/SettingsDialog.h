#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QVector>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include "Types.h"

class ConfigManager;

/// Per-camera parameter row inside the settings dialog
struct CameraParamWidgets {
    QLineEdit*      source;
    QLineEdit*      modelPath;
    QLineEdit*      name;
    QCheckBox*      enabled;
    QDoubleSpinBox* confidenceThreshold;
    QDoubleSpinBox* nmsThreshold;
    QSpinBox*       inputWidth;
    QSpinBox*       inputHeight;
    QDoubleSpinBox* smoothingAlpha;
    QSpinBox*       trackMaxLost;
    QSpinBox*       inferenceIntervalMs;
    // Group boxes for mode-dependent visibility
    QGroupBox*      modelGroup     = nullptr;
    QGroupBox*      detectionGroup = nullptr;
};

/// Settings dialog: shows all camera parameters in tabs, saves on confirm
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(ConfigManager* configManager, QWidget* parent = nullptr);

signals:
    /// Emitted after user clicks Save — caller should reload pipeline
    void settingsSaved();

private slots:
    void onSave();

private:
    void buildUI();
    QWidget* buildCameraTab(int index, const CameraConfig& cfg);
    void populateWidgets(int index, const CameraConfig& cfg);
    CameraConfig collectConfig(int index) const;

    ConfigManager*              mConfigManager;
    QVector<CameraParamWidgets> mWidgets;  // one per camera tab
};

#endif // SETTINGSDIALOG_H
