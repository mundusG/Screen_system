#include "ServiceLauncher.h"
#include <QProcess>
#include <QDebug>

ServiceLauncher::ServiceLauncher(QObject* parent)
    : QObject(parent)
{
}

ServiceLauncher::~ServiceLauncher()
{
}

bool ServiceLauncher::isMosquittoRunning() const
{
    QProcess proc;
    proc.start("pgrep", QStringList() << "-x" << "mosquitto");
    proc.waitForFinished(3000);
    return proc.exitCode() == 0;
}
