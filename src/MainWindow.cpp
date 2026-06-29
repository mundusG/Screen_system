#include "MainWindow.h"
#include "VideoWidget.h"
#include "SidebarWidget.h"
#include "BottomControlBar.h"
#include "AlertPanel.h"
#include "CameraCapture.h"
#include "ImageStreamSource.h"
#include "InferenceEngine.h"
#include "SmoothingFilter.h"
#include "ConfigManager.h"
#include "SettingsDialog.h"
#include "InferenceSubscriber.h"
#include "ServiceLauncher.h"
#include "AlarmController.h"
#include "ThreadedSoundPlayer.h"
#include "DefectImageStore.h"
#include "DefectImageBrowserDialog.h"
#include "AlertFilter.h"
#include "Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QDateTime>
#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QTimer>
#include <QDebug>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , mCentralWidget(nullptr)
    , mRootLayout(nullptr)
    , mSidebar(nullptr)
    , mCenterContainer(nullptr)
    , mCenterLayout(nullptr)
    , mGridLayout(nullptr)
    , mGridContainer(nullptr)
    , mBottomBar(nullptr)
    , mAlertPanel(nullptr)
    , mConfigManager(new ConfigManager(this))
    , mServiceLauncher(new ServiceLauncher(this))
    , mAlarmController(new AlarmController(this))
    , mThreadedSoundPlayer(new ThreadedSoundPlayer(this))
    , mDefectImageStore(new DefectImageStore(this))
    , mDefectImageBrowser(nullptr)
    , mRunning(false)
    , mStartTime(0)
    , mSelectedCamera(0)
    , mGridMode(2) // 2x4 default
    , mSystemMode("local_inference")
{
    setupUI();
    if (!mDefectImageStore->initialize()) {
        qWarning() << "MainWindow: DefectImageStore initialization FAILED, defect images will not be saved!";
    } else {
        qDebug() << "MainWindow: DefectImageStore initialized OK, count:" << mDefectImageStore->count();
    }

    mStatusTimer = new QTimer(this);
    connect(mStatusTimer, &QTimer::timeout, this, &MainWindow::updatePanels);
    mStatusTimer->start(2000);

    qRegisterMetaType<FrameData>("FrameData");
    qRegisterMetaType<InferenceResult>("InferenceResult");
    qRegisterMetaType<DisplayResult>("DisplayResult");
    qRegisterMetaType<CameraConfig>("CameraConfig");
}

MainWindow::~MainWindow()
{
    stopAll();
}

