#include "SettingsDialog.h"
#include "ConfigManager.h"

#include <QTabWidget>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QMessageBox>

SettingsDialog::SettingsDialog(ConfigManager* configManager, QWidget* parent)
    : QDialog(parent)
    , mConfigManager(configManager)
{
    setWindowTitle("Settings");
    setMinimumSize(900, 580);
    resize(960, 620);
    buildUI();
}

void SettingsDialog::buildUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* tabs = new QTabWidget(this);
    tabs->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #1a3a60; background: #0a0e1a; }"
        "QTabBar::tab { background: #0c1428; color: #a0c8ff; padding: 8px 16px; min-width: 80px; "
        "    border: 1px solid #1a3a60; border-bottom: none; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
        "QTabBar::tab:selected { background: #0f3460; color: #e0f0ff; }"
        "QTabBar::tab:hover { background: #142a50; }"
    );

    auto configs = mConfigManager->allConfigs();
    mWidgets.resize(configs.size());

    for (int i = 0; i < configs.size(); ++i) {
        QWidget* tab = buildCameraTab(i, configs[i]);
        tabs->addTab(tab, QString("Camera %1").arg(i + 1));
    }

    mainLayout->addWidget(tabs);

    // Buttons
    auto* btnBox = new QDialogButtonBox(this);
    auto* saveBtn   = btnBox->addButton(QString::fromUtf8("保存"), QDialogButtonBox::AcceptRole);
    auto* cancelBtn = btnBox->addButton(QString::fromUtf8("取消"), QDialogButtonBox::RejectRole);
    saveBtn->setStyleSheet("QPushButton { background-color: #00a854; color: white; padding: 8px 24px; border-radius: 4px; font-weight: bold; }"
                           "QPushButton:hover { background-color: #00c964; }");
    cancelBtn->setStyleSheet("QPushButton { background-color: #1a3a60; color: #a0c8ff; padding: 8px 24px; border-radius: 4px; }"
                             "QPushButton:hover { background-color: #234a70; }");

    connect(saveBtn,   &QPushButton::clicked, this, &SettingsDialog::onSave);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    mainLayout->addWidget(btnBox);

    setStyleSheet("QDialog { background-color: #0a0e1a; color: #e0f0ff; }"
                  "QLabel { color: #e0f0ff; }"
                  "QLineEdit, QDoubleSpinBox, QSpinBox { background: #0c1428; color: #e0f0ff; "
                  "    border: 1px solid #1a3a60; padding: 4px; border-radius: 3px; }"
                  "QLineEdit:focus, QDoubleSpinBox:focus, QSpinBox:focus { border: 1px solid #00d4ff; }"
                  "QCheckBox { color: #e0f0ff; }"
                  "QGroupBox { color: #e0f0ff; border: 1px solid #1a3a60; border-radius: 6px; margin-top: 10px; }"
                  "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }");
}

