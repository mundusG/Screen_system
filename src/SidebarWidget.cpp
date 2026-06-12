#include "SidebarWidget.h"
#include "Theme.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <cmath>

SidebarWidget::SidebarWidget(QWidget* parent)
    : QWidget(parent)
{
    setFixedWidth(Theme::SidebarWidth);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);

    mItems = {
        {QString::fromUtf8("Live View"), 1, true,  true},
        {QString::fromUtf8("Settings"),  3, false, true},
    };
}

void SidebarWidget::setRunning(bool running)
{
    mRunning = running;
    update();
}

QRect SidebarWidget::itemRect(int index) const
{
    int y = 60 + index * 56;
    return QRect(0, y, width(), 52);
}

int SidebarWidget::hitTest(const QPoint& pos) const
{
    for (int i = 0; i < mItems.size(); ++i) {
        if (itemRect(i).contains(pos))
            return i;
    }
    // Start/stop button at bottom
    int btnY = height() - 64;
    if (QRect(0, btnY, width(), 52).contains(pos))
        return -2; // special: start/stop
    return -1;
}

void SidebarWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background
    p.fillRect(rect(), Theme::sidebarBg());

    // Right edge separator
    p.setPen(Theme::separator());
    p.drawLine(width() - 1, 0, width() - 1, height());

    // Logo/brand area at top
    {
        QFont f;
        f.setPointSize(14);
        f.setBold(true);
        p.setFont(f);
        p.setPen(Theme::accent());
        p.drawText(QRect(0, 12, width(), 36), Qt::AlignCenter, "SI");
    }

    // Navigation items
    for (int i = 0; i < mItems.size(); ++i) {
        const auto& item = mItems[i];
        QRect r = itemRect(i);

        // Hover/active background
        if (item.active) {
            p.fillRect(r, Theme::sidebarActive());
            // Active indicator bar on left
            p.fillRect(QRect(0, r.top() + 8, 3, r.height() - 16), Theme::accent());
        } else if (i == mHoveredIndex && item.enabled) {
            p.fillRect(r, Theme::sidebarHover());
        }

        // Icon
        QColor iconColor;
        if (item.active) iconColor = Theme::accent();
        else if (!item.enabled) iconColor = Theme::textMuted();
        else iconColor = Theme::textSecondary();

        QRect iconRect(r.left() + (width() - 22) / 2, r.top() + 8, 22, 22);
        drawIcon(p, item.iconType, iconRect, iconColor);

        // Label
        QFont f;
        f.setPointSize(7);
        p.setFont(f);
        p.setPen(iconColor);
        p.drawText(QRect(0, r.top() + 32, width(), 16), Qt::AlignHCenter, item.label);
    }

    // Start/Stop button at bottom
    {
        int btnY = height() - 64;
        QRect btnRect(8, btnY, width() - 16, 40);

        if (mHoveredIndex == -2) {
            p.setBrush(Theme::buttonHover());
        } else {
            p.setBrush(mRunning ? QColor(180, 40, 40) : QColor(0, 160, 80));
        }
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(btnRect, 6, 6);

        // Power icon
        QColor ic = Qt::white;
        int cx = btnRect.center().x();
        int cy = btnRect.center().y();
        p.setPen(QPen(ic, 2.2, Qt::SolidLine, Qt::RoundCap));
        p.setBrush(Qt::NoBrush);
        // Arc (open circle)
        p.drawArc(cx - 8, cy - 8, 16, 16, 60 * 16, 240 * 16);
        // Vertical line at top
        p.drawLine(cx, cy - 10, cx, cy - 2);
    }
}

void SidebarWidget::drawIcon(QPainter& p, int iconType, const QRect& r, const QColor& color)
{
    p.setPen(QPen(color, 1.5));
    p.setBrush(Qt::NoBrush);

    int cx = r.center().x();
    int cy = r.center().y();

    switch (iconType) {
    case 0: // Dashboard - grid icon
        p.drawRect(cx - 8, cy - 8, 7, 7);
        p.drawRect(cx + 1, cy - 8, 7, 7);
        p.drawRect(cx - 8, cy + 1, 7, 7);
        p.drawRect(cx + 1, cy + 1, 7, 7);
        break;
    case 1: // Live View - eye icon
        {
            QPainterPath eye;
            eye.moveTo(cx - 10, cy);
            eye.cubicTo(cx - 5, cy - 7, cx + 5, cy - 7, cx + 10, cy);
            eye.cubicTo(cx + 5, cy + 7, cx - 5, cy + 7, cx - 10, cy);
            p.drawPath(eye);
            p.drawEllipse(QPointF(cx, cy), 3, 3);
        }
        break;
    case 2: // Playback - play/rewind icon
        {
            QPolygonF tri;
            tri << QPointF(cx - 4, cy - 7) << QPointF(cx - 4, cy + 7) << QPointF(cx + 7, cy);
            p.drawPolygon(tri);
            p.drawLine(cx - 7, cy - 7, cx - 7, cy + 7);
        }
        break;
    case 3: // Settings - gear icon
        {
            p.drawEllipse(QPointF(cx, cy), 4, 4);
            for (int a = 0; a < 360; a += 45) {
                double rad = a * 3.14159265 / 180.0;
                int x1 = cx + static_cast<int>(6 * cos(rad));
                int y1 = cy + static_cast<int>(6 * sin(rad));
                int x2 = cx + static_cast<int>(9 * cos(rad));
                int y2 = cy + static_cast<int>(9 * sin(rad));
                p.drawLine(x1, y1, x2, y2);
            }
        }
        break;
    }
}

void SidebarWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;

    int idx = hitTest(event->pos());
    if (idx == -2) {
        emit startStopToggled();
    } else if (idx >= 0 && idx < mItems.size() && mItems[idx].enabled) {
        if (mItems[idx].iconType == 3) {
            emit settingsRequested();
        }
    }
}

void SidebarWidget::mouseMoveEvent(QMouseEvent* event)
{
    int idx = hitTest(event->pos());
    if (idx != mHoveredIndex) {
        mHoveredIndex = idx;
        update();
    }
}

void SidebarWidget::leaveEvent(QEvent*)
{
    mHoveredIndex = -1;
    update();
}