void MainWindow::setupUI()
{
    qDebug() << "MainWindow::setupUI() - START";
    setWindowTitle("Screen Inference System");
    resize(1920, 1080);
    setMinimumSize(1280, 720);
    qDebug() << "MainWindow::setupUI() - Window created, size:" << size();

    // App-wide dark theme
    setStyleSheet(QString(
        "QMainWindow { background-color: %1; }"
        "QToolTip { background-color: #16213e; color: #e0f0ff; border: 1px solid #0f3460; "
        "           padding: 4px; font-size: 11px; }"
        "QScrollBar:vertical { background: #0a0e1a; width: 6px; }"
        "QScrollBar::handle:vertical { background: #1a3a60; border-radius: 3px; min-height: 20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    ).arg(Theme::background().name()));

    // Central widget — plain dark background (no background image)
    mCentralWidget = new QWidget(this);
    mCentralWidget->setStyleSheet(QString("background-color: %1;").arg(Theme::background().name()));
    setCentralWidget(mCentralWidget);

    // Remove menu bar — actions are in sidebar and bottom bar
    menuBar()->hide();
    statusBar()->hide();

    // Root layout: Sidebar | Center | AlertPanel
    mRootLayout = new QHBoxLayout(mCentralWidget);
    mRootLayout->setContentsMargins(0, 0, 0, 0);
    mRootLayout->setSpacing(0);

    // --- Left Sidebar ---
    mSidebar = new SidebarWidget(mCentralWidget);
    connect(mSidebar, &SidebarWidget::settingsRequested, this, &MainWindow::openSettings);
    connect(mSidebar, &SidebarWidget::startStopToggled, this, [this]() {
        if (mRunning) stopAll(); else startAll();
    });
    mRootLayout->addWidget(mSidebar);

    // --- Center: Grid + Bottom Bar ---
    mCenterContainer = new QWidget(mCentralWidget);
    mCenterContainer->setStyleSheet("background: transparent;");
    mCenterLayout = new QVBoxLayout(mCenterContainer);
    mCenterLayout->setContentsMargins(Theme::LayoutMargin, Theme::LayoutMargin,
                                       Theme::LayoutMargin, 0);
    mCenterLayout->setSpacing(Theme::LayoutMargin);

    // Video grid
    mGridContainer = new QWidget(mCenterContainer);
    mGridContainer->setStyleSheet("background: transparent;");
    mGridLayout = new QGridLayout(mGridContainer);
    mGridLayout->setSpacing(Theme::GridSpacing);
    mGridLayout->setContentsMargins(0, 0, 0, 0);

    static const char* CAM_NAMES[8] = {
        "\xe4\xba\xa7\xe7\xba\xbf\x31 \xe5\x89\x8d",
        "\xe4\xba\xa7\xe7\xba\xbf\x31 \xe5\x90\x8e",
        "\xe4\xba\xa7\xe7\xba\xbf\x31 \xe5\xb7\xa6",
        "\xe4\xba\xa7\xe7\xba\xbf\x31 \xe5\x8f\xb3",
        "\xe4\xba\xa7\xe7\xba\xbf\x32 \xe5\x89\x8d",
        "\xe4\xba\xa7\xe7\xba\xbf\x32 \xe5\x90\x8e",
        "\xe4\xba\xa7\xe7\xba\xbf\x32 \xe5\xb7\xa6",
        "\xe4\xba\xa7\xe7\xba\xbf\x32 \xe5\x8f\xb3"
    };

    for (int i = 0; i < 8; ++i) {
        QString title = QString::fromUtf8(CAM_NAMES[i]);
        auto* videoWidget = new VideoWidget(i, title, mGridContainer);
        mVideoWidgets.append(videoWidget);

        connect(videoWidget, &VideoWidget::confidenceThresholdChanged,
                this, &MainWindow::onConfidenceThresholdChanged);
        connect(videoWidget, &VideoWidget::clicked,
                this, &MainWindow::onCameraClicked);
    }

    // Set initial selection
    mVideoWidgets[0]->setSelected(true);

    // Apply default 2x4 grid
    applyGridLayout(mGridMode);

    mCenterLayout->addWidget(mGridContainer, 1);

    // Bottom control bar
    mBottomBar = new BottomControlBar(mCenterContainer);
    mBottomBar->setGridMode(mGridMode);
    connect(mBottomBar, &BottomControlBar::gridModeChanged, this, &MainWindow::onGridModeChanged);
    connect(mBottomBar, &BottomControlBar::fullscreenToggled, this, &MainWindow::onToggleFullscreen);
    connect(mBottomBar, &BottomControlBar::settingsRequested, this, &MainWindow::openSettings);
    connect(mBottomBar, &BottomControlBar::snapshotRequested, this, &MainWindow::onSnapshotRequested);
    mCenterLayout->addWidget(mBottomBar);

    mRootLayout->addWidget(mCenterContainer, 1); // stretch = 1 (takes remaining space)

    // --- Right Alert Panel ---
    mAlertPanel = new AlertPanel(mCentralWidget);
    connect(mAlertPanel, &AlertPanel::cameraToggleRequested,
            this, &MainWindow::toggleCamera);
    connect(mAlertPanel, &AlertPanel::imageLibraryRequested,
            this, &MainWindow::openDefectImageBrowser);
    mRootLayout->addWidget(mAlertPanel);

    qDebug() << "MainWindow::setupUI() - COMPLETE, showing window...";
    show();
    qDebug() << "MainWindow::setupUI() - Window shown, isVisible:" << isVisible();
}

bool MainWindow::initialize(const QString& configPath, bool forceImageMode)
{
    QString path = configPath.isEmpty() ? ConfigManager::resolveConfigPath() : configPath;

    qDebug() << "MainWindow: Loading config from" << path;

    if (!mConfigManager->loadFromFile(path)) {
        qWarning() << "MainWindow: Failed to load config, using defaults";
        for (int i = 0; i < 8; ++i) {
            CameraConfig cfg;
            cfg.cameraId = i;
            cfg.name     = QString::fromUtf8("\xe6\x91\x84\xe5\x83\x8f\xe5\xa4\xb4 %1").arg(i + 1);
            cfg.source   = QString::number(i);
            cfg.modelPath = QString("camera_%1.onnx").arg(i);
            mConfigManager->setCameraConfig(i, cfg);
        }
    }

    // Determine system mode (--image-mode flag overrides config)
    mSystemMode = forceImageMode ? QStringLiteral("image_stream") : mConfigManager->systemMode();
    qDebug() << "MainWindow: System mode:" << mSystemMode
             << (forceImageMode ? "(forced by --image-mode)" : "");

    // Auto-start dependency services
    connect(mServiceLauncher, &ServiceLauncher::serviceError,
            this, &MainWindow::onCameraError);

    if (mConfigManager->checkMosquitto()) {
        if (!mServiceLauncher->isMosquittoRunning()) {
            qWarning() << "MainWindow: mosquitto is NOT running! MQTT features will not work.";
        } else {
            qDebug() << "MainWindow: mosquitto is running";
        }
    }

    if (mConfigManager->bridgeEnabled()) {
        QString exeDir = QCoreApplication::applicationDirPath();
        QString scriptPath = QDir(exeDir).absoluteFilePath(
            "../" + mConfigManager->bridgeScript());
        QString configPath = QDir(exeDir).absoluteFilePath(
            "../" + mConfigManager->bridgeConfig());
        scriptPath = QFileInfo(scriptPath).canonicalFilePath();
        configPath = QFileInfo(configPath).canonicalFilePath();

        if (!scriptPath.isEmpty() && !configPath.isEmpty()) {
            mServiceLauncher->startNNBridge(
                mConfigManager->bridgePython(), scriptPath, configPath);
        } else {
            qWarning() << "MainWindow: nn_bridge script or config not found";
        }
    }

    // Initialize one MQTT subscriber per configured source. Each source owns
    // a disjoint global camera ID range, so two inference systems can feed the
    // same display without their camera IDs or subscriptions being mixed.
    if (mSystemMode == "mqtt_subscribe" || mSystemMode == "image_stream") {
        const auto mqttSources = mConfigManager->mqttSources();
        for (const auto& source : mqttSources) {
            if (!source.enabled) {
                qDebug() << "MainWindow: MQTT source disabled:" << source.id;
                continue;
            }

            auto* subscriber = new InferenceSubscriberThread(this);
            subscriber->setDiscoveryTopic(source.discoveryTopic);
            mInferenceSubscribers[source.id] = subscriber;

            connect(subscriber, &InferenceSubscriberThread::inferenceFinished,
                    this, [this, source](const InferenceResult& result) {
                if (!source.acceptsCamera(result.cameraId)) {
                    qWarning() << "MainWindow: Ignoring camera" << result.cameraId
                               << "from MQTT source" << source.id
                               << "(allowed range:" << source.cameraIdMin
                               << "-" << source.cameraIdMax << ")";
                    return;
                }
                onInferenceFinished(result);
            });
            connect(subscriber, &InferenceSubscriberThread::channelsDiscovered,
                    this, [this, source](const QVector<ChannelInfo>& channels) {
                QVector<ChannelInfo> accepted;
                accepted.reserve(channels.size());
                for (const auto& channel : channels) {
                    if (source.acceptsCamera(channel.cameraId)) {
                        accepted.append(channel);
                    } else {
                        qWarning() << "MainWindow: Ignoring discovered camera" << channel.cameraId
                                   << "from MQTT source" << source.id
                                   << "(allowed range:" << source.cameraIdMin
                                   << "-" << source.cameraIdMax << ")";
                    }
                }
                if (!accepted.isEmpty()) {
                    onChannelsDiscovered(accepted, source.id);
                }
            });
            connect(subscriber, &InferenceSubscriberThread::error,
                    this, [this, source](const QString& message) {
                onCameraError(QString("MQTT source %1: %2").arg(source.id, message));
            });

            // Discovery is subscribed immediately; inference topics are added
            // dynamically after each source announces its own channels.
            QTimer::singleShot(0, this, [subscriber, source]() {
                subscriber->connectAndSubscribe(
                    source.broker, source.clientId, QStringList(),
                    source.username, source.password);
            });
        }
    }

    auto configs = mConfigManager->allConfigs();
    for (const auto& cfg : configs) {
        // Update video widget titles from config
        if (cfg.cameraId < mVideoWidgets.size()) {
            mVideoWidgets[cfg.cameraId]->setTitle(cfg.name);
        }
        // Update alert panel device names
        mAlertPanel->updateDeviceStatus(cfg.cameraId, cfg.name, false, 0.0, 0);

        // In mqtt_subscribe mode, pipelines are created when channels are discovered
        if (mSystemMode == "mqtt_subscribe")
            continue;

        // In image_stream mode, set up cameras that have a snapshot URL;
        // others will be set up when channels are discovered via MQTT
        if (mSystemMode == "image_stream") {
            if (cfg.snapshotUrl.isEmpty())
                continue;
        }

        if (cfg.enabled) {
            setupCameraPipeline(cfg.cameraId, cfg);
        }
    }

    qDebug() << "MainWindow: Initialized with" << configs.size() << "cameras";

    // In mqtt_subscribe and image_stream modes, auto-start without waiting
    // for the user to press the Start button.  Channels already configured
    // with snapshot URLs (image_stream) start immediately; MQTT-discovered
    // channels auto-start in onChannelsDiscovered once the bridge announces
    // them and mRunning is true.
    if (mSystemMode == "mqtt_subscribe" || mSystemMode == "image_stream") {
        QTimer::singleShot(1500, this, [this]() {
            if (!mRunning) startAll();
        });
    }

    return true;
}

bool MainWindow::setupCameraPipeline(int cameraId, const CameraConfig& config)
{
    // Cache camera config for later use (source resolution, alert settings, etc.)
    mCameraConfigs[cameraId] = config;

    // Create alert filter pipeline from config
    mAlertPipelines[cameraId] = new AlertFilterPipeline(config.alertConfig);

    auto* smoother = new SmoothingFilter(cameraId, this);
    smoother->setAlpha(config.smoothingAlpha);
    smoother->setMaxLostFrames(config.trackMaxLost);

    connect(smoother, &SmoothingFilter::displayResultReady,
            this, &MainWindow::onDisplayResultReady);

    mSmoothingFilters[cameraId] = smoother;

    // Set up periodic alert snapshot timer
    if (config.alertSnapshotIntervalMs > 0) {
        auto* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this, cameraId]() {
            onAlertSnapshotTimer(cameraId);
        });
        timer->start(config.alertSnapshotIntervalMs);
        mAlertSnapshotTimers[cameraId] = timer;
        qDebug() << "MainWindow: Alert snapshot timer for camera" << cameraId
                 << "interval:" << config.alertSnapshotIntervalMs << "ms";
    }

    // Determine camera mode: use camera-specific mode if set, otherwise use system mode
    QString cameraMode = config.mode.isEmpty() ? mSystemMode : config.mode;

    // Only setup inference engine and camera capture for local_inference mode
    if (cameraMode == "local_inference") {
        auto* inference = new InferenceEngine(cameraId, this);

        bool modelLoaded = false;
        if (!config.modelPath.isEmpty()) {
            QString resolvedPath = ConfigManager::resolveModelPath(config.modelPath);
            modelLoaded = inference->loadModel(resolvedPath,
                                               config.inputWidth, config.inputHeight);
        }
        inference->setConfidenceThreshold(config.confidenceThreshold);
        inference->setNmsThreshold(config.nmsThreshold);

        connect(inference, &InferenceEngine::inferenceFinished,
                this, &MainWindow::onInferenceFinished);
        connect(inference, &InferenceEngine::error,
                this, &MainWindow::onCameraError);

        mInferenceEngines[cameraId] = inference;

        auto* camera = new CameraThread(cameraId, this);
        camera->setInferenceInterval(config.inferenceIntervalMs);

        connect(camera, &CameraThread::displayFrameReady,
                this, &MainWindow::onDisplayFrameReady);
        connect(camera, &CameraThread::inferenceFrameReady,
                this, &MainWindow::onInferenceFrameReady);
        connect(camera, &CameraThread::fpsUpdated,
                this, &MainWindow::onFpsUpdated);
        connect(camera, &CameraThread::error,
                this, &MainWindow::onCameraError);

        mCameraThreads[cameraId] = camera;
        mCameraRunning[cameraId] = false; // Will be set to true when started
        mAlertPanel->setCameraRunning(cameraId, false);

        if (cameraId < mVideoWidgets.size()) {
            mVideoWidgets[cameraId]->setConfidenceThreshold(config.confidenceThreshold);
        }

        qDebug() << "MainWindow: Camera" << cameraId << "pipeline setup (local_inference)."
                 << "source:" << config.source
                 << "model:" << (modelLoaded ? "loaded" : "not loaded")
                 << "(camera will be opened asynchronously when started)";

        return true; // Return true, actual opening happens in startAll()
    }
    else if (cameraMode == "mqtt_subscribe") {
        // SmoothingFilter already created above (unconditionally)
        // Create CameraThread for RTSP video display (no InferenceEngine needed)
        auto* camera = new CameraThread(cameraId, this);
        camera->setInferenceInterval(config.inferenceIntervalMs);

        connect(camera, &CameraThread::displayFrameReady,
                this, &MainWindow::onDisplayFrameReady);
        connect(camera, &CameraThread::inferenceFrameReady,
                this, &MainWindow::onInferenceFrameReady);
        connect(camera, &CameraThread::fpsUpdated,
                this, &MainWindow::onFpsUpdated);
        connect(camera, &CameraThread::error,
                this, &MainWindow::onCameraError);

        mCameraThreads[cameraId] = camera;
        mCameraRunning[cameraId] = false;
        mAlertPanel->setCameraRunning(cameraId, false);

        if (cameraId < mVideoWidgets.size()) {
            mVideoWidgets[cameraId]->setConfidenceThreshold(config.confidenceThreshold);
        }

        qDebug() << "MainWindow: Camera" << cameraId << "in mqtt_subscribe mode"
                 << "source:" << config.source << "(RTSP video + MQTT detections)";

        return true;
    }
    else if (cameraMode == "image_stream") {
        // SmoothingFilter already created above (unconditionally)
        // Create ImageStreamThread for periodic snapshot display (no RTSP decoding)
        auto* imgStream = new ImageStreamThread(cameraId, this);
        imgStream->setSnapshotInterval(config.snapshotIntervalMs);

        connect(imgStream, &ImageStreamThread::displayFrameReady,
                this, &MainWindow::onDisplayFrameReady);
        connect(imgStream, &ImageStreamThread::fpsUpdated,
                this, &MainWindow::onFpsUpdated);
        connect(imgStream, &ImageStreamThread::error,
                this, &MainWindow::onCameraError);

        mImageStreamThreads[cameraId] = imgStream;
        mCameraRunning[cameraId] = false;
        mAlertPanel->setCameraRunning(cameraId, false);

        if (cameraId < mVideoWidgets.size()) {
            mVideoWidgets[cameraId]->setConfidenceThreshold(config.confidenceThreshold);
        }

        qDebug() << "MainWindow: Camera" << cameraId << "in image_stream mode"
                 << "snapshotUrl:" << config.snapshotUrl
                 << "interval:" << config.snapshotIntervalMs << "ms";

        return true;
    }

    qWarning() << "MainWindow: Unsupported camera mode:" << cameraMode;
    return false;
}

