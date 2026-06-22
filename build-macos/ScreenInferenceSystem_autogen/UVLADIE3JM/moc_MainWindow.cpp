/****************************************************************************
** Meta object code from reading C++ file 'MainWindow.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.19)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/MainWindow.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#include <QtCore/QVector>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MainWindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.19. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_MainWindow_t {
    QByteArrayData data[31];
    char stringdata0[420];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_MainWindow_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_MainWindow_t qt_meta_stringdata_MainWindow = {
    {
QT_MOC_LITERAL(0, 0, 10), // "MainWindow"
QT_MOC_LITERAL(1, 11, 8), // "startAll"
QT_MOC_LITERAL(2, 20, 0), // ""
QT_MOC_LITERAL(3, 21, 7), // "stopAll"
QT_MOC_LITERAL(4, 29, 12), // "openSettings"
QT_MOC_LITERAL(5, 42, 12), // "toggleCamera"
QT_MOC_LITERAL(6, 55, 8), // "cameraId"
QT_MOC_LITERAL(7, 64, 28), // "onConfidenceThresholdChanged"
QT_MOC_LITERAL(8, 93, 9), // "threshold"
QT_MOC_LITERAL(9, 103, 19), // "onDisplayFrameReady"
QT_MOC_LITERAL(10, 123, 9), // "FrameData"
QT_MOC_LITERAL(11, 133, 5), // "frame"
QT_MOC_LITERAL(12, 139, 21), // "onInferenceFrameReady"
QT_MOC_LITERAL(13, 161, 19), // "onInferenceFinished"
QT_MOC_LITERAL(14, 181, 15), // "InferenceResult"
QT_MOC_LITERAL(15, 197, 6), // "result"
QT_MOC_LITERAL(16, 204, 20), // "onDisplayResultReady"
QT_MOC_LITERAL(17, 225, 13), // "DisplayResult"
QT_MOC_LITERAL(18, 239, 12), // "onFpsUpdated"
QT_MOC_LITERAL(19, 252, 3), // "fps"
QT_MOC_LITERAL(20, 256, 13), // "onCameraError"
QT_MOC_LITERAL(21, 270, 7), // "message"
QT_MOC_LITERAL(22, 278, 15), // "onCameraClicked"
QT_MOC_LITERAL(23, 294, 17), // "onGridModeChanged"
QT_MOC_LITERAL(24, 312, 4), // "mode"
QT_MOC_LITERAL(25, 317, 18), // "onToggleFullscreen"
QT_MOC_LITERAL(26, 336, 19), // "onSnapshotRequested"
QT_MOC_LITERAL(27, 356, 20), // "onChannelsDiscovered"
QT_MOC_LITERAL(28, 377, 20), // "QVector<ChannelInfo>"
QT_MOC_LITERAL(29, 398, 8), // "channels"
QT_MOC_LITERAL(30, 407, 12) // "mqttSourceId"

    },
    "MainWindow\0startAll\0\0stopAll\0openSettings\0"
    "toggleCamera\0cameraId\0"
    "onConfidenceThresholdChanged\0threshold\0"
    "onDisplayFrameReady\0FrameData\0frame\0"
    "onInferenceFrameReady\0onInferenceFinished\0"
    "InferenceResult\0result\0onDisplayResultReady\0"
    "DisplayResult\0onFpsUpdated\0fps\0"
    "onCameraError\0message\0onCameraClicked\0"
    "onGridModeChanged\0mode\0onToggleFullscreen\0"
    "onSnapshotRequested\0onChannelsDiscovered\0"
    "QVector<ChannelInfo>\0channels\0"
    "mqttSourceId"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_MainWindow[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      16,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   94,    2, 0x0a /* Public */,
       3,    0,   95,    2, 0x0a /* Public */,
       4,    0,   96,    2, 0x0a /* Public */,
       5,    1,   97,    2, 0x0a /* Public */,
       7,    2,  100,    2, 0x08 /* Private */,
       9,    1,  105,    2, 0x08 /* Private */,
      12,    1,  108,    2, 0x08 /* Private */,
      13,    1,  111,    2, 0x08 /* Private */,
      16,    1,  114,    2, 0x08 /* Private */,
      18,    2,  117,    2, 0x08 /* Private */,
      20,    1,  122,    2, 0x08 /* Private */,
      22,    1,  125,    2, 0x08 /* Private */,
      23,    1,  128,    2, 0x08 /* Private */,
      25,    0,  131,    2, 0x08 /* Private */,
      26,    0,  132,    2, 0x08 /* Private */,
      27,    2,  133,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    6,
    QMetaType::Void, QMetaType::Int, QMetaType::Float,    6,    8,
    QMetaType::Void, 0x80000000 | 10,   11,
    QMetaType::Void, 0x80000000 | 10,   11,
    QMetaType::Void, 0x80000000 | 14,   15,
    QMetaType::Void, 0x80000000 | 17,   15,
    QMetaType::Void, QMetaType::Int, QMetaType::Double,    6,   19,
    QMetaType::Void, QMetaType::QString,   21,
    QMetaType::Void, QMetaType::Int,    6,
    QMetaType::Void, QMetaType::Int,   24,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 28, QMetaType::QString,   29,   30,

       0        // eod
};

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MainWindow *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->startAll(); break;
        case 1: _t->stopAll(); break;
        case 2: _t->openSettings(); break;
        case 3: _t->toggleCamera((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 4: _t->onConfidenceThresholdChanged((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< float(*)>(_a[2]))); break;
        case 5: _t->onDisplayFrameReady((*reinterpret_cast< const FrameData(*)>(_a[1]))); break;
        case 6: _t->onInferenceFrameReady((*reinterpret_cast< const FrameData(*)>(_a[1]))); break;
        case 7: _t->onInferenceFinished((*reinterpret_cast< const InferenceResult(*)>(_a[1]))); break;
        case 8: _t->onDisplayResultReady((*reinterpret_cast< const DisplayResult(*)>(_a[1]))); break;
        case 9: _t->onFpsUpdated((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< double(*)>(_a[2]))); break;
        case 10: _t->onCameraError((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 11: _t->onCameraClicked((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 12: _t->onGridModeChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 13: _t->onToggleFullscreen(); break;
        case 14: _t->onSnapshotRequested(); break;
        case 15: _t->onChannelsDiscovered((*reinterpret_cast< const QVector<ChannelInfo>(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 5:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< FrameData >(); break;
            }
            break;
        case 6:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< FrameData >(); break;
            }
            break;
        case 7:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< InferenceResult >(); break;
            }
            break;
        case 8:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< DisplayResult >(); break;
            }
            break;
        case 15:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QVector<ChannelInfo> >(); break;
            }
            break;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_MainWindow.data,
    qt_meta_data_MainWindow,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_MainWindow.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 16)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 16;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 16)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 16;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
