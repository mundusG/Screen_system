#include "AlertPanel.h"
#include "Theme.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>

AlertPanel::AlertPanel(QWidget* parent)
    : QWidget(parent)
{
    setFixedWidth(Theme::RightPanelWidth);
    setAttribute(Qt::WA_TranslucentBackground);

    mDevices.resize(8);
    mCameraRunning.fill(false, 8);
    for (int i = 0; i < 8; ++i) {
        mDevices[i].name = QString::fromUtf8("Camera %1").arg(i + 1);
        mDevices[i].online = false;
    }
}

void AlertPanel::updateDeviceStatus(int cameraId, const QString& name, bool online, double fps, int dets)
{
    if (cameraId < 0 || cameraId >= mDevices.size()) return;
    mDevices[cameraId].name = name;
    mDevices[cameraId].online = online;
    mDevices[cameraId].fps = fps;
    mDevices[cameraId].detections = dets;
    update();
}

void AlertPanel::addAlert(int cameraId, const QString& cameraName, int classId, float confidence, const QImage& thumbnail, bool periodic)
{
    AlertEntry entry;
    entry.cameraId = cameraId;
    entry.cameraName = cameraName;
    entry.classId = classId;
    entry.confidence = confidence;
    entry.timestamp = QDateTime::currentDateTime();
    entry.thumbnail = thumbnail;
    entry.periodic = periodic;

    mAlerts.push_front(entry);
    if (static_cast<int>(mAlerts.size()) > MaxAlerts)
        mAlerts.pop_back();

    update();
}

void AlertPanel::clearAlerts()
{
    mAlerts.clear();
    update();
}

void AlertPanel::setCameraRunning(int cameraId, bool running)
{
    if (cameraId < 0 || cameraId >= mCameraRunning.size()) return;
    mCameraRunning[cameraId] = running;
    update();
}

void AlertPanel::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background
    p.fillRect(rect(), Theme::sidebarBg());

    // Left edge separator
    p.setPen(Theme::separator());
    p.drawLine(0, 0, 0, height());

    int margin = 12;
    int contentW = width() - margin * 2;

    // --- Device List Section ---
    int deviceSectionH = 44 + mDevices.size() * 32 + 16;
    QRect deviceArea(margin, 8, contentW, deviceSectionH);
    drawDeviceList(p, deviceArea);

    // Separator
    int sepY = deviceArea.bottom() + 8;
    p.setPen(Theme::separator());
    p.drawLine(margin, sepY, margin + contentW, sepY);

    // --- Alert Section ---
    QRect alertArea(margin, sepY + 8, contentW, height() - sepY - 16);
    drawAlertList(p, alertArea);
}

