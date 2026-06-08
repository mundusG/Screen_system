#include "BottomControlBar.h"
#include "Theme.h"
#include <QPainter>
#include <QMouseEvent>
#include <QDateTime>

BottomControlBar::BottomControlBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(Theme::BottomBarHeight);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);

    mClockTimer = new QTimer(this);
    connect(mClockTimer, &QTimer::timeout, this, [this]() { update(); });
    mClockTimer->start(1000);
}

void BottomControlBar::setGridMode(int mode)
{
    if (mGridMode != mode && mode >= 0 && mode <= 3) {
        mGridMode = mode;
        update();
    }
}

void BottomControlBar::rebuildButtons()
{
    mButtons.clear();

    int y = 6;
    int bh = height() - 12;
    int x = 16;
    int gap = 4;

    const char* gridLabels[] = {"1x1", "2x2", "2x4", "3x3"};
    for (int i = 0; i < 4; ++i) {
        int bw = 44;
        mButtons.append({QString(gridLabels[i]), QRect(x, y, bw, bh), i});
        x += bw + gap;
    }

    // Separator gap
    x += 16;

    // Snapshot
    mButtons.append({QString::fromUtf8("Snapshot"), QRect(x, y, 72, bh), 10});
    x += 76;

    // Fullscreen
    mButtons.append({QString::fromUtf8("Fullscreen"), QRect(x, y, 80, bh), 11});
    x += 84;

    // Settings
    mButtons.append({QString::fromUtf8("Settings"), QRect(x, y, 68, bh), 12});
}

int BottomControlBar::hitTest(const QPoint& pos) const
{
    for (const auto& btn : mButtons) {
        if (btn.rect.contains(pos))
            return btn.action;
    }
    return -99;
}

void BottomControlBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    rebuildButtons();

    // Background
    p.fillRect(rect(), Theme::bottomBarBg());

    // Top separator line
    p.setPen(Theme::separator());
    p.drawLine(0, 0, width(), 0);

    // Buttons
    QFont font;
    font.setPointSize(9);
    font.setBold(true);
    p.setFont(font);

    for (const auto& btn : mButtons) {
        bool isGridBtn = (btn.action >= 0 && btn.action <= 3);
        bool active = isGridBtn && (btn.action == mGridMode);
        bool hovered = (btn.action == mHoveredAction);

        QColor bg, fg;
        if (active) {
            bg = Theme::buttonActive();
            fg = Qt::white;
        } else if (hovered) {
            bg = Theme::buttonHover();
            fg = Theme::textPrimary();
        } else {
            bg = Theme::buttonBg();
            fg = Theme::textSecondary();
        }

        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(btn.rect, 4, 4);

        // Subtle border on grid buttons
        if (isGridBtn && !active) {
            p.setPen(QPen(Theme::borderSubtle(), 1));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(btn.rect, 4, 4);
        }

        p.setPen(fg);
        p.drawText(btn.rect, Qt::AlignCenter, btn.label);
    }

    // Separator between grid buttons and action buttons
    int sepX = mButtons[3].rect.right() + 12;
    p.setPen(QPen(Theme::separator(), 1));
    p.drawLine(sepX, 10, sepX, height() - 10);

    // Clock on the right
    {
        QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd  HH:mm:ss");
        QFont clockFont("Monospace", 9);
        p.setFont(clockFont);
        QFontMetrics fm(clockFont);
        int tw = fm.horizontalAdvance(timeStr);
        p.setPen(Theme::textMuted());
        p.drawText(width() - tw - 16, 0, tw, height(), Qt::AlignVCenter | Qt::AlignRight, timeStr);
    }
}

void BottomControlBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;

    int action = hitTest(event->pos());
    if (action >= 0 && action <= 3) {
        mGridMode = action;
        update();
        emit gridModeChanged(action);
    } else if (action == 10) {
        emit snapshotRequested();
    } else if (action == 11) {
        emit fullscreenToggled();
    } else if (action == 12) {
        emit settingsRequested();
    }
}

void BottomControlBar::mouseMoveEvent(QMouseEvent* event)
{
    int action = hitTest(event->pos());
    if (action != mHoveredAction) {
        mHoveredAction = action;
        update();
    }
}

void BottomControlBar::leaveEvent(QEvent*)
{
    mHoveredAction = -99;
    update();
}