void MainWindow::teardownCameraPipeline(int cameraId)
{
    mCameraRunning.remove(cameraId);
    mAlertPanel->setCameraRunning(cameraId, false);

    // Clean up alert snapshot timer
    if (mAlertSnapshotTimers.contains(cameraId)) {
        mAlertSnapshotTimers[cameraId]->stop();
        delete mAlertSnapshotTimers[cameraId];
        mAlertSnapshotTimers.remove(cameraId);
    }

    // Clean up alert filter pipeline
    if (mAlertPipelines.contains(cameraId)) {
        delete mAlertPipelines[cameraId];
        mAlertPipelines.remove(cameraId);
    }

    // Clean up source fallback state
    mCameraFallbackSources.remove(cameraId);
    if (mCameraSourceFallbackTimers.contains(cameraId)) {
        mCameraSourceFallbackTimers[cameraId]->stop();
        delete mCameraSourceFallbackTimers[cameraId];
        mCameraSourceFallbackTimers.remove(cameraId);
    }

    // Clean up cached data
    mCameraConfigs.remove(cameraId);
    mLatestDefectDetections.remove(cameraId);
    mLatestDefectConf.remove(cameraId);

    if (mCameraThreads.contains(cameraId)) {
        auto* camera = mCameraThreads[cameraId];
        mCameraThreads.remove(cameraId);
        delete camera;
    }

    if (mImageStreamThreads.contains(cameraId)) {
        auto* imgStream = mImageStreamThreads[cameraId];
        mImageStreamThreads.remove(cameraId);
        delete imgStream;
    }

    if (mInferenceEngines.contains(cameraId)) {
        auto* inference = mInferenceEngines[cameraId];
        mInferenceEngines.remove(cameraId);
        delete inference;
    }

    if (mSmoothingFilters.contains(cameraId)) {
        auto* smoother = mSmoothingFilters[cameraId];
        mSmoothingFilters.remove(cameraId);
        delete smoother;
    }

    if (cameraId < mVideoWidgets.size()) {
        mVideoWidgets[cameraId]->reset();
    }
}