void AlertPanel::drawDeviceList(QPainter& p, const QRect& area)
{
    int x = area.left();
    int y = area.top();

    // Section title
    QFont titleFont;
    titleFont.setPointSize(11);
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.setPen(Theme::textPrimary());
    p.drawText(x, y, area.width(), 24, Qt::AlignVCenter, QString::fromUtf8("Device List"));

    // Online count badge
    int onlineCount = 0;
    for (const auto& d : mDevices)
        if (d.online) onlineCount++;

    QFont badgeFont;
    badgeFont.setPointSize(8);
    p.setFont(badgeFont);
    QString badge = QString("%1/%2").arg(onlineCount).arg(mDevices.size());
    QFontMetrics bfm(badgeFont);
    int bw = bfm.horizontalAdvance(badge) + 12;
    QRect badgeRect(x + area.width() - bw, y + 2, bw, 20);
    p.setPen(Qt::NoPen);
    p.setBrush(onlineCount > 0 ? QColor(0, 160, 80, 60) : QColor(100, 100, 100, 60));
    p.drawRoundedRect(badgeRect, 10, 10);
    p.setPen(onlineCount > 0 ? Theme::onlineGreen() : Theme::offlineGray());
    p.drawText(badgeRect, Qt::AlignCenter, badge);

    y += 32;

    // Device rows
    QFont rowFont;
    rowFont.setPointSize(9);

    for (int i = 0; i < mDevices.size(); ++i) {
        const auto& dev = mDevices[i];
        int rowY = y + i * 32;

        // Hover-style background
        p.setPen(Qt::NoPen);
        p.setBrush(Theme::cardBg());
        p.drawRoundedRect(x, rowY, area.width(), 28, 4, 4);

        // Status dot
        QColor dotColor = dev.online ? Theme::onlineGreen() : Theme::offlineGray();
        p.setBrush(dotColor);
        p.drawEllipse(QPointF(x + 12, rowY + 14), 4, 4);

        // Name
        p.setFont(rowFont);
        p.setPen(dev.online ? Theme::textPrimary() : Theme::textMuted());
        p.drawText(x + 24, rowY, area.width() - 104, 28, Qt::AlignVCenter, dev.name);

        // Status text
        QFont smallFont;
        smallFont.setPointSize(7);
        p.setFont(smallFont);
        if (dev.online) {
            QString info = QString("%1 FPS").arg(dev.fps, 0, 'f', 0);
            p.setPen(Theme::textMuted());
            p.drawText(x + area.width() - 80, rowY, 46, 28, Qt::AlignVCenter | Qt::AlignRight, info);
        } else {
            p.setPen(Theme::offlineGray());
            p.drawText(x + area.width() - 80, rowY, 46, 28, Qt::AlignVCenter | Qt::AlignRight, "Offline");
        }

        // Power toggle button
        QRect toggleRect = toggleHitRect(i);
        bool running = mCameraRunning.value(i, false);
        drawPowerIcon(p, toggleRect, running);
    }
}

void AlertPanel::drawAlertList(QPainter& p, const QRect& area)
{
    int x = area.left();
    int y = area.top();

    // Section title
    QFont titleFont;
    titleFont.setPointSize(11);
    titleFont.setBold(true);
    p.setFont(titleFont);
    p.setPen(Theme::textPrimary());
    p.drawText(x, y, area.width(), 24, Qt::AlignVCenter, QString::fromUtf8("Alerts"));

    QRect libraryRect = imageLibraryButtonRect(area);
    p.setPen(QPen(Theme::borderMedium(), 1));
    p.setBrush(Theme::buttonBg());
    p.drawRoundedRect(libraryRect, 4, 4);
    QFont libraryFont;
    libraryFont.setPointSize(8);
    libraryFont.setBold(true);
    p.setFont(libraryFont);
    p.setPen(Theme::textSecondary());
    p.drawText(libraryRect, Qt::AlignCenter, QString::fromUtf8("查看更多"));

    // Alert count
    if (!mAlerts.empty()) {
        QFont badgeFont;
        badgeFont.setPointSize(8);
        badgeFont.setBold(true);
        p.setFont(badgeFont);
        QString count = QString::number(mAlerts.size());
        QFontMetrics bfm(badgeFont);
        int bw = bfm.horizontalAdvance(count) + 12;
        QRect badgeRect(libraryRect.left() - bw - 6, y + 2, bw, 20);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 82, 82, 60));
        p.drawRoundedRect(badgeRect, 10, 10);
        p.setPen(Theme::alertRed());
        p.drawText(badgeRect, Qt::AlignCenter, count);
    }

    y += 32;

    if (mAlerts.empty()) {
        QFont f;
        f.setPointSize(9);
        p.setFont(f);
        p.setPen(Theme::textMuted());
        p.drawText(x, y, area.width(), 40, Qt::AlignCenter, QString::fromUtf8("No alerts"));
        return;
    }

    // Alert entries
    int entryH = 72;
    int maxVisible = (area.bottom() - y) / entryH;

    for (int i = 0; i < std::min(static_cast<int>(mAlerts.size()), maxVisible); ++i) {
        const auto& alert = mAlerts[i];
        int ey = y + i * entryH;

        if (ey + entryH > area.bottom()) break;

        // Card background
        p.setPen(Qt::NoPen);
        p.setBrush(Theme::cardBg());
        p.drawRoundedRect(x, ey, area.width(), entryH - 4, 4, 4);

        // Severity indicator (left bar)
        QColor severity;
        if (alert.periodic) {
            severity = Theme::onlineGreen();
        } else {
            severity = (alert.confidence > 0.8f) ? Theme::alertRed() : Theme::warningOrange();
        }
        p.setBrush(severity);
        p.drawRect(x, ey + 4, 3, entryH - 12);

        // Thumbnail
        int textX = x + 10;
        if (!alert.thumbnail.isNull()) {
            int thumbH = entryH - 8;
            int thumbW = alert.thumbnail.width() * thumbH / alert.thumbnail.height();
            QRect thumbRect(x + 8, ey + 4, thumbW, thumbH - 4);
            p.drawImage(thumbRect, alert.thumbnail);
            textX = x + 8 + thumbW + 6;
        }

        // Camera name
        QFont nameFont;
        nameFont.setPointSize(9);
        nameFont.setBold(true);
        p.setFont(nameFont);
        p.setPen(Theme::textPrimary());
        p.drawText(textX, ey + 4, area.width() - (textX - x) - 4, 20, Qt::AlignVCenter,
                   alert.cameraName);

        // Class + confidence
        QFont detailFont;
        detailFont.setPointSize(7);
        p.setFont(detailFont);
        QColor detailColor;
        QString label;
        if (alert.periodic) {
            detailColor = Theme::onlineGreen();
            label = QString::fromUtf8("正常");
        } else {
            detailColor = (alert.confidence > 0.8f) ? Theme::alertRed() : Theme::warningOrange();
            label = QString::fromUtf8("异常");
        }
        p.setPen(detailColor);
        QString detail = QString("%1 | %2%")
            .arg(label)
            .arg(static_cast<int>(alert.confidence * 100));
        p.drawText(textX, ey + 24, area.width() - (textX - x) - 4, 16, Qt::AlignVCenter, detail);

        // Time
        p.drawText(textX, ey + 40, area.width() - (textX - x) - 4, 16, Qt::AlignVCenter,
                   alert.timestamp.toString("HH:mm:ss"));
    }
}

