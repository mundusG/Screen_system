#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QTimer>
#include <QMap>
#include <opencv2/core.hpp>
#include "src/ConfigManager.h"
#include "src/CameraCapture.h"
#include "src/RKNNInferenceEngine.h"
#include "src/InferencePublisher.h"
#include "src/Types.h"

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("RKNNInferenceNode");
    app.setApplicationVersion("1.0.0");

    // Limit OpenCV thread usage on edge device
    cv::setNumThreads(2);

    // Command line parsing
    QCommandLineParser parser;
    parser.setApplicationDescription("RKNN Inference Node - Edge device inference publisher");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption configOpt(QStringList() << "c" << "config",
                                 "Path to config JSON file", "path");
    parser.addOption(configOpt);
    parser.process(app);

    // Load config
    ConfigManager configManager;
    QString configPath = parser.value(configOpt);
    if (configPath.isEmpty()) {
        configPath = ConfigManager::resolveConfigPath();
    }

    if (!configManager.loadFromFile(configPath)) {
        qCritical() << "Failed to load config from:" << configPath;
        return 1;
    }

    qDebug() << "RKNNInferenceNode: Config loaded from" << configPath;
    qDebug() << "RKNNInferenceNode: System mode:" << configManager.systemMode();

    // Create shared MQTT publisher
    InferencePublisher* publisher = new InferencePublisher(&app);

    // Connect to MQTT broker
    QString broker = configManager.mqttBroker();
    QString clientId = configManager.mqttClientId();
    if (clientId.isEmpty()) clientId = "rknn_inference_node";

    qDebug() << "RKNNInferenceNode: Connecting to MQTT broker:" << broker;

    if (!publisher->connectToBroker(broker, clientId,
                                     configManager.mqttUsername(),
                                     configManager.mqttPassword())) {
        qWarning() << "RKNNInferenceNode: MQTT connection initiated (async)";
    }

    // Setup pipelines for each enabled mqtt_publish camera
    QMap<int, CameraThread*> cameraThreads;
    QMap<int, RKNNInferenceEngine*> inferenceEngines;
    int pipelineCount = 0;

    auto configs = configManager.allConfigs();
    for (const auto& cfg : configs) {
        if (!cfg.enabled) continue;

        QString mode = cfg.mode.isEmpty() ? configManager.systemMode() : cfg.mode;
        if (mode != "mqtt_publish") continue;

        int camId = cfg.cameraId;

        // Create RKNN inference engine
        auto* engine = new RKNNInferenceEngine(camId, &app);
        if (!cfg.modelPath.isEmpty()) {
            QString modelPath = ConfigManager::resolveModelPath(cfg.modelPath);
            if (!engine->loadModel(modelPath, cfg.inputWidth, cfg.inputHeight)) {
                qWarning() << "RKNNInferenceNode: Failed to load model for camera" << camId
                           << "path:" << modelPath;
                delete engine;
                continue;
            }
        } else {
            qWarning() << "RKNNInferenceNode: No model path for camera" << camId;
            delete engine;
            continue;
        }

        engine->setConfidenceThreshold(cfg.confidenceThreshold);
        engine->setNmsThreshold(cfg.nmsThreshold);

        // Create camera thread
        auto* camera = new CameraThread(camId, &app);
        camera->setInferenceInterval(cfg.inferenceIntervalMs);

        // Connect pipeline: Camera → Engine → Publisher
        QObject::connect(camera, &CameraThread::inferenceFrameReady,
                         engine, &RKNNInferenceEngine::requestInference);
        QObject::connect(engine, &RKNNInferenceEngine::inferenceFinished,
                         publisher, &InferencePublisher::onInferenceFinished);
        QObject::connect(engine, &RKNNInferenceEngine::error,
                         [camId](const QString& msg) {
                             qWarning() << "RKNNInferenceNode: Engine error cam" << camId << ":" << msg;
                         });
        QObject::connect(camera, &CameraThread::error,
                         [camId](const QString& msg) {
                             qWarning() << "RKNNInferenceNode: Camera error cam" << camId << ":" << msg;
                         });
        QObject::connect(camera, &CameraThread::fpsUpdated,
                         [](int id, double fps) {
                             static int counter = 0;
                             if (++counter % 10 == 0) {
                                 qDebug() << "  Camera" << id << "FPS:" << fps;
                             }
                         });

        cameraThreads[camId] = camera;
        inferenceEngines[camId] = engine;
        pipelineCount++;

        qDebug() << "RKNNInferenceNode: Pipeline created for camera" << camId
                 << "source:" << cfg.source
                 << "interval:" << cfg.inferenceIntervalMs << "ms";
    }

    if (pipelineCount == 0) {
        qCritical() << "RKNNInferenceNode: No mqtt_publish pipelines configured. Exiting.";
        return 1;
    }

    qDebug() << "RKNNInferenceNode:" << pipelineCount << "pipeline(s) ready. Starting cameras...";

    // Start all cameras asynchronously
    QTimer::singleShot(500, [&]() {
        for (auto it = cameraThreads.begin(); it != cameraThreads.end(); ++it) {
            int camId = it.key();
            QString source;
            for (const auto& cfg : configs) {
                if (cfg.cameraId == camId) { source = cfg.source; break; }
            }
            qDebug() << "RKNNInferenceNode: Starting camera" << camId << "source:" << source;
            it.value()->requestStart(source);
        }
    });

    // Periodic status report
    QTimer statusTimer;
    QObject::connect(&statusTimer, &QTimer::timeout, [&]() {
        qDebug() << "RKNNInferenceNode: Running," << pipelineCount << "pipeline(s) active,"
                 << "MQTT connected:" << publisher->isConnected();
    });
    statusTimer.start(30000);

    return app.exec();
}
