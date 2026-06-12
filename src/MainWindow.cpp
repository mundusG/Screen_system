#include "MainWindow.h"
#include "VideoWidget.h"
#include "SidebarWidget.h"
#include "BottomControlBar.h"
#include "AlertPanel.h"
#include "CameraCapture.h"
#include "InferenceEngine.h"
#include "SmoothingFilter.h"
#include "ConfigManager.h"
#include "SettingsDialog.h"
#include "InferenceSubscriber.h"
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
    , mInferenceSubscriber(nullptr)
    , mRunning(false)
    , mStartTime(0)
    , mSelectedCamera(0)
    , mGridMode(2) // 2x4 default
    , mSystemMode("local_inference")
{
    setupUI();

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
    mRootLayout->addWidget(mAlertPanel);

    qDebug() << "MainWindow::setupUI() - COMPLETE, showing window...";
    show();
    qDebug() << "MainWindow::setupUI() - Window shown, isVisible:" << isVisible();
}

bool MainWindow::initialize(const QString& configPath)
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

    // Determine system mode
    mSystemMode = mConfigManager->systemMode();
    qDebug() << "MainWindow: System mode:" << mSystemMode;

    // Initialize MQTT subscriber if in subscribe mode (async connection)
    if (mSystemMode == "mqtt_subscribe") {
        mInferenceSubscriber = new InferenceSubscriber(this);
        connect(mInferenceSubscriber, &InferenceSubscriber::inferenceFinished,
                this, &MainWindow::onInferenceFinished);
        connect(mInferenceSubscriber, &InferenceSubscriber::error,
                this, &MainWindow::onCameraError);

        // Build topic list from camera configs
        QStringList topics;
        auto configs = mConfigManager->allConfigs();
        for (const auto& cfg : configs) {
            if (cfg.enabled && !cfg.mqttTopic.isEmpty()) {
                topics.append(cfg.mqttTopic);
            }
        }

        // Connect to MQTT broker asynchronously using QTimer
        if (!topics.isEmpty()) {
            QTimer::singleShot(0, this, [this, topics]() {
                mInferenceSubscriber->connectAndSubscribe(
                    mConfigManager->mqttBroker(),
                    mConfigManager->mqttClientId(),
                    topics,
                    mConfigManager->mqttUsername(),
                    mConfigManager->mqttPassword()
                );
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

        if (cfg.enabled) {
            setupCameraPipeline(cfg.cameraId, cfg);
        }
    }

    qDebug() << "MainWindow: Initialized with" << configs.size() << "cameras";
    return true;
}

bool MainWindow::setupCameraPipeline(int cameraId, const CameraConfig& config)
{
    auto* smoother = new SmoothingFilter(cameraId, this);
    smoother->setAlpha(config.smoothingAlpha);
    smoother->setMaxLostFrames(config.trackMaxLost);

    connect(smoother, &SmoothingFilter::displayResultReady,
            this, &MainWindow::onDisplayResultReady);

    mSmoothingFilters[cameraId] = smoother;

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

    qWarning() << "MainWindow: Unsupported camera mode:" << cameraMode;
    return false;
}

void MainWindow::teardownCameraPipeline(int cameraId)
{
    mCameraRunning.remove(cameraId);
    mAlertPanel->setCameraRunning(cameraId, false);

    if (mCameraThreads.contains(cameraId)) {
        auto* camera = mCameraThreads[cameraId];
        mCameraThreads.remove(cameraId);
        delete camera;
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

void MainWindow::startAll()
{
    if (mRunning) return;

    mRunning = true;
    mStartTime = QDateTime::currentMSecsSinceEpoch();
    mSidebar->setRunning(true);

    for (auto it = mCameraThreads.begin(); it != mCameraThreads.end(); ++it) {
        int camId = it.key();
        auto configs = mConfigManager->allConfigs();
        QString source;
        for (const auto& cfg : configs) {
            if (cfg.cameraId == camId) { source = cfg.source; break; }
        }
        it.value()->requestStart(source);
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

    for (auto* widget : mVideoWidgets) {
        widget->showNoSignal();
    }

    for (auto* smoother : mSmoothingFilters) {
        smoother->reset();
    }

    updatePanels();
    qDebug() << "MainWindow: All systems stopped";
}

void MainWindow::toggleCamera(int cameraId)
{
    if (!mCameraThreads.contains(cameraId)) return;

    bool running = mCameraRunning.value(cameraId, false);

    if (running) {
        mCameraThreads[cameraId]->requestStop();
        mCameraRunning[cameraId] = false;
        mAlertPanel->setCameraRunning(cameraId, false);
        if (cameraId < mVideoWidgets.size())
            mVideoWidgets[cameraId]->showNoSignal();
    } else {
        auto configs = mConfigManager->allConfigs();
        QString source;
        for (const auto& cfg : configs) {
            if (cfg.cameraId == cameraId) { source = cfg.source; break; }
        }
        mCameraThreads[cameraId]->requestStart(source);
        mCameraRunning[cameraId] = true;
        mAlertPanel->setCameraRunning(cameraId, true);
    }
}

void MainWindow::openSettings()
{
    SettingsDialog dlg(mConfigManager, this);
    connect(&dlg, &SettingsDialog::settingsSaved, this, [this]() {
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

// ================================================================
// Pipeline signal handlers
// ================================================================

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
    if (camId >= 0 && camId < mVideoWidgets.size()) {
        mVideoWidgets[camId]->updateDisplayFrame(frame);
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

    qDebug() << "MainWindow::onInferenceFinished: camera" << camId
             << "detections:" << result.detections.size();

    if (mSmoothingFilters.contains(camId)) {
        mSmoothingFilters[camId]->processInferenceResult(result);
    } else {
        qWarning() << "MainWindow::onInferenceFinished: No SmoothingFilter for camera" << camId;
    }

    if (camId < mVideoWidgets.size()) {
        mVideoWidgets[camId]->updateInferenceTime(result.inferenceTimeMs);
    }

    // Feed alerts for high-confidence detections
    QImage thumbnail;
    bool thumbnailGrabbed = false;
    for (const auto& det : result.detections) {
        if (det.confidence >= 0.6f && camId < mVideoWidgets.size()) {
            if (!thumbnailGrabbed) {
                thumbnail = mVideoWidgets[camId]->grabThumbnail(100);
                thumbnailGrabbed = true;
            }
            auto configs = mConfigManager->allConfigs();
            QString camName;
            for (const auto& cfg : configs) {
                if (cfg.cameraId == camId) { camName = cfg.name; break; }
            }
            mAlertPanel->addAlert(camId, camName, det.classId, det.confidence, thumbnail);
        }
    }
}

void MainWindow::onDisplayResultReady(const DisplayResult& result)
{
    int camId = result.cameraId;
    qDebug() << "MainWindow::onDisplayResultReady: camera" << camId
             << "detections:" << result.detections.size();

    if (camId >= 0 && camId < mVideoWidgets.size()) {
        mVideoWidgets[camId]->updateDetectionOverlay(result);
        qDebug() << "MainWindow::onDisplayResultReady: Updated VideoWidget" << camId;
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
            bool online = w->hasSignal() || mCameraThreads.contains(i);
            mAlertPanel->updateDeviceStatus(i, name, online, fps, dets);
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