void AlertPanel::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;

    // Check toggle buttons in device list
    for (int i = 0; i < mDevices.size(); ++i) {
        if (toggleHitRect(i).contains(event->pos())) {
            emit cameraToggleRequested(i);
            return;
        }
    }

    int margin = 12;
    int contentW = width() - margin * 2;
    int deviceSectionH = 44 + mDevices.size() * 32 + 16;
    QRect deviceArea(margin, 8, contentW, deviceSectionH);
    int sepY = deviceArea.bottom() + 8;
    QRect alertArea(margin, sepY + 8, contentW, height() - sepY - 16);
    if (imageLibraryButtonRect(alertArea).contains(event->pos())) {
        emit imageLibraryRequested();
        return;
    }

    // Check if click is on an alert entry
    int alertStartY = 8 + deviceSectionH + 16 + 32;
    int entryH = 72;

    int clickY = event->pos().y();
    if (clickY >= alertStartY) {
        int idx = (clickY - alertStartY) / entryH;
        if (idx >= 0 && idx < static_cast<int>(mAlerts.size())) {
            emit alertClicked(mAlerts[idx].cameraId);
        }
    }
}

QRect AlertPanel::toggleHitRect(int row) const
{
    int margin = 12;
    int y = 8 + 32 + row * 32;
    return QRect(margin + width() - margin * 2 - 24, y + 4, 20, 20);
}

QRect AlertPanel::imageLibraryButtonRect(const QRect& alertArea) const
{
    return QRect(alertArea.right() - 58, alertArea.top() + 1, 58, 22);
}

void AlertPanel::drawPowerIcon(QPainter& p, const QRect& r, bool running)
{
    QColor color = running ? Theme::onlineGreen() : Theme::offlineGray();
    int cx = r.center().x();
    int cy = r.center().y();
    int radius = 7;

    // Circle arc (open at top)
    p.setPen(QPen(color, 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawArc(cx - radius, cy - radius, radius * 2, radius * 2, 50 * 16, 260 * 16);

    // Vertical line at top
    p.drawLine(cx, cy - radius, cx, cy - 2);
}
