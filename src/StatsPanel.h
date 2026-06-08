#ifndef STATSPANEL_H
#define STATSPANEL_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QVector>
#include <deque>

class StatsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit StatsPanel(QWidget* parent = nullptr);

    void updateStats(int totalDetections, double avgFps, int activeCams,
                     qint64 uptimeMs, const QVector<int>& perCamDets,
                     const QVector<double>& perCamFps);

    void setRunning(bool running);

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    void paintChart();
    void rebuildCamRows(int count);

    QLabel* mDetLabel    = nullptr;
    QLabel* mCamLabel    = nullptr;
    QWidget*     mChartWidget    = nullptr;
    QWidget*     mCamTable       = nullptr;
    QVBoxLayout* mCamTableLayout = nullptr;
    QVector<QLabel*> mCamRows;
    std::deque<int>  mHistory;
    bool mRunning = false;
};

#endif // STATSPANEL_H
