/****************************************************************************
** Meta object code from reading C++ file 'ImageStreamSource.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.19)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/ImageStreamSource.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ImageStreamSource.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.19. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_ImageStreamSource_t {
    QByteArrayData data[19];
    char stringdata0[193];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_ImageStreamSource_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_ImageStreamSource_t qt_meta_stringdata_ImageStreamSource = {
    {
QT_MOC_LITERAL(0, 0, 17), // "ImageStreamSource"
QT_MOC_LITERAL(1, 18, 17), // "displayFrameReady"
QT_MOC_LITERAL(2, 36, 0), // ""
QT_MOC_LITERAL(3, 37, 9), // "FrameData"
QT_MOC_LITERAL(4, 47, 5), // "frame"
QT_MOC_LITERAL(5, 53, 5), // "error"
QT_MOC_LITERAL(6, 59, 7), // "message"
QT_MOC_LITERAL(7, 67, 10), // "fpsUpdated"
QT_MOC_LITERAL(8, 78, 8), // "cameraId"
QT_MOC_LITERAL(9, 87, 3), // "fps"
QT_MOC_LITERAL(10, 91, 11), // "startStream"
QT_MOC_LITERAL(11, 103, 10), // "stopStream"
QT_MOC_LITERAL(12, 114, 12), // "requestStart"
QT_MOC_LITERAL(13, 127, 3), // "url"
QT_MOC_LITERAL(14, 131, 11), // "requestStop"
QT_MOC_LITERAL(15, 143, 8), // "fetchOne"
QT_MOC_LITERAL(16, 152, 19), // "onHttpReplyFinished"
QT_MOC_LITERAL(17, 172, 14), // "QNetworkReply*"
QT_MOC_LITERAL(18, 187, 5) // "reply"

    },
    "ImageStreamSource\0displayFrameReady\0"
    "\0FrameData\0frame\0error\0message\0"
    "fpsUpdated\0cameraId\0fps\0startStream\0"
    "stopStream\0requestStart\0url\0requestStop\0"
    "fetchOne\0onHttpReplyFinished\0"
    "QNetworkReply*\0reply"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_ImageStreamSource[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       9,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   59,    2, 0x06 /* Public */,
       5,    1,   62,    2, 0x06 /* Public */,
       7,    2,   65,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
      10,    0,   70,    2, 0x0a /* Public */,
      11,    0,   71,    2, 0x0a /* Public */,
      12,    1,   72,    2, 0x0a /* Public */,
      14,    0,   75,    2, 0x0a /* Public */,
      15,    0,   76,    2, 0x0a /* Public */,
      16,    1,   77,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, QMetaType::QString,    6,
    QMetaType::Void, QMetaType::Int, QMetaType::Double,    8,    9,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   13,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 17,   18,

       0        // eod
};

void ImageStreamSource::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ImageStreamSource *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->displayFrameReady((*reinterpret_cast< const FrameData(*)>(_a[1]))); break;
        case 1: _t->error((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 2: _t->fpsUpdated((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< double(*)>(_a[2]))); break;
        case 3: _t->startStream(); break;
        case 4: _t->stopStream(); break;
        case 5: _t->requestStart((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 6: _t->requestStop(); break;
        case 7: _t->fetchOne(); break;
        case 8: _t->onHttpReplyFinished((*reinterpret_cast< QNetworkReply*(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (ImageStreamSource::*)(const FrameData & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ImageStreamSource::displayFrameReady)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (ImageStreamSource::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ImageStreamSource::error)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (ImageStreamSource::*)(int , double );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ImageStreamSource::fpsUpdated)) {
                *result = 2;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject ImageStreamSource::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ImageStreamSource.data,
    qt_meta_data_ImageStreamSource,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *ImageStreamSource::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ImageStreamSource::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ImageStreamSource.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ImageStreamSource::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 9;
    }
    return _id;
}

// SIGNAL 0
void ImageStreamSource::displayFrameReady(const FrameData & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void ImageStreamSource::error(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void ImageStreamSource::fpsUpdated(int _t1, double _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}
struct qt_meta_stringdata_ImageStreamThread_t {
    QByteArrayData data[10];
    char stringdata0[91];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_ImageStreamThread_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_ImageStreamThread_t qt_meta_stringdata_ImageStreamThread = {
    {
QT_MOC_LITERAL(0, 0, 17), // "ImageStreamThread"
QT_MOC_LITERAL(1, 18, 17), // "displayFrameReady"
QT_MOC_LITERAL(2, 36, 0), // ""
QT_MOC_LITERAL(3, 37, 9), // "FrameData"
QT_MOC_LITERAL(4, 47, 5), // "frame"
QT_MOC_LITERAL(5, 53, 5), // "error"
QT_MOC_LITERAL(6, 59, 7), // "message"
QT_MOC_LITERAL(7, 67, 10), // "fpsUpdated"
QT_MOC_LITERAL(8, 78, 8), // "cameraId"
QT_MOC_LITERAL(9, 87, 3) // "fps"

    },
    "ImageStreamThread\0displayFrameReady\0"
    "\0FrameData\0frame\0error\0message\0"
    "fpsUpdated\0cameraId\0fps"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_ImageStreamThread[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       3,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   29,    2, 0x06 /* Public */,
       5,    1,   32,    2, 0x06 /* Public */,
       7,    2,   35,    2, 0x06 /* Public */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, QMetaType::QString,    6,
    QMetaType::Void, QMetaType::Int, QMetaType::Double,    8,    9,

       0        // eod
};

void ImageStreamThread::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ImageStreamThread *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->displayFrameReady((*reinterpret_cast< const FrameData(*)>(_a[1]))); break;
        case 1: _t->error((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 2: _t->fpsUpdated((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< double(*)>(_a[2]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (ImageStreamThread::*)(const FrameData & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ImageStreamThread::displayFrameReady)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (ImageStreamThread::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ImageStreamThread::error)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (ImageStreamThread::*)(int , double );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ImageStreamThread::fpsUpdated)) {
                *result = 2;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject ImageStreamThread::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ImageStreamThread.data,
    qt_meta_data_ImageStreamThread,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *ImageStreamThread::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ImageStreamThread::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ImageStreamThread.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ImageStreamThread::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void ImageStreamThread::displayFrameReady(const FrameData & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void ImageStreamThread::error(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void ImageStreamThread::fpsUpdated(int _t1, double _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
