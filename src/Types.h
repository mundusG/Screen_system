#ifndef TYPES_H
#define TYPES_H

#include <QString>
#include <QRectF>
#include <QPointF>
#include <QVector>
#include <QMap>
#include <QColor>
#include <QMetaType>
#include <QJsonArray>
#include <opencv2/core.hpp>

/// Bounding box in normalized coordinates [0, 1] or pixel coordinates
struct BoundingBox {
    float x      = 0.0f;  // center x
    float y      = 0.0f;  // center y
    float width  = 0.0f;
    float height = 0.0f;

    BoundingBox() = default;
    BoundingBox(float x_, float y_, float w_, float h_)
        : x(x_), y(y_), width(w_), height(h_) {}

    // Convert to corner representation
    float left()   const { return x - width  / 2.0f; }
    float top()    const { return y - height / 2.0f; }
    float right()  const { return x + width  / 2.0f; }
    float bottom() const { return y + height / 2.0f; }

    QRectF toRectF() const {
        return QRectF(left(), top(), width, height);
    }

    // IoU with another box
    float iou(const BoundingBox& other) const {
        float interLeft   = std::max(left(),   other.left());
        float interTop    = std::max(top(),    other.top());
        float interRight  = std::min(right(),  other.right());
        float interBottom = std::min(bottom(), other.bottom());
        if (interLeft >= interRight || interTop >= interBottom) return 0.0f;

        float interArea = (interRight - interLeft) * (interBottom - interTop);
        float area1     = width * height;
        float area2     = other.width * other.height;
        float unionArea = area1 + area2 - interArea;
        return (unionArea > 0) ? interArea / unionArea : 0.0f;
    }

    // Check if this box contains a point
    bool containsPoint(float px, float py) const {
        return px >= left() && px <= right() &&
               py >= top() && py <= bottom();
    }

    // Check if this box contains the center of another box
    bool containsCenterOf(const BoundingBox& other) const {
        return containsPoint(other.x, other.y);
    }
};

/// A single detection result
struct Detection {
    int         trackId     = -1;      // tracking ID (assigned by tracker)
    int         classId     = 0;       // class index
    float       confidence  = 0.0f;    // confidence score [0, 1]
    BoundingBox bbox;                  // bounding box
    QString     className;             // human-readable class name
    bool        filtered    = false;   // whether this detection passed confidence filter
    bool        normalized  = false;   // true if bbox is in [0,1] normalized coords
};

/// Alert configuration — parsed from per-camera "alert_config" JSON section
struct AlertConfig {
    bool            enabled         = true;
    QVector<int>    defectClassIds  = {1};     // class IDs treated as "defect"
    float           minConfidence   = 0.6f;    // minimum confidence to trigger alert
    QJsonArray      filters;                   // raw JSON array of filter rules for pipeline factory
};

/// Per-camera configuration
struct CameraConfig {
    int     cameraId        = 0;
    QString source;                       // RTSP URL, /dev/videoN, or file path
    QString modelPath;                    // path to .onnx model weights
    QString name;                         // display name
    bool    enabled         = true;
    QString mode            = "local_inference"; // "local_inference", "mqtt_publish", "mqtt_subscribe"
    QString mqttTopic;                    // MQTT topic for this camera (subscribe mode)

    // Inference settings
    float   confidenceThreshold = 0.5f;  // minimum confidence to show detection
    float   nmsThreshold        = 0.45f; // NMS IoU threshold
    int     inputWidth          = 640;   // model input width
    int     inputHeight         = 640;   // model input height

    // Smoothing settings
    float   smoothingAlpha  = 0.3f;      // EMA alpha (0 = no smoothing, 1 = instant)
    int     trackMaxLost     = 5;        // max frames before track is removed

    // Inference rate
    int     inferenceIntervalMs = 1000;  // ms between inference frames

    // Image stream mode
    QString snapshotUrl;                       // HTTP/RTSP/file URL for periodic snapshot
    int     snapshotIntervalMs = 1000;         // ms between snapshot fetches

    // Alert / snapshot
    int         alertSnapshotIntervalMs = 0;   // ms between periodic alert snapshots (0=disabled, MQTT-trigger only)
    AlertConfig alertConfig;                   // per-camera alert filtering configuration

    // Color mapping: classId -> QColor name
    QMap<int, QString> classColors;

    CameraConfig() {
        // Default class colors
        classColors[0] = "#00FF00";  // green
        classColors[1] = "#FF0000";  // red
        // Additional classes default to yellow
    }
};

/// One MQTT broker connection and the global camera IDs it is allowed to serve.
/// Multiple sources let one display subscribe to independent inference systems.
struct MqttSourceConfig {
    QString id;
    bool    enabled = true;
    QString broker;
    QString clientId;
    QString username;
    QString password;
    QString discoveryTopic;
    int     cameraIdMin = 0;
    int     cameraIdMax = 7;

    bool acceptsCamera(int cameraId) const {
        return cameraId >= cameraIdMin && cameraId <= cameraIdMax;
    }
};

/// Frame with metadata passed between pipeline stages
struct FrameData {
    int         cameraId = 0;
    cv::Mat     image;          // OpenCV BGR image
    qint64      timestamp = 0;  // capture timestamp (ms)
    int         frameIndex = 0; // sequential frame number

    FrameData() = default;
};

/// Inference result for a single frame
struct InferenceResult {
    int              cameraId   = 0;
    qint64           timestamp  = 0;
    int              frameIndex = 0;
    QVector<Detection> detections;  // raw detections from model
    float            inferenceTimeMs = 0.0f;
    int              frameWidth  = 0;  // original frame width (for coordinate normalization)
    int              frameHeight = 0;  // original frame height
    QByteArray       frameJpeg;       // optional: base64-decoded JPEG thumbnail
                                      // from inference device; when present, guarantees
                                      // frame and boxes are from the same capture.
};

/// Smoothed result ready for display
struct DisplayResult {
    int              cameraId    = 0;
    qint64           timestamp   = 0;
    QVector<Detection> detections;  // smoothed detections
    bool             fresh       = false; // whether this is a new inference result
};

/// Channel info discovered from inference bridge via MQTT
struct ChannelInfo {
    int     cameraId        = 0;
    int     chid            = 0;
    QString name;
    QString previewUrl;         // rtsp://host:port/preview/chid
    QString snapshotUrl;        // http://host:port/snapshot/chid (for image_stream mode)
    QString inferenceTopic;     // inference/camera/{id}/detections
};

// Register types for Qt signal/slot system
Q_DECLARE_METATYPE(FrameData)
Q_DECLARE_METATYPE(InferenceResult)
Q_DECLARE_METATYPE(DisplayResult)
Q_DECLARE_METATYPE(ChannelInfo)
Q_DECLARE_METATYPE(CameraConfig)
Q_DECLARE_METATYPE(cv::Mat)

#endif // TYPES_H
