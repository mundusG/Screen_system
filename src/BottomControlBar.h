#ifndef BOTTOMCONTROLBAR_H
#define BOTTOMCONTROLBAR_H

#include <QWidget>
#include <QTimer>

class BottomControlBar : public QWidget
{
    Q_OBJECT

public:
    explicit BottomControlBar(QWidget* parent = nullptr);

    int currentGridMode() const { return mGridMode; }

signals:
    void gridModeChanged(int mode); // 0=1x1, 1=2x2, 2=2x4, 3=3x3
    void snapshotRequested();
    void fullscreenToggled();
    void settingsRequested();

public slots:
    void setGridMode(int mode);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct Button {
        QString label;
        QRect rect;
        int action; // -1=grid modes start, 0-3=grid, 10=snapshot, 11=fullscreen, 12=settings
    };

    void rebuildButtons();
    int hitTest(const QPoint& pos) const;

    int mGridMode = 2; // default 2x4
    int mHoveredAction = -99;
    QVector<Button> mButtons;
    QTimer* mClockTimer;
};

#endif // BOTTOMCONTROLBAR_H
