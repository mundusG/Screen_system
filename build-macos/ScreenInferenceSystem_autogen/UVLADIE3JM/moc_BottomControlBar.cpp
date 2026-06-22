/****************************************************************************
** Meta object code from reading C++ file 'BottomControlBar.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.19)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/BottomControlBar.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'BottomControlBar.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.19. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_BottomControlBar_t {
    QByteArrayData data[8];
    char stringdata0[105];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_BottomControlBar_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_BottomControlBar_t qt_meta_stringdata_BottomControlBar = {
    {
QT_MOC_LITERAL(0, 0, 16), // "BottomControlBar"
QT_MOC_LITERAL(1, 17, 15), // "gridModeChanged"
QT_MOC_LITERAL(2, 33, 0), // ""
QT_MOC_LITERAL(3, 34, 4), // "mode"
QT_MOC_LITERAL(4, 39, 17), // "snapshotRequested"
QT_MOC_LITERAL(5, 57, 17), // "fullscreenToggled"
QT_MOC_LITERAL(6, 75, 17), // "settingsRequested"
QT_MOC_LITERAL(7, 93, 11) // "setGridMode"

    },
    "BottomControlBar\0gridModeChanged\0\0"
    "mode\0snapshotRequested\0fullscreenToggled\0"
    "settingsRequested\0setGridMode"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_BottomControlBar[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   39,    2, 0x06 /* Public */,
       4,    0,   42,    2, 0x06 /* Public */,
       5,    0,   43,    2, 0x06 /* Public */,
       6,    0,   44,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       7,    1,   45,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void, QMetaType::Int,    3,

       0        // eod
};

void BottomControlBar::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<BottomControlBar *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->gridModeChanged((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 1: _t->snapshotRequested(); break;
        case 2: _t->fullscreenToggled(); break;
        case 3: _t->settingsRequested(); break;
        case 4: _t->setGridMode((*reinterpret_cast< int(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (BottomControlBar::*)(int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&BottomControlBar::gridModeChanged)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (BottomControlBar::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&BottomControlBar::snapshotRequested)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (BottomControlBar::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&BottomControlBar::fullscreenToggled)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (BottomControlBar::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&BottomControlBar::settingsRequested)) {
                *result = 3;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject BottomControlBar::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_BottomControlBar.data,
    qt_meta_data_BottomControlBar,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *BottomControlBar::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *BottomControlBar::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_BottomControlBar.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int BottomControlBar::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
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
void BottomControlBar::gridModeChanged(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void BottomControlBar::snapshotRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void BottomControlBar::fullscreenToggled()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void BottomControlBar::settingsRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
