#ifndef ALARMCONTROLLER_H
#define ALARMCONTROLLER_H

#include <QObject>
#include <QElapsedTimer>

class QSoundEffect;

// Owns the single application-wide alarm player. All callers submit a request;
// this class serializes playback and enforces one shared cooldown.
class AlarmController final : public QObject
{
    Q_OBJECT

public:
    explicit AlarmController(QObject* parent = nullptr);

    // Multiple requests are intentionally coalesced. A request is either played
    // once, queued until the audio backend becomes ready, or ignored in cooldown.
    void requestAlarm();

private slots:
    void onSoundStatusChanged();
    void onSoundPlayingChanged();

private:
    bool isCoolingDown();
    void startPlayback();

    QSoundEffect* mSound;
    QElapsedTimer mCooldownTimer;
    bool mCooldownActive = false;
    bool mPendingRequest = false;
    bool mAwaitingPlaybackStart = false;
};

#endif // ALARMCONTROLLER_H
