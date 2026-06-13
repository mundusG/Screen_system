#include "ServiceLauncher.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

ServiceLauncher::ServiceLauncher(QObject* parent)
    : QObject(parent)
{
}

ServiceLauncher::~ServiceLauncher()
{
    stopAll();
}

bool ServiceLauncher::isMosquittoRunning() const
{
    QProcess proc;
    proc.start("pgrep", QStringList() << "-x" << "mosquitto");
    proc.waitForFinished(3000);
    return proc.exitCode() == 0;
}

bool ServiceLauncher::startNNBridge(const QString& pythonBin,
                                     const QString& scriptPath,
                                     const QString& configPath)
{
    if (mBridgeProcess) {
        qDebug() << "ServiceLauncher: nn_bridge already running";
        return true;
    }

    if (!QFileInfo::exists(scriptPath)) {
        QString msg = QString("nn_bridge script not found: %1").arg(scriptPath);
        qWarning() << "ServiceLauncher:" << msg;
        emit serviceError("nn_bridge", msg);
        return false;
    }

    if (!QFileInfo::exists(configPath)) {
        QString msg = QString("nn_bridge config not found: %1").arg(configPath);
        qWarning() << "ServiceLauncher:" << msg;
        emit serviceError("nn_bridge", msg);
        return false;
    }

    mBridgeProcess = new QProcess(this);
    mBridgeProcess->setWorkingDirectory(QFileInfo(scriptPath).absolutePath());

    connect(mBridgeProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ServiceLauncher::onBridgeFinished);
    connect(mBridgeProcess, &QProcess::errorOccurred,
            this, &ServiceLauncher::onBridgeError);
    connect(mBridgeProcess, &QProcess::readyReadStandardOutput,
            this, &ServiceLauncher::onBridgeStdout);
    connect(mBridgeProcess, &QProcess::readyReadStandardError,
            this, &ServiceLauncher::onBridgeStderr);

    QStringList args;
    args << scriptPath << configPath;

    qDebug() << "ServiceLauncher: Starting nn_bridge:" << pythonBin << args;
    mBridgeProcess->start(pythonBin, args);

    if (!mBridgeProcess->waitForStarted(5000)) {
        QString msg = QString("Failed to start nn_bridge: %1")
                          .arg(mBridgeProcess->errorString());
        qWarning() << "ServiceLauncher:" << msg;
        emit serviceError("nn_bridge", msg);
        delete mBridgeProcess;
        mBridgeProcess = nullptr;
        return false;
    }

    qDebug() << "ServiceLauncher: nn_bridge started, PID:" << mBridgeProcess->processId();
    return true;
}

void ServiceLauncher::stopAll()
{
    if (!mBridgeProcess)
        return;

    qDebug() << "ServiceLauncher: Stopping nn_bridge...";
    mBridgeProcess->terminate();
    if (!mBridgeProcess->waitForFinished(3000)) {
        qWarning() << "ServiceLauncher: nn_bridge did not exit, killing";
        mBridgeProcess->kill();
        mBridgeProcess->waitForFinished(2000);
    }

    delete mBridgeProcess;
    mBridgeProcess = nullptr;
    qDebug() << "ServiceLauncher: nn_bridge stopped";
}

void ServiceLauncher::onBridgeFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus);
    qWarning() << "ServiceLauncher: nn_bridge exited, code:" << exitCode;
    emit serviceError("nn_bridge",
        QString("nn_bridge exited unexpectedly (code %1)").arg(exitCode));
    mBridgeProcess->deleteLater();
    mBridgeProcess = nullptr;
}

void ServiceLauncher::onBridgeError(QProcess::ProcessError error)
{
    Q_UNUSED(error);
    qWarning() << "ServiceLauncher: nn_bridge process error:"
               << (mBridgeProcess ? mBridgeProcess->errorString() : "unknown");
}

void ServiceLauncher::onBridgeStdout()
{
    if (mBridgeProcess)
        qDebug().noquote() << "[nn_bridge]" << mBridgeProcess->readAllStandardOutput().trimmed();
}

void ServiceLauncher::onBridgeStderr()
{
    if (mBridgeProcess)
        qDebug().noquote() << "[nn_bridge]" << mBridgeProcess->readAllStandardError().trimmed();
}