void MainWindow::saveAlertSnapshot(int cameraId)
{
    if (cameraId >= mVideoWidgets.size())
        return;

    QVector<Detection> defects = mLatestDefectDetections.value(cameraId);
    float bestConf = mLatestDefectConf.value(cameraId, 0.0f);

    if (defects.isEmpty()) {
        qDebug() << "AlertTimer: cam" << cameraId << "skip - no defects";
        return;
    }

    QString camName;
    auto configs = mConfigManager->allConfigs();
    for (const auto& cfg : configs) {
        if (cfg.cameraId == cameraId) { camName = cfg.name; break; }
    }
    if (camName.isEmpty() && mChannelInfos.contains(cameraId))
        camName = mChannelInfos[cameraId].name;
    if (camName.isEmpty())
        camName = QString("Camera %1").arg(cameraId + 1);

    int defectClassId = 1;
    for (const auto& det : defects) {
        if (det.confidence >= bestConf) {
            defectClassId = det.classId;
            break;
        }
    }

    QImage thumbnail = mVideoWidgets[cameraId]->grabThumbnail(100);
    mAlertPanel->addAlert(cameraId, camName, defectClassId, bestConf, thumbnail, true);
    qDebug() << "AlertTimer: cam" << cameraId << "pushed to alert panel, conf:" << bestConf;
}

void MainWindow::onAlertSnapshotTimer(int cameraId)
{
    saveAlertSnapshot(cameraId);
}

void MainWindow::tryFallbackSource(int cameraId)
{
    if (!mCameraFallbackSources.contains(cameraId))
        return;
    if (!mCameraRunning.value(cameraId, false))
        return;

    QString fallback = mCameraFallbackSources[cameraId];

    bool isCameraThread = mCameraThreads.contains(cameraId);
    bool isImageStream  = mImageStreamThreads.contains(cameraId);
    if (!isCameraThread && !isImageStream)
        return;

    qWarning() << "MainWindow: Camera" << cameraId
               << "config source failed, switching to fallback:" << fallback;

    // Update the visual source indicator
    if (cameraId < mVideoWidgets.size())
        mVideoWidgets[cameraId]->setSourceLabel("PRV");

    if (isCameraThread) {
        mCameraThreads[cameraId]->requestStop();
        QTimer::singleShot(500, this, [this, cameraId, fallback]() {
            if (mCameraThreads.contains(cameraId) && mCameraRunning.value(cameraId, false)) {
                mCameraThreads[cameraId]->requestStart(fallback);
                qDebug() << "MainWindow: Camera" << cameraId << "restarted with fallback source (CameraThread)";
            }
        });
    } else {
        mImageStreamThreads[cameraId]->requestStop();
        QTimer::singleShot(500, this, [this, cameraId, fallback]() {
            if (mImageStreamThreads.contains(cameraId) && mCameraRunning.value(cameraId, false)) {
                mImageStreamThreads[cameraId]->requestStart(fallback);
                qDebug() << "MainWindow: Camera" << cameraId << "restarted with fallback source (ImageStreamThread)";
            }
        });
    }

    // Clean up fallback state
    mCameraFallbackSources.remove(cameraId);
    if (mCameraSourceFallbackTimers.contains(cameraId)) {
        mCameraSourceFallbackTimers[cameraId]->stop();
        delete mCameraSourceFallbackTimers[cameraId];
        mCameraSourceFallbackTimers.remove(cameraId);
    }
}

bool MainWindow::hasConfigSource(int cameraId) const
{
    auto configs = mConfigManager->allConfigs();
    for (const auto& cfg : configs) {
        if (cfg.cameraId == cameraId && !cfg.source.isEmpty())
            return true;
    }
    return false;
}

QString MainWindow::resolveCameraSource(int cameraId) const
{
    // Priority 1: explicit source from config file (user-configured RTSP/device)
    auto configs = mConfigManager->allConfigs();
    for (const auto& cfg : configs) {
        if (cfg.cameraId == cameraId && !cfg.source.isEmpty())
            return cfg.source;
    }
    // Priority 2: preview URL from MQTT channel discovery
    if (mChannelInfos.contains(cameraId))
        return mChannelInfos[cameraId].previewUrl;
    return QString();
}

