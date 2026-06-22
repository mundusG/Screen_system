#include "AlarmController.h"

#include <QDebug>
#include <QSoundEffect>
#include <QUrl>

namespace {
constexpr qint64 AlarmCooldownMs = 10 * 1000;
}

AlarmController::AlarmController(QObject* parent)
    : QObject(parent)
    , mSound(new QSoundEffect(this))
{
    connect(mSound, &QSoundEffect::statusChanged,
            this, &AlarmController::onSoundStatusChanged);
    connect(mSound, &QSoundEffect::playingChanged,
            this, &AlarmController::onSoundPlayingChanged);

    // QSoundEffect uses the operating system's selected default output: HDMI,
    // USB, Bluetooth, or the local speaker.
    mSound->setSource(QUrl(QStringLiteral("qrc:/sounds/defect_alarm.wav")));
    mSound->setVolume(0.85f);
    mSound->setLoopCount(1);
}

void AlarmController::requestAlarm()
{
    if (isCoolingDown()) {
        return;
    }

    if (mPendingRequest) {
        return;
    }

    if (mSound->status() == QSoundEffect::Ready) {
        startPlayback();
        return;
    }

    if (mSound->status() == QSoundEffect::Error) {
        qWarning() << "AlarmController: audio backend error; alarm request was not played for"
                   << mSound->source();
        return;
    }

    // The source is still loading. Preserve one request only; additional
    // detections are merged until QSoundEffect reports Ready.
    mPendingRequest = true;
    qDebug() << "AlarmController: alarm request queued while audio backend loads";
}

void AlarmController::onSoundStatusChanged()
{
    const QSoundEffect::Status status = mSound->status();
    if (status == QSoundEffect::Error) {
        const bool hadPendingRequest = mPendingRequest || mAwaitingPlaybackStart;
        mPendingRequest = false;
        mAwaitingPlaybackStart = false;
        mCooldownActive = false;
        qWarning() << "AlarmController: audio backend entered Error state;"
                   << (hadPendingRequest ? "queued alarm discarded" : "no alarm was played")
                   << "source:" << mSound->source();
        return;
    }

    if (status == QSoundEffect::Ready && mPendingRequest) {
        mPendingRequest = false;
        if (!isCoolingDown()) {
            startPlayback();
        }
    }
}

void AlarmController::onSoundPlayingChanged()
{
    if (mSound->isPlaying()) {
        mAwaitingPlaybackStart = false;
        qDebug() << "AlarmController: actual alarm playback started";
    }
}

bool AlarmController::isCoolingDown()
{
    if (!mCooldownActive) {
        return false;
    }

    if (mCooldownTimer.elapsed() < AlarmCooldownMs) {
        return true;
    }

    mCooldownActive = false;
    return false;
}

void AlarmController::startPlayback()
{
    if (isCoolingDown() || mSound->status() != QSoundEffect::Ready) {
        return;
    }

    mAwaitingPlaybackStart = true;
    mSound->play();
    mCooldownTimer.start();
    mCooldownActive = true;
}
