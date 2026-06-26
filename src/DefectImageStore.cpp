#include "DefectImageStore.h"
#include "Theme.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QPainter>
#include <QPointer>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QThreadPool>
#include <QRunnable>

#include <algorithm>

namespace {

constexpr int DefectClassId = 1;

struct SaveRequest {
    QPointer<DefectImageStore> store;
    QString filePath;
    QString fileName;
    int cameraId = -1;
    QString location;
    QDateTime timestamp;
    float confidence = 0.0f;
    QImage frame;
    QVector<Detection> detections;
};

static QRectF detectionRect(const Detection& det, const QSize& imageSize)
{
    const qreal w = imageSize.width();
    const qreal h = imageSize.height();

    qreal x = 0.0;
    qreal y = 0.0;
    qreal bw = 0.0;
    qreal bh = 0.0;

    if (det.normalized) {
        x = det.bbox.left() * w;
        y = det.bbox.top() * h;
        bw = det.bbox.width * w;
        bh = det.bbox.height * h;
    } else {
        x = det.bbox.left();
        y = det.bbox.top();
        bw = det.bbox.width;
        bh = det.bbox.height;
    }

    QRectF rect(x, y, bw, bh);
    return rect.intersected(QRectF(0, 0, w, h));
}

static void drawDefectBoxes(QImage& image, const QVector<Detection>& detections)
{
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int base = std::max(2, std::min(image.width(), image.height()) / 260);
    QFont font("Sans", std::max(10, base * 5), QFont::Bold);
    p.setFont(font);
    QFontMetrics fm(font);

    for (const auto& det : detections) {
        if (det.classId != DefectClassId || det.filtered)
            continue;

        QRectF r = detectionRect(det, image.size());
        if (r.width() <= 1.0 || r.height() <= 1.0)
            continue;

        QColor color = Theme::alertRed();
        p.setPen(QPen(color, base + 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r);

        QString label = QString::fromUtf8("异常 %1%").arg(qRound(det.confidence * 100.0f));
        int textW = fm.horizontalAdvance(label) + base * 6;
        int textH = fm.height() + base * 2;
        int tx = static_cast<int>(r.left());
        int ty = static_cast<int>(r.top()) - textH - 2;
        if (ty < 0)
            ty = static_cast<int>(r.top()) + 2;
        if (tx + textW > image.width())
            tx = std::max(0, image.width() - textW - 2);

        QRect labelRect(tx, ty, textW, textH);
        QColor bg = color;
        bg.setAlpha(210);
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(labelRect, 3, 3);
        p.setPen(Qt::white);
        p.drawText(labelRect.adjusted(base * 3, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, label);
    }
}

class SaveImageTask : public QRunnable
{
public:
    explicit SaveImageTask(const SaveRequest& request)
        : mRequest(request)
    {
        setAutoDelete(true);
    }

    void run() override
    {
        if (mRequest.frame.isNull()) {
            postFailed(QStringLiteral("empty frame"));
            return;
        }

        QImage image = mRequest.frame.convertToFormat(QImage::Format_RGB32);
        drawDefectBoxes(image, mRequest.detections);

        QDir dir(QFileInfo(mRequest.filePath).absolutePath());
        if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
            postFailed(QStringLiteral("cannot create storage directory"));
            return;
        }

        if (!image.save(mRequest.filePath, "JPG", 90)) {
            postFailed(QStringLiteral("image save failed"));
            return;
        }

        DefectImageRecord record;
        record.filePath = mRequest.filePath;
        record.fileName = mRequest.fileName;
        record.cameraId = mRequest.cameraId;
        record.location = mRequest.location;
        record.timestamp = mRequest.timestamp;
        record.confidence = mRequest.confidence;
        record.imageSize = image.size();

        if (mRequest.store) {
            auto* store = mRequest.store.data();
            QMetaObject::invokeMethod(store, [store, record]() {
                store->onAsyncImageSaved(record);
            }, Qt::QueuedConnection);
        }
    }

private:
    void postFailed(const QString& reason)
    {
        if (mRequest.store) {
            auto* store = mRequest.store.data();
            const QString path = mRequest.filePath;
            QMetaObject::invokeMethod(store, [store, path, reason]() {
                store->onAsyncSaveFailed(path, reason);
            }, Qt::QueuedConnection);
        }
    }

    SaveRequest mRequest;
};

} // namespace

DefectImageStore::DefectImageStore(QObject* parent)
    : QObject(parent)
{
    applyDefaults();
}

bool DefectImageStore::initialize(const QString& configPath)
{
    applyDefaults();
    const QString path = resolveConfigPath(configPath);
    loadConfig(path);

    {
        QMutexLocker locker(&mMutex);
        if (mConfig.storageDir.trimmed().isEmpty())
            mConfig.storageDir = defaultStorageDir();

        QDir dir(mConfig.storageDir);
        if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
            qWarning() << "DefectImageStore: cannot create storage dir:" << mConfig.storageDir;
            return false;
        }
        mInitialized = true;
    }

