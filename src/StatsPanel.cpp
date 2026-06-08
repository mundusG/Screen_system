#include "StatsPanel.h"
#include <QPainter>
#include <QLinearGradient>
#include <QPolygonF>
#include <QEvent>
#include <algorithm>

StatsPanel::StatsPanel(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMinimumWidth(320);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 20, 16, 20);
    root->setSpacing(16);

    // Title
    auto* titleLabel = new QLabel(QString::fromUtf8("系统统计"), this);
    titleLabel->setStyleSheet(
        "color: #e0f0ff; font-size: 18px; font-weight: bold; "
        "border-bottom: 1px solid rgba(100,180,255,0.4); padding-bottom: 8px;");
    root->addWidget(titleLabel);

    // Stat cards
    auto makeCard = [&](const QString& label, QLabel*& valueOut) {
        auto* card = new QWidget(this);
        card->setStyleSheet("QWidget { background: rgba(10,30,60,0.72); border-radius: 8px; }");
        auto* vl = new QVBoxLayout(card);
        vl->setContentsMargins(14, 10, 14, 10);
        vl->setSpacing(4);
        auto* lbl = new QLabel(label, card);
        lbl->setStyleSheet("color: rgba(180,210,255,0.85); font-size: 12px;");
        valueOut = new QLabel("--", card);
        valueOut->setStyleSheet("color: #ffffff; font-size: 22px; font-weight: bold;");
        vl->addWidget(lbl);
        vl->addWidget(valueOut);
        root->addWidget(card);
    };

    makeCard(QString::fromUtf8("当前检测总数"), mDetLabel);
    makeCard(QString::fromUtf8("活跃摄像头"),   mCamLabel);

    // Chart
    auto* chartTitle = new QLabel(QString::fromUtf8("检测数量趋势"), this);
    chartTitle->setStyleSheet("color: rgba(180,210,255,0.85); font-size: 12px; margin-top: 4px;");
    root->addWidget(chartTitle);

    mChartWidget = new QWidget(this);
    mChartWidget->setMinimumHeight(140);
    mChartWidget->setStyleSheet("QWidget { background: rgba(10,30,60,0.72); border-radius: 8px; }");
    mChartWidget->installEventFilter(this);
    root->addWidget(mChartWidget);

    // Per-camera table
    auto* camTitle = new QLabel(QString::fromUtf8("各路摄像头"), this);
    camTitle->setStyleSheet("color: rgba(180,210,255,0.85); font-size: 12px; margin-top: 4px;");
    root->addWidget(camTitle);

    mCamTable = new QWidget(this);
    mCamTable->setStyleSheet("QWidget { background: rgba(10,30,60,0.72); border-radius: 8px; }");
    mCamTableLayout = new QVBoxLayout(mCamTable);
    mCamTableLayout->setContentsMargins(12, 8, 12, 8);
    mCamTableLayout->setSpacing(4);
    root->addWidget(mCamTable);

    root->addStretch();
}

void StatsPanel::setRunning(bool running)
{
    mRunning = running;
}

void StatsPanel::updateStats(int totalDetections, double avgFps, int activeCams,
                              qint64 uptimeMs, const QVector<int>& perCamDets,
                              const QVector<double>& perCamFps)
{
    mDetLabel->setText(QString::number(totalDetections));
    mCamLabel->setText(QString("%1 / 8").arg(activeCams));

    mHistory.push_back(totalDetections);
    if (mHistory.size() > 60) mHistory.pop_front();

    rebuildCamRows(perCamDets.size());
    for (int i = 0; i < perCamDets.size(); ++i) {
        mCamRows[i]->setText(QString::fromUtf8("摄像头 %1  目标:%2  FPS:%3")
            .arg(i + 1)
            .arg(perCamDets[i])
            .arg(i < perCamFps.size() ? QString("%1").arg(perCamFps[i], 0, 'f', 1) : QString("--")));
    }

    mChartWidget->repaint();
}

void StatsPanel::rebuildCamRows(int count)
{
    if (count == mCamRows.size()) return;
    while (QLayoutItem* item = mCamTableLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    mCamRows.clear();
    for (int i = 0; i < count; ++i) {
        auto* row = new QLabel(mCamTable);
        row->setStyleSheet("color: #c0d8f0; font-size: 11px;");
        mCamTableLayout->addWidget(row);
        mCamRows.append(row);
    }
}

void StatsPanel::paintEvent(QPaintEvent*)
{
    // Widget itself is transparent; child cards handle their own background
}

bool StatsPanel::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == mChartWidget && ev->type() == QEvent::Paint) {
        paintChart();
        return true;
    }
    return QWidget::eventFilter(obj, ev);
}

void StatsPanel::paintChart()
{
    QPainter p(mChartWidget);
    p.setRenderHint(QPainter::Antialiasing);
    QRect r = mChartWidget->rect().adjusted(12, 10, -12, -10);

    // Grid lines
    p.setPen(QPen(QColor(80, 130, 200, 60), 1));
    for (int i = 0; i <= 4; ++i) {
        int y = r.top() + r.height() * i / 4;
        p.drawLine(r.left(), y, r.right(), y);
    }

    if (mHistory.empty()) return;

    int maxVal = 1;
    for (int v : mHistory) maxVal = std::max(maxVal, v);

    QVector<QPointF> pts;
    int n = static_cast<int>(mHistory.size());
    for (int i = 0; i < n; ++i) {
        double x = r.left() + static_cast<double>(i) / std::max(n - 1, 1) * r.width();
        double y = r.bottom() - static_cast<double>(mHistory[i]) / maxVal * r.height();
        pts.append({x, y});
    }

    // Fill
    QPolygonF poly(pts);
    poly.append({pts.last().x(),  static_cast<double>(r.bottom())});
    poly.append({pts.first().x(), static_cast<double>(r.bottom())});
    QLinearGradient grad(0, r.top(), 0, r.bottom());
    grad.setColorAt(0, QColor(0, 160, 255, 120));
    grad.setColorAt(1, QColor(0,  80, 180,  20));
    p.setBrush(grad);
    p.setPen(Qt::NoPen);
    p.drawPolygon(poly);

    // Line
    p.setPen(QPen(QColor(0, 200, 255), 2));
    p.setBrush(Qt::NoBrush);
    for (int i = 1; i < pts.size(); ++i)
        p.drawLine(pts[i-1], pts[i]);

    // Y max label
    p.setPen(QColor(160, 210, 255, 180));
    QFont f; f.setPointSize(9); p.setFont(f);
    p.drawText(r.left(), r.top() + 10, QString::number(maxVal));
}