void MainWindow::startAll()
{
    if (mRunning) return;

    mRunning = true;
    mStartTime = QDateTime::currentMSecsSinceEpoch();
    mSidebar->setRunning(true);

    // Start CameraThreads
    for (auto it = mCameraThreads.begin(); it != mCameraThreads.end(); ++it) {
        int camId = it.key();
        QString source = resolveCameraSource(camId);

        // Determine fallback and source label
        QString previewUrl;
        if (mChannelInfos.contains(camId))
            previewUrl = mChannelInfos[camId].previewUrl;
        bool usingConfig = hasConfigSource(camId);
        if (usingConfig) {
            if (camId < mVideoWidgets.size())
                mVideoWidgets[camId]->setSourceLabel("CFG");
            // Set up fallback only if config source differs from preview URL
            if (!previewUrl.isEmpty() && source != previewUrl) {
                mCameraFallbackSources[camId] = previewUrl;
                auto* fallbackTimer = new QTimer(this);
                fallbackTimer->setSingleShot(true);
                connect(fallbackTimer, &QTimer::timeout, this, [this, camId]() {
                    tryFallbackSource(camId);
                });
                fallbackTimer->start(12000);
                mCameraSourceFallbackTimers[camId] = fallbackTimer;
                qDebug() << "MainWindow: Camera" << camId
                         << "using config source:" << source
                         << "fallback:" << previewUrl << "(12s timeout)";
            } else {
                qDebug() << "MainWindow: Camera" << camId
                         << "using config source:" << source << "(no fallback)";
            }
        } else {
            if (camId < mVideoWidgets.size())
                mVideoWidgets[camId]->setSourceLabel(QString());
        }

        it.value()->requestStart(source);
        mCameraRunning[camId] = true;
        mAlertPanel->setCameraRunning(camId, true);
        if (camId < mVideoWidgets.size()) {
            mVideoWidgets[camId]->showNoSignal();
        }
    }

    // Start ImageStreamThreads
    for (auto it = mImageStreamThreads.begin(); it != mImageStreamThreads.end(); ++it) {
        int camId = it.key();
        QString url = resolveCameraSource(camId);

        // Build fallback URL chain: config snapshotUrl → channel snapshotUrl → channel previewUrl
        QString fallbackUrl;
        auto configs = mConfigManager->allConfigs();
        for (const auto& cfg : configs) {
            if (cfg.cameraId == camId && !cfg.snapshotUrl.isEmpty()) {
                fallbackUrl = cfg.snapshotUrl; break;
            }
        }
        if (fallbackUrl.isEmpty() && mChannelInfos.contains(camId)) {
            const auto& ch = mChannelInfos[camId];
            fallbackUrl = ch.snapshotUrl.isEmpty() ? ch.previewUrl : ch.snapshotUrl;
        }

        // If no config source, use the fallback chain as primary
        if (url.isEmpty())
            url = fallbackUrl;

        // Source label: show CFG whenever config has a source, regardless of fallback
        bool usingConfig = hasConfigSource(camId);
        if (usingConfig) {
            if (camId < mVideoWidgets.size())
                mVideoWidgets[camId]->setSourceLabel("CFG");
            // Set up fallback only if config source differs from the fallback URL
            if (!fallbackUrl.isEmpty() && url != fallbackUrl) {
                mCameraFallbackSources[camId] = fallbackUrl;
                auto* fallbackTimer = new QTimer(this);
                fallbackTimer->setSingleShot(true);
                connect(fallbackTimer, &QTimer::timeout, this, [this, camId]() {
                    tryFallbackSource(camId);
                });
                fallbackTimer->start(12000);
                mCameraSourceFallbackTimers[camId] = fallbackTimer;
                qDebug() << "MainWindow: Camera" << camId
                         << "(image_stream) using config source:" << url
                         << "fallback:" << fallbackUrl << "(12s timeout)";
            } else {
                qDebug() << "MainWindow: Camera" << camId
                         << "(image_stream) using config source:" << url << "(no fallback)";
            }
        } else {
            if (camId < mVideoWidgets.size())
                mVideoWidgets[camId]->setSourceLabel(QString());
        }

        it.value()->requestStart(url);
        mCameraRunning[camId] = true;
        mAlertPanel->setCameraRunning(camId, true);
        if (camId < mVideoWidgets.size()) {
            mVideoWidgets[camId]->showNoSignal();
        }
    }

    updatePanels();
    qDebug() << "MainWindow: All systems started";
}

void MainWindow::stopAll()
{
    if (!mRunning) return;

    mRunning = false;
    mSidebar->setRunning(false);

    for (auto it = mCameraThreads.begin(); it != mCameraThreads.end(); ++it) {
        it.value()->requestStop();
        mCameraRunning[it.key()] = false;
        mAlertPanel->setCameraRunning(it.key(), false);
    }

    for (auto it = mImageStreamThreads.begin(); it != mImageStreamThreads.end(); ++it) {
        it.value()->requestStop();
        mCameraRunning[it.key()] = false;
        mAlertPanel->setCameraRunning(it.key(), false);
    }

    for (auto* widget : mVideoWidgets) {
        widget->showNoSignal();
    }

    for (auto* smoother : mSmoothingFilters) {
        smoother->reset();
    }

    // Cancel all pending source fallback timers
    for (auto it = mCameraSourceFallbackTimers.begin(); it != mCameraSourceFallbackTimers.end(); ++it) {
        it.value()->stop();
        delete it.value();
    }
    mCameraSourceFallbackTimers.clear();
    mCameraFallbackSources.clear();

    updatePanels();
    qDebug() << "MainWindow: All systems stopped";
}

void MainWindow::toggleCamera(int cameraId)
{
    bool isCameraThread = mCameraThreads.contains(cameraId);
    bool isImageStream  = mImageStreamThreads.contains(cameraId);

    if (!isCameraThread && !isImageStream) return;

    bool running = mCameraRunning.value(cameraId, false);

    if (running) {
        if (isCameraThread)
            mCameraThreads[cameraId]->requestStop();
        else
            mImageStreamThreads[cameraId]->requestStop();
        mCameraRunning[cameraId] = false;
        mAlertPanel->setCameraRunning(cameraId, false);
        if (cameraId < mVideoWidgets.size())
            mVideoWidgets[cameraId]->showNoSignal();
    } else {
        QString source;
        if (mChannelInfos.contains(cameraId)) {
            source = mChannelInfos[cameraId].previewUrl;
        } else {
            auto configs = mConfigManager->allConfigs();
            for (const auto& cfg : configs) {
                if (cfg.cameraId == cameraId) {
                    source = isImageStream ? cfg.snapshotUrl : cfg.source;
                    break;
                }
            }
        }
        if (isCameraThread)
            mCameraThreads[cameraId]->requestStart(source);
        else
            mImageStreamThreads[cameraId]->requestStart(source);
        mCameraRunning[cameraId] = true;
        mAlertPanel->setCameraRunning(cameraId, true);
    }
}

void MainWindow::openSettings()
{
    SettingsDialog dlg(mConfigManager, this);
    connect(&dlg, &SettingsDialog::settingsSaved, this, [this]() {
        if (mSystemMode == "mqtt_subscribe" || mSystemMode == "image_stream") {
            // Just update detection params in-place, no pipeline teardown needed
            auto configs = mConfigManager->allConfigs();
            for (const auto& cfg : configs) {
                if (cfg.cameraId < mVideoWidgets.size())
                    mVideoWidgets[cfg.cameraId]->setTitle(cfg.name);

                if (mSmoothingFilters.contains(cfg.cameraId)) {
                    mSmoothingFilters[cfg.cameraId]->setAlpha(cfg.smoothingAlpha);
                    mSmoothingFilters[cfg.cameraId]->setMaxLostFrames(cfg.trackMaxLost);
                }
                if (mVideoWidgets.size() > cfg.cameraId)
                    mVideoWidgets[cfg.cameraId]->setConfidenceThreshold(cfg.confidenceThreshold);

                // Update snapshot interval for image_stream cameras
                if (mImageStreamThreads.contains(cfg.cameraId)) {
                    mImageStreamThreads[cfg.cameraId]->setSnapshotInterval(cfg.snapshotIntervalMs);
                }

                mAlertPanel->updateDeviceStatus(cfg.cameraId, cfg.name,
                    mCameraRunning.value(cfg.cameraId, false), 0.0, 0);
            }
            return;
        }

        bool wasRunning = mRunning;
        if (wasRunning) stopAll();

        for (int camId : mCameraThreads.keys())
            teardownCameraPipeline(camId);

        auto configs = mConfigManager->allConfigs();
        for (const auto& cfg : configs) {
            if (cfg.enabled)
                setupCameraPipeline(cfg.cameraId, cfg);

            if (cfg.cameraId < mVideoWidgets.size())
                mVideoWidgets[cfg.cameraId]->setTitle(cfg.name);

            mAlertPanel->updateDeviceStatus(cfg.cameraId, cfg.name, false, 0.0, 0);
        }

        if (wasRunning) startAll();
    });
    dlg.exec();
}