    scanStorageDir();
    return true;
}

int DefectImageStore::count() const
{
    QMutexLocker locker(&mMutex);
    return mRecords.size();
}

int DefectImageStore::pageSize() const
{
    QMutexLocker locker(&mMutex);
    return mConfig.pageSize;
}

void DefectImageStore::setPageSize(int pageSize)
{
    QMutexLocker locker(&mMutex);
    mConfig.pageSize = std::max(1, pageSize);
}

int DefectImageStore::pageCount(int pageSize) const
{
    QMutexLocker locker(&mMutex);
    int size = pageSize > 0 ? pageSize : mConfig.pageSize;
    size = std::max(1, size);
    return std::max(1, (mRecords.size() + size - 1) / size);
}

QVector<DefectImageRecord> DefectImageStore::recordsForPage(int pageIndex, int pageSize) const
{
    QMutexLocker locker(&mMutex);
    int size = pageSize > 0 ? pageSize : mConfig.pageSize;
    size = std::max(1, size);
    int start = std::max(0, pageIndex) * size;

    QVector<DefectImageRecord> page;
    if (start >= mRecords.size())
        return page;

    int end = std::min(start + size, mRecords.size());
    page.reserve(end - start);
    for (int i = start; i < end; ++i)
        page.append(mRecords[i]);
    return page;
}

QString DefectImageStore::locationName(int cameraId, const QString& fallbackName) const
{
    QMutexLocker locker(&mMutex);
    if (mConfig.locations.contains(cameraId) && !mConfig.locations[cameraId].trimmed().isEmpty())
        return mConfig.locations[cameraId].trimmed();
    if (!fallbackName.trimmed().isEmpty())
        return fallbackName.trimmed();
    return QString::fromUtf8("摄像头 %1").arg(cameraId + 1);
}

void DefectImageStore::saveDefectImage(int cameraId,
                                       const QString& fallbackName,
                                       const QImage& frame,
                                       const QVector<Detection>& detections,
                                       float confidence,
                                       qint64 timestampMs)
{
    if (frame.isNull() || detections.isEmpty()) {
        qWarning() << "DefectImageStore::saveDefectImage: skipping -"
                   << "frame.null:" << frame.isNull()
                   << "detections.empty:" << detections.isEmpty();
        return;
    }

    QString storageDir;
    QString location;
    int sequence = 0;
    {
        QMutexLocker locker(&mMutex);
        if (!mInitialized) {
            qWarning() << "DefectImageStore::saveDefectImage: NOT initialized, skipping save";
            return;
        }
        storageDir = mConfig.storageDir;
        location = mConfig.locations.value(cameraId, fallbackName).trimmed();
        sequence = ++mSaveSequence;
    }

    if (location.isEmpty())
        location = fallbackName.trimmed().isEmpty() ? QString::fromUtf8("摄像头 %1").arg(cameraId + 1)
                                                    : fallbackName.trimmed();

    QDateTime ts = timestampMs > 0
        ? QDateTime::fromMSecsSinceEpoch(timestampMs)
        : QDateTime::currentDateTime();
    if (!ts.isValid())
        ts = QDateTime::currentDateTime();

    QString safeLocation = sanitizeFilePart(location);
    QString timePart = ts.toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    QString baseName = QStringLiteral("%1_cam%2_%3_seq%4_conf%5.jpg")
        .arg(timePart)
        .arg(cameraId)
        .arg(safeLocation)
        .arg(sequence, 6, 10, QChar('0'))
        .arg(qBound(0, qRound(confidence * 100.0f), 100));
    QString filePath = makeUniquePath(storageDir, baseName);

    SaveRequest request;
    request.store = this;
    request.filePath = filePath;
    request.fileName = QFileInfo(filePath).fileName();
    request.cameraId = cameraId;
    request.location = location;
    request.timestamp = ts;
    request.confidence = confidence;
    request.frame = frame;
    request.detections = detections;

    qDebug() << "DefectImageStore: enqueuing save for camera" << cameraId
             << "path:" << filePath
             << "dets:" << detections.size()
             << "conf:" << confidence;

    QThreadPool::globalInstance()->start(new SaveImageTask(request));
}

