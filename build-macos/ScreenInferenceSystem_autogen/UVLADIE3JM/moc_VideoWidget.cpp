/****************************************************************************
** Meta object code from reading C++ file 'VideoWidget.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.19)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/VideoWidget.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'VideoWidget.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.19. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_VideoWidget_t {
    QByteArrayData data[21];
    char stringdata0[257];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_VideoWidget_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_VideoWidget_t qt_meta_stringdata_VideoWidget = {
    {
QT_MOC_LITERAL(0, 0, 11), // "VideoWidget"
QT_MOC_LITERAL(1, 12, 26), // "confidenceThresholdChanged"
QT_MOC_LITERAL(2, 39, 0), // ""
QT_MOC_LITERAL(3, 40, 8), // "cameraId"
QT_MOC_LITERAL(4, 49, 9), // "threshold"
QT_MOC_LITERAL(5, 59, 7), // "clicked"
QT_MOC_LITERAL(6, 67, 20), // "defectOverlayPainted"
QT_MOC_LITERAL(7, 88, 10), // "confidence"
QT_MOC_LITERAL(8, 99, 18), // "updateDisplayFrame"
QT_MOC_LITERAL(9, 118, 9), // "FrameData"
QT_MOC_LITERAL(10, 128, 5), // "frame"
QT_MOC_LITERAL(11, 134, 22), // "updateDetectionOverlay"
QT_MOC_LITERAL(12, 157, 13), // "DisplayResult"
QT_MOC_LITERAL(13, 171, 6), // "result"
QT_MOC_LITERAL(14, 178, 9), // "updateFps"
QT_MOC_LITERAL(15, 188, 3), // "fps"
QT_MOC_LITERAL(16, 192, 19), // "updateInferenceTime"
QT_MOC_LITERAL(17, 212, 2), // "ms"
QT_MOC_LITERAL(18, 215, 22), // "setConfidenceThreshold"
QT_MOC_LITERAL(19, 238, 12), // "showNoSignal"
QT_MOC_LITERAL(20, 251, 5) // "reset"

    },
    "VideoWidget\0confidenceThresholdChanged\0"
    "\0cameraId\0threshold\0clicked\0"
    "defectOverlayPainted\0confidence\0"
    "updateDisplayFrame\0FrameData\0frame\0"
    "updateDetectionOverlay\0DisplayResult\0"
    "result\0updateFps\0fps\0updateInferenceTime\0"
    "ms\0setConfidenceThreshold\0showNoSignal\0"
    "reset"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_VideoWidget[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      10,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    2,   64,    2, 0x06 /* Public */,
       5,    1,   69,    2, 0x06 /* Public */,
       6,    2,   72,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       8,    1,   77,    2, 0x0a /* Public */,
      11,    1,   80,    2, 0x0a /* Public */,
      14,    1,   83,    2, 0x0a /* Public */,
      16,    1,   86,    2, 0x0a /* Public */,
      18,    1,   89,    2, 0x0a /* Public */,
      19,    0,   92,    2, 0x0a /* Public */,
      20,    0,   93,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int, QMetaType::Float,    3,    4,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::Int, QMetaType::Float,    3,    7,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 9,   10,
    QMetaType::Void, 0x80000000 | 12,   13,
    QMetaType::Void, QMetaType::Double,   15,
    QMetaType::Void, QMetaType::Float,   17,
    QMetaType::Void, QMetaType::Float,    4,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void VideoWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<VideoWidget *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->confidenceThresholdChanged((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< float(*)>(_a[2]))); break;
        case 1: _t->clicked((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 2: _t->defectOverlayPainted((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< float(*)>(_a[2]))); break;
        case 3: _t->updateDisplayFrame((*reinterpret_cast< const FrameData(*)>(_a[1]))); break;
        case 4: _t->updateDetectionOverlay((*reinterpret_cast< const DisplayResult(*)>(_a[1]))); break;
        case 5: _t->updateFps((*reinterpret_cast< double(*)>(_a[1]))); break;
        case 6: _t->updateInferenceTime((*reinterpret_cast< float(*)>(_a[1]))); break;
        case 7: _t->setConfidenceThreshold((*reinterpret_cast< float(*)>(_a[1]))); break;
        case 8: _t->showNoSignal(); break;
        case 9: _t->reset(); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 3:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< FrameData >(); break;
            }
            break;
        case 4:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< DisplayResult >(); break;
            }
            break;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (VideoWidget::*)(int , float );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VideoWidget::confidenceThresholdChanged)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (VideoWidget::*)(int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VideoWidget::clicked)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (VideoWidget::*)(int , float );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VideoWidget::defectOverlayPainted)) {
                *result = 2;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject VideoWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_VideoWidget.data,
    qt_meta_data_VideoWidget,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *VideoWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *VideoWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_VideoWidget.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int VideoWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
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
void VideoWidget::confidenceThresholdChanged(int _t1, float _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void VideoWidget::clicked(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void VideoWidget::defectOverlayPainted(int _t1, float _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
