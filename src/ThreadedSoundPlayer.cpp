#include "ThreadedSoundPlayer.h"

#include <QDebug>
#include <QSoundEffect>
#include <QUrl>
#include <QDateTime>

namespace {
// Shared cooldown window: at most one playback per this interval, regardless of
// how many class-1 frames arrive. Tune as needed (AlarmController uses 10 s).
constexpr qint64 SoundCooldownMs = 10 * 1000;
}

// ---------------------------------------------------------------------------
// SoundPlayWorker
// ---------------------------------------------------------------------------

SoundPlayWorker::SoundPlayWorker(QObject* parent)
    : QThread(parent)
{
}

void SoundPlayWorker::run()
{
    // Simulate a time-consuming task running off the GUI thread. Do NOT call
    // any QSoundEffect method here — playback must happen in the main thread.
    // QThread::msleep(300);

    // Hand control back to the main thread. With a default (AutoConnection)
    // connect, this emission is queued onto the receiver's thread.
    emit playRequested();
}

// ---------------------------------------------------------------------------
// ThreadedSoundPlayer
// ---------------------------------------------------------------------------

ThreadedSoundPlayer::ThreadedSoundPlayer(QObject* parent)
    : QObject(parent)
    , mSound(new QSoundEffect(this)) // created on the main thread
{
    // QSoundEffect uses the operating system's selected default output.
    mSound->setSource(QUrl(QStringLiteral("qrc:/sounds/defect_alarm.wav")));
    mSound->setVolume(0.85f);
    mSound->setLoopCount(1);

    // The source loads asynchronously and, on the PulseAudio backend, can dip
    // back to Loading around playback. Play any deferred request once Ready.
    connect(mSound, &QSoundEffect::statusChanged,
            this, &ThreadedSoundPlayer::onStatusChanged);
}

void ThreadedSoundPlayer::trigger()
{
    // The worker QThread object lives on the main thread (created here), so its
    // playRequested() signal — emitted from run() on the worker thread — is
    // delivered to playSound() on the main thread via a queued connection.
    auto* worker = new SoundPlayWorker(this);

    // Requirement 3: default AutoConnection -> queued across threads, thread-safe.
    connect(worker, &SoundPlayWorker::playRequested,
            this, &ThreadedSoundPlayer::playSound);

    // Requirement 4: the thread destroys itself when finished -> no leak.
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);

    worker->start();
}

void ThreadedSoundPlayer::playSound()
{
    qDebug() << QThread::currentThread() << " playSound: " << mSound->status();
    // Runs on the main thread — safe to touch QSoundEffect here.
    if (isCoolingDown()) {
	qDebug() << "playSound: isCoolingDown: " << QDateTime::currentMSecsSinceEpoch() - mLastPlayMs;
        return;
    }

    if (mSound->status() == QSoundEffect::Ready) {
        startPlayback();
        return;
    }

    if (mSound->status() == QSoundEffect::Error) {
        qWarning() << "ThreadedSoundPlayer: audio backend error, source:"
                   << mSound->source();
        return;
    }

    // Null/Loading: the source isn't ready yet. Defer and play when it becomes
    // Ready. Multiple requests during loading coalesce into a single playback.
    // mPending = true;
}

void ThreadedSoundPlayer::onStatusChanged()
{
    if (mSound->status() == QSoundEffect::Ready && mPending) {
        mPending = false;
        if (!isCoolingDown()) {
            startPlayback();
        }
    }
}

bool ThreadedSoundPlayer::isCoolingDown()
{
    // mLastPlayMs == 0 means it has never played -> first play allowed.
    if (mLastPlayMs == 0) {
        return false;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    return (now - mLastPlayMs) < SoundCooldownMs;
}

void ThreadedSoundPlayer::startPlayback()
{
    mLastPlayMs = QDateTime::currentMSecsSinceEpoch(); // record this playback time
    mSound->play();
}