void MainWindow::onCameraClicked(int cameraId)
{
    if (cameraId == mSelectedCamera) return;

    // Deselect previous
    if (mSelectedCamera >= 0 && mSelectedCamera < mVideoWidgets.size())
        mVideoWidgets[mSelectedCamera]->setSelected(false);

    // Select new
    mSelectedCamera = cameraId;
    if (cameraId >= 0 && cameraId < mVideoWidgets.size())
        mVideoWidgets[cameraId]->setSelected(true);

    // If in 1x1 mode, switch to this camera
    if (mGridMode == 0)
        applyGridLayout(0);
}

void MainWindow::onGridModeChanged(int mode)
{
    mGridMode = mode;
    applyGridLayout(mode);
}

void MainWindow::applyGridLayout(int mode)
{
    // Remove all widgets from grid (without deleting them)
    while (mGridLayout->count() > 0) {
        QLayoutItem* item = mGridLayout->takeAt(0);
        if (item->widget())
            item->widget()->hide();
        delete item;
    }

    // Clear stretch settings and reset max heights
    for (int i = 0; i < 8; ++i) {
        mGridLayout->setRowStretch(i, 0);
        mGridLayout->setColumnStretch(i, 0);
    }
    for (auto* vw : mVideoWidgets)
        vw->setMaximumHeight(QWIDGETSIZE_MAX);

    int numDataRows = 1;

    switch (mode) {
    case 0: { // 1x1 — selected camera only
        int cam = mSelectedCamera;
        if (cam < 0 || cam >= mVideoWidgets.size()) cam = 0;
        mVideoWidgets[cam]->show();
        mGridLayout->addWidget(mVideoWidgets[cam], 0, 0);
        mGridLayout->setColumnStretch(0, 1);
        numDataRows = 1;
        break;
    }
    case 1: { // 2x2 — 4 cameras starting from selected block
        int start = (mSelectedCamera / 4) * 4;
        for (int i = 0; i < 4; ++i) {
            int camIdx = start + i;
            if (camIdx < mVideoWidgets.size()) {
                mVideoWidgets[camIdx]->show();
                mGridLayout->addWidget(mVideoWidgets[camIdx], i / 2, i % 2);
            }
        }
        mGridLayout->setColumnStretch(0, 1);
        mGridLayout->setColumnStretch(1, 1);
        numDataRows = 2;
        break;
    }
    case 2: { // 2x4 — all 8 cameras (default)
        for (int i = 0; i < 8; ++i) {
            mVideoWidgets[i]->show();
            mGridLayout->addWidget(mVideoWidgets[i], i / 4, i % 4);
        }
        for (int c = 0; c < 4; ++c)
            mGridLayout->setColumnStretch(c, 1);
        numDataRows = 2;
        break;
    }
    case 3: { // 3x3 — all 8 cameras + 1 empty
        for (int i = 0; i < 8; ++i) {
            mVideoWidgets[i]->show();
            mGridLayout->addWidget(mVideoWidgets[i], i / 3, i % 3);
        }
        for (int c = 0; c < 3; ++c)
            mGridLayout->setColumnStretch(c, 1);
        numDataRows = 3;
        break;
    }
    }

    // Data rows don't stretch — spacer row absorbs leftover vertical space
    for (int r = 0; r < numDataRows; ++r)
        mGridLayout->setRowStretch(r, 0);
    mGridLayout->setRowStretch(numDataRows, 1);

    QTimer::singleShot(0, this, &MainWindow::constrainVideoAspectRatios);
}

void MainWindow::constrainVideoAspectRatios()
{
    for (auto* vw : mVideoWidgets) {
        if (vw->isVisible() && vw->width() > 0) {
            int maxH = vw->width() * 9 / 16;
            vw->setMaximumHeight(maxH);
        }
    }
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    QTimer::singleShot(0, this, &MainWindow::constrainVideoAspectRatios);
}

void MainWindow::onToggleFullscreen()
{
    if (isFullScreen())
        showNormal();
    else
        showFullScreen();
}

void MainWindow::onSnapshotRequested()
{
    if (mSelectedCamera < 0 || mSelectedCamera >= mVideoWidgets.size()) return;
    QImage frame = mVideoWidgets[mSelectedCamera]->grabFullFrame();
    if (frame.isNull()) {
        qWarning() << "Snapshot: No frame available for camera" << mSelectedCamera;
        return;
    }

    QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (dir.isEmpty()) dir = QDir::homePath();
    QDir().mkpath(dir);
    QString path = dir + QString("/snapshot_cam%1_%2.png")
                       .arg(mSelectedCamera)
                       .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    if (frame.save(path)) {
        qDebug() << "Snapshot saved:" << path;
    } else {
        qWarning() << "Snapshot failed to save:" << path;
    }
}

void MainWindow::openDefectImageBrowser()
{
    if (!mDefectImageBrowser) {
        mDefectImageBrowser = new DefectImageBrowserDialog(mDefectImageStore, this);
        mDefectImageBrowser->setAttribute(Qt::WA_DeleteOnClose);
        connect(mDefectImageBrowser, &QObject::destroyed, this, [this]() {
            mDefectImageBrowser = nullptr;
        });
    }

    mDefectImageBrowser->show();
    mDefectImageBrowser->raise();
    mDefectImageBrowser->activateWindow();
}

// ================================================================
// Pipeline signal handlers
// ================================================================