QWidget* SettingsDialog::buildCameraTab(int index, const CameraConfig& cfg)
{
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* container = new QWidget();
    auto* vlay = new QVBoxLayout(container);
    vlay->setSpacing(10);

    // --- Source group ---
    auto* srcGroup = new QGroupBox(QString::fromUtf8("视频源"));
    auto* srcForm  = new QFormLayout(srcGroup);
    srcForm->setLabelAlignment(Qt::AlignRight);

    auto& w = mWidgets[index];

    w.name   = new QLineEdit(cfg.name);
    w.source = new QLineEdit(cfg.source);
    w.source->setPlaceholderText("0  /  /dev/video0  /  rtsp://...");
    w.enabled = new QCheckBox();
    w.enabled->setChecked(cfg.enabled);

    srcForm->addRow(QString::fromUtf8("名称:"),   w.name);
    srcForm->addRow(QString::fromUtf8("来源:"),   w.source);
    srcForm->addRow(QString::fromUtf8("启用:"),   w.enabled);
    vlay->addWidget(srcGroup);

    // --- Model group ---
    auto* mdlGroup = new QGroupBox(QString::fromUtf8("模型"));
    auto* mdlForm  = new QFormLayout(mdlGroup);
    mdlForm->setLabelAlignment(Qt::AlignRight);

    w.modelPath   = new QLineEdit(cfg.modelPath);
    w.modelPath->setPlaceholderText("models/camera_0.onnx");
    w.inputWidth  = new QSpinBox();  w.inputWidth->setRange(32, 2048);  w.inputWidth->setValue(cfg.inputWidth);
    w.inputHeight = new QSpinBox();  w.inputHeight->setRange(32, 2048); w.inputHeight->setValue(cfg.inputHeight);

    mdlForm->addRow(QString::fromUtf8("模型路径:"),    w.modelPath);
    mdlForm->addRow(QString::fromUtf8("输入宽度:"),    w.inputWidth);
    mdlForm->addRow(QString::fromUtf8("输入高度:"),    w.inputHeight);
    vlay->addWidget(mdlGroup);

    // --- Detection group ---
    auto* detGroup = new QGroupBox(QString::fromUtf8("检测参数"));
    auto* detForm  = new QFormLayout(detGroup);
    detForm->setLabelAlignment(Qt::AlignRight);

    w.confidenceThreshold = new QDoubleSpinBox(); w.confidenceThreshold->setRange(0.01, 1.0); w.confidenceThreshold->setSingleStep(0.05); w.confidenceThreshold->setDecimals(2); w.confidenceThreshold->setValue(cfg.confidenceThreshold);
    w.nmsThreshold        = new QDoubleSpinBox(); w.nmsThreshold->setRange(0.01, 1.0);        w.nmsThreshold->setSingleStep(0.05);        w.nmsThreshold->setDecimals(2);        w.nmsThreshold->setValue(cfg.nmsThreshold);
    w.inferenceIntervalMs = new QSpinBox();       w.inferenceIntervalMs->setRange(33, 10000); w.inferenceIntervalMs->setSuffix(" ms");    w.inferenceIntervalMs->setValue(cfg.inferenceIntervalMs);

    detForm->addRow(QString::fromUtf8("置信度阈值:"),     w.confidenceThreshold);
    detForm->addRow(QString::fromUtf8("NMS 阈值:"),       w.nmsThreshold);
    detForm->addRow(QString::fromUtf8("推理间隔:"),       w.inferenceIntervalMs);
    vlay->addWidget(detGroup);

    // --- Tracking group ---
    auto* trkGroup = new QGroupBox(QString::fromUtf8("平滑 & 追踪"));
    auto* trkForm  = new QFormLayout(trkGroup);
    trkForm->setLabelAlignment(Qt::AlignRight);

    w.smoothingAlpha = new QDoubleSpinBox(); w.smoothingAlpha->setRange(0.0, 1.0); w.smoothingAlpha->setSingleStep(0.05); w.smoothingAlpha->setDecimals(2); w.smoothingAlpha->setValue(cfg.smoothingAlpha);
    w.trackMaxLost   = new QSpinBox();       w.trackMaxLost->setRange(1, 60);                                                                                                                          w.trackMaxLost->setValue(cfg.trackMaxLost);

    trkForm->addRow(QString::fromUtf8("平滑系数 (EMA α):"), w.smoothingAlpha);
    trkForm->addRow(QString::fromUtf8("最大丢失帧数:"),     w.trackMaxLost);
    vlay->addWidget(trkGroup);

    vlay->addStretch();
    scroll->setWidget(container);
    return scroll;
}

CameraConfig SettingsDialog::collectConfig(int index) const
{
    const auto& w = mWidgets[index];
    auto configs = mConfigManager->allConfigs();
    CameraConfig cfg = (index < configs.size()) ? configs[index] : CameraConfig();

    cfg.name                = w.name->text().trimmed();
    cfg.source              = w.source->text().trimmed();
    cfg.enabled             = w.enabled->isChecked();
    cfg.modelPath           = w.modelPath->text().trimmed();
    cfg.inputWidth          = w.inputWidth->value();
    cfg.inputHeight         = w.inputHeight->value();
    cfg.confidenceThreshold = static_cast<float>(w.confidenceThreshold->value());
    cfg.nmsThreshold        = static_cast<float>(w.nmsThreshold->value());
    cfg.inferenceIntervalMs = w.inferenceIntervalMs->value();
    cfg.smoothingAlpha      = static_cast<float>(w.smoothingAlpha->value());
    cfg.trackMaxLost        = w.trackMaxLost->value();

    return cfg;
}

void SettingsDialog::onSave()
{
    // Write each camera's params back to ConfigManager
    for (int i = 0; i < mWidgets.size(); ++i) {
        mConfigManager->setCameraConfig(i, collectConfig(i));
    }

    // Persist to the default JSON file so settings survive restart
    QString path = ConfigManager::resolveConfigPath();
    if (!mConfigManager->saveToFile(path)) {
        QMessageBox::warning(this, "Save Failed",
            QString("Could not write config to:\n%1").arg(path));
        return;
    }

    emit settingsSaved();
    accept();
}
