/****************************************************************************
** Meta object code from reading C++ file 'InferenceSubscriber.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.19)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/InferenceSubscriber.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#include <QtCore/QVector>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'InferenceSubscriber.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.19. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_InferenceSubscriber_t {
    QByteArrayData data[17];
    char stringdata0[217];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_InferenceSubscriber_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_InferenceSubscriber_t qt_meta_stringdata_InferenceSubscriber = {
    {
QT_MOC_LITERAL(0, 0, 19), // "InferenceSubscriber"
QT_MOC_LITERAL(1, 20, 17), // "inferenceFinished"
QT_MOC_LITERAL(2, 38, 0), // ""
QT_MOC_LITERAL(3, 39, 15), // "InferenceResult"
QT_MOC_LITERAL(4, 55, 6), // "result"
QT_MOC_LITERAL(5, 62, 18), // "channelsDiscovered"
QT_MOC_LITERAL(6, 81, 20), // "QVector<ChannelInfo>"
QT_MOC_LITERAL(7, 102, 8), // "channels"
QT_MOC_LITERAL(8, 111, 5), // "error"
QT_MOC_LITERAL(9, 117, 7), // "message"
QT_MOC_LITERAL(10, 125, 17), // "onMessageReceived"
QT_MOC_LITERAL(11, 143, 5), // "topic"
QT_MOC_LITERAL(12, 149, 7), // "payload"
QT_MOC_LITERAL(13, 157, 15), // "onMqttConnected"
QT_MOC_LITERAL(14, 173, 18), // "onMqttDisconnected"
QT_MOC_LITERAL(15, 192, 11), // "onMqttError"
QT_MOC_LITERAL(16, 204, 12) // "flushResults"

    },
    "InferenceSubscriber\0inferenceFinished\0"
    "\0InferenceResult\0result\0channelsDiscovered\0"
    "QVector<ChannelInfo>\0channels\0error\0"
    "message\0onMessageReceived\0topic\0payload\0"
    "onMqttConnected\0onMqttDisconnected\0"
    "onMqttError\0flushResults"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_InferenceSubscriber[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       8,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   54,    2, 0x06 /* Public */,
       5,    1,   57,    2, 0x06 /* Public */,
       8,    1,   60,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
      10,    2,   63,    2, 0x08 /* Private */,
      13,    0,   68,    2, 0x08 /* Private */,
      14,    0,   69,    2, 0x08 /* Private */,
      15,    1,   70,    2, 0x08 /* Private */,
      16,    0,   73,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, 0x80000000 | 6,    7,
    QMetaType::Void, QMetaType::QString,    9,

 // slots: parameters
    QMetaType::Void, QMetaType::QString, QMetaType::QByteArray,   11,   12,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    9,
    QMetaType::Void,

       0        // eod
};

void InferenceSubscriber::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<InferenceSubscriber *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->inferenceFinished((*reinterpret_cast< const InferenceResult(*)>(_a[1]))); break;
        case 1: _t->channelsDiscovered((*reinterpret_cast< const QVector<ChannelInfo>(*)>(_a[1]))); break;
        case 2: _t->error((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 3: _t->onMessageReceived((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< const QByteArray(*)>(_a[2]))); break;
        case 4: _t->onMqttConnected(); break;
        case 5: _t->onMqttDisconnected(); break;
        case 6: _t->onMqttError((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 7: _t->flushResults(); break;
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
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QVector<ChannelInfo> >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (InferenceSubscriber::*)(const InferenceResult & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&InferenceSubscriber::inferenceFinished)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (InferenceSubscriber::*)(const QVector<ChannelInfo> & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&InferenceSubscriber::channelsDiscovered)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (InferenceSubscriber::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&InferenceSubscriber::error)) {
                *result = 2;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject InferenceSubscriber::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_InferenceSubscriber.data,
    qt_meta_data_InferenceSubscriber,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *InferenceSubscriber::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *InferenceSubscriber::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_InferenceSubscriber.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int InferenceSubscriber::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void InferenceSubscriber::inferenceFinished(const InferenceResult & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void InferenceSubscriber::channelsDiscovered(const QVector<ChannelInfo> & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void InferenceSubscriber::error(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}
struct qt_meta_stringdata_InferenceSubscriberThread_t {
    QByteArrayData data[10];
    char stringdata0[131];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_InferenceSubscriberThread_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_InferenceSubscriberThread_t qt_meta_stringdata_InferenceSubscriberThread = {
    {
QT_MOC_LITERAL(0, 0, 25), // "InferenceSubscriberThread"
QT_MOC_LITERAL(1, 26, 17), // "inferenceFinished"
QT_MOC_LITERAL(2, 44, 0), // ""
QT_MOC_LITERAL(3, 45, 15), // "InferenceResult"
QT_MOC_LITERAL(4, 61, 6), // "result"
QT_MOC_LITERAL(5, 68, 18), // "channelsDiscovered"
QT_MOC_LITERAL(6, 87, 20), // "QVector<ChannelInfo>"
QT_MOC_LITERAL(7, 108, 8), // "channels"
QT_MOC_LITERAL(8, 117, 5), // "error"
QT_MOC_LITERAL(9, 123, 7) // "message"

    },
    "InferenceSubscriberThread\0inferenceFinished\0"
    "\0InferenceResult\0result\0channelsDiscovered\0"
    "QVector<ChannelInfo>\0channels\0error\0"
    "message"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_InferenceSubscriberThread[] = {

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
       8,    1,   35,    2, 0x06 /* Public */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, 0x80000000 | 6,    7,
    QMetaType::Void, QMetaType::QString,    9,

       0        // eod
};

void InferenceSubscriberThread::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<InferenceSubscriberThread *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->inferenceFinished((*reinterpret_cast< const InferenceResult(*)>(_a[1]))); break;
        case 1: _t->channelsDiscovered((*reinterpret_cast< const QVector<ChannelInfo>(*)>(_a[1]))); break;
        case 2: _t->error((*reinterpret_cast< const QString(*)>(_a[1]))); break;
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
        case 1:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QVector<ChannelInfo> >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (InferenceSubscriberThread::*)(const InferenceResult & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&InferenceSubscriberThread::inferenceFinished)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (InferenceSubscriberThread::*)(const QVector<ChannelInfo> & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&InferenceSubscriberThread::channelsDiscovered)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (InferenceSubscriberThread::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&InferenceSubscriberThread::error)) {
                *result = 2;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject InferenceSubscriberThread::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_InferenceSubscriberThread.data,
    qt_meta_data_InferenceSubscriberThread,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *InferenceSubscriberThread::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *InferenceSubscriberThread::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_InferenceSubscriberThread.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int InferenceSubscriberThread::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void InferenceSubscriberThread::inferenceFinished(const InferenceResult & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void InferenceSubscriberThread::channelsDiscovered(const QVector<ChannelInfo> & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void InferenceSubscriberThread::error(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
