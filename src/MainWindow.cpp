#include "MainWindow.h"
#include "VideoWidget.h"
#include "StatsPanel.h"
#include "CameraCapture.h"
#include "InferenceEngine.h"
#include "SmoothingFilter.h"
#include "ConfigManager.h"
#include "SettingsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QDateTime>
#include <QApplication>
#include <QDebug>
#include <QLabel>
#include <QPainter>
#include <QPixmap>

namespace {
class BackgroundWidget : public QWidget {
public:
    explicit BackgroundWidget(QWidget* parent = nullptr)
        : QWidget(parent), mBg(":/home_image.png") {}
protected:
    void paintEvent(QPaintEvent* event) override {
        QPainter p(this);
        p.drawPixmap(rect(), mBg);
        QWidget::paintEvent(event);
    }
private:
    QPixmap mBg;
};
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , mCentralWidget(nullptr)
    , mRootLayout(nullptr)
    , mGridLayout(nullptr)
    , mConfigManager(new ConfigManager(this))
    , mStatsPanel(nullptr)
    , mRunning(false)
    , mStartTime(0)
{
    setupUI();

    // Status update timer
    mStatusTimer = new QTimer(this);
    connect(mStatusTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    mStatusTimer->start(2000);  // every 2 seconds

    // Register metatypes
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
    // Window properties — optimized for large screen display
    setWindowTitle("Screen Inference System");
    resize(1920, 1080);
    setMinimumSize(1280, 720);

    // Dark theme
    setStyleSheet(
        "QMainWindow { background-color: #1a1a2e; }"
        "QMenuBar { background-color: #16213e; color: #e0e0e0; font-size: 13px; }"
        "QMenuBar::item:selected { background-color: #0f3460; }"
        "QMenu { background-color: #16213e; color: #e0e0e0; border: 1px solid #0f3460; }"
        "QMenu::item:selected { background-color: #0f3460; }"
        "QStatusBar { background-color: #16213e; color: #e0e0e0; font-size: 12px; }"
        "QGroupBox { color: #e0e0e0; border: 1px solid #333; border-radius: 4px; margin-top: 8px; "
        "            font-size: 12px; font-weight: bold; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 5px; }"
        "QPushButton { background-color: #0f3460; color: white; border: none; "
        "              padding: 6px 16px; border-radius: 3px; font-size: 12px; }"
        "QPushButton:hover { background-color: #1a5276; }"
        "QPushButton:pressed { background-color: #0a2647; }"
    );

    // Central widget — paints background image behind all children
    mCentralWidget = new BackgroundWidget(this);
    setCentralWidget(mCentralWidget);

    // Root layout: video grid (left, 3 parts) | stats panel (right, 1 part)
    mRootLayout = new QHBoxLayout(mCentralWidget);
    mRootLayout->setContentsMargins(8, 8, 8, 8);
    mRootLayout->setSpacing(10);

    // --- Video grid: 2 rows x 4 columns ---
    mGridLayout = new QGridLayout();
    mGridLayout->setSpacing(4);
    mGridLayout->setContentsMargins(0, 0, 0, 0);

    static const char* CAM_NAMES[8] = {
        "产线1 前", "产线1 后", "产线1 左", "产线1 右",
        "产线2 前", "产线2 后", "产线2 左", "产线2 右"
    };

    for (int i = 0; i < 8; ++i) {
        int row = i / 4;
        int col = i % 4;
        QString title = QString::fromUtf8(CAM_NAMES[i]);

        auto* videoWidget = new VideoWidget(i, title, mCentralWidget);
        mVideoWidgets.append(videoWidget);

        connect(videoWidget, &VideoWidget::confidenceThresholdChanged,
                this, &MainWindow::onConfidenceThresholdChanged);
        connect(videoWidget, &VideoWidget::clicked, this, [this](int camId) {
            qDebug() << "Camera" << camId << "clicked";
        });

        mGridLayout->addWidget(videoWidget, row, col);
        mGridLayout->setColumnStretch(col, 1);
    }
    // Rows are sized by heightForWidth (16:9); push leftover space to bottom
    mGridLayout->setRowStretch(2, 1);

    // Wrap grid in a container so we can set stretch ratio
    auto* gridContainer = new QWidget(mCentralWidget);
    gridContainer->setAttribute(Qt::WA_TranslucentBackground);
    auto* gcLayout = new QVBoxLayout(gridContainer);
    gcLayout->setContentsMargins(0, 0, 0, 0);
    gcLayout->addLayout(mGridLayout);

    mRootLayout->addWidget(gridContainer, 3);   // 3/4 width for cameras

    // --- Right stats panel ---
    mStatsPanel = new StatsPanel(mCentralWidget);
    mRootLayout->addWidget(mStatsPanel, 1);     // 1/4 width for stats

    statusBar()->hide();

    // Menu bar — 文件(F)
    QMenu* fileMenu = menuBar()->addMenu(QString::fromUtf8("文件(&F)"));
    QAction* startAllAct = fileMenu->addAction(QString::fromUtf8("启动全部"));
    connect(startAllAct, &QAction::triggered, this, &MainWindow::startAll);
    QAction* stopAllAct = fileMenu->addAction(QString::fromUtf8("停止全部"));
    connect(stopAllAct, &QAction::triggered, this, &MainWindow::stopAll);
    fileMenu->addSeparator();
    QAction* settingsAct = fileMenu->addAction(QString::fromUtf8("设置..."));
    connect(settingsAct, &QAction::triggered, this, &MainWindow::openSettings);
    fileMenu->addSeparator();
    QAction* quitAct = fileMenu->addAction(QString::fromUtf8("退出"));
    connect(quitAct, &QAction::triggered, this, &QMainWindow::close);
}

bool MainWindow::initialize(const QString& configPath)
{
    QString path = configPath.isEmpty() ? ConfigManager::resolveConfigPath() : configPath;

    qDebug() << "MainWindow: Loading config from" << path;

    if (!mConfigManager->loadFromFile(path)) {
        qWarning() << "MainWindow: Failed to load config, using defaults";
        // Create default 8-camera config
        for (int i = 0; i < 8; ++i) {
            CameraConfig cfg;
            cfg.cameraId = i;
            cfg.name     = QString::fromUtf8("摄像头 %1").arg(i + 1);
            cfg.source   = QString::number(i);  // /dev/videoN or camera index
            cfg.modelPath = QString("camera_%1.onnx").arg(i);
            mConfigManager->setCameraConfig(i, cfg);
        }
    }

    // Setup pipeline for each camera
    auto configs = mConfigManager->allConfigs();
    for (const auto& cfg : configs) {
        if (cfg.enabled) {
            setupCameraPipeline(cfg.cameraId, cfg);
        }
    }

    qDebug() << "MainWindow: Initialized with" << configs.size() << "cameras";
    return true;
}

bool MainWindow::setupCameraPipeline(int cameraId, const CameraConfig& config)
{
    // --- Create smoothing filter (main thread) ---
    auto* smoother = new SmoothingFilter(cameraId, this);
    smoother->setAlpha(config.smoothingAlpha);
    smoother->setMaxLostFrames(config.trackMaxLost);

    connect(smoother, &SmoothingFilter::displayResultReady,
            this, &MainWindow::onDisplayResultReady);

    mSmoothingFilters[cameraId] = smoother;

    // --- Create inference engine (own thread) ---
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

    // --- Create camera capture (own thread) ---
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

    // Open camera source
    bool opened = camera->open(config.source);

    // Update video widget title
    if (cameraId < mVideoWidgets.size()) {
        mVideoWidgets[cameraId]->setConfidenceThreshold(config.confidenceThreshold);
    }

    qDebug() << "MainWindow: Camera" << cameraId << "pipeline setup."
             << "source:" << config.source
             << "model:" << (modelLoaded ? "loaded" : "not loaded")
             << "opened:" << opened;

    return opened;
}

void MainWindow::teardownCameraPipeline(int cameraId)
{
    // Remove in reverse order

    if (mCameraThreads.contains(cameraId)) {
        auto* camera = mCameraThreads[cameraId];
        mCameraThreads.remove(cameraId);
        delete camera;  // stops capture + thread
    }

    if (mInferenceEngines.contains(cameraId)) {
        auto* inference = mInferenceEngines[cameraId];
        mInferenceEngines.remove(cameraId);
        delete inference;  // stops inference thread
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

    // Start capture threads for all cameras
    for (auto it = mCameraThreads.begin(); it != mCameraThreads.end(); ++it) {
        // Capture thread starts automatically via thread->start() in constructor
        // If it was stopped, we need to recreate
        int camId = it.key();
        if (camId < mVideoWidgets.size()) {
            mVideoWidgets[camId]->showNoSignal();
        }
    }

    updateStatusBar();
    qDebug() << "MainWindow: All systems started";
}

void MainWindow::stopAll()
{
    if (!mRunning) return;

    mRunning = false;

    // Stop all captures
    for (auto* camera : mCameraThreads) {
        camera->close();
    }

    // Reset all video widgets
    for (auto* widget : mVideoWidgets) {
        widget->showNoSignal();
    }

    // Reset all smoothers
    for (auto* smoother : mSmoothingFilters) {
        smoother->reset();
    }

    updateStatusBar();
    qDebug() << "MainWindow: All systems stopped";
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

            // Update VideoWidget title regardless of enabled state
            if (cfg.cameraId < mVideoWidgets.size())
                mVideoWidgets[cfg.cameraId]->setTitle(cfg.name);
        }

        if (wasRunning) startAll();
    });
    dlg.exec();
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