void DefectImageStore::onAsyncImageSaved(const DefectImageRecord& record)
{
    qDebug() << "DefectImageStore: image saved OK:" << record.fileName;
    {
        QMutexLocker locker(&mMutex);
        mRecords.prepend(record);
        sortRecordsLocked();
        enforceLimitLocked();
    }
    emit recordsChanged();
}

void DefectImageStore::onAsyncSaveFailed(const QString& path, const QString& reason)
{
    qWarning() << "DefectImageStore: failed to save" << path << reason;
}

QString DefectImageStore::resolveConfigPath(const QString& explicitPath)
{
    if (!explicitPath.isEmpty())
        return explicitPath;

    const QString envPath = qEnvironmentVariable("SCREEN_SYSTEM_DEFECT_CONFIG");
    if (!envPath.isEmpty() && QFile::exists(envPath))
        return envPath;

    const QString exeDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/defect_images.json",
        exeDir + "/config/defect_images.json",
        exeDir + "/../config/defect_images.json",
        QDir::currentPath() + "/config/defect_images.json",
        exeDir + "/config/defect_images.json.example",
        exeDir + "/../config/defect_images.json.example",
        QDir::currentPath() + "/config/defect_images.json.example"
    };

    for (const QString& path : candidates) {
        if (!path.isEmpty() && QFile::exists(path))
            return QFileInfo(path).absoluteFilePath();
    }
    return QDir::currentPath() + "/config/defect_images.json";
}

QString DefectImageStore::defaultStorageDir()
{
    const QString exeDir = QCoreApplication::applicationDirPath();
    const QString projectDir = QDir(exeDir).absoluteFilePath(QStringLiteral(".."));
    return QDir(projectDir).absoluteFilePath(QStringLiteral("defect_images"));
}

QString DefectImageStore::sanitizeFilePart(const QString& text)
{
    QString result;
    result.reserve(text.size());
    for (const QChar ch : text.trimmed()) {
        if (ch.isLetterOrNumber() || ch == '_' || ch == '-')
            result.append(ch);
        else
            result.append('_');
    }
    while (result.contains(QStringLiteral("__")))
        result.replace(QStringLiteral("__"), QStringLiteral("_"));
    result = result.trimmed();
    if (result.isEmpty())
        result = QStringLiteral("unknown");
    return result.left(48);
}

QString DefectImageStore::makeUniquePath(const QString& dir, const QString& baseName)
{
    QString path = QDir(dir).absoluteFilePath(baseName);
    if (!QFile::exists(path))
        return path;

    QFileInfo info(baseName);
    QString stem = info.completeBaseName();
    QString suffix = info.suffix();
    for (int i = 1; i < 1000; ++i) {
        QString candidate = QDir(dir).absoluteFilePath(
            QStringLiteral("%1_%2.%3").arg(stem).arg(i).arg(suffix));
        if (!QFile::exists(candidate))
            return candidate;
    }
    return path;
}

