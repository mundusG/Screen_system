#ifndef SERVICELAUNCHER_H
#define SERVICELAUNCHER_H

#include <QObject>

class ServiceLauncher : public QObject
{
    Q_OBJECT
public:
    explicit ServiceLauncher(QObject* parent = nullptr);
    ~ServiceLauncher() override;

    bool isMosquittoRunning() const;

signals:
    void serviceError(const QString& serviceName, const QString& message);
};

#endif // SERVICELAUNCHER_H
