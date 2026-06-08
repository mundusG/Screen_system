#include "VideoWidget.h"
#include "Theme.h"
#include <QPainter>
#include <QPen>
#include <QFont>
#include <QMouseEvent>
#include <QDateTime>
#include <algorithm>

VideoWidget::VideoWidget(int cameraId, const QString& title, QWidget* parent)
    : QWidget(parent)
    , mCameraId(cameraId)
    , mTitle(title)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(160, 90);
    setCursor(Qt::PointingHandCursor);

    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);

    mClassColors[0] = QColor(0,   255, 0);
    mClassColors[1] = QColor(255, 0,   0);

    mTickTimer = new QTimer(this);
    connect(mTickTimer, &QTimer::timeout, this, [this]() {
        if (mLivePulseDir) {
            mLivePulse -= 0.05f;
            if (mLivePulse <= 0.6f) mLivePulseDir = false;
        } else {
            mLivePulse += 0.05f;
            if (mLivePulse >= 1.0f) mLivePulseDir = true;
        }
        update();
    });
    mTickTimer->start(80);
}

VideoWidget::~VideoWidget() = default;

int VideoWidget::heightForWidth(int w) const { return w * 9 / 16; }
bool VideoWidget::hasHeightForWidth() const  { return true; }

void VideoWidget::setSelected(bool selected)
{
    if (mSelected != selected) {
        mSelected = selected;
        update();
    }
}

void VideoWidget::setConfidenceThreshold(float threshold)
{
    mConfThreshold = threshold;
}

void VideoWidget::updateDisplayFrame(const FrameData& frame)
{
    if (frame.image.empty()) return;
    {
        QMutexLocker locker(&mFrameMutex);
        mCurrentFrame = frame.image.clone();
    }
    mLastFrameTime = QDateTime::currentMSecsSinceEpoch();
    mHasSignal = true;
    update();
}

void VideoWidget::updateDetectionOverlay(const DisplayResult& result)
{
    {
        QMutexLocker locker(&mDetectionMutex);
        mDetections = result.detections;
    }
    mLastDetectionTime = QDateTime::currentMSecsSinceEpoch();
    mHasDetections = !result.detections.isEmpty();

    int activeCount = 0;
    for (const auto& det : result.detections)
        if (!det.filtered) activeCount++;
    mDetectionCount = activeCount;

    update();
}

void VideoWidget::updateFps(double fps)
{
    mCurrentFps = fps;
}

void VideoWidget::updateInferenceTime(float ms)
{
    mCurrentInferenceTimeMs = ms;
}

void VideoWidget::showNoSignal()
{
    mHasSignal = false;
    update();
}

void VideoWidget::reset()
{
    mHasSignal = false;
    mHasDetections = false;
    mCurrentFps = 0.0;
    mCurrentInferenceTimeMs = 0.0f;
    mDetectionCount = 0;
    {
        QMutexLocker locker(&mFrameMutex);
        mCurrentFrame.release();
    }
    {
        QMutexLocker locker(&mDetectionMutex);
        mDetections.clear();
    }
    update();
}

void VideoWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const int w = width();
    const int h = height();

    int drawW = w;
    int drawH = w * 9 / 16;
    if (drawH > h) { drawH = h; drawW = h * 16 / 9; }
    const int ox = (w - drawW) / 2;
    const int oy = (h - drawH) / 2;
    const QRect videoRect(ox, oy, drawW, drawH);

    // Dark background
    painter.fillRect(rect(), Theme::tileBg());

    {
        QMutexLocker locker(&mFrameMutex);
        if (!mCurrentFrame.empty()) {
            QImage img = matToQImage(mCurrentFrame);
            if (!img.isNull()) {
                QImage scaled = img.scaled(drawW, drawH, Qt::IgnoreAspectRatio,
                                           Qt::SmoothTransformation);
                painter.setOpacity(0.92);
                painter.drawImage(ox, oy, scaled);
                painter.setOpacity(1.0);

                const float scaleX = static_cast<float>(drawW) / mCurrentFrame.cols;
                const float scaleY = static_cast<float>(drawH) / mCurrentFrame.rows;

                QMutexLocker detLocker(&mDetectionMutex);
                for (const auto& det : mDetections) {
                    if (det.filtered) continue;

                    float bx = det.bbox.left()  * scaleX + ox;
                    float by = det.bbox.top()   * scaleY + oy;
                    float bw = det.bbox.width   * scaleX;
                    float bh = det.bbox.height  * scaleY;

                    QColor color;
                    if (mClassColors.contains(det.classId)) {
                        color = mClassColors[det.classId];
                    } else {
                        int hue = (det.classId * 67 + 180) % 360;
                        color = QColor::fromHsv(hue, 255, 255);
                        mClassColors[det.classId] = color;
                    }

                    // Detection box with corner accents
                    painter.setPen(QPen(color, 2.0));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawRect(QRectF(bx, by, bw, bh));

                    // Corner accents (thicker short lines at corners)
                    float cornerLen = std::min(bw, bh) * 0.15f;
                    cornerLen = std::max(cornerLen, 6.0f);
                    QPen cornerPen(color, 3.0);
                    painter.setPen(cornerPen);
                    // Top-left
                    painter.drawLine(QPointF(bx, by), QPointF(bx + cornerLen, by));
                    painter.drawLine(QPointF(bx, by), QPointF(bx, by + cornerLen));
                    // Top-right
                    painter.drawLine(QPointF(bx + bw, by), QPointF(bx + bw - cornerLen, by));
                    painter.drawLine(QPointF(bx + bw, by), QPointF(bx + bw, by + cornerLen));
                    // Bottom-left
                    painter.drawLine(QPointF(bx, by + bh), QPointF(bx + cornerLen, by + bh));
                    painter.drawLine(QPointF(bx, by + bh), QPointF(bx, by + bh - cornerLen));
                    // Bottom-right
                    painter.drawLine(QPointF(bx + bw, by + bh), QPointF(bx + bw - cornerLen, by + bh));
                    painter.drawLine(QPointF(bx + bw, by + bh), QPointF(bx + bw, by + bh - cornerLen));

                    // Label badge
                    QString label = QString("%1 %2%")
                        .arg(det.classId)
                        .arg(det.confidence * 100, 0, 'f', 0);
                    QFont font("Monospace", 9, QFont::Bold);
                    painter.setFont(font);
                    QFontMetrics fm(font);
                    int textW = fm.horizontalAdvance(label) + 10;
                    int textH = fm.height() + 4;

                    QColor badgeBg = color;
                    badgeBg.setAlpha(200);
                    painter.setBrush(badgeBg);
                    painter.setPen(Qt::NoPen);
                    painter.drawRoundedRect(QRectF(bx, by - textH - 1, textW, textH), 2, 2);
                    painter.setPen(Qt::white);
                    painter.drawText(QRectF(bx + 5, by - textH - 1, textW - 5, textH),
                                     Qt::AlignVCenter | Qt::AlignLeft, label);

                    if (det.trackId >= 0) {
                        QFont smallFont("Monospace", 7);
                        painter.setFont(smallFont);
                        painter.setPen(QColor(200, 220, 255, 200));
                        painter.drawText(QPointF(bx + 4, by + bh - 4),
                                         QString("ID:%1").arg(det.trackId));
                    }
                }
            }
        } else {
            // No signal state
            painter.setPen(Theme::textMuted());
            QFont font = painter.font();
            font.setPointSize(13);
            painter.setFont(font);
            painter.drawText(videoRect, Qt::AlignCenter, QString::fromUtf8("NO SIGNAL"));

            // Subtle scan line effect
            painter.setPen(QPen(QColor(30, 50, 80, 30), 1));
            for (int y = videoRect.top(); y < videoRect.bottom(); y += 4) {
                painter.drawLine(videoRect.left(), y, videoRect.right(), y);
            }
        }
    }

    drawTitleBadge(painter);
    drawLiveIndicator(painter, videoRect);
    drawTimestamp(painter, videoRect);
    drawStatusOverlay(painter, videoRect);
    drawBorder(painter, videoRect);
}

void VideoWidget::drawLiveIndicator(QPainter& p, const QRect& videoRect)
{
    if (!mHasSignal) return;

    int rx = videoRect.right() - 60;
    int ry = videoRect.top() + 6;

    // "LIVE" badge
    QFont font("Monospace", 8, QFont::Bold);
    p.setFont(font);

    QColor dotColor = Theme::liveGreen();
    dotColor.setAlphaF(mLivePulse);

    // Background pill
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 160));
    p.drawRoundedRect(rx, ry, 54, 18, 9, 9);

    // Green dot
    p.setBrush(dotColor);
    p.drawEllipse(QPointF(rx + 10, ry + 9), 4, 4);

    // "LIVE" text
    p.setPen(Theme::liveGreen());
    p.drawText(QRectF(rx + 18, ry, 34, 18), Qt::AlignVCenter, "LIVE");
}

void VideoWidget::drawTimestamp(QPainter& p, const QRect& videoRect)
{
    QString timeStr = QDateTime::currentDateTime().toString("HH:mm:ss");
    QFont font("Monospace", 8);
    p.setFont(font);
    QFontMetrics fm(font);

    int tw = fm.horizontalAdvance(timeStr) + 12;
    int th = fm.height() + 6;
    int tx = videoRect.left() + 4;
    int ty = videoRect.bottom() - th - 4;

    p.setPen(Qt::NoPen);
    p.setBrush(Theme::tileOverlayBg());
    p.drawRoundedRect(tx, ty, tw, th, 3, 3);

    p.setPen(Theme::textSecondary());
    p.drawText(QRectF(tx + 6, ty, tw - 6, th), Qt::AlignVCenter, timeStr);
}