void MainWindow::onChannelsDiscovered(const QVector<ChannelInfo>& channels,
                                      const QString& mqttSourceId)
{
    qDebug() << "MainWindow: Discovered" << channels.size() << "channel(s) from MQTT source"
             << mqttSourceId;

    bool pipelinesCreated = false;
    for (const auto& ch : channels) {
        // Skip if pipeline already exists for this camera
        if (mCameraThreads.contains(ch.cameraId) ||
            mImageStreamThreads.contains(ch.cameraId))
            continue;

        mChannelInfos[ch.cameraId] = ch;

        // Update VideoWidget title
        if (ch.cameraId < mVideoWidgets.size())
            mVideoWidgets[ch.cameraId]->setTitle(ch.name);

        // Update AlertPanel
        mAlertPanel->updateDeviceStatus(ch.cameraId, ch.name, false, 0.0, 0);

        // Create pipeline using a default CameraConfig
        CameraConfig cfg;
        cfg.cameraId = ch.cameraId;
        cfg.name = ch.name;
        cfg.mqttTopic = ch.inferenceTopic;
        cfg.enabled = true;

        if (mSystemMode == "image_stream") {
            cfg.mode = "image_stream";
            // Use config source if explicitly set, otherwise use channel URLs
            QString configSource = resolveCameraSource(ch.cameraId);
            if (!configSource.isEmpty()) {
                cfg.source = configSource;
                cfg.snapshotUrl = configSource;
            } else {
                cfg.snapshotUrl = ch.snapshotUrl.isEmpty() ? ch.previewUrl : ch.snapshotUrl;
                cfg.source = cfg.snapshotUrl;
            }
        } else {
            cfg.mode = "mqtt_subscribe";
            // Use config source if explicitly set, otherwise fall back to preview URL
            cfg.source = resolveCameraSource(ch.cameraId);
            if (cfg.source.isEmpty())
                cfg.source = ch.previewUrl;
        }

        // Merge alert config from config file (if camera has explicit config)
        CameraConfig fileCfg = mConfigManager->cameraConfig(ch.cameraId);
        if (fileCfg.cameraId == ch.cameraId) {
            cfg.alertConfig = fileCfg.alertConfig;
            cfg.alertSnapshotIntervalMs = fileCfg.alertSnapshotIntervalMs;
        }

        setupCameraPipeline(ch.cameraId, cfg);
        pipelinesCreated = true;

        // Subscribe to inference topic
        if (mInferenceSubscribers.contains(mqttSourceId) && !ch.inferenceTopic.isEmpty())
            mInferenceSubscribers[mqttSourceId]->subscribeTopic(ch.inferenceTopic);

        qDebug() << "MainWindow: Channel" << ch.cameraId
                 << "name:" << ch.name
                 << "preview:" << ch.previewUrl
                 << "snapshot:" << ch.snapshotUrl
                 << "topic:" << ch.inferenceTopic;
    }

    // Auto-start new pipelines if system is already running
    if (pipelinesCreated && mRunning) {
        for (const auto& ch : channels) {
            bool running = mCameraRunning.value(ch.cameraId, false);
            if (running) continue;

            if (mCameraThreads.contains(ch.cameraId)) {
                QString src = resolveCameraSource(ch.cameraId);
                if (src.isEmpty()) src = ch.previewUrl;

                // Source label: show CFG whenever config has a source
                bool usingConfig = hasConfigSource(ch.cameraId);
                if (usingConfig) {
                    if (ch.cameraId < mVideoWidgets.size())
                        mVideoWidgets[ch.cameraId]->setSourceLabel("CFG");
                    // Set up fallback only if config source differs from preview URL
                    if (!ch.previewUrl.isEmpty() && src != ch.previewUrl) {
                        mCameraFallbackSources[ch.cameraId] = ch.previewUrl;
                        auto* fallbackTimer = new QTimer(this);
                        fallbackTimer->setSingleShot(true);
                        connect(fallbackTimer, &QTimer::timeout, this, [this, camId = ch.cameraId]() {
                            tryFallbackSource(camId);
                        });
                        fallbackTimer->start(12000);
                        mCameraSourceFallbackTimers[ch.cameraId] = fallbackTimer;
                        qDebug() << "MainWindow: Camera" << ch.cameraId
                                 << "(auto-start) using config source:" << src
                                 << "fallback:" << ch.previewUrl << "(12s timeout)";
                    } else {
                        qDebug() << "MainWindow: Camera" << ch.cameraId
                                 << "(auto-start) using config source:" << src << "(no fallback)";
                    }
                } else {
                    if (ch.cameraId < mVideoWidgets.size())
                        mVideoWidgets[ch.cameraId]->setSourceLabel(QString());
                }

                mCameraThreads[ch.cameraId]->requestStart(src);
            } else if (mImageStreamThreads.contains(ch.cameraId)) {
                QString url = resolveCameraSource(ch.cameraId);
                QString fbUrl = ch.snapshotUrl.isEmpty() ? ch.previewUrl : ch.snapshotUrl;
                if (url.isEmpty()) url = fbUrl;

                // Source label: show CFG whenever config has a source
                bool usingConfig = hasConfigSource(ch.cameraId);
                if (usingConfig) {
                    if (ch.cameraId < mVideoWidgets.size())
                        mVideoWidgets[ch.cameraId]->setSourceLabel("CFG");
                    // Set up fallback only if config source differs from fallback URL
                    if (!fbUrl.isEmpty() && url != fbUrl) {
                        mCameraFallbackSources[ch.cameraId] = fbUrl;
                        auto* fallbackTimer = new QTimer(this);
                        fallbackTimer->setSingleShot(true);
                        connect(fallbackTimer, &QTimer::timeout, this, [this, camId = ch.cameraId]() {
                            tryFallbackSource(camId);
                        });
                        fallbackTimer->start(12000);
                        mCameraSourceFallbackTimers[ch.cameraId] = fallbackTimer;
                        qDebug() << "MainWindow: Camera" << ch.cameraId
                                 << "(auto-start image_stream) using config source:" << url
                                 << "fallback:" << fbUrl << "(12s timeout)";
                    } else {
                        qDebug() << "MainWindow: Camera" << ch.cameraId
                                 << "(auto-start image_stream) using config source:" << url << "(no fallback)";
                    }
                } else {
                    if (ch.cameraId < mVideoWidgets.size())
                        mVideoWidgets[ch.cameraId]->setSourceLabel(QString());
                }

                mImageStreamThreads[ch.cameraId]->requestStart(url);
            } else {
                continue;
            }
            mCameraRunning[ch.cameraId] = true;
            mAlertPanel->setCameraRunning(ch.cameraId, true);
        }
        updatePanels();
    }
}

void MainWindow::onConfidenceThresholdChanged(int cameraId, float threshold)
{
    mConfigManager->setConfidenceThreshold(cameraId, threshold);

    if (mInferenceEngines.contains(cameraId)) {
        mInferenceEngines[cameraId]->setConfidenceThreshold(threshold);
    }
}

void MainWindow::onDisplayFrameReady(const FrameData& frame)
{
    int camId = frame.cameraId;

    // First successful frame — cancel the source fallback timer
    if (mCameraSourceFallbackTimers.contains(camId)) {
        mCameraSourceFallbackTimers[camId]->stop();
        delete mCameraSourceFallbackTimers[camId];
        mCameraSourceFallbackTimers.remove(camId);
        mCameraFallbackSources.remove(camId);
        qDebug() << "MainWindow: Camera" << camId << "config source verified, fallback cancelled";
    }

    if (camId >= 0 && camId < mVideoWidgets.size()) {
        mVideoWidgets[camId]->updateDisplayFrame(frame);
    }
    // Signal capture thread that frame was consumed (backpressure)
    if (mCameraThreads.contains(camId)) {
        mCameraThreads[camId]->notifyFrameConsumed();
    }
}

void MainWindow::onInferenceFrameReady(const FrameData& frame)
{
    int camId = frame.cameraId;
    if (mInferenceEngines.contains(camId)) {
        emit mInferenceEngines[camId]->requestInference(frame);
    }
}

