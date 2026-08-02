#ifndef DATABIND_H
#define DATABIND_H

// ============================================================================
//  DataBind.h — QObject 属性 ↔ QWidget 数据绑定库（仅头文件）
//
//  特性：
//    - 双向绑定：QLineEdit / QTextEdit / QPlainTextEdit / QCheckBox /
//                QRadioButton / QSlider / QSpinBox / QDoubleSpinBox / QComboBox
//    - 单向绑定：QLabel / QProgressBar / QPushButton(Enabled)（数据 → UI）
//    - 批量绑定：bindAll(...) 可变参数 / 同类型 QList
//    - 统一解绑：unbind / unbindAll / unbindWidget
//    - 防重复绑定、对象销毁自动清理、同线程即时同步且无死循环
//
//  兼容：Qt 5.12+，C++14
//
//  构建要求：本头文件含一个 Q_OBJECT 中继类 PropertyRelay，必须经 MOC 处理。
//            qmake 工程将本文件加入 HEADERS（AUTOMOC 默认开启）；
//            CMake 工程开启 set(CMAKE_AUTOMOC ON) 即可。
//
//  用法示例：
//    class Model : public QObject {
//        Q_OBJECT
//        Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
//        ...
//    };
//    Sqz::bind(model, lineEdit, "text");        // 双向
//    Sqz::bind(model, label, "text");           // 单向
//    Sqz::bindAll(model, lineEdit, spinBox, checkBox);
//
//  说明：绑定使用 AutoConnection（同线程为即时直连）。重入保护由库内部的
//        共享标志完成，Model 无需任何 syncLock 成员。
// ============================================================================

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
#include <QHash>
#include <QPair>
#include <QPointer>
#include <QSharedPointer>
#include <QMetaProperty>
#include <QMetaMethod>
#include <QDebug>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QRadioButton>
#include <functional>
#include <utility>

