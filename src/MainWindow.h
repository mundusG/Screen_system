#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QVector>
#include <QMap>
#include <QLabel>
#include <QTimer>
#include "Types.h"

class VideoWidget;
class CameraThread;
class ImageStreamThread;
class InferenceEngine;
class SmoothingFilter;
class ConfigManager;
class SettingsDialog;
class SidebarWidget;
class BottomControlBar;
class AlertPanel;
class InferenceSubscriberThread;
class ServiceLauncher;
class AlarmController;
class ThreadedSoundPlayer;
class DefectImageStore;
class DefectImageBrowserDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    bool initialize(const QString& configPath = QString(), bool forceImageMode = false);

public slots:
    void startAll();
    void stopAll();
    void openSettings();
    void toggleCamera(int cameraId);

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onConfidenceThresholdChanged(int cameraId, float threshold);
    void onDisplayFrameReady(const FrameData& frame);
    void onInferenceFrameReady(const FrameData& frame);
    void onInferenceFinished(const InferenceResult& result);
    void onDisplayResultReady(const DisplayResult& result);
    void onFpsUpdated(int cameraId, double fps);
    void onCameraError(const QString& message);
    void onCameraClicked(int cameraId);
    void onGridModeChanged(int mode);
    void onToggleFullscreen();
    void onSnapshotRequested();
    void openDefectImageBrowser();
    void onChannelsDiscovered(const QVector<ChannelInfo>& channels, const QString& mqttSourceId);

private:
    void setupUI();
    bool setupCameraPipeline(int cameraId, const CameraConfig& config);
    void teardownCameraPipeline(int cameraId);
    void updatePanels();
    void applyGridLayout(int mode);
    void constrainVideoAspectRatios();
    // UI - main structure
    QWidget*            mCentralWidget;
    QHBoxLayout*        mRootLayout;
    SidebarWidget*      mSidebar;
    QWidget*            mCenterContainer;
    QVBoxLayout*        mCenterLayout;
    QGridLayout*        mGridLayout;
    QWidget*            mGridContainer;
    BottomControlBar*   mBottomBar;
    AlertPanel*         mAlertPanel;

    QVector<VideoWidget*> mVideoWidgets;

    // Pipeline components
    QMap<int, CameraThread*>      mCameraThreads;
    QMap<int, ImageStreamThread*> mImageStreamThreads;
    QMap<int, InferenceEngine*>   mInferenceEngines;
    QMap<int, SmoothingFilter*>   mSmoothingFilters;

    ConfigManager* mConfigManager;
    QMap<QString, InferenceSubscriberThread*> mInferenceSubscribers;
    ServiceLauncher* mServiceLauncher;
    AlarmController* mAlarmController;
    ThreadedSoundPlayer* mThreadedSoundPlayer;
    DefectImageStore* mDefectImageStore;
    DefectImageBrowserDialog* mDefectImageBrowser;

    bool    mRunning;
    qint64  mStartTime;
    QTimer* mStatusTimer;

    int     mSelectedCamera;
    int     mGridMode;
    QMap<int, bool> mCameraRunning;
    QMap<int, ChannelInfo> mChannelInfos;
    QString mSystemMode; // "local_inference", "mqtt_publish", "mqtt_subscribe"
};

#endif // MAINWINDOW_H
