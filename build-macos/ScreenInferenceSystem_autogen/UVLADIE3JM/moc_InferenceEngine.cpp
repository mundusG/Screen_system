/****************************************************************************
** Meta object code from reading C++ file 'InferenceEngine.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.19)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/InferenceEngine.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'InferenceEngine.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.19. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_InferenceWorker_t {
    QByteArrayData data[14];
    char stringdata0[146];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_InferenceWorker_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_InferenceWorker_t qt_meta_stringdata_InferenceWorker = {
    {
QT_MOC_LITERAL(0, 0, 15), // "InferenceWorker"
QT_MOC_LITERAL(1, 16, 17), // "inferenceFinished"
QT_MOC_LITERAL(2, 34, 0), // ""
QT_MOC_LITERAL(3, 35, 15), // "InferenceResult"
QT_MOC_LITERAL(4, 51, 6), // "result"
QT_MOC_LITERAL(5, 58, 5), // "error"
QT_MOC_LITERAL(6, 64, 7), // "message"
QT_MOC_LITERAL(7, 72, 12), // "runInference"
QT_MOC_LITERAL(8, 85, 9), // "FrameData"
QT_MOC_LITERAL(9, 95, 5), // "frame"
QT_MOC_LITERAL(10, 101, 11), // "reloadModel"
QT_MOC_LITERAL(11, 113, 9), // "modelPath"
QT_MOC_LITERAL(12, 123, 10), // "inputWidth"
QT_MOC_LITERAL(13, 134, 11) // "inputHeight"

    },
    "InferenceWorker\0inferenceFinished\0\0"
    "InferenceResult\0result\0error\0message\0"
    "runInference\0FrameData\0frame\0reloadModel\0"
    "modelPath\0inputWidth\0inputHeight"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_InferenceWorker[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   34,    2, 0x06 /* Public */,
       5,    1,   37,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       7,    1,   40,    2, 0x0a /* Public */,
      10,    3,   43,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, QMetaType::QString,    6,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 8,    9,
    QMetaType::Void, QMetaType::QString, QMetaType::Int, QMetaType::Int,   11,   12,   13,

       0        // eod
};

void InferenceWorker::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<InferenceWorker *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->inferenceFinished((*reinterpret_cast< const InferenceResult(*)>(_a[1]))); break;
        case 1: _t->error((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 2: _t->runInference((*reinterpret_cast< const FrameData(*)>(_a[1]))); break;
        case 3: _t->reloadModel((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])),(*reinterpret_cast< int(*)>(_a[3]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< InferenceResult >(); break;
            }
            break;
        case 2:
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
            using _t = void (InferenceWorker::*)(const InferenceResult & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&InferenceWorker::inferenceFinished)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (InferenceWorker::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&InferenceWorker::error)) {
                *result = 1;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject InferenceWorker::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_InferenceWorker.data,
    qt_meta_data_InferenceWorker,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *InferenceWorker::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *InferenceWorker::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_InferenceWorker.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int InferenceWorker::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
void InferenceWorker::inferenceFinished(const InferenceResult & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void InferenceWorker::error(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
struct qt_meta_stringdata_InferenceEngine_t {
    QByteArrayData data[14];
    char stringdata0[157];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_InferenceEngine_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_InferenceEngine_t qt_meta_stringdata_InferenceEngine = {
    {
QT_MOC_LITERAL(0, 0, 15), // "InferenceEngine"
QT_MOC_LITERAL(1, 16, 17), // "inferenceFinished"
QT_MOC_LITERAL(2, 34, 0), // ""
QT_MOC_LITERAL(3, 35, 15), // "InferenceResult"
QT_MOC_LITERAL(4, 51, 6), // "result"
QT_MOC_LITERAL(5, 58, 5), // "error"
QT_MOC_LITERAL(6, 64, 7), // "message"
QT_MOC_LITERAL(7, 72, 16), // "requestInference"
QT_MOC_LITERAL(8, 89, 9), // "FrameData"
QT_MOC_LITERAL(9, 99, 5), // "frame"
QT_MOC_LITERAL(10, 105, 18), // "requestReloadModel"
QT_MOC_LITERAL(11, 124, 9), // "modelPath"
QT_MOC_LITERAL(12, 134, 10), // "inputWidth"
QT_MOC_LITERAL(13, 145, 11) // "inputHeight"

    },
    "InferenceEngine\0inferenceFinished\0\0"
    "InferenceResult\0result\0error\0message\0"
    "requestInference\0FrameData\0frame\0"
    "requestReloadModel\0modelPath\0inputWidth\0"
    "inputHeight"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_InferenceEngine[] = {

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
       7,    1,   40,    2, 0x06 /* Public */,
      10,    3,   43,    2, 0x06 /* Public */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, QMetaType::QString,    6,
    QMetaType::Void, 0x80000000 | 8,    9,
    QMetaType::Void, QMetaType::QString, QMetaType::Int, QMetaType::Int,   11,   12,   13,

       0        // eod
};

void InferenceEngine::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<InferenceEngine *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->inferenceFinished((*reinterpret_cast< const InferenceResult(*)>(_a[1]))); break;
        case 1: _t->error((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 2: _t->requestInference((*reinterpret_cast< const FrameData(*)>(_a[1]))); break;
        case 3: _t->requestReloadModel((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])),(*reinterpret_cast< int(*)>(_a[3]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< InferenceResult >(); break;
            }
            break;
        case 2:
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
            using _t = void (InferenceEngine::*)(const InferenceResult & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&InferenceEngine::inferenceFinished)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (InferenceEngine::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&InferenceEngine::error)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (InferenceEngine::*)(const FrameData & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&InferenceEngine::requestInference)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (InferenceEngine::*)(const QString & , int , int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&InferenceEngine::requestReloadModel)) {
                *result = 3;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject InferenceEngine::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_InferenceEngine.data,
    qt_meta_data_InferenceEngine,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *InferenceEngine::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *InferenceEngine::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_InferenceEngine.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int InferenceEngine::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
void InferenceEngine::inferenceFinished(const InferenceResult & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void InferenceEngine::error(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void InferenceEngine::requestInference(const FrameData & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void InferenceEngine::requestReloadModel(const QString & _t1, int _t2, int _t3)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
