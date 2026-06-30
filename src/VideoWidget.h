#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QWidget>
#include <QMutex>
#include <QTimer>
#include <opencv2/imgproc.hpp>
#include "Types.h"

class VideoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit VideoWidget(int cameraId, const QString& title, QWidget* parent = nullptr);
    ~VideoWidget() override;

    int    cameraId()       const { return mCameraId; }
    void   setTitle(const QString& title) { mTitle = title; update(); }
    float  confidenceThreshold() const { return mConfThreshold; }
    int    detectionCount() const { return mDetectionCount; }
    double currentFps()     const { return mCurrentFps; }
    bool   isSelected()     const { return mSelected; }
    bool   hasSignal()      const { return mHasSignal; }
    QImage grabThumbnail(int maxWidth = 120) const;
    QImage grabFullFrame() const;

    /// Return a scaled BGR frame for the save pipeline.
    /// The BGR→RGB conversion and final scaling happen on the worker thread,
    /// so the main thread only pays for a lightweight cv::resize + clone.
    cv::Mat grabFrameForSave(int maxDim = 960) const;

    void setSelected(bool selected);
    void setSourceLabel(const QString& label) { mSourceLabel = label; update(); }

signals:
    void confidenceThresholdChanged(int cameraId, float threshold);
    void clicked(int cameraId);

public slots:
    void updateDisplayFrame(const FrameData& frame);
    void updateDetectionOverlay(const DisplayResult& result);
    void updateFps(double fps);
    void updateInferenceTime(float ms);
    void setConfidenceThreshold(float threshold);
    void showNoSignal();
    void reset();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    int  heightForWidth(int w) const override;
    bool hasHeightForWidth() const override;

private:
    QImage matToQImage(const cv::Mat& mat);
    void drawLiveIndicator(QPainter& p, const QRect& videoRect);
    void drawTimestamp(QPainter& p, const QRect& videoRect);
    void drawStatusOverlay(QPainter& p, const QRect& videoRect);
    void drawTitleBadge(QPainter& p);
    void drawBorder(QPainter& p, const QRect& videoRect);

    int     mCameraId;
    QString mTitle;
    float   mConfThreshold = 0.5f;
    bool    mSelected = false;

    cv::Mat            mCurrentFrame;
    QImage             mDisplayImage;
    mutable QMutex     mFrameMutex;

    QVector<Detection> mDetections;
    mutable QMutex     mDetectionMutex;

    QMap<int, QColor>  mClassColors;

    qint64  mLastFrameTime      = 0;
    qint64  mLastDetectionTime  = 0;

    double  mCurrentFps             = 0.0;
    float   mCurrentInferenceTimeMs = 0.0f;
    bool    mHasSignal              = false;
    bool    mHasDetections          = false;
    int     mDetectionCount         = 0;
    QString mSourceLabel;              // "CFG" or "PRV" for source type display

    QTimer* mTickTimer = nullptr;
    float   mLivePulse = 1.0f;
    bool    mLivePulseDir = false;

    static constexpr qint64 kDetectionTimeoutMs = 2000;
};

#endif // VIDEOWIDGET_H
