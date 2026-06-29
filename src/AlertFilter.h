#ifndef ALERTFILTER_H
#define ALERTFILTER_H

#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include "Types.h"

// ================================================================
// Abstract filter rule — one check in the pipeline
// ================================================================
class AlertFilterRule
{
public:
    virtual ~AlertFilterRule() = default;

    /// Return true if @p detection passes this rule, given the full
    /// set of @p allDetections (which may include reference-class boxes).
    virtual bool accept(const Detection& detection,
                        const QVector<Detection>& allDetections) const = 0;

    /// Factory: create a rule from its JSON config object.
    /// Returns nullptr if the rule is disabled or the type is unknown.
    static AlertFilterRule* createFromJson(const QJsonObject& obj);
};

// ================================================================
// Containment filter: detection centre must lie inside at least one
// detection whose classId is listed in reference_class_ids.
// ================================================================
class ContainmentFilterRule : public AlertFilterRule
{
public:
    explicit ContainmentFilterRule(const QJsonObject& obj);

    bool accept(const Detection& detection,
                const QVector<Detection>& allDetections) const override;

private:
    QVector<int> mRefClassIds;
};

// ================================================================
// Pipeline: chains zero or more rules with AND logic.
// A detection passes only when every enabled rule accepts it.
// ================================================================
class AlertFilterPipeline
{
public:
    explicit AlertFilterPipeline(const AlertConfig& config);
    ~AlertFilterPipeline();

    /// Run all enabled rules on every detection in @p allDetections.
    /// Returns the subset of detections whose classId is listed in
    /// defectClassIds AND which pass all filter rules.
    QVector<Detection> filter(const QVector<Detection>& allDetections) const;

    const AlertConfig& config() const { return mConfig; }

private:
    AlertConfig mConfig;
    QVector<AlertFilterRule*> mRules;   // owned
};

#endif // ALERTFILTER_H
