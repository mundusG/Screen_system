#ifndef THREADEDSOUNDPLAYER_H
#define THREADEDSOUNDPLAYER_H

#include <QObject>
#include <QThread>

class QSoundEffect;

// Worker thread: runs a simulated time-consuming task off the GUI thread, then
// emits playRequested() when done. It NEVER touches the QSoundEffect — playback
// is handled in the main thread via the signal/slot relay below.
class SoundPlayWorker final : public QThread
{
    Q_OBJECT

public:
    explicit SoundPlayWorker(QObject* parent = nullptr);

signals:
    void playRequested();

protected:
    void run() override;
};

// Owns the single QSoundEffect, created and used only on the main thread.
// trigger() spawns a worker thread; its playRequested() signal is delivered
// back to playSound() on the main thread (default AutoConnection -> queued
// across threads), so QSoundEffect is never called from the worker thread.
class ThreadedSoundPlayer final : public QObject
{
    Q_OBJECT

public:
    explicit ThreadedSoundPlayer(QObject* parent = nullptr);

    // Start one worker thread. Coexists with AlarmController; called once per
    // frame that contains any class-1 detection, and by the test button.
    void trigger();

private slots:
    void playSound();
    void onStatusChanged();

private:
    bool isCoolingDown();
    void startPlayback();

    QSoundEffect* mSound;
    bool mPending = false;            // a play was requested while the source wasn't Ready
    qint64 mLastPlayMs = 0;           // epoch ms of the last playback (0 = never played)
};

#endif // THREADEDSOUNDPLAYER_H
