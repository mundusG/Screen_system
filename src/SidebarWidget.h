#ifndef SIDEBARWIDGET_H
#define SIDEBARWIDGET_H

#include <QWidget>

class SidebarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SidebarWidget(QWidget* parent = nullptr);

signals:
    void settingsRequested();
    void startStopToggled();

public slots:
    void setRunning(bool running);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct NavItem {
        QString label;
        int iconType; // 0=grid, 1=eye, 2=play, 3=gear
        bool active;
        bool enabled;
    };

    int hitTest(const QPoint& pos) const;
    QRect itemRect(int index) const;
    void drawIcon(QPainter& p, int iconType, const QRect& r, const QColor& color);

    QVector<NavItem> mItems;
    int mHoveredIndex = -1;
    bool mRunning = false;
};

#endif // SIDEBARWIDGET_H
