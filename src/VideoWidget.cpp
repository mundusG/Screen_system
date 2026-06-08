#include "VideoWidget.h"
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
    // Enforce 16:9 aspect ratio via size policy
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(160, 90);
    setCursor(Qt::PointingHandCursor);

    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);

    mClassColors[0] = QColor(0,   255, 0);
    mClassColors[1] = QColor(255, 0,   0);
}

VideoWidget::~VideoWidget() = default;

// Keep widget at 16:9
int VideoWidget::heightForWidth(int w) const { return w * 9 / 16; }
bool VideoWidget::hasHeightForWidth() const  { return true; }

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

    // Compute 16:9 rect centered in widget
    int drawW = w;
    int drawH = w * 9 / 16;
    if (drawH > h) { drawH = h; drawW = h * 16 / 9; }
    const int ox = (w - drawW) / 2;
    const int oy = (h - drawH) / 2;
    const QRect videoRect(ox, oy, drawW, drawH);

    // Dark semi-transparent background
    painter.fillRect(rect(), QColor(20, 25, 35, 200));

    {
        QMutexLocker locker(&mFrameMutex);
        if (!mCurrentFrame.empty()) {
            QImage img = matToQImage(mCurrentFrame);
            if (!img.isNull()) {
                QImage scaled = img.scaled(drawW, drawH, Qt::IgnoreAspectRatio,
                                           Qt::SmoothTransformation);
                painter.setOpacity(0.88);
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

                    painter.setPen(QPen(color, 2.5));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawRect(QRectF(bx, by, bw, bh));

                    QString label = QString("%1 %2%")
                        .arg(det.classId)
                        .arg(det.confidence * 100, 0, 'f', 0);
                    QFont font("Monospace", 10, QFont::Bold);
                    painter.setFont(font);
                    QFontMetrics fm(font);
                    int textW = fm.horizontalAdvance(label) + 8;
                    int textH = fm.height() + 4;
                    painter.setBrush(color);
                    painter.setPen(Qt::NoPen);
                    painter.drawRect(QRectF(bx, by - textH, textW, textH));
                    painter.setPen(Qt::white);
                    painter.drawText(QRectF(bx + 4, by - textH, textW - 4, textH),
                                     Qt::AlignVCenter | Qt::AlignLeft, label);

                    if (det.trackId >= 0) {
                        QFont smallFont("Monospace", 8);
                        painter.setFont(smallFont);
                        painter.setPen(Qt::white);
                        painter.drawText(QPointF(bx + 4, by + bh - 4),
                                         QString("ID:%1").arg(det.trackId));
                    }
                }
            }
        } else {
            painter.setPen(QColor(120, 130, 140));
            QFont font = painter.font();
            font.setPointSize(14);
            painter.setFont(font);
            painter.drawText(videoRect, Qt::AlignCenter, QString::fromUtf8("无视频信号"));
        }
    }

    // Camera title — always top-left of widget
    {
        QFont titleFont;
        titleFont.setPointSize(12);
        titleFont.setBold(true);
        painter.setFont(titleFont);
        QFontMetrics fm(titleFont);
        int titleW = fm.horizontalAdvance(mTitle) + 20;
        int titleH = fm.height() + 10;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 150));
        painter.drawRect(0, 0, titleW, titleH);
        painter.setPen(Qt::white);
        painter.drawText(QRectF(8, 0, titleW - 8, titleH),
                         Qt::AlignVCenter, mTitle);
    }

    // Border
    painter.setPen(QPen(QColor(60, 70, 80), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(videoRect.adjusted(0, 0, -1, -1));
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