QDateTime DefectImageStore::parseTimestamp(const QString& value)
{
    QDateTime ts = QDateTime::fromString(value, QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    if (!ts.isValid())
        return QDateTime();
    return ts;
}

bool DefectImageStore::loadConfig(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "DefectImageStore: using defaults, config not found:" << path;
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "DefectImageStore: invalid config:" << path << error.errorString();
        return false;
    }

    QJsonObject root = doc.object();
    QMutexLocker locker(&mMutex);

    QString storageDir = root["storage_dir"].toString(mConfig.storageDir);
    if (!storageDir.trimmed().isEmpty()) {
        QFileInfo storageInfo(storageDir);
        if (storageInfo.isAbsolute()) {
            mConfig.storageDir = storageInfo.absoluteFilePath();
        } else {
            const QString exeDir = QCoreApplication::applicationDirPath();
            const QString projectDir = QDir(exeDir).absoluteFilePath(QStringLiteral(".."));
            mConfig.storageDir = QDir(projectDir).absoluteFilePath(storageDir);
        }
    }

    mConfig.maxImages = std::max(1, root["max_images"].toInt(mConfig.maxImages));
    mConfig.pageSize = std::max(1, root["page_size"].toInt(mConfig.pageSize));

    QJsonObject locs = root["locations"].toObject();
    for (auto it = locs.begin(); it != locs.end(); ++it) {
        bool ok = false;
        int cameraId = it.key().toInt(&ok);
        if (ok)
            mConfig.locations[cameraId] = it.value().toString();
    }
    return true;
}

void DefectImageStore::applyDefaults()
{
    QMutexLocker locker(&mMutex);
    mConfig.storageDir.clear();
    mConfig.maxImages = 500;
    mConfig.pageSize = 20;
    mConfig.locations.clear();
    for (int i = 0; i < 8; ++i)
        mConfig.locations[i] = QString::fromUtf8("摄像头 %1").arg(i + 1);
}

void DefectImageStore::scanStorageDir()
{
    QString dirPath;
    QMap<int, QString> locations;
    {
        QMutexLocker locker(&mMutex);
        dirPath = mConfig.storageDir;
        locations = mConfig.locations;
        mRecords.clear();
    }

    QDir dir(dirPath);
    const QFileInfoList files = dir.entryInfoList(
        QStringList() << QStringLiteral("*.jpg") << QStringLiteral("*.jpeg") << QStringLiteral("*.png"),
        QDir::Files,
        QDir::Time);

    QVector<DefectImageRecord> records;
    records.reserve(files.size());

    QRegularExpression withConf(
        QStringLiteral("^(\\d{8}_\\d{6}_\\d{3})_cam(\\d+)_(.+?)(?:_seq\\d+)?_conf(\\d{1,3})\\.(jpg|jpeg|png)$"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpression legacy(
        QStringLiteral("^(\\d{8}_\\d{6}_\\d{3})_cam(\\d+)_(.+)\\.(jpg|jpeg|png)$"),
        QRegularExpression::CaseInsensitiveOption);

    for (const QFileInfo& info : files) {
        QString name = info.fileName();
        QRegularExpressionMatch match = withConf.match(name);
        bool hasConf = match.hasMatch();
        if (!hasConf)
            match = legacy.match(name);
        if (!match.hasMatch())
            continue;

        DefectImageRecord record;
        record.filePath = info.absoluteFilePath();
        record.fileName = name;
        record.timestamp = parseTimestamp(match.captured(1));
        record.cameraId = match.captured(2).toInt();
        record.location = locations.value(record.cameraId).trimmed();
        if (record.location.isEmpty())
            record.location = match.captured(3);
        record.confidence = hasConf ? match.captured(4).toFloat() / 100.0f : 0.0f;
        if (!record.timestamp.isValid())
            record.timestamp = info.lastModified();
        records.append(record);
    }

    {
        QMutexLocker locker(&mMutex);
        mRecords = records;
        sortRecordsLocked();
        enforceLimitLocked();
    }
    emit recordsChanged();
}

void DefectImageStore::enforceLimitLocked()
{
    while (mRecords.size() > mConfig.maxImages) {
        DefectImageRecord old = mRecords.last();
        mRecords.removeLast();
        if (!old.filePath.isEmpty() && QFile::exists(old.filePath)) {
            if (!QFile::remove(old.filePath))
                qWarning() << "DefectImageStore: failed to remove old image:" << old.filePath;
        }
    }
}

void DefectImageStore::sortRecordsLocked()
{
    std::sort(mRecords.begin(), mRecords.end(),
              [](const DefectImageRecord& a, const DefectImageRecord& b) {
        qint64 at = a.timestamp.toMSecsSinceEpoch();
        qint64 bt = b.timestamp.toMSecsSinceEpoch();
        if (at != bt)
            return at > bt;
        return a.fileName > b.fileName;
    });
}
