/****************************************************************************
** Meta object code from reading C++ file 'CameraCapture.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.19)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/CameraCapture.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'CameraCapture.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.19. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_CameraCapture_t {
    QByteArrayData data[18];
    char stringdata0[189];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_CameraCapture_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_CameraCapture_t qt_meta_stringdata_CameraCapture = {
    {
QT_MOC_LITERAL(0, 0, 13), // "CameraCapture"
QT_MOC_LITERAL(1, 14, 17), // "displayFrameReady"
QT_MOC_LITERAL(2, 32, 0), // ""
QT_MOC_LITERAL(3, 33, 9), // "FrameData"
QT_MOC_LITERAL(4, 43, 5), // "frame"
QT_MOC_LITERAL(5, 49, 19), // "inferenceFrameReady"
QT_MOC_LITERAL(6, 69, 5), // "error"
QT_MOC_LITERAL(7, 75, 7), // "message"
QT_MOC_LITERAL(8, 83, 10), // "fpsUpdated"
QT_MOC_LITERAL(9, 94, 8), // "cameraId"
QT_MOC_LITERAL(10, 103, 3), // "fps"
QT_MOC_LITERAL(11, 107, 12), // "startCapture"
QT_MOC_LITERAL(12, 120, 11), // "stopCapture"
QT_MOC_LITERAL(13, 132, 10), // "captureOne"
QT_MOC_LITERAL(14, 143, 11), // "requestStop"
QT_MOC_LITERAL(15, 155, 12), // "requestStart"
QT_MOC_LITERAL(16, 168, 6), // "source"
QT_MOC_LITERAL(17, 175, 13) // "frameConsumed"

    },
    "CameraCapture\0displayFrameReady\0\0"
    "FrameData\0frame\0inferenceFrameReady\0"
    "error\0message\0fpsUpdated\0cameraId\0fps\0"
    "startCapture\0stopCapture\0captureOne\0"
    "requestStop\0requestStart\0source\0"
    "frameConsumed"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_CameraCapture[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      10,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   64,    2, 0x06 /* Public */,
       5,    1,   67,    2, 0x06 /* Public */,
       6,    1,   70,    2, 0x06 /* Public */,
       8,    2,   73,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
      11,    0,   78,    2, 0x0a /* Public */,
      12,    0,   79,    2, 0x0a /* Public */,
      13,    0,   80,    2, 0x0a /* Public */,
      14,    0,   81,    2, 0x0a /* Public */,
      15,    1,   82,    2, 0x0a /* Public */,
      17,    0,   85,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, QMetaType::QString,    7,
    QMetaType::Void, QMetaType::Int, QMetaType::Double,    9,   10,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,   16,
    QMetaType::Void,

       0        // eod
};

void CameraCapture::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<CameraCapture *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->displayFrameReady((*reinterpret_cast< const FrameData(*)>(_a[1]))); break;
        case 1: _t->inferenceFrameReady((*reinterpret_cast< const FrameData(*)>(_a[1]))); break;
        case 2: _t->error((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 3: _t->fpsUpdated((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< double(*)>(_a[2]))); break;
        case 4: _t->startCapture(); break;
        case 5: _t->stopCapture(); break;
        case 6: _t->captureOne(); break;
        case 7: _t->requestStop(); break;
        case 8: _t->requestStart((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 9: _t->frameConsumed(); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< FrameData >(); break;
            }
            break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< FrameData >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (CameraCapture::*)(const FrameData & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CameraCapture::displayFrameReady)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (CameraCapture::*)(const FrameData & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CameraCapture::inferenceFrameReady)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (CameraCapture::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CameraCapture::error)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (CameraCapture::*)(int , double );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CameraCapture::fpsUpdated)) {
                *result = 3;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject CameraCapture::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CameraCapture.data,
    qt_meta_data_CameraCapture,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *CameraCapture::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *CameraCapture::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CameraCapture.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int CameraCapture::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    return _id;
}

// SIGNAL 0
void CameraCapture::displayFrameReady(const FrameData & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void CameraCapture::inferenceFrameReady(const FrameData & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void CameraCapture::error(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void CameraCapture::fpsUpdated(int _t1, double _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
struct qt_meta_stringdata_CameraThread_t {
    QByteArrayData data[11];
    char stringdata0[106];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_CameraThread_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_CameraThread_t qt_meta_stringdata_CameraThread = {
    {
QT_MOC_LITERAL(0, 0, 12), // "CameraThread"
QT_MOC_LITERAL(1, 13, 17), // "displayFrameReady"
QT_MOC_LITERAL(2, 31, 0), // ""
QT_MOC_LITERAL(3, 32, 9), // "FrameData"
QT_MOC_LITERAL(4, 42, 5), // "frame"
QT_MOC_LITERAL(5, 48, 19), // "inferenceFrameReady"
QT_MOC_LITERAL(6, 68, 5), // "error"
QT_MOC_LITERAL(7, 74, 7), // "message"
QT_MOC_LITERAL(8, 82, 10), // "fpsUpdated"
QT_MOC_LITERAL(9, 93, 8), // "cameraId"
QT_MOC_LITERAL(10, 102, 3) // "fps"

    },
    "CameraThread\0displayFrameReady\0\0"
    "FrameData\0frame\0inferenceFrameReady\0"
    "error\0message\0fpsUpdated\0cameraId\0fps"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_CameraThread[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   34,    2, 0x06 /* Public */,
       5,    1,   37,    2, 0x06 /* Public */,
       6,    1,   40,    2, 0x06 /* Public */,
       8,    2,   43,    2, 0x06 /* Public */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, QMetaType::QString,    7,
    QMetaType::Void, QMetaType::Int, QMetaType::Double,    9,   10,

       0        // eod
};

void CameraThread::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<CameraThread *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->displayFrameReady((*reinterpret_cast< const FrameData(*)>(_a[1]))); break;
        case 1: _t->inferenceFrameReady((*reinterpret_cast< const FrameData(*)>(_a[1]))); break;
        case 2: _t->error((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 3: _t->fpsUpdated((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< double(*)>(_a[2]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< FrameData >(); break;
            }
            break;
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< FrameData >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (CameraThread::*)(const FrameData & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CameraThread::displayFrameReady)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (CameraThread::*)(const FrameData & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CameraThread::inferenceFrameReady)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (CameraThread::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CameraThread::error)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (CameraThread::*)(int , double );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&CameraThread::fpsUpdated)) {
                *result = 3;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject CameraThread::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_CameraThread.data,
    qt_meta_data_CameraThread,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *CameraThread::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *CameraThread::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CameraThread.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int CameraThread::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void CameraThread::displayFrameReady(const FrameData & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void CameraThread::inferenceFrameReady(const FrameData & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void CameraThread::error(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void CameraThread::fpsUpdated(int _t1, double _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
