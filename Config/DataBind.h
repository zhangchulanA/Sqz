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
#include <QPointer>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QRadioButton>
#include <QHash>
#include <QPair>

namespace Sqz {

// 绑定上下文：记录绑定关系，用于解绑、防重复绑定
struct BindKey
{
    QObject* data;
    QObject* widget;
    QByteArray prop;

    bool operator==(const BindKey& other) const
    {
        return data == other.data && widget == other.widget && prop == other.prop;
    }
};
inline uint qHash(const BindKey& key, uint seed = 0)
{
    return qHash(qMakePair(qHash(key.data, seed), qHash(key.widget, seed)), seed) ^ qHash(key.prop, seed);
}

namespace Internal {
// 全局绑定记录表，用于防重复绑定、统一解绑
static QHash<BindKey, QList<QMetaObject::Connection>> g_bindConnections;

// 编译期校验：类型必须继承 QObject
template<typename T>
constexpr bool IsQObjectDerived = std::is_base_of_v<QObject, T>;

// 安全获取属性Notify信号
template <typename DataType>
QMetaMethod findNotifySignal(DataType* data, const char* propName)
{
    static_assert(IsQObjectDerived<DataType>, "DataType must inherit QObject and use Q_OBJECT");
    if (!data || !propName)
    {
        qWarning() << "[DataBind] data or propName is null";
        return QMetaMethod();
    }
    const QMetaObject* meta = data->metaObject();
    int idx = meta->indexOfProperty(propName);
    if (idx < 0)
    {
        qWarning() << "[DataBind] Property not found:" << propName << " on object:" << data;
        return QMetaMethod();
    }
    QMetaProperty prop = meta->property(idx);
    if (!prop.hasNotifySignal())
    {
        qWarning() << "[DataBind] Property has no NOTIFY signal:" << propName;
        return QMetaMethod();
    }
    return prop.notifySignal();
}

// 获取属性元类型
template <typename DataType>
QMetaType::Type getPropertyType(DataType* data, const char* propName)
{
    static_assert(IsQObjectDerived<DataType>, "DataType must inherit QObject and use Q_OBJECT");
    if (!data || !propName) return QMetaType::UnknownType;
    const QMetaObject* meta = data->metaObject();
    int idx = meta->indexOfProperty(propName);
    if (idx < 0) return QMetaType::UnknownType;
    QMetaProperty prop = meta->property(idx);
    return static_cast<QMetaType::Type>(prop.type());
}

// 检查该绑定是否已存在（防重复绑定）
inline bool isBindExists(QObject* data, QObject* widget, const QByteArray& prop)
{
    BindKey key{data, widget, prop};
    return g_bindConnections.contains(key) && !g_bindConnections[key].isEmpty();
}

// 保存连接到全局记录表
inline void storeConnection(QObject* data, QObject* widget, const QByteArray& prop, QMetaObject::Connection conn)
{
    BindKey key{data, widget, prop};
    g_bindConnections[key].append(conn);
}

// 清理单个绑定
inline void unbindSingle(QObject* data, QObject* widget, const QByteArray& prop)
{
    BindKey key{data, widget, prop};
    if (!g_bindConnections.contains(key)) return;
    auto& conns = g_bindConnections[key];
    for (auto& c : conns)
    {
        QObject::disconnect(c);
    }
    conns.clear();
    g_bindConnections.remove(key);
}

// 数值安全截断到控件范围
inline int clampInt(int val, int min, int max)
{
    return val < min ? min : (val > max ? max : val);
}
inline double clampDouble(double val, double min, double max)
{
    return val < min ? min : (val > max ? max : val);
}

// 双向绑定同步锁基类（Model 需要持有 bool _syncLock = false;）
#define DATABIND_SYNC_LOCK() bool syncLock = false;
#define DATABIND_LOCK() if (syncLock) return; syncLock = true;
#define DATABIND_UNLOCK() syncLock = false;

} // namespace Internal

// ===================== 解绑接口 =====================
/**
 * @brief 解绑单个 data-widget-prop 绑定
 */
template<typename DataType>
void unbind(DataType* data, QWidget* widget, const char* prop)
{
    if (!data || !widget || !prop) return;
    Internal::unbindSingle(data, widget, QByteArray(prop));
}

/**
 * @brief 解绑 data 上所有绑定
 */
template<typename DataType>
void unbindAll(DataType* data)
{
    QList<BindKey> toRemove;
    for (auto it = Internal::g_bindConnections.begin(); it != Internal::g_bindConnections.end(); ++it)
    {
        if (it.key().data == data)
        {
            for (auto& c : it.value()) QObject::disconnect(c);
            toRemove.append(it.key());
        }
    }
    for (auto& k : toRemove) Internal::g_bindConnections.remove(k);
}

/**
 * @brief 解绑某个控件所有绑定
 */
inline void unbindWidget(QWidget* widget)
{
    QList<BindKey> toRemove;
    for (auto it = Internal::g_bindConnections.begin(); it != Internal::g_bindConnections.end(); ++it)
    {
        if (it.key().widget == widget)
        {
            for (auto& c : it.value()) QObject::disconnect(c);
            toRemove.append(it.key());
        }
    }
    for (auto& k : toRemove) Internal::g_bindConnections.remove(k);
}

// ===================== QLineEdit 双向绑定 =====================
template <typename DataType>
void bind(DataType *data, QLineEdit *widget, const char* prop)
{
    using namespace Internal;
    static_assert(IsQObjectDerived<DataType>, "DataType must inherit QObject");
    if (!data || !widget || !prop) return;
    QByteArray propBa(prop);
    if (isBindExists(data, widget, propBa))
    {
        qWarning() << "[DataBind] Duplicate bind skipped, prop:" << prop;
        return;
    }
    QMetaMethod notifySignal = findNotifySignal(data, prop);
    if (!notifySignal.isValid()) return;

    QPointer<DataType> dataPtr = data;
    QPointer<QLineEdit> wPtr = widget;

    // 数据 -> UI
    auto conn1 = QObject::connect(data, notifySignal, widget, [dataPtr, propBa, wPtr]() {
        if (!dataPtr || !wPtr) return;
        QVariant value = dataPtr->property(propBa.constData());
        QString txt;
        if (value.canConvert<QString>())
        {
            txt = value.toString();
        }
        else
        {
            qWarning() << "[DataBind] Type convert fail, prop:" << propBa << " value:" << value;
        }
        bool syncLock = false;
        if (syncLock) return;
        syncLock = true;
        wPtr->setText(txt);
        syncLock = false;
    }, Qt::QueuedConnection);
    storeConnection(data, widget, propBa, conn1);

    // UI -> 数据
    auto conn2 = QObject::connect(widget, &QLineEdit::textChanged, data, [dataPtr, propBa](const QString& text) {
        if (!dataPtr) return;
        bool syncLock = false;
        if (syncLock) return;
        syncLock = true;
        dataPtr->setProperty(propBa.constData(), text);
        syncLock = false;
    }, Qt::QueuedConnection);
    storeConnection(data, widget, propBa, conn2);
}

// ===================== QTextEdit 双向绑定 =====================
template <typename DataType>
void bind(DataType *data, QTextEdit *widget, const char* prop)
{
    using namespace Internal;
    static_assert(IsQObjectDerived<DataType>, "DataType must inherit QObject");
    if (!data || !widget || !prop) return;
    QByteArray propBa(prop);
    if (isBindExists(data, widget, propBa))
    {
        qWarning() << "[DataBind] Duplicate bind skipped, prop:" << prop;
        return;
    }
    QMetaMethod notifySignal = findNotifySignal(data, prop);
    if (!notifySignal.isValid()) return;

    QPointer<DataType> dataPtr = data;
    QPointer<QTextEdit> wPtr = widget;

    auto conn1 = QObject::connect(data, notifySignal, widget, [dataPtr, propBa, wPtr]() {
        if (!dataPtr || !wPtr) return;
        QVariant val = dataPtr->property(propBa.constData());
        QString txt = val.canConvert<QString>() ? val.toString() : "";
        bool syncLock = false;
        if (syncLock) return;
        syncLock = true;
        wPtr->setPlainText(txt);
        syncLock = false;
    }, Qt::QueuedConnection);
    storeConnection(data, widget, propBa, conn1);

    auto conn2 = QObject::connect(widget, &QTextEdit::textChanged, data, [dataPtr, propBa]() {
        if (!dataPtr) return;
        bool syncLock = false;
        if (syncLock) return;
        syncLock = true;
        dataPtr->setProperty(propBa.constData(), dataPtr->document()->toPlainText());
        syncLock = false;
    }, Qt::QueuedConnection);
    storeConnection(data, widget, propBa, conn2);
}

// ===================== QPlainTextEdit 双向绑定 =====================
template <typename DataType>
void bind(DataType *data, QPlainTextEdit *widget, const char* prop)
{
    using namespace Internal;
    static_assert(IsQObjectDerived<DataType>, "DataType must inherit QObject");
    if (!data || !widget || !prop) return;
    QByteArray propBa(prop);
    if (isBindExists(data, widget, propBa))
    {
        qWarning() << "[DataBind] Duplicate bind skipped, prop:" << prop;
        return;
    }
    QMetaMethod notifySignal = findNotifySignal(data, prop);
    if (!notifySignal.isValid()) return;

    QPointer<DataType> dataPtr = data;
    QPointer<QPlainTextEdit> wPtr = widget;

    auto conn1 = QObject::connect(data, notifySignal, widget, [dataPtr, propBa, wPtr]() {
        if (!dataPtr || !wPtr) return;
        QVariant val = dataPtr->property(propBa.constData());
        QString txt = val.canConvert<QString>() ? val.toString() : "";
        bool syncLock = false;
        if (syncLock) return;
        syncLock = true;
        wPtr->setPlainText(txt);
        syncLock = false;
    }, Qt::QueuedConnection);
    storeConnection(data, widget, propBa, conn1);

    auto conn2 = QObject::connect(widget, &QPlainTextEdit::textChanged, data, [dataPtr, propBa]() {
        if (!dataPtr) return;
        bool syncLock = false;
        if (syncLock) return;
        syncLock = true;
        dataPtr->setProperty(propBa.constData(), dataPtr->toPlainText());
        syncLock = false;
    }, Qt::QueuedConnection);
    storeConnection(data, widget, propBa, conn2);
}

// ===================== QCheckBox 双向绑定 =====================
template <typename DataType>
void bind(DataType *data, QCheckBox *widget, const char* prop)
{
    using namespace Internal;
    static_assert(IsQObjectDerived<DataType>, "DataType must inherit QObject");
    if (!data || !widget || !prop) return;
    QByteArray propBa(prop);
    if (isBindExists(data, widget, propBa))
    {
        qWarning() << "[DataBind] Duplicate bind skipped, prop:" << prop;
        return;
    }
    QMetaMethod notifySignal = findNotifySignal(data, prop);
    if (!notifySignal.isValid()) return;

    QPointer<DataType> dataPtr = data;
    QPointer<QCheckBox> wPtr = widget;

    auto conn1 = QObject::connect(data, notifySignal, widget, [dataPtr, propBa, wPtr]() {
        if (!dataPtr || !wPtr) return;
        QVariant val = dataPtr->property(propBa.constData());
        bool ck = val.canConvert<bool>() ? val.toBool() : false;
        bool syncLock = false;
        if (syncLock) return;
        syncLock = true;
        wPtr->setChecked(ck);
        syncLock = false;
    }, Qt::QueuedConnection);
    storeConnection(data, widget, propBa, conn1);

    auto conn2 = QObject::connect(widget, &QCheckBox::toggled, data, [dataPtr, propBa](bool checked) {
        if (!dataPtr) return;
        bool syncLock = false;
        if (syncLock) return;
        syncLock = true;
        dataPtr->setProperty(propBa.constData(), checked);
        syncLock = false;
    }, Qt::QueuedConnection);
    storeConnection(data, widget, propBa, conn2);
}

// ===================== QRadioButton 双向绑定 =====================
template <typename DataType>
void bind(DataType *data, QRadioButton *widget, const char* prop)
{
    using namespace Internal;
    static_assert(IsQObjectDerived<DataType>, "DataType must inherit QObject");
    if (!data || !widget || !prop) return;
    QByteArray propBa(prop);
    if (isBindExists(data, widget, propBa))
    {
        qWarning() << "[DataBind] Duplicate bind skipped, prop:" << prop;
        return;
    }
    QMetaMethod notifySignal = findNotifySignal(data, prop);
    if (!notifySignal.isValid()) return;

    QPointer<DataType> dataPtr = data;
    QPointer<QRadioButton> wPtr = widget;

    auto conn1 = QObject::connect(data, notifySignal, widget, [dataPtr, propBa, wPtr]() {
        if (!dataPtr || !wPtr) return;
        QVariant val = dataPtr->property(propBa.constData());
        bool ck = val.canConvert<bool>() ? val.toBool() : false;
        bool syncLock = false;
        if (syncLock) return;
        syncLock = true;
        wPtr->setChecked(ck);
        syncLock = false;
    }, Qt::QueuedConnection);
    storeConnection(data, widget, propBa, conn1);

    auto conn2 = QObject::connect(widget, &QRadioButton::toggled, data, [dataPtr, propBa](bool checked) {
        if (!dataPtr) return;
        bool syncLock = false;
        if (syncLock) return;
        syncLock = true;
        dataPtr->setProperty(propBa.constData(), checked);
        syncLock = false;
    }, Qt::QueuedConnection);
    storeConnection(data, widget, propBa, conn2);
}

// ===================== QSlider 双向绑定 =====================
template <typename DataType>
void bind(DataType *data, QSlider *widget, const char* prop)
{
    using namespace Internal;
    static_assert(IsQObjectDerived<DataType>, "DataType must inherit QObject");
    if (!data || !widget || !prop) return;
    QByteArray propBa(prop);
    if (isBindExists(data, widget, propBa))
    {
        qWarning() << "[DataBind] Duplicate bind skipped, prop:" << prop;
        return;
    }
    QMetaMethod notifySignal = findNotifySignal(data, prop);
    if (!notifySignal.isValid()) return;

    QPointer<DataType> dataPtr = data;
    QPointer<QSlider> wPtr = widget;

    auto conn1 = QObject::connect(data, notifySignal, widget, [dataPtr, propBa, wPtr]() {
        if (!dataPtr || !wPtr) return;
        QVariant val = dataPtr->property(propBa.constData());
        int v = val.canConvert<int>() ? val.toInt() : wPtr->minimum();
        v = clampInt(v, wPtr->minimum(), wPtr->maximum());
        bool syncLock = false;
        if (syncLock) return;
        syncLock = true;
        wPtr->setValue(v);
        syncLock = false;
    }, Qt::QueuedConnection);
    storeConnection(data, widget, propBa, conn1);

    auto conn2 = QObject::connect(widget, &QSlider::valueChanged, data, [dataPtr, propBa](int value) {
        if (!dataPtr) return;
        bool syncLock = false;
        if (syncLock) return;
        syncLock = true;
        dataPtr->setProperty(propBa.constData(), value);
        syncLock = false;
    }, Qt::QueuedConnection);
    storeConnection(data, widget, propBa, conn2);
}

// ===================== QSpinBox 双向绑定 =====================
template <typename DataType>
void bind(DataType *data, QSpinBox *widget, const char* prop)
{
    using namespace Internal;
    static_assert(IsQObjectDerived<DataType>, "DataType must inherit QObject");
    if (!data || !widget || !prop) return;
    QByteArray propBa(prop);
    if (isBindExists(data, widget, propBa))
    {
        qWarning() << "[DataBind] Duplicate bind skipped, prop:" << prop;
        return;
    }
    QMetaMethod notifySignal = findNotifySignal(data, prop);
    if (!notifySignal.isValid()) return;

    QPointer<DataType> dataPtr = data;
    QPointer<QSpinBox> wPtr = widget;

    auto conn1 = QObject::connect(data, notifySignal, widget, [dataPtr, propBa, wPtr]() {
        if (!dataPtr || !wPtr) return;
        QVariant val = dataPtr->property(propBa.constData());
        int v = val.canConvert<int>() ? val.toInt() : wPtr->minimum();
        v = clampInt(v, wPtr->minimum(), wPtr->maximum());
        bool syncLock = false;
        if (syncLock) return;
        syncLock = true;
        wPtr->setValue(v);
        syncLock = false;
    }, Qt::QueuedConnection);
    storeConnection(data, widget, propBa, conn1);

    auto conn2 = QObject::connect(widget, QOverload<int>::of(&QSpinBox::valueChanged),
        data, [dataPtr, propBa](int value) {
            if (!dataPtr) return;
            bool syncLock = false;
            if (syncLock) return;
            syncLock = true;
            dataPtr->setProperty(propBa.constData(), value);
            syncLock = false;
        }, Qt::QueuedConnection);
    storeConnection(data, widget, propBa, conn2);
}

// ===================== QDoubleSpinBox 双向绑定 =====================
template <typename DataType>
void bind(DataType *data, QDoubleSpinBox *widget, const char* prop)
{
    using namespace Internal;
    static_assert(IsQObjectDerived<DataType>, "DataType must inherit QObject");
    if (!data || !widget || !prop) return;
    QByteArray propBa(prop);
    if (isBindExists(data, widget, propBa))
    {
        qWarning() << "[DataBind] Duplicate bind skipped, prop:" << prop;
        return;
    }
    QMetaMethod notifySignal = findNotifySignal(data, prop);
    if (!notifySignal.isValid()) return;

    QPointer<DataType> dataPtr = data;
    QPointer<QDoubleSpinBox> wPtr = widget;

    auto conn1 = QObject::connect(data, notifySignal, widget, [dataPtr, propBa, wPtr]() {
        if (!dataPtr || !wPtr) return;
        QVariant val = dataPtr->property(propBa.constData());
        double v = val.canConvert<double>() ? val.toDouble() : wPtr->minimum();
        v = clampDouble(v, wPtr->minimum(), wPtr->maximum());
        bool syncLock = false;
        if (syncLock) return;
        syncLock = true;
        wPtr->setValue(v);
        syncLock = false;
    }, Qt::QueuedConnection);
    storeConnection(data, widget, propBa, conn1);

    auto conn2 = QObject::connect(widget, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        data, [dataPtr, propBa](double value) {
            if (!dataPtr) return;
            bool syncLock = false;
            if (syncLock) return;
            syncLock = true;
            dataPtr->setProperty(propBa.constData(), value);
            syncLock = false;
        }, Qt::QueuedConnection);
    storeConnection(data, widget, propBa, conn2);
}

// ===================== QComboBox 双向绑定（currentIndex） =====================
template <typename DataType>
void bind(DataType *data, QComboBox *widget, const char* prop)
{
    using namespace Internal;
    static_assert(IsQObjectDerived<DataType>, "DataType must inherit QObject");
    if (!data || !widget || !prop) return;
    QByteArray propBa(prop);
    if (isBindExists(data, widget, propBa))
    {
        qWarning() << "[DataBind] Duplicate bind skipped, prop:" << prop;
        return;
    }
    QMetaMethod notifySignal = findNotifySignal(data, prop);
    if (!notifySignal.isValid()) return;

    QPointer<DataType> dataPtr = data;
    QPointer<QComboBox> wPtr = widget;

    auto conn1 = QObject::connect(data, notifySignal, widget, [dataPtr, propBa, wPtr]() {
        if (!dataPtr || !wPtr) return;
        QVariant val = dataPtr->property(propBa.constData());
        int idx = val.canConvert<int>() ? val.toInt() : 0;
        idx = clampInt(idx, 0, wPtr->count() - 1);
        bool syncLock = false;
        if (syncLock) return;
        syncLock = true;
        wPtr->setCurrentIndex(idx);
        syncLock = false;
    }, Qt::QueuedConnection);
    storeConnection(data, widget, propBa, conn1);

    auto conn2 = QObject::connect(widget, QOverload<int>::of(&QComboBox::currentIndexChanged),
        data, [dataPtr, propBa](int index) {
            if (!dataPtr) return;
            bool syncLock = false;
            if (syncLock) return;
            syncLock = true;
            dataPtr->setProperty(propBa.constData(), index);
            syncLock = false;
        }, Qt::QueuedConnection);
    storeConnection(data, widget, propBa, conn2);
}

// ===================== QLabel 单向绑定 =====================
template <typename DataType>
void bind(DataType *data, QLabel *widget, const char* prop)
{
    using namespace Internal;
    static_assert(IsQObjectDerived<DataType>, "DataType must inherit QObject");
    if (!data || !widget || !prop) return;
    QByteArray propBa(prop);
    if (isBindExists(data, widget, propBa))
    {
        qWarning() << "[DataBind] Duplicate bind skipped, prop:" << prop;
        return;
    }
    QMetaMethod notifySignal = findNotifySignal(data, prop);
    if (!notifySignal.isValid()) return;

    QPointer<DataType> dataPtr = data;
    QPointer<QLabel> wPtr = widget;

    auto conn1 = QObject::connect(data, notifySignal, widget, [dataPtr, propBa, wPtr]() {
        if (!dataPtr || !wPtr) return;
        QVariant val = dataPtr->property(propBa.constData());
        QString txt = val.canConvert<QString>() ? val.toString() : "";
        wPtr->setText(txt);
    }, Qt::QueuedConnection);
    storeConnection(data, widget, propBa, conn1);
}

// ===================== QProgressBar 单向绑定 =====================
template <typename DataType>
void bind(DataType *data, QProgressBar *widget, const char* prop)
{
    using namespace Internal;
    static_assert(IsQObjectDerived<DataType>, "DataType must inherit QObject");
    if (!data || !widget || !prop) return;
    QByteArray propBa(prop);
    if (isBindExists(data, widget, propBa))
    {
        qWarning() << "[DataBind] Duplicate bind skipped, prop:" << prop;
        return;
    }
    QMetaMethod notifySignal = findNotifySignal(data, prop);
    if (!notifySignal.isValid()) return;

    QPointer<DataType> dataPtr = data;
    QPointer<QProgressBar> wPtr = widget;

    auto conn1 = QObject::connect(data, notifySignal, widget, [dataPtr, propBa, wPtr]() {
        if (!dataPtr || !wPtr) return;
        QVariant val = dataPtr->property(propBa.constData());
        int v = val.canConvert<int>() ? val.toInt() : wPtr->minimum();
        v = clampInt(v, wPtr->minimum(), wPtr->maximum());
        wPtr->setValue(v);
    }, Qt::QueuedConnection);
    storeConnection(data, widget, propBa, conn1);
}

// ===================== QPushButton enabled 双向绑定 bindEnabled =====================
template <typename DataType>
void bindEnabled(DataType *data, QPushButton *widget, const char* prop)
{
    using namespace Internal;
    static_assert(IsQObjectDerived<DataType>, "DataType must inherit QObject");
    if (!data || !widget || !prop) return;
    QByteArray propBa(prop);
    if (isBindExists(data, widget, propBa))
    {
        qWarning() << "[DataBind] Duplicate bind skipped, prop:" << prop;
        return;
    }
    QMetaMethod notifySignal = findNotifySignal(data, prop);
    if (!notifySignal.isValid()) return;

    QPointer<DataType> dataPtr = data;
    QPointer<QPushButton> wPtr = widget;

    // 数据 → UI
    auto conn1 = QObject::connect(data, notifySignal, widget, [dataPtr, propBa, wPtr]() {
        if (!dataPtr || !wPtr) return;
        QVariant val = dataPtr->property(propBa.constData());
        bool en = val.canConvert<bool>() ? val.toBool() : true;
        wPtr->setEnabled(en);
    }, Qt::QueuedConnection);
    storeConnection(data, widget, propBa, conn1);

    // UI enabled 变更反向同步（支持代码 setEnabled 写回 Model）
    auto conn2 = QObject::connect(widget, &QPushButton::enabledChanged, data, [dataPtr, propBa](bool en) {
        if (!dataPtr) return;
        dataPtr->setProperty(propBa.constData(), en);
    }, Qt::QueuedConnection);
    storeConnection(data, widget, propBa, conn2);
}

// ===================== 无参默认绑定（修复全部错误属性名） =====================
template <typename DataType>
void bind(DataType *data, QLineEdit *widget) { bind(data, widget, "text"); }

template <typename DataType>
void bind(DataType *data, QTextEdit *widget) { bind(data, widget, "text"); }

template <typename DataType>
void bind(DataType *data, QPlainTextEdit *widget) { bind(data, widget, "text"); }

template <typename DataType>
void bind(DataType *data, QCheckBox *widget) { bind(data, widget, "checked"); }

template <typename DataType>
void bind(DataType *data, QRadioButton *widget) { bind(data, widget, "checked"); }

template <typename DataType>
void bind(DataType *data, QSlider *widget) { bind(data, widget, "value"); }

template <typename DataType>
void bind(DataType *data, QSpinBox *widget) { bind(data, widget, "value"); }

template <typename DataType>
void bind(DataType *data, QDoubleSpinBox *widget) { bind(data, widget, "value"); }

template <typename DataType>
void bind(DataType *data, QComboBox *widget) { bind(data, widget, "currentIndex"); }

template <typename DataType>
void bind(DataType *data, QLabel *widget) { bind(data, widget, "text"); }

template <typename DataType>
void bind(DataType *data, QProgressBar *widget) { bind(data, widget, "value"); }

template <typename DataType>
void bindEnabled(DataType *data, QPushButton *widget) { bindEnabled(data, widget, "enabled"); }

// ===================== 批量绑定 bindAll =====================
// 可变参数混合控件批量默认绑定
template <typename DataType, typename... Widgets>
void bindAll(DataType *data, Widgets... widgets)
{
    (bind(data, widgets), ...);
}

// 同类型控件列表批量默认绑定
template <typename DataType, typename WidgetType>
void bindAll(DataType *data, const QList<WidgetType*> &widgets)
{
    for (WidgetType *w : widgets)
    {
        bind(data, w);
    }
}

// 批量解绑所有绑定
template<typename DataType>
void unbindAllWidgets(DataType* data)
{
    unbindAll(data);
}

} // namespace Sqz

#endif // DATABIND_H
