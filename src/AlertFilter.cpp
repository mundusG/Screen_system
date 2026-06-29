#include "AlertFilter.h"
#include <QDebug>

// ================================================================
// Factory
// ================================================================

AlertFilterRule* AlertFilterRule::createFromJson(const QJsonObject& obj)
{
    if (!obj["enabled"].toBool(true))
        return nullptr;

    QString type = obj["type"].toString();
    if (type == "containment")
        return new ContainmentFilterRule(obj);

    qWarning() << "AlertFilterRule: unknown type:" << type;
    return nullptr;
}

// ================================================================
// ContainmentFilterRule
// ================================================================

ContainmentFilterRule::ContainmentFilterRule(const QJsonObject& obj)
{
    QJsonArray refs = obj["reference_class_ids"].toArray();
    for (const auto& v : refs)
        mRefClassIds.append(v.toInt());
}

bool ContainmentFilterRule::accept(const Detection& detection,
                                    const QVector<Detection>& allDetections) const
{
    for (const auto& other : allDetections) {
        if (other.filtered)
            continue;
        if (!mRefClassIds.contains(other.classId))
            continue;
        if (other.bbox.containsCenterOf(detection.bbox))
            return true;
    }
    return false;
}

// ================================================================
// AlertFilterPipeline
// ================================================================

AlertFilterPipeline::AlertFilterPipeline(const AlertConfig& config)
    : mConfig(config)
{
    for (const auto& val : config.filters) {
        AlertFilterRule* rule = AlertFilterRule::createFromJson(val.toObject());
        if (rule)
            mRules.append(rule);
    }
}

AlertFilterPipeline::~AlertFilterPipeline()
{
    qDeleteAll(mRules);
}

QVector<Detection> AlertFilterPipeline::filter(
    const QVector<Detection>& allDetections) const
{
    QVector<Detection> result;

    for (const auto& det : allDetections) {
        if (det.filtered)
            continue;
        if (!mConfig.defectClassIds.contains(det.classId))
            continue;

        bool passed = true;
        for (const auto* rule : mRules) {
            if (!rule->accept(det, allDetections)) {
                passed = false;
                break;
            }
        }

        if (passed)
            result.append(det);
    }

    return result;
}
