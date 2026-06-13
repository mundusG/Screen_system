#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QMessageBox>
#include <QFont>
#include <QFontDatabase>
#include <QTimer>
#include "src/MainWindow.h"
#include "src/ConfigManager.h"

int main(int argc, char* argv[])
{
    // High-DPI support
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);
    app.setApplicationName("ScreenInferenceSystem");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("ScreenInferenceSystem");

    // Set default font for Chinese text rendering (WSLg)
    {
        // Pick the first available CJK font
        const QStringList candidates = {
            "WenQuanYi Micro Hei",
            "WenQuanYi Zen Hei",
            "Noto Sans CJK SC",
            "Noto Sans SC",
            "Source Han Sans CN",
            "AR PL UMing CN",
            "DejaVu Sans",
        };
        QFontDatabase db;
        QString chosen;
        for (const QString& name : candidates) {
            if (db.hasFamily(name)) {
                chosen = name;
                break;
            }
        }
        if (chosen.isEmpty()) {
            qWarning() << "No CJK font found — Chinese text may render as boxes. "
                          "Install fonts-wqy-microhei or fonts-noto-cjk.";
            chosen = "Sans Serif";
        } else {
            qDebug() << "Using CJK font:" << chosen;
        }
        QFont defaultFont(chosen, 10);
        defaultFont.setStyleHint(QFont::SansSerif);
        app.setFont(defaultFont);
    }

    // Command line parsing
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QString::fromUtf8("基于Qt5 + OpenCV DNN的多路摄像头YOLO实时推理大屏显示系统\n"
                          "支持8路摄像头同时显示，各自独立模型权重，实时推理结果叠加显示"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption configOption(
        QStringList() << "c" << "config",
        QString::fromUtf8("配置文件路径 (JSON格式)"),
        "config_path");
    parser.addOption(configOption);

    QCommandLineOption fullscreenOption(
        QStringList() << "f" << "fullscreen",
        QString::fromUtf8("启动时直接全屏显示"));
    parser.addOption(fullscreenOption);

    QCommandLineOption cameraCountOption(
        QStringList() << "n" << "num-cameras",
        QString::fromUtf8("摄像头数量 (默认8路)"),
        "number", "8");
    parser.addOption(cameraCountOption);

    parser.process(app);

    // Set OpenCV threading to avoid conflicts with Qt threads
    // (OpenCV may try to use TBB or OpenMP internally)
    cv::setNumThreads(2);  // limit OpenCV internal threads per inference worker

    // Create and initialize main window
    MainWindow mainWindow;

    // Determine config path: command-line arg takes priority, then auto-detect
    QString configPath;
    if (parser.isSet(configOption)) {
        configPath = parser.value(configOption);
    } else {
        configPath = ConfigManager::resolveConfigPath();
    }

    // Initialize system
    if (!mainWindow.initialize(configPath)) {
        QMessageBox::warning(
            nullptr,
            "Initialization Warning",
            QString::fromUtf8("部分摄像头或模型加载失败。\n请检查配置文件中的设备路径和模型文件是否存在。"));
        // Continue anyway — user can fix config at runtime
    }

    // Fullscreen mode
    if (parser.isSet(fullscreenOption)) {
        mainWindow.showFullScreen();
    } else {
        mainWindow.showMaximized();
    }

    // Don't auto-start in mqtt_subscribe mode to avoid blocking
    // User can manually start via UI button
    // QTimer::singleShot(100, &mainWindow, &MainWindow::startAll);

    int result = app.exec();

    // Cleanup
    mainWindow.stopAll();

    qDebug() << "Application exiting with code:" << result;
    return result;
}
