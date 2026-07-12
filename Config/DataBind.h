#ifndef DATABIND_H
#define DATABIND_H

#include <QObject>
#include <QLineEdit>
#include <QCheckBox>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QList>
#include <QMetaProperty>
#include <QMetaMethod>
#include <QDebug>

namespace Sqz {
namespace DataBind {

// ============================================================================
// 一、核心：通过属性名查找信号
// ============================================================================

/**
 * @brief 通过属性名获取 NOTIFY 信号
 */
template <typename DataType>
QMetaMethod findNotifySignal(DataType* data, const char* propName) {
    const QMetaObject* meta = data->metaObject();
    int idx = meta->indexOfProperty(propName);
    if (idx < 0) {
        qWarning() << "Property not found:" << propName;
        return QMetaMethod();
    }

    QMetaProperty prop = meta->property(idx);
    if (!prop.hasNotifySignal()) {
        qWarning() << "Property has no NOTIFY signal:" << propName;
        return QMetaMethod();
    }

    return prop.notifySignal();
}

/**
 * @brief 通过属性名获取属性类型
 */
template <typename DataType>
QMetaType::Type getPropertyType(DataType* data, const char* propName) {
    const QMetaObject* meta = data->metaObject();
    int idx = meta->indexOfProperty(propName);
    if (idx < 0) return QMetaType::UnknownType;

    QMetaProperty prop = meta->property(idx);
    return static_cast<QMetaType::Type>(prop.type());
}

// ============================================================================
// 二、真正的绑定函数（使用 prop 参数）
// ============================================================================

/**
 * @brief QLineEdit 双向绑定（真正的属性名绑定）
 */
template <typename DataType>
void bind(DataType *data, QLineEdit *widget, const char* prop) {
    if (!data || !widget || !prop) return;

    // 获取属性的 NOTIFY 信号
    QMetaMethod notifySignal = findNotifySignal(data, prop);
    if (!notifySignal.isValid()) return;

    // 数据 → UI：连接 NOTIFY 信号到控件的 setText
    QObject::connect(data, notifySignal, widget, [data, prop, widget]() {
        QVariant value = data->property(prop);
        if (value.canConvert<QString>()) {
            widget->setText(value.toString());
        }
    });

    // UI → 数据：连接控件的 textChanged 信号到数据的 setter
    QObject::connect(widget, &QLineEdit::textChanged, data, [data, prop](const QString& text) {
        data->setProperty(prop, text);
    });
}

/**
 * @brief QCheckBox 双向绑定
 */
template <typename DataType>
void bind(DataType *data, QCheckBox *widget, const char* prop) {
    if (!data || !widget || !prop) return;

    QMetaMethod notifySignal = findNotifySignal(data, prop);
    if (!notifySignal.isValid()) return;

    // 数据 → UI
    QObject::connect(data, notifySignal, widget, [data, prop, widget]() {
        QVariant value = data->property(prop);
        if (value.canConvert<bool>()) {
            widget->setChecked(value.toBool());
        }
    });

    // UI → 数据
    QObject::connect(widget, &QCheckBox::toggled, data, [data, prop](bool checked) {
        data->setProperty(prop, checked);
    });
}

/**
 * @brief QSlider 双向绑定
 */
template <typename DataType>
void bind(DataType *data, QSlider *widget, const char* prop) {
    if (!data || !widget || !prop) return;

    QMetaMethod notifySignal = findNotifySignal(data, prop);
    if (!notifySignal.isValid()) return;

    // 数据 → UI
    QObject::connect(data, notifySignal, widget, [data, prop, widget]() {
        QVariant value = data->property(prop);
        if (value.canConvert<int>()) {
            widget->setValue(value.toInt());
        }
    });

    // UI → 数据
    QObject::connect(widget, &QSlider::valueChanged, data, [data, prop](int value) {
        data->setProperty(prop, value);
    });
}

/**
 * @brief QSpinBox 双向绑定
 */
template <typename DataType>
void bind(DataType *data, QSpinBox *widget, const char* prop) {
    if (!data || !widget || !prop) return;

    QMetaMethod notifySignal = findNotifySignal(data, prop);
    if (!notifySignal.isValid()) return;

    // 数据 → UI
    QObject::connect(data, notifySignal, widget, [data, prop, widget]() {
        QVariant value = data->property(prop);
        if (value.canConvert<int>()) {
            widget->setValue(value.toInt());
        }
    });

    // UI → 数据
    QObject::connect(widget, QOverload<int>::of(&QSpinBox::valueChanged),
                     data, [data, prop](int value) {
        data->setProperty(prop, value);
    });
}

/**
 * @brief QDoubleSpinBox 双向绑定
 */
template <typename DataType>
void bind(DataType *data, QDoubleSpinBox *widget, const char* prop) {
    if (!data || !widget || !prop) return;

    QMetaMethod notifySignal = findNotifySignal(data, prop);
    if (!notifySignal.isValid()) return;

    // 数据 → UI
    QObject::connect(data, notifySignal, widget, [data, prop, widget]() {
        QVariant value = data->property(prop);
        if (value.canConvert<double>()) {
            widget->setValue(value.toDouble());
        }
    });

    // UI → 数据
    QObject::connect(widget, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                     data, [data, prop](double value) {
        data->setProperty(prop, value);
    });
}

/**
 * @brief QComboBox 双向绑定
 */
template <typename DataType>
void bind(DataType *data, QComboBox *widget, const char* prop) {
    if (!data || !widget || !prop) return;

    QMetaMethod notifySignal = findNotifySignal(data, prop);
    if (!notifySignal.isValid()) return;

    // 数据 → UI
    QObject::connect(data, notifySignal, widget, [data, prop, widget]() {
        QVariant value = data->property(prop);
        if (value.canConvert<int>()) {
            widget->setCurrentIndex(value.toInt());
        }
    });

    // UI → 数据
    QObject::connect(widget, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     data, [data, prop](int index) {
        data->setProperty(prop, index);
    });
}

/**
 * @brief QLabel 单向绑定（只读）
 */
template <typename DataType>
void bind(DataType *data, QLabel *widget, const char* prop) {
    if (!data || !widget || !prop) return;

    QMetaMethod notifySignal = findNotifySignal(data, prop);
    if (!notifySignal.isValid()) return;

    // 数据 → UI
    QObject::connect(data, notifySignal, widget, [data, prop, widget]() {
        QVariant value = data->property(prop);
        if (value.canConvert<QString>()) {
            widget->setText(value.toString());
        }
    });
}

/**
 * @brief QProgressBar 单向绑定（只读）
 */
template <typename DataType>
void bind(DataType *data, QProgressBar *widget, const char* prop) {
    if (!data || !widget || !prop) return;

    QMetaMethod notifySignal = findNotifySignal(data, prop);
    if (!notifySignal.isValid()) return;

    // 数据 → UI
    QObject::connect(data, notifySignal, widget, [data, prop, widget]() {
        QVariant value = data->property(prop);
        if (value.canConvert<int>()) {
            widget->setValue(value.toInt());
        }
    });
}

/**
 * @brief QPushButton 启用状态绑定（只读）
 */
template <typename DataType>
void bindEnabled(DataType *data, QPushButton *widget, const char* prop) {
    if (!data || !widget || !prop) return;

    QMetaMethod notifySignal = findNotifySignal(data, prop);
    if (!notifySignal.isValid()) return;

    // 数据 → UI
    QObject::connect(data, notifySignal, widget, [data, prop, widget]() {
        QVariant value = data->property(prop);
        if (value.canConvert<bool>()) {
            widget->setEnabled(value.toBool());
        }
    });
}

// ============================================================================
// 三、兼容旧版：无属性名版本（绑定到默认属性）
// ============================================================================

template <typename DataType>
void bind(DataType *data, QLineEdit *widget) {
    bind(data, widget, "text");
}

template <typename DataType>
void bind(DataType *data, QCheckBox *widget) {
    bind(data, widget, "enabled");
}

template <typename DataType>
void bind(DataType *data, QLabel *widget) {
    bind(data, widget, "text");
}

template <typename DataType>
void bind(DataType *data, QSlider *widget) {
    bind(data, widget, "value");
}

template <typename DataType>
void bind(DataType *data, QSpinBox *widget) {
    bind(data, widget, "value");
}

template <typename DataType>
void bind(DataType *data, QDoubleSpinBox *widget) {
    bind(data, widget, "doubleValue");
}

template <typename DataType>
void bind(DataType *data, QComboBox *widget) {
    bind(data, widget, "index");
}

template <typename DataType>
void bind(DataType *data, QProgressBar *widget) {
    bind(data, widget, "value");
}

template <typename DataType>
void bindEnabled(DataType *data, QPushButton *widget) {
    bindEnabled(data, widget, "buttonEnabled");
}

// ============================================================================
// 四、批量绑定
// ============================================================================

template <typename DataType, typename... Widgets>
void bindAll(DataType *data, Widgets... widgets) {
    (bind(data, widgets), ...);
}

template <typename DataType, typename WidgetType>
void bindAll(DataType *data, const QList<WidgetType*> &widgets) {
    for (WidgetType *w : widgets) {
        bind(data, w);
    }
}

} // namespace DataBind
} // namespace Sqz

#endif // DATABIND_H