void MainWindow::onInferenceFinished(const InferenceResult& result)
{
    int camId = result.cameraId;

    if (!mCameraRunning.value(camId, false))
        return;

    if (mSmoothingFilters.contains(camId)) {
        mSmoothingFilters[camId]->processInferenceResult(result);
    } else {
        qWarning() << "MainWindow::onInferenceFinished: No SmoothingFilter for camera" << camId;
    }

    if (camId < mVideoWidgets.size()) {
        mVideoWidgets[camId]->updateInferenceTime(result.inferenceTimeMs);
    }

    QString camName;
    auto configs = mConfigManager->allConfigs();
    for (const auto& cfg : configs) {
        if (cfg.cameraId == camId) {
            camName = cfg.name;
            break;
        }
    }

    // Run detections through the alert filter pipeline
    QVector<Detection> defectDetections;
    float bestDefectConf = 0.0f;
    if (mAlertPipelines.contains(camId)) {
        defectDetections = mAlertPipelines[camId]->filter(result.detections);
        for (const auto& det : defectDetections) {
            if (det.confidence > bestDefectConf)
                bestDefectConf = det.confidence;
        }
    } else {
        // Fallback: no pipeline configured, use legacy hardcoded behavior
        for (const auto& det : result.detections) {
            if (det.filtered || det.classId != 1)
                continue;
            QVector<Detection> temp;
            temp.append(det); // unused
            defectDetections.append(det);
            if (det.confidence > bestDefectConf)
                bestDefectConf = det.confidence;
        }
    }

    // Apply per-camera minimum confidence threshold from alert config
    float minConf = 0.6f;
    if (mAlertPipelines.contains(camId))
        minConf = mAlertPipelines[camId]->config().minConfidence;

    // Cache latest defect detections for periodic alert snapshot timer
    mLatestDefectDetections[camId] = defectDetections;
    mLatestDefectConf[camId] = bestDefectConf;

    // One inference result containing any defect detection creates one
    // request. AlarmController serializes playback and enforces the global
    // 10-second cooldown across all cameras.
    if (!defectDetections.isEmpty()) {
        // mAlarmController->requestAlarm();
        // Independent QThread-relay player (coexists with AlarmController):
        // one worker thread per frame that contains any defect detection.
        mThreadedSoundPlayer->trigger();
    }

    if (bestDefectConf >= minConf && camId < mVideoWidgets.size()) {
        // Determine the actual classId of the highest-confidence defect
        int defectClassId = 1;
        for (const auto& det : defectDetections) {
            if (det.confidence >= bestDefectConf) {
                defectClassId = det.classId;
                break;
            }
        }

        QImage thumbnail = mVideoWidgets[camId]->grabThumbnail(100);
        if (camName.isEmpty() && mChannelInfos.contains(camId))
            camName = mChannelInfos[camId].name;
        if (camName.isEmpty())
            camName = QString("Camera %1").arg(camId + 1);
        mAlertPanel->addAlert(camId, camName, defectClassId, bestDefectConf, thumbnail);

        QImage frame = mVideoWidgets[camId]->grabFullFrame();
        if (!frame.isNull()) {
            mDefectImageStore->saveDefectImage(
                camId, camName, frame, defectDetections, bestDefectConf, result.timestamp);
        } else {
            qWarning() << "MainWindow: grabFullFrame returned null for camera" << camId
                       << "- defect image NOT saved";
        }
    }
}

void MainWindow::onDisplayResultReady(const DisplayResult& result)
{
    int camId = result.cameraId;

    if (!mCameraRunning.value(camId, false))
        return;

    if (camId >= 0 && camId < mVideoWidgets.size()) {
        // Apply alert filter pipeline to display detections so that
        // defect boxes failing the filter are hidden from the video widget.
        if (mAlertPipelines.contains(camId)) {
            DisplayResult filtered = result;
            QVector<Detection> passed = mAlertPipelines[camId]->filter(result.detections);
            const auto& defectIds = mAlertPipelines[camId]->config().defectClassIds;
            for (auto& det : filtered.detections) {
                if (!defectIds.contains(det.classId))
                    continue;
                bool inPassed = false;
                for (const auto& p : passed) {
                    if (p.trackId == det.trackId && p.classId == det.classId) {
                        inPassed = true;
                        break;
                    }
                }
                if (!inPassed)
                    det.filtered = true;
            }
            mVideoWidgets[camId]->updateDetectionOverlay(filtered);
        } else {
            mVideoWidgets[camId]->updateDetectionOverlay(result);
        }
    } else {
        qWarning() << "MainWindow::onDisplayResultReady: Invalid camera ID" << camId;
    }
}

void MainWindow::onFpsUpdated(int cameraId, double fps)
{
    if (cameraId >= 0 && cameraId < mVideoWidgets.size()) {
        mVideoWidgets[cameraId]->updateFps(fps);
    }
}

void MainWindow::onCameraError(const QString& message)
{
    qWarning() << "MainWindow: Camera error:" << message;
}

void MainWindow::updatePanels()
{
    auto configs = mConfigManager->allConfigs();
    for (int i = 0; i < mVideoWidgets.size(); ++i) {
        QString name = (i < configs.size()) ? configs[i].name : QString("Camera %1").arg(i + 1);
        bool camRunning = mCameraRunning.value(i, false);

        if (!camRunning) {
            mAlertPanel->updateDeviceStatus(i, name, false, 0.0, 0);
        } else {
            auto* w = mVideoWidgets[i];
            double fps = w->currentFps();
            int dets = w->detectionCount();
            bool online = w->hasSignal() || mCameraThreads.contains(i) || mImageStreamThreads.contains(i);
            mAlertPanel->updateDeviceStatus(i, name, online, fps, dets);
        }
    }

    // Health self-check: log RSS every 60s (30 ticks × 2s)
    static int healthTick = 0;
    healthTick++;
    if (healthTick % 30 == 0) {
        double rssMB = -1.0;
        QFile f("/proc/self/status");
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray data = f.readAll();
            f.close();
            int idx = data.indexOf("VmRSS:");
            if (idx >= 0) {
                int end = data.indexOf('\n', idx);
                QByteArray line = data.mid(idx, end - idx);
                QList<QByteArray> parts = line.split('\t');
                if (parts.size() >= 2)
                    rssMB = parts.last().trimmed().toDouble() / 1024.0;
            }
        }
        int onlineCams = 0;
        for (auto it = mCameraRunning.begin(); it != mCameraRunning.end(); ++it)
            if (it.value()) onlineCams++;
        qDebug() << "Health: RSS=" << rssMB << "MB, running=" << mRunning
                 << ", online_cams=" << onlineCams
                 << ", mode=" << mSystemMode
                 << ", uptime_min=" << (mStartTime > 0 ? (QDateTime::currentMSecsSinceEpoch() - mStartTime) / 60000 : 0);

        if (rssMB > 800.0) {
            qWarning() << "HIGH MEMORY WARNING: RSS=" << rssMB << "MB exceeds 800MB threshold!";
        }
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    stopAll();
    event->accept();
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        if (isFullScreen()) {
            showNormal();
        }
        break;
    case Qt::Key_F11:
    case Qt::Key_F:
        if (event->modifiers() & Qt::ControlModifier) {
            // Reserved
        } else {
            onToggleFullscreen();
        }
        break;
    case Qt::Key_Space:
        if (mRunning) {
            stopAll();
        } else {
            startAll();
        }
        break;
    case Qt::Key_1:
        onGridModeChanged(0);
        mBottomBar->setGridMode(0);
        break;
    case Qt::Key_2:
        onGridModeChanged(1);
        mBottomBar->setGridMode(1);
        break;
    case Qt::Key_3:
        onGridModeChanged(2);
        mBottomBar->setGridMode(2);
        break;
    case Qt::Key_4:
        onGridModeChanged(3);
        mBottomBar->setGridMode(3);
        break;
    default:
        break;
    }

    QMainWindow::keyPressEvent(event);
}