void VideoWidget::drawStatusOverlay(QPainter& p, const QRect& videoRect)
{
    if (mCurrentFps <= 0.0 && mCurrentInferenceTimeMs <= 0.0f) return;

    QString info;
    if (mCurrentFps > 0.0)
        info += QString("%1 FPS").arg(mCurrentFps, 0, 'f', 1);
    if (mCurrentInferenceTimeMs > 0.0f) {
        if (!info.isEmpty()) info += " | ";
        info += QString("%1ms").arg(static_cast<int>(mCurrentInferenceTimeMs));
    }
    if (mDetectionCount > 0) {
        if (!info.isEmpty()) info += " | ";
        info += QString::fromUtf8("\xe7\x9b\xae\xe6\xa0\x87:%1").arg(mDetectionCount);
    }

    QFont font("Monospace", 7);
    p.setFont(font);
    QFontMetrics fm(font);

    int tw = fm.horizontalAdvance(info) + 12;
    int th = fm.height() + 6;
    int tx = videoRect.right() - tw - 4;
    int ty = videoRect.bottom() - th - 4;

    p.setPen(Qt::NoPen);
    p.setBrush(Theme::tileOverlayBg());
    p.drawRoundedRect(tx, ty, tw, th, 3, 3);

    p.setPen(Theme::accent());
    p.drawText(QRectF(tx + 6, ty, tw - 6, th), Qt::AlignVCenter, info);
}

void VideoWidget::drawTitleBadge(QPainter& p)
{
    QFont titleFont;
    titleFont.setPointSize(10);
    titleFont.setBold(true);
    p.setFont(titleFont);
    QFontMetrics fm(titleFont);

    int titleW = fm.horizontalAdvance(mTitle) + 32;
    int titleH = fm.height() + 8;

    // Camera icon (small rect with lens)
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 180));
    p.drawRoundedRect(2, 2, titleW, titleH, 3, 3);

    // Accent left edge
    p.setBrush(Theme::accent());
    p.drawRect(2, 2, 3, titleH);

    // Camera icon drawn as small geometric shape
    int iconX = 10;
    int iconY = titleH / 2 - 2;
    p.setPen(QPen(Theme::accent(), 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawRect(iconX, iconY, 8, 6);
    p.drawEllipse(QPointF(iconX + 4, iconY + 3), 2, 2);

    // Title text
    p.setPen(Theme::textPrimary());
    p.drawText(QRectF(24, 2, titleW - 24, titleH),
               Qt::AlignVCenter, mTitle);
}

void VideoWidget::drawBorder(QPainter& p, const QRect& videoRect)
{
    if (mSelected) {
        // Glow effect: outer soft border
        QPen glowPen(Theme::accentGlow(), 4);
        p.setPen(glowPen);
        p.setBrush(Qt::NoBrush);
        p.drawRect(videoRect.adjusted(-2, -2, 2, 2));

        // Inner bright border
        QPen borderPen(Theme::accentBorderBright(), 1.5);
        p.setPen(borderPen);
        p.drawRect(videoRect.adjusted(0, 0, -1, -1));
    } else {
        QPen borderPen(Theme::tileBorderNormal(), 1);
        p.setPen(borderPen);
        p.setBrush(Qt::NoBrush);
        p.drawRect(videoRect.adjusted(0, 0, -1, -1));
    }
}

void VideoWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked(mCameraId);
    QWidget::mousePressEvent(event);
}

QImage VideoWidget::matToQImage(const cv::Mat& mat)
{
    if (mat.empty()) return QImage();
    switch (mat.type()) {
    case CV_8UC3: {
        QImage img(mat.data, mat.cols, mat.rows,
                   static_cast<int>(mat.step), QImage::Format_RGB888);
        return img.rgbSwapped();
    }
    case CV_8UC1: {
        QImage img(mat.data, mat.cols, mat.rows,
                   static_cast<int>(mat.step), QImage::Format_Grayscale8);
        return img;
    }
    default: {
        cv::Mat converted;
        if (mat.channels() == 4)
            cv::cvtColor(mat, converted, cv::COLOR_BGRA2BGR);
        else if (mat.channels() == 1)
            cv::cvtColor(mat, converted, cv::COLOR_GRAY2BGR);
        else
            converted = mat.clone();
        QImage img(converted.data, converted.cols, converted.rows,
                   static_cast<int>(converted.step), QImage::Format_RGB888);
        return img.rgbSwapped();
    }
    }
}
