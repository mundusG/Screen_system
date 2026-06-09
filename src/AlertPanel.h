#ifndef ALERTPANEL_H
#define ALERTPANEL_H

#include <QWidget>
#include <QVector>
#include <QDateTime>
#include <deque>

struct AlertEntry {
    int cameraId;
    QString cameraName;
    int classId;
    float confidence;
    QDateTime timestamp;
};

struct DeviceStatus {
    QString name;
    bool online = false;
    double fps = 0.0;
    int detections = 0;
};

class AlertPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AlertPanel(QWidget* parent = nullptr);

    void updateDeviceStatus(int cameraId, const QString& name, bool online, double fps, int dets);
    void addAlert(int cameraId, const QString& cameraName, int classId, float confidence);
    void clearAlerts();
    void setCameraRunning(int cameraId, bool running);

signals:
    void alertClicked(int cameraId);
    void cameraToggleRequested(int cameraId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void drawDeviceList(QPainter& p, const QRect& area);
    void drawAlertList(QPainter& p, const QRect& area);
    void drawPowerIcon(QPainter& p, const QRect& r, bool running);
    QRect toggleHitRect(int row) const;

    QVector<DeviceStatus> mDevices;
    QVector<bool> mCameraRunning;
    std::deque<AlertEntry> mAlerts;
    static constexpr int MaxAlerts = 20;
    int mScrollOffset = 0;
};

#endif // ALERTPANEL_H
