/****************************************************************************
** Meta object code from reading C++ file 'ServiceLauncher.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.19)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/ServiceLauncher.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ServiceLauncher.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.19. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_ServiceLauncher_t {
    QByteArrayData data[14];
    char stringdata0[181];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_ServiceLauncher_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_ServiceLauncher_t qt_meta_stringdata_ServiceLauncher = {
    {
QT_MOC_LITERAL(0, 0, 15), // "ServiceLauncher"
QT_MOC_LITERAL(1, 16, 12), // "serviceError"
QT_MOC_LITERAL(2, 29, 0), // ""
QT_MOC_LITERAL(3, 30, 11), // "serviceName"
QT_MOC_LITERAL(4, 42, 7), // "message"
QT_MOC_LITERAL(5, 50, 16), // "onBridgeFinished"
QT_MOC_LITERAL(6, 67, 8), // "exitCode"
QT_MOC_LITERAL(7, 76, 20), // "QProcess::ExitStatus"
QT_MOC_LITERAL(8, 97, 10), // "exitStatus"
QT_MOC_LITERAL(9, 108, 13), // "onBridgeError"
QT_MOC_LITERAL(10, 122, 22), // "QProcess::ProcessError"
QT_MOC_LITERAL(11, 145, 5), // "error"
QT_MOC_LITERAL(12, 151, 14), // "onBridgeStdout"
QT_MOC_LITERAL(13, 166, 14) // "onBridgeStderr"

    },
    "ServiceLauncher\0serviceError\0\0serviceName\0"
    "message\0onBridgeFinished\0exitCode\0"
    "QProcess::ExitStatus\0exitStatus\0"
    "onBridgeError\0QProcess::ProcessError\0"
    "error\0onBridgeStdout\0onBridgeStderr"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_ServiceLauncher[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    2,   39,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       5,    2,   44,    2, 0x08 /* Private */,
       9,    1,   49,    2, 0x08 /* Private */,
      12,    0,   52,    2, 0x08 /* Private */,
      13,    0,   53,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::QString, QMetaType::QString,    3,    4,

 // slots: parameters
    QMetaType::Void, QMetaType::Int, 0x80000000 | 7,    6,    8,
    QMetaType::Void, 0x80000000 | 10,   11,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void ServiceLauncher::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ServiceLauncher *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->serviceError((*reinterpret_cast< const QString(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        case 1: _t->onBridgeFinished((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< QProcess::ExitStatus(*)>(_a[2]))); break;
        case 2: _t->onBridgeError((*reinterpret_cast< QProcess::ProcessError(*)>(_a[1]))); break;
        case 3: _t->onBridgeStdout(); break;
        case 4: _t->onBridgeStderr(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (ServiceLauncher::*)(const QString & , const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ServiceLauncher::serviceError)) {
                *result = 0;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject ServiceLauncher::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ServiceLauncher.data,
    qt_meta_data_ServiceLauncher,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *ServiceLauncher::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ServiceLauncher::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ServiceLauncher.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int ServiceLauncher::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void ServiceLauncher::serviceError(const QString & _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
