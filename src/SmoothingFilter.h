#ifndef SMOOTHINGFILTER_H
#define SMOOTHINGFILTER_H

#include <QObject>
#include <QHash>
#include <QQueue>
#include <QPair>
#include "Types.h"

/// Internal track state for a single tracked object
struct TrackState {
    int         trackId     = 0;
    int         classId     = 0;
    QString     className;
    BoundingBox smoothedBBox;       // current smoothed position
    BoundingBox rawBBox;           // latest raw detection
    float       confidence  = 0.0f;
    int         framesSinceUpdate = 0;  // frames since last matched detection
    bool        active      = true;
    bool        normalized  = false;    // whether bbox coords are normalized [0,1]
};

/// Performs IoU-based tracking and EMA smoothing of bounding boxes.
/// One instance per camera, runs on the main thread.
class SmoothingFilter : public QObject
{
    Q_OBJECT

public:
    explicit SmoothingFilter(int cameraId, QObject* parent = nullptr);
    ~SmoothingFilter() override = default;

    /// Set EMA smoothing alpha (0 = heavy smoothing, 1 = no smoothing / instant)
    void setAlpha(float alpha);

    /// Set max frames a track can be lost before removal
    void setMaxLostFrames(int maxLost);

    /// Reset all track states
    void reset();

signals:
    /// Emitted when smoothed detections are ready for display
    void displayResultReady(const DisplayResult& result);

public slots:
    /// Process new inference result, apply tracking + smoothing
    void processInferenceResult(const InferenceResult& result);

private:
    /// Match detections to existing tracks via greedy IoU
    QVector<QPair<int,int>> matchDetections(
        const QVector<Detection>& detections,
        const QHash<int, TrackState>& tracks);

    /// Apply EMA smoothing: smoothed = alpha * raw + (1-alpha) * prev_smoothed
    BoundingBox smoothBBox(const BoundingBox& raw, const BoundingBox& prev);

    /// Generate a new unique track ID
    int nextTrackId();

    int     mCameraId;
    float   mAlpha;
    int     mMaxLostFrames;
    int     mNextTrackId;

    QHash<int, TrackState> mTracks;  // trackId -> state
};

#endif // SMOOTHINGFILTER_H
