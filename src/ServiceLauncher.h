#ifndef SERVICELAUNCHER_H
#define SERVICELAUNCHER_H

#include <QObject>
#include <QProcess>

class ServiceLauncher : public QObject
{
    Q_OBJECT
public:
    explicit ServiceLauncher(QObject* parent = nullptr);
    ~ServiceLauncher() override;

    bool isMosquittoRunning() const;

    bool startNNBridge(const QString& pythonBin,
                       const QString& scriptPath,
                       const QString& configPath);

    void stopAll();

signals:
    void serviceError(const QString& serviceName, const QString& message);

private slots:
    void onBridgeFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onBridgeError(QProcess::ProcessError error);
    void onBridgeStdout();
    void onBridgeStderr();

private:
    QProcess* mBridgeProcess = nullptr;
};

#endif // SERVICELAUNCHER_H
