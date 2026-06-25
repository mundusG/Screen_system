#ifndef DEFECTIMAGESTORE_H
#define DEFECTIMAGESTORE_H

#include <QObject>
#include <QDateTime>
#include <QImage>
#include <QMap>
#include <QMutex>
#include <QSize>
#include <QVector>
#include "Types.h"

struct DefectImageRecord {
    QString filePath;
    QString fileName;
    int cameraId = -1;
    QString location;
    QDateTime timestamp;
    float confidence = 0.0f;
    QSize imageSize;
};

class DefectImageStore : public QObject
{
    Q_OBJECT

public:
    explicit DefectImageStore(QObject* parent = nullptr);

    bool initialize(const QString& configPath = QString());

    int count() const;
    int pageSize() const;
    void setPageSize(int pageSize);
    int pageCount(int pageSize = -1) const;
    QVector<DefectImageRecord> recordsForPage(int pageIndex, int pageSize = -1) const;

    QString locationName(int cameraId, const QString& fallbackName = QString()) const;

    void saveDefectImage(int cameraId,
                         const QString& fallbackName,
                         const QImage& frame,
                         const QVector<Detection>& detections,
                         float confidence,
                         qint64 timestampMs = 0);

    void onAsyncImageSaved(const DefectImageRecord& record);
    void onAsyncSaveFailed(const QString& path, const QString& reason);

signals:
    void recordsChanged();

private:
    struct Config {
        QString storageDir;
        int maxImages = 500;
        int pageSize = 20;
        QMap<int, QString> locations;
    };

    static QString resolveConfigPath(const QString& explicitPath = QString());
    static QString defaultStorageDir();
    static QString sanitizeFilePart(const QString& text);
    static QString makeUniquePath(const QString& dir, const QString& baseName);
    static QDateTime parseTimestamp(const QString& value);

    bool loadConfig(const QString& path);
    void applyDefaults();
    void scanStorageDir();
    void enforceLimitLocked();
    void sortRecordsLocked();

    mutable QMutex mMutex;
    Config mConfig;
    QVector<DefectImageRecord> mRecords;
    int mSaveSequence = 0;
    bool mInitialized = false;
};

#endif // DEFECTIMAGESTORE_H
