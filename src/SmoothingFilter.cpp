#include "SmoothingFilter.h"
#include <QtMath>
#include <QDebug>
#include <algorithm>

SmoothingFilter::SmoothingFilter(int cameraId, QObject* parent)
    : QObject(parent)
    , mCameraId(cameraId)
    , mAlpha(0.3f)
    , mMaxLostFrames(5)
    , mNextTrackId(0)
{
}

void SmoothingFilter::setAlpha(float alpha)
{
    mAlpha = qBound(0.0f, alpha, 1.0f);
}

void SmoothingFilter::setMaxLostFrames(int maxLost)
{
    mMaxLostFrames = qMax(1, maxLost);
}

void SmoothingFilter::reset()
{
    mTracks.clear();
    mNextTrackId = 0;
}

void SmoothingFilter::processInferenceResult(const InferenceResult& result)
{
    // Filter detections below confidence threshold is done in InferenceEngine
    // Here we handle tracking + smoothing

    QVector<Detection> validDetections;
    for (const auto& det : result.detections) {
        if (!det.filtered) {
            validDetections.append(det);
        }
    }

    // Match detections to existing tracks
    QVector<QPair<int,int>> matches = matchDetections(validDetections, mTracks);

    // Track which detection indices were matched
    QSet<int> matchedDetIndices;
    QSet<int> matchedTrackIds;

    for (const auto& pair : matches) {
        int detIdx   = pair.first;
        int trackId  = pair.second;

        matchedDetIndices.insert(detIdx);
        matchedTrackIds.insert(trackId);

        const Detection& det = validDetections[detIdx];
        TrackState& track    = mTracks[trackId];

        // Apply EMA smoothing on the displacement between frames
        BoundingBox smoothed = smoothBBox(det.bbox, track.smoothedBBox);

        track.smoothedBBox       = smoothed;
        track.rawBBox            = det.bbox;
        track.confidence         = det.confidence;
        track.framesSinceUpdate  = 0;
        track.active             = true;
        track.normalized         = det.normalized;
    }

    // Create new tracks for unmatched detections
    for (int i = 0; i < validDetections.size(); ++i) {
        if (matchedDetIndices.contains(i)) continue;

        const Detection& det = validDetections[i];
        int newTrackId = nextTrackId();

        TrackState newTrack;
        newTrack.trackId          = newTrackId;
        newTrack.classId          = det.classId;
        newTrack.smoothedBBox     = det.bbox;  // first detection: no smoothing
        newTrack.rawBBox          = det.bbox;
        newTrack.confidence       = det.confidence;
        newTrack.framesSinceUpdate = 0;
        newTrack.active           = true;
        newTrack.normalized       = det.normalized;

        mTracks[newTrackId] = newTrack;
    }

    // Update lost-frame counters for unmatched tracks
    QList<int> tracksToRemove;
    for (auto it = mTracks.begin(); it != mTracks.end(); ++it) {
        if (matchedTrackIds.contains(it.key())) continue;

        it.value().framesSinceUpdate++;
        if (it.value().framesSinceUpdate > mMaxLostFrames) {
            tracksToRemove.append(it.key());
        }
    }

    // Remove stale tracks
    for (int trackId : tracksToRemove) {
        mTracks.remove(trackId);
    }

    // Build display result with smoothed detections
    DisplayResult displayResult;
    displayResult.cameraId  = mCameraId;
    displayResult.timestamp = result.timestamp;
    displayResult.fresh     = true;

    for (auto it = mTracks.begin(); it != mTracks.end(); ++it) {
        const TrackState& track = it.value();
        if (!track.active) continue;

        Detection d;
        d.trackId    = track.trackId;
        d.classId    = track.classId;
        d.confidence = track.confidence;
        d.bbox       = track.smoothedBBox;
        d.filtered   = false;
        d.normalized = track.normalized;
        displayResult.detections.append(d);
    }

    emit displayResultReady(displayResult);
}

QVector<QPair<int,int>> SmoothingFilter::matchDetections(
    const QVector<Detection>& detections,
    const QHash<int, TrackState>& tracks)
{
    // Greedy IoU-based matching
    // Build all (detIdx, trackId, iou) pairs above threshold
    struct Match {
        int   detIdx;
        int   trackId;
        float iou;
    };

    QVector<Match> candidates;
    for (int d = 0; d < detections.size(); ++d) {
        for (auto it = tracks.begin(); it != tracks.end(); ++it) {
            float iou = detections[d].bbox.iou(it.value().smoothedBBox);
            if (iou > 0.3f) {  // IoU threshold for matching
                candidates.append({d, it.key(), iou});
            }
        }
    }

    // Sort by IoU descending (best matches first)
    std::sort(candidates.begin(), candidates.end(),
              [](const Match& a, const Match& b) { return a.iou > b.iou; });

    // Greedy assignment
    QSet<int> usedDets;
    QSet<int> usedTracks;
    QVector<QPair<int,int>> matches;

    for (const auto& m : candidates) {
        if (usedDets.contains(m.detIdx) || usedTracks.contains(m.trackId)) {
            continue;
        }
        usedDets.insert(m.detIdx);
        usedTracks.insert(m.trackId);
        matches.append({m.detIdx, m.trackId});
    }

    return matches;
}

BoundingBox SmoothingFilter::smoothBBox(const BoundingBox& raw, const BoundingBox& prev)
{
    // EMA: smoothed = alpha * raw + (1 - alpha) * prev_smoothed
    // This smooths the DISPLACEMENT between frames
    BoundingBox result;
    result.x      = mAlpha * raw.x      + (1.0f - mAlpha) * prev.x;
    result.y      = mAlpha * raw.y      + (1.0f - mAlpha) * prev.y;
    result.width  = mAlpha * raw.width  + (1.0f - mAlpha) * prev.width;
    result.height = mAlpha * raw.height + (1.0f - mAlpha) * prev.height;
    return result;
}

int SmoothingFilter::nextTrackId()
{
    return mNextTrackId++;
}
