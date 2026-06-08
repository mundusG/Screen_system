#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVector>
#include <QMap>
#include <QLabel>
#include <QStatusBar>
#include <QTimer>
#include "Types.h"
#include "StatsPanel.h"

class VideoWidget;
class CameraThread;
class InferenceEngine;
class SmoothingFilter;
class ConfigManager;
class SettingsDialog;

/// Main application window with 2x4 camera grid + right-side stats panel
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    bool initialize(const QString& configPath = QString());

public slots:
    void startAll();
    void stopAll();
    void openSettings();

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onConfidenceThresholdChanged(int cameraId, float threshold);
    void onDisplayFrameReady(const FrameData& frame);
    void onInferenceFrameReady(const FrameData& frame);
    void onInferenceFinished(const InferenceResult& result);
    void onDisplayResultReady(const DisplayResult& result);
    void onFpsUpdated(int cameraId, double fps);
    void onCameraError(const QString& message);

private:
    void setupUI();
    bool setupCameraPipeline(int cameraId, const CameraConfig& config);
    void teardownCameraPipeline(int cameraId);
    void updateStatusBar();

    // UI
    QWidget*              mCentralWidget;
    QHBoxLayout*          mRootLayout;     // left grid | right panel
    QGridLayout*          mGridLayout;
    QVector<VideoWidget*> mVideoWidgets;
    StatsPanel*           mStatsPanel;

    // Pipeline components
    QMap<int, CameraThread*>    mCameraThreads;
    QMap<int, InferenceEngine*> mInferenceEngines;
    QMap<int, SmoothingFilter*> mSmoothingFilters;

    ConfigManager* mConfigManager;

    bool    mRunning;
    qint64  mStartTime;
    QTimer* mStatusTimer;
};

#endif // MAINWINDOW_H