    qDebug() << "MainWindow: Camera" << cameraId
             << "confidence threshold =" << threshold;
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
        // Queue inference via signal to worker thread
        emit mInferenceEngines[camId]->requestInference(frame);
    }
}

void MainWindow::onInferenceFinished(const InferenceResult& result)
{
    int camId = result.cameraId;

    // Forward to smoothing filter
    if (mSmoothingFilters.contains(camId)) {
        mSmoothingFilters[camId]->processInferenceResult(result);
    }

    // Update inference time display
    if (camId < mVideoWidgets.size()) {
        mVideoWidgets[camId]->updateInferenceTime(result.inferenceTimeMs);
    }
}

void MainWindow::onDisplayResultReady(const DisplayResult& result)
{
    int camId = result.cameraId;
    if (camId >= 0 && camId < mVideoWidgets.size()) {
        mVideoWidgets[camId]->updateDetectionOverlay(result);
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
    statusBar()->showMessage(message, 5000);
}

void MainWindow::updateStatusBar()
{
    qint64 elapsed = mRunning ? (QDateTime::currentMSecsSinceEpoch() - mStartTime) : 0;

    if (!mRunning) {
        mStatsPanel->setRunning(false);
        mStatsPanel->updateStats(0, 0.0, 0, 0, {}, {});
        return;
    }

    QVector<int>    perCamDets(mVideoWidgets.size(), 0);
    QVector<double> perCamFps(mVideoWidgets.size(), 0.0);
    int totalDets = 0;
    double totalFps = 0.0;
    int activeCams = 0;

    for (int i = 0; i < mVideoWidgets.size(); ++i) {
        auto* w = mVideoWidgets[i];
        int d = w->detectionCount();
        double f = w->currentFps();
        perCamDets[i] = d;
        perCamFps[i]  = f;
        totalDets += d;
        if (f > 0.0) { totalFps += f; ++activeCams; }
    }
    double avgFps = activeCams > 0 ? totalFps / activeCams : 0.0;

    mStatsPanel->setRunning(true);
    mStatsPanel->updateStats(totalDets, avgFps, activeCams, elapsed, perCamDets, perCamFps);
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
            // Reserved for future: fullscreen single camera
        } else {
            if (isFullScreen()) {
                showNormal();
            } else {
                showFullScreen();
            }
        }
        break;
    case Qt::Key_Space:
        if (mRunning) {
            stopAll();
        } else {
            startAll();
        }
        break;
    default:
        break;
    }

    QMainWindow::keyPressEvent(event);
}