namespace Sqz {

namespace Internal {

// ==================== 类型特征 ====================
// 编译期校验：数据类型必须继承 QObject 并使用 Q_OBJECT
template<typename T>
constexpr bool IsQObjectDerived = std::is_base_of<QObject, T>::value;

// ==================== data→UI 信号中继 ====================
// Qt5 的 connect 不支持把 QMetaMethod(运行时解析的 NOTIFY 信号) 连到 functor；
// 唯一可行：QMetaMethod 信号 → QMetaMethod 槽。故用此中继 QObject 承载一个槽，
// 槽内调用 std::function。每个绑定创建一个中继，parent 到控件以自动回收。
class PropertyRelay : public QObject
{
    Q_OBJECT
public:
    explicit PropertyRelay(QObject* parent = nullptr) : QObject(parent) {}
    std::function<void()> fn;   // 绑定时设置：读取属性并更新控件
public Q_SLOTS:
    void activate() { if (fn) fn(); }
};

} // namespace Internal

// ==================== 绑定键与哈希 ====================
// 绑定上下文：记录 (data, widget, prop) 三元组，用于防重复绑定与统一解绑
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

// Qt5 qHash 签名（返回 uint）；移位 + 异或组合，避免逐次构造临时 QPair
inline uint qHash(const BindKey& key, uint seed = 0)
{
    return qHash(key.data, seed)
         ^ (qHash(key.widget, seed) << 1)
         ^ (qHash(key.prop, seed) << 2);
}

namespace Internal {

// ==================== 绑定记录 ====================
struct BindingRecord
{
    QList<QMetaObject::Connection> conns;   // 该绑定的所有连接
    PropertyRelay* relay = nullptr;         // data→UI 中继（无 parent，由库统一释放，避免与控件销毁双重释放）
};

// ==================== 全局绑定记录表 ====================
// 原版为头文件内 static 变量，会导致每个翻译单元各持一份（绑定跨 TU 不可见）。
// 改用 inline 函数 + 函数内 static 局部（C++11 magic statics），保证全进程唯一。
inline QHash<BindKey, BindingRecord>& bindTable()
{
    static QHash<BindKey, BindingRecord> table;
    return table;
}

// ==================== 重入保护 ====================
// 同一绑定的 data→UI 与 UI→data 两个回调共享此标志，避免互相触发死循环。
// RAII：构造时置位（若已被占用则不激活），析构时复位，异常安全。
class SyncGuard
{
public:
    explicit SyncGuard(QSharedPointer<bool> flag) : m_flag(std::move(flag))
    {
        m_active = !(*m_flag);
        if (m_active) {
            *m_flag = true;
        }
    }
    ~SyncGuard() { if (m_active) *m_flag = false; }
    explicit operator bool() const { return m_active; }
    SyncGuard(const SyncGuard&) = delete;
    SyncGuard& operator=(const SyncGuard&) = delete;
private:
    QSharedPointer<bool> m_flag;
    bool m_active = false;
};

// ==================== 工具函数 ====================
inline int clampInt(int val, int lo, int hi)
{
    return val < lo ? lo : (val > hi ? hi : val);
}
inline double clampDouble(double val, double lo, double hi)
{
    return val < lo ? lo : (val > hi ? hi : val);
}

// 安全获取属性的 NOTIFY 信号
template <typename DataType>
QMetaMethod findNotifySignal(DataType* data, const char* propName)
{
    static_assert(IsQObjectDerived<DataType>, "DataType must inherit QObject and use Q_OBJECT");
    if (!data || !propName) {
        qWarning() << "[DataBind] data or propName is null";
        return QMetaMethod();
    }
    const QMetaObject* meta = data->metaObject();
    int idx = meta->indexOfProperty(propName);
    if (idx < 0) {
        qWarning() << "[DataBind] Property not found:" << propName << "on object:" << data;
        return QMetaMethod();
    }
    QMetaProperty prop = meta->property(idx);
    if (!prop.hasNotifySignal()) {
        qWarning() << "[DataBind] Property has no NOTIFY signal:" << propName;
        return QMetaMethod();
    }
    return prop.notifySignal();
}

// 检查该绑定是否已存在（防重复绑定）
inline bool isBindExists(QObject* data, QObject* widget, const QByteArray& prop)
{
    BindKey key{data, widget, prop};
    const auto it = bindTable().constFind(key);
    return it != bindTable().constEnd() && !it->conns.isEmpty();
}

// 追加连接到记录（relay 仅在首次设置时写入）
inline void storeConnection(QObject* data, QObject* widget, const QByteArray& prop,
                            const QMetaObject::Connection& conn, PropertyRelay* relay = nullptr)
{
    BindingRecord& rec = bindTable()[BindKey{data, widget, prop}];
    rec.conns.append(conn);
    if (relay) {
        rec.relay = relay;
    }
}

// 清理单个绑定：断开连接、删除中继、移除记录
inline void unbindSingle(QObject* data, QObject* widget, const QByteArray& prop)
{
    BindKey key{data, widget, prop};
    const auto it = bindTable().find(key);
    if (it == bindTable().end()) return;
    for (const auto& c : it->conns) {
        QObject::disconnect(c);
    }
    if (it->relay) {
        delete it->relay;   // 中继 parent 到 widget；此处显式释放避免悬挂
    }
    bindTable().erase(it);
}

// 对象销毁时自动清理其在表中的绑定条目（避免悬挂指针累积）
inline void watchDestruction(QObject* data, QObject* widget, const QByteArray& prop)
{
    QObject::connect(data, &QObject::destroyed, widget, [data, widget, prop](QObject*) {
        unbindSingle(data, widget, prop);
    });
    QObject::connect(widget, &QObject::destroyed, data, [data, widget, prop](QObject*) {
        unbindSingle(data, widget, prop);
    });
}

// ==================== 绑定预处理 ====================
struct BindingSetup
{
    QByteArray propBa;
    QMetaMethod notifySignal;
    QSharedPointer<bool> guardFlag;   // 重入保护标志（双向绑定的两个回调共享）
    bool ok = false;
};

// 执行空指针校验、防重复、NOTIFY 信号获取、重入标志创建
template<typename DataType, typename WidgetType>
BindingSetup prepareBinding(DataType* data, WidgetType* widget, const char* prop)
{
    static_assert(IsQObjectDerived<DataType>, "DataType must inherit QObject and use Q_OBJECT");
    BindingSetup s;
    if (!data || !widget || !prop) {
        return s;
    }
    s.propBa = QByteArray(prop);
    if (isBindExists(data, widget, s.propBa)) {
        qWarning() << "[DataBind] Duplicate bind skipped, prop:" << prop;
        return s;
    }
    s.notifySignal = findNotifySignal(data, prop);
    if (!s.notifySignal.isValid()) {
        return s;
    }
    s.guardFlag = QSharedPointer<bool>::create();   // 值初始化为 false
    s.ok = true;
    return s;
}

// ==================== 连接构造 ====================
// data → widget：经 PropertyRelay 中继监听属性 NOTIFY，读取属性值并更新控件。
// outRelay 返回新建的中继（caller 负责记入记录以便解绑时释放）。
template<typename DataType, typename WidgetType, typename Setter>
QMetaObject::Connection connectDataToWidget(DataType* data, WidgetType* widget,
        const QMetaMethod& notifySignal, const QByteArray& propBa,
        const QSharedPointer<bool>& guardFlag, Setter setter, PropertyRelay*& outRelay)
{
    QPointer<DataType> dataPtr = data;
    QPointer<WidgetType> wPtr = widget;

    // 创建中继（不设 parent，由绑定记录统一管理生命周期，避免控件销毁时双重释放）
    PropertyRelay* relay = new PropertyRelay();
    relay->fn = [=]() {
        if (!dataPtr || !wPtr) return;
        SyncGuard g(guardFlag);
        if (!g) return;
        setter(dataPtr->property(propBa.constData()), wPtr.data());
    };
    outRelay = relay;

    // 连接：QMetaMethod 信号 → 中继的 activate 槽（QMetaMethod），AutoConnection 同线程直连
    const QMetaObject* rm = relay->metaObject();
    const int slotIdx = rm->indexOfSlot("activate()");
    return QObject::connect(data, notifySignal, relay, rm->method(slotIdx), Qt::AutoConnection);
}

// widget → data：监听控件信号（PMF），读取控件当前值并写回属性。
// 槽取 0 参数，对带参信号（如 valueChanged(int)）Qt 自动丢弃多余参数。
template<typename DataType, typename WidgetType, typename Signal, typename Getter>
QMetaObject::Connection connectWidgetToData(DataType* data, WidgetType* widget,
        Signal signal, const QByteArray& propBa,
        const QSharedPointer<bool>& guardFlag, Getter getter)
{
    QPointer<DataType> dataPtr = data;
    QPointer<WidgetType> wPtr = widget;
    return QObject::connect(widget, signal, data, [=]() {
        if (!dataPtr || !wPtr) return;
        SyncGuard g(guardFlag);
        if (!g) return;
        dataPtr->setProperty(propBa.constData(), getter(wPtr.data()));
    }, Qt::AutoConnection);
}

// ===================== 通用双向 / 单向绑定核心 =====================
// 双向：data↔widget。setter: void(const QVariant&, WidgetType*)，
//                    getter: QVariant(WidgetType*)，sig 为控件变更信号。
template<typename DataType, typename WidgetType, typename Signal, typename Setter, typename Getter>
void bindTwoWay(DataType* data, WidgetType* widget, const char* prop,
                Signal sig, Setter setter, Getter getter)
{
    BindingSetup s = prepareBinding(data, widget, prop);
    if (!s.ok) return;
    PropertyRelay* relay = nullptr;
    auto conn1 = connectDataToWidget(data, widget, s.notifySignal, s.propBa, s.guardFlag, setter, relay);
    auto conn2 = connectWidgetToData(data, widget, sig, s.propBa, s.guardFlag, getter);
    storeConnection(data, widget, s.propBa, conn1, relay);
    storeConnection(data, widget, s.propBa, conn2);
    watchDestruction(data, widget, s.propBa);
}

// 单向：data→widget（QLabel / QProgressBar / QPushButton-Enabled）。仅需 setter。
template<typename DataType, typename WidgetType, typename Setter>
void bindOneWay(DataType* data, WidgetType* widget, const char* prop, Setter setter)
{
    BindingSetup s = prepareBinding(data, widget, prop);
    if (!s.ok) return;
    PropertyRelay* relay = nullptr;
    auto conn1 = connectDataToWidget(data, widget, s.notifySignal, s.propBa, s.guardFlag, setter, relay);
    storeConnection(data, widget, s.propBa, conn1, relay);
    watchDestruction(data, widget, s.propBa);
}

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
    if (!data) return;
    QList<BindKey> toRemove;
    auto& table = Internal::bindTable();
    for (auto it = table.begin(); it != table.end(); ++it) {
        if (it.key().data == data) {
            for (const auto& c : it.value().conns) {
                QObject::disconnect(c);
            }
            if (it.value().relay) {
                delete it.value().relay;
            }
            toRemove.append(it.key());
        }
    }
    for (const auto& k : toRemove) {
        table.remove(k);
    }
}

/**
 * @brief 解绑某个控件所有绑定
 */
inline void unbindWidget(QWidget* widget)
{
    if (!widget) return;
    QList<BindKey> toRemove;
    auto& table = Internal::bindTable();
    for (auto it = table.begin(); it != table.end(); ++it) {
        if (it.key().widget == widget) {
            for (const auto& c : it.value().conns) {
                QObject::disconnect(c);
            }
            if (it.value().relay) {
                delete it.value().relay;
            }
            toRemove.append(it.key());
        }
    }
    for (const auto& k : toRemove) {
        table.remove(k);
    }
}

// ===================== 各控件绑定 =====================
// 双向：QLineEdit（字符串）
template <typename DataType>
void bind(DataType* data, QLineEdit* widget, const char* prop)
{
    Internal::bindTwoWay(data, widget, prop, &QLineEdit::textChanged,
        [](const QVariant& v, QLineEdit* w) {
            w->setText(v.canConvert<QString>() ? v.toString() : QString());
        },
        [](QLineEdit* w) { return w->text(); });
}

// 双向：QTextEdit（纯文本）
template <typename DataType>
void bind(DataType* data, QTextEdit* widget, const char* prop)
{
    Internal::bindTwoWay(data, widget, prop, &QTextEdit::textChanged,
        [](const QVariant& v, QTextEdit* w) {
            w->setPlainText(v.canConvert<QString>() ? v.toString() : QString());
        },
        [](QTextEdit* w) { return w->toPlainText(); });
}

// 双向：QPlainTextEdit（纯文本）
template <typename DataType>
void bind(DataType* data, QPlainTextEdit* widget, const char* prop)
{
    Internal::bindTwoWay(data, widget, prop, &QPlainTextEdit::textChanged,
        [](const QVariant& v, QPlainTextEdit* w) {
            w->setPlainText(v.canConvert<QString>() ? v.toString() : QString());
        },
        [](QPlainTextEdit* w) { return w->toPlainText(); });
}

// 双向：QCheckBox（布尔）
template <typename DataType>
void bind(DataType* data, QCheckBox* widget, const char* prop)
{
    Internal::bindTwoWay(data, widget, prop, &QCheckBox::toggled,
        [](const QVariant& v, QCheckBox* w) {
            w->setChecked(v.canConvert<bool>() ? v.toBool() : false);
        },
        [](QCheckBox* w) { return w->isChecked(); });
}

// 双向：QRadioButton（布尔）
template <typename DataType>
void bind(DataType* data, QRadioButton* widget, const char* prop)
{
    Internal::bindTwoWay(data, widget, prop, &QRadioButton::toggled,
        [](const QVariant& v, QRadioButton* w) {
            w->setChecked(v.canConvert<bool>() ? v.toBool() : false);
        },
        [](QRadioButton* w) { return w->isChecked(); });
}

// 双向：QSlider（整数，范围截断）
template <typename DataType>
void bind(DataType* data, QSlider* widget, const char* prop)
{
    Internal::bindTwoWay(data, widget, prop, &QSlider::valueChanged,
        [](const QVariant& v, QSlider* w) {
            int iv = v.canConvert<int>() ? v.toInt() : w->minimum();
            w->setValue(Internal::clampInt(iv, w->minimum(), w->maximum()));
        },
        [](QSlider* w) { return w->value(); });
}

// 双向：QSpinBox（整数，范围截断）
template <typename DataType>
void bind(DataType* data, QSpinBox* widget, const char* prop)
{
    Internal::bindTwoWay(data, widget, prop, QOverload<int>::of(&QSpinBox::valueChanged),
        [](const QVariant& v, QSpinBox* w) {
            int iv = v.canConvert<int>() ? v.toInt() : w->minimum();
            w->setValue(Internal::clampInt(iv, w->minimum(), w->maximum()));
        },
        [](QSpinBox* w) { return w->value(); });
}

// 双向：QDoubleSpinBox（浮点，范围截断）
template <typename DataType>
void bind(DataType* data, QDoubleSpinBox* widget, const char* prop)
{
    Internal::bindTwoWay(data, widget, prop, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        [](const QVariant& v, QDoubleSpinBox* w) {
            double dv = v.canConvert<double>() ? v.toDouble() : w->minimum();
            w->setValue(Internal::clampDouble(dv, w->minimum(), w->maximum()));
        },
        [](QDoubleSpinBox* w) { return w->value(); });
}

// 双向：QComboBox（currentIndex）
template <typename DataType>
void bind(DataType* data, QComboBox* widget, const char* prop)
{
    Internal::bindTwoWay(data, widget, prop, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [](const QVariant& v, QComboBox* w) {
            const int max = w->count() - 1;
            if (max < 0) { w->setCurrentIndex(-1); return; }
            int idx = v.canConvert<int>() ? v.toInt() : 0;
            w->setCurrentIndex(Internal::clampInt(idx, 0, max));
        },
        [](QComboBox* w) { return w->currentIndex(); });
}

// 单向：QLabel（字符串）
template <typename DataType>
void bind(DataType* data, QLabel* widget, const char* prop)
{
    Internal::bindOneWay(data, widget, prop,
        [](const QVariant& v, QLabel* w) {
            w->setText(v.canConvert<QString>() ? v.toString() : QString());
        });
}

// 单向：QProgressBar（整数，范围截断）
template <typename DataType>
void bind(DataType* data, QProgressBar* widget, const char* prop)
{
    Internal::bindOneWay(data, widget, prop,
        [](const QVariant& v, QProgressBar* w) {
            int iv = v.canConvert<int>() ? v.toInt() : w->minimum();
            w->setValue(Internal::clampInt(iv, w->minimum(), w->maximum()));
        });
}

// 单向：QPushButton enabled（data → UI）
// 说明：QWidget 无 enabled 变更信号，故只能单向（数据驱动按钮可用状态）。
template <typename DataType>
void bindEnabled(DataType* data, QPushButton* widget, const char* prop)
{
    Internal::bindOneWay(data, widget, prop,
        [](const QVariant& v, QPushButton* w) {
            w->setEnabled(v.canConvert<bool>() ? v.toBool() : true);
        });
}

// ===================== 无参默认绑定（按控件默认属性） =====================
template <typename DataType> void bind(DataType* data, QLineEdit* w)       { bind(data, w, "text"); }
template <typename DataType> void bind(DataType* data, QTextEdit* w)      { bind(data, w, "text"); }
template <typename DataType> void bind(DataType* data, QPlainTextEdit* w) { bind(data, w, "text"); }
template <typename DataType> void bind(DataType* data, QCheckBox* w)      { bind(data, w, "checked"); }
template <typename DataType> void bind(DataType* data, QRadioButton* w)   { bind(data, w, "checked"); }
template <typename DataType> void bind(DataType* data, QSlider* w)        { bind(data, w, "value"); }
template <typename DataType> void bind(DataType* data, QSpinBox* w)       { bind(data, w, "value"); }
template <typename DataType> void bind(DataType* data, QDoubleSpinBox* w) { bind(data, w, "value"); }
template <typename DataType> void bind(DataType* data, QComboBox* w)       { bind(data, w, "currentIndex"); }
template <typename DataType> void bind(DataType* data, QLabel* w)         { bind(data, w, "text"); }
template <typename DataType> void bind(DataType* data, QProgressBar* w)   { bind(data, w, "value"); }
template <typename DataType> void bindEnabled(DataType* data, QPushButton* w) { bindEnabled(data, w, "enabled"); }

// ===================== 批量绑定 bindAll =====================
// 可变参数混合控件批量默认绑定（C++14 数组展开，替代 C++17 折叠表达式）
template <typename DataType, typename... Widgets>
void bindAll(DataType* data, Widgets... widgets)
{
    using expand = int[];
    (void)expand{ 0, (bind(data, widgets), 0)... };
}

// 同类型控件列表批量默认绑定
template <typename DataType, typename WidgetType>
void bindAll(DataType* data, const QList<WidgetType*>& widgets)
{
    for (WidgetType* w : widgets) {
        bind(data, w);
    }
}

// 批量解绑 data 上所有绑定（unbindAll 的语义别名）
template<typename DataType>
void unbindAllWidgets(DataType* data)
{
    unbindAll(data);
}

// 保留旧版同步锁宏（已废弃：新绑定内部自带重入保护，Model 无需声明这些成员）。
// 仅用于向后兼容已使用 DATABIND_SYNC_LOCK() 的 Model 类，不影响新逻辑。
#define DATABIND_SYNC_LOCK() bool syncLock = false;
#define DATABIND_LOCK() if (syncLock) return; syncLock = true;
#define DATABIND_UNLOCK() syncLock = false;

} // namespace Sqz

#endif // DATABIND_H
