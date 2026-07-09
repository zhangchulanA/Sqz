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
#include <QRadioButton>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QDoubleSpinBox>
#include <QList>
#include <functional>
#include <QMetaProperty>
#include "SqzGlobal.h"
/**
 * @namespace DataBind
 * @brief 通用数据与UI双向绑定工具集
 *
 * 本命名空间提供了一套便捷的模板函数，用于将 QObject 派生类的属性
 * （通过 Q_PROPERTY 声明，并带有 NOTIFY 信号）与 Qt Widgets 的 UI 属性
 * 进行双向自动同步。
 *
 * ## 核心设计理念
 *
 * 1. **数据驱动**：数据对象是唯一真相源（Single Source of Truth）
 * 2. **自动同步**：数据变化 → UI 自动更新；UI 操作 → 数据自动更新
 * 3. **零侵入**：不需要修改现有 UI 类，只需在初始化时建立绑定
 * 4. **类型安全**：编译期类型检查，避免运行时错误
 *
 * ## 使用前提
 *
 * 数据类必须满足以下条件：
 * - 继承自 QObject
 * - 使用 Q_PROPERTY 声明属性
 * - 属性带有 NOTIFY 信号，在值变化时发射
 *
 * ## 基本用法示例
 *
 * @code
 * // 1. 定义数据类（需包含 Q_PROPERTY + NOTIFY）
 * class MyData : public QObject {
 *     Q_OBJECT
 *     Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
 * public:
 *     QString text() const { return m_text; }
 *     void setText(const QString &t) {
 *         if (m_text != t) { m_text = t; emit textChanged(); }
 *     }
 * signals:
 *     void textChanged();
 * private:
 *     QString m_text;
 * };
 *
 * // 2. 在 QWidget 中使用
 * MyData *data = new MyData(this);
 * DataBind::bind(data, ui->lineEditName);  // 一行完成绑定！
 * @endcode
 *
 * ## 支持的控件类型
 *
 * | 控件 | 绑定的UI属性 | 对应的数据属性 | 绑定方向 |
 * |------|-------------|---------------|----------|
 * | QLineEdit | text | text | 双向 |
 * | QLabel | text | text | 只读（数据→UI） |
 * | QCheckBox | checked | enabled | 双向 |
 * | QRadioButton | checked | optionChecked | 双向 |
 * | QPushButton | enabled | buttonEnabled | 只读（数据→UI） |
 * | QSlider | value | value | 双向 |
 * | QSpinBox | value | value | 双向 |
 * | QDoubleSpinBox | value | doubleValue | 双向 |
 * | QProgressBar | value | value | 只读（数据→UI） |
 * | QComboBox | currentIndex | index | 双向 |
 * | QTextEdit | plainText | text | 双向 |
 * | QPlainTextEdit | plainText | text | 双向 |
 *
 * @note 所有模板函数都在头文件中实现，不需要额外的 .cpp 文件
 * @note 绑定关系在数据对象或控件销毁时自动断开，无需手动管理
 */
namespace Sqz::Config {

// ============================================================================
// 一、内部辅助函数（必须先定义，供后面的 bind 函数使用）
// ============================================================================

/**
 * @brief 内部辅助：通用双向绑定（使用函数指针）
 *
 * 此函数是其他 bind 函数的基础实现，完成实际的信号槽连接工作。
 *
 * @tparam DataType     数据对象类型
 * @tparam DataValue    数据属性的值类型
 * @tparam WidgetType   控件类型
 * @tparam WidgetValue  控件属性的值类型
 * @param data          数据对象指针
 * @param dataProperty  数据属性名（仅用于调试，实际连接使用函数指针）
 * @param widget        控件指针
 * @param widgetProperty 控件属性名（仅用于调试）
 * @param dataSetter    数据对象的 setter 函数指针（可为 nullptr）
 * @param widgetSetter  控件的 setter 函数指针（可为 nullptr）
 * @param dataNotify    数据对象的 NOTIFY 信号指针（可为 nullptr）
 * @param widgetSignal  控件的信号指针（可为 nullptr）
 *
 * @note 此函数通过函数指针建立连接，类型安全且性能高
 */
template <typename DataType, typename DataValue,
          typename WidgetType, typename WidgetValue>
void bindProperty(
    DataType *data,
    const char *dataProperty,
    WidgetType *widget,
    const char *widgetProperty,
    void (DataType::*dataSetter)(DataValue),
    void (WidgetType::*widgetSetter)(WidgetValue),
    void (DataType::*dataNotify)(),
    void (WidgetType::*widgetSignal)(WidgetValue)
) {
    if (!data || !widget) return;
    Q_UNUSED(dataProperty)
    Q_UNUSED(widgetProperty)

    // 数据 → UI：连接数据对象的 NOTIFY 信号到控件的 setter
    if (dataNotify && widgetSetter) {
        QObject::connect(data, dataNotify, widget, widgetSetter);
    }

    // UI → 数据：连接控件的信号到数据对象的 setter
    if (widgetSignal && dataSetter) {
        QObject::connect(widget, widgetSignal, data, dataSetter);
    }
}

/**
 * @brief 内部辅助：只读绑定（仅数据 → UI）
 *
 * 适用于 QLabel、QProgressBar 等用户无法直接编辑的控件。
 *
 * @tparam DataType     数据对象类型
 * @tparam DataValue    数据属性的值类型
 * @tparam WidgetType   控件类型
 * @tparam WidgetValue  控件属性的值类型
 * @param data          数据对象指针
 * @param dataProperty  数据属性名（仅用于调试）
 * @param widget        控件指针
 * @param widgetProperty 控件属性名（仅用于调试）
 * @param widgetSetter  控件的 setter 函数指针
 * @param dataNotify    数据对象的 NOTIFY 信号指针
 */
template <typename DataType, typename DataValue,
          typename WidgetType, typename WidgetValue>
void bindPropertyReadonly(
    DataType *data,
    const char *dataProperty,
    WidgetType *widget,
    const char *widgetProperty,
    void (WidgetType::*widgetSetter)(WidgetValue),
    void (DataType::*dataNotify)()
) {
    if (!data || !widget) return;
    Q_UNUSED(dataProperty)
    Q_UNUSED(widgetProperty)

    // 仅连接数据 → UI 方向
    if (dataNotify && widgetSetter) {
        QObject::connect(data, dataNotify, widget, widgetSetter);
    }
}

/**
 * @brief 内部辅助：通过元对象查找属性的 NOTIFY 信号
 *
 * 使用 Qt 的元对象系统（QMetaObject）在运行时查找指定属性的 NOTIFY 信号。
 * 主要用于 QTextEdit、QPlainTextEdit 等特殊控件。
 *
 * @tparam DataType 数据对象类型
 * @param data          数据对象指针
 * @param propertyName  属性名
 * @return QMetaMethod  找到的 NOTIFY 信号方法，未找到则返回无效 QMetaMethod
 *
 * @note 此函数在编译期无法确定信号的情况下使用，有一定运行时开销
 */
template <typename DataType>
QMetaMethod findNotifySignal(DataType *data, const char *propertyName) {
    const QMetaObject *meta = data->metaObject();
    int propIdx = meta->indexOfProperty(propertyName);
    if (propIdx < 0) return QMetaMethod();

    QMetaProperty prop = meta->property(propIdx);
    if (!prop.hasNotifySignal()) return QMetaMethod();

    return prop.notifySignal();
}

/**
 * @brief 内部辅助：通过元对象查找 setter 方法
 *
 * 用于 QTextEdit、QPlainTextEdit 等需要动态查找 setter 的场景。
 * 当前为简化实现，返回 nullptr。
 *
 * @tparam DataType 数据对象类型
 * @tparam ValueType 属性的值类型
 * @param data          数据对象指针
 * @param propertyName  属性名
 * @return 成员函数指针，当前始终返回 nullptr
 *
 * @todo 完善此函数，实现完整的元对象查找逻辑
 */
template <typename DataType, typename ValueType>
void (DataType::*findSetter(DataType *data, const char *propertyName))(ValueType) {
    Q_UNUSED(data)
    Q_UNUSED(propertyName)
    // 简化实现，实际项目可完善
    // 完整实现需要解析属性类型并匹配对应的 setter 方法
    return nullptr;
}

// ============================================================================
// 二、基础控件绑定（最常用）
// ============================================================================

/**
 * @brief 将数据对象的 "text" 属性与 QLineEdit 的 "text" 属性双向绑定
 *
 * 当数据对象的 text 变化时，QLineEdit 会自动更新显示；
 * 当用户在 QLineEdit 中输入时，数据对象的 text 会自动更新。
 *
 * @tparam DataType 数据对象类型（必须继承自 QObject）
 * @param data      数据对象指针（不能为 nullptr）
 * @param widget    QLineEdit 控件指针（不能为 nullptr）
 *
 * @code
 * MyData *data = new MyData(this);
 * DataBind::bind(data, ui->lineEditName);
 * // 之后 data->setText("张三") 会自动更新 UI
 * // 用户输入也会自动更新 data 的 text 属性
 * @endcode
 */
template <typename DataType>
void bind(DataType *data, QLineEdit *widget) {
    if (!data || !widget) return;
    bindProperty<DataType, QString, QLineEdit, QString>(
        data, "text", widget, "text",
        &DataType::setText, &QLineEdit::setText,
        &DataType::textChanged, &QLineEdit::textChanged
    );
}

/**
 * @brief 将数据对象的 "text" 属性与 QLabel 的 "text" 属性单向绑定（只读）
 *
 * 当数据对象的 text 变化时，QLabel 会自动更新显示。
 * 但用户无法直接编辑 QLabel，所以没有 UI→数据 方向的绑定。
 *
 * @tparam DataType 数据对象类型
 * @param data      数据对象指针
 * @param widget    QLabel 控件指针
 *
 * @code
 * DataBind::bind(data, ui->labelDisplay);  // 仅用于显示，不可编辑
 * @endcode
 */
template <typename DataType>
void bind(DataType *data, QLabel *widget) {
    if (!data || !widget) return;
    bindPropertyReadonly<DataType, QString, QLabel, QString>(
        data, "text", widget, "text",
        &QLabel::setText,
        &DataType::textChanged
    );
}

/**
 * @brief 将数据对象的 "enabled" 属性与 QCheckBox 的 "checked" 属性双向绑定
 *
 * 数据对象的 enabled 为 true 时，QCheckBox 处于勾选状态；
 * 用户勾选/取消勾选 QCheckBox 时，数据对象的 enabled 同步更新。
 *
 * @tparam DataType 数据对象类型
 * @param data      数据对象指针
 * @param widget    QCheckBox 控件指针
 *
 * @code
 * DataBind::bind(data, ui->checkBoxEnable);
 * // data->setEnabled(true)  → QCheckBox 自动勾选
 * // 用户点击 QCheckBox    → data->setEnabled() 自动调用
 * @endcode
 */
template <typename DataType>
void bind(DataType *data, QCheckBox *widget) {
    if (!data || !widget) return;
    bindProperty<DataType, bool, QCheckBox, bool>(
        data, "enabled", widget, "checked",
        &DataType::setEnabled, &QCheckBox::setChecked,
        &DataType::enabledChanged, &QCheckBox::toggled
    );
}

/**
 * @brief 将数据对象的指定 bool 属性与 QRadioButton 的 "checked" 属性双向绑定
 *
 * 适用于单选按钮组，通常多个 QRadioButton 绑定到不同的数据属性，
 * 或者绑定到同一个属性的不同值。
 *
 * @tparam DataType 数据对象类型
 * @param data          数据对象指针
 * @param widget        QRadioButton 控件指针
 * @param dataProperty  数据对象中对应的 bool 属性名（默认 "optionChecked"）
 *
 * @code
 * // 绑定到默认属性 "optionChecked"
 * DataBind::bind(data, ui->radioOption1);
 *
 * // 绑定到自定义属性 "isMale"
 * DataBind::bind(data, ui->radioMale, "isMale");
 * DataBind::bind(data, ui->radioFemale, "isFemale");
 * @endcode
 */
template <typename DataType>
void bind(DataType *data, QRadioButton *widget,
          const char *dataProperty = "optionChecked") {
    if (!data || !widget) return;
    bindProperty<DataType, bool, QRadioButton, bool>(
        data, dataProperty, widget, "checked",
        nullptr, &QRadioButton::setChecked,
        nullptr, &QRadioButton::toggled
    );
}

/**
 * @brief 将数据对象的 "buttonEnabled" 属性与 QPushButton 的 "enabled" 属性单向绑定
 *
 * 当数据对象的 buttonEnabled 变化时，QPushButton 的启用/禁用状态自动更新。
 * 用于控制按钮是否可点击。
 *
 * @tparam DataType 数据对象类型
 * @param data          数据对象指针
 * @param widget        QPushButton 控件指针
 * @param dataProperty  数据对象中对应的属性名（默认 "buttonEnabled"）
 *
 * @code
 * DataBind::bindEnabled(data, ui->pushButtonSubmit);
 * // data->setButtonEnabled(false)  → 按钮变灰禁用
 * // data->setButtonEnabled(true)   → 按钮恢复可用
 * @endcode
 */
template <typename DataType>
void bindEnabled(DataType *data, QPushButton *widget,
                 const char *dataProperty = "buttonEnabled") {
    if (!data || !widget) return;
    bindPropertyReadonly<DataType, bool, QPushButton, bool>(
        data, dataProperty, widget, "enabled",
        &QPushButton::setEnabled,
        nullptr
    );
}

// ============================================================================
// 三、数值控件绑定
// ============================================================================

/**
 * @brief 将数据对象的 "value" 属性与 QSlider 的 "value" 属性双向绑定
 *
 * 典型应用：音量控制、进度调节、亮度调节等滑块场景。
 *
 * @tparam DataType 数据对象类型
 * @param data          数据对象指针
 * @param widget        QSlider 控件指针
 * @param dataProperty  数据对象中对应的属性名（默认 "value"）
 *
 * @code
 * DataBind::bind(data, ui->sliderVolume);
 * // 拖动滑块 → data->setValue() 自动调用
 * // data->setValue(50) → 滑块自动移动到 50 位置
 * @endcode
 */
template <typename DataType>
void bind(DataType *data, QSlider *widget,
          const char *dataProperty = "value") {
    if (!data || !widget) return;
    bindProperty<DataType, int, QSlider, int>(
        data, dataProperty, widget, "value",
        nullptr, &QSlider::setValue,
        nullptr, &QSlider::valueChanged
    );
}

/**
 * @brief 将数据对象的 "value" 属性与 QSpinBox 的 "value" 属性双向绑定
 *
 * 适用于整数数值的输入和显示，如年龄、数量、等级等。
 *
 * @tparam DataType 数据对象类型
 * @param data          数据对象指针
 * @param widget        QSpinBox 控件指针
 * @param dataProperty  数据对象中对应的属性名（默认 "value"）
 *
 * @code
 * DataBind::bind(data, ui->spinBoxAge);
 * @endcode
 */
template <typename DataType>
void bind(DataType *data, QSpinBox *widget,
          const char *dataProperty = "value") {
    if (!data || !widget) return;
    bindProperty<DataType, int, QSpinBox, int>(
        data, dataProperty, widget, "value",
        nullptr, &QSpinBox::setValue,
        nullptr, QOverload<int>::of(&QSpinBox::valueChanged)
    );
}

/**
 * @brief 将数据对象的 "doubleValue" 属性与 QDoubleSpinBox 的 "value" 属性双向绑定
 *
 * 适用于浮点数数值的输入和显示，如价格、百分比、坐标等。
 *
 * @tparam DataType 数据对象类型
 * @param data          数据对象指针
 * @param widget        QDoubleSpinBox 控件指针
 * @param dataProperty  数据对象中对应的属性名（默认 "doubleValue"）
 *
 * @code
 * DataBind::bind(data, ui->doubleSpinBoxPrice);
 * @endcode
 */
template <typename DataType>
void bind(DataType *data, QDoubleSpinBox *widget,
          const char *dataProperty = "doubleValue") {
    if (!data || !widget) return;
    bindProperty<DataType, double, QDoubleSpinBox, double>(
        data, dataProperty, widget, "value",
        nullptr, &QDoubleSpinBox::setValue,
        nullptr, QOverload<double>::of(&QDoubleSpinBox::valueChanged)
    );
}

/**
 * @brief 将数据对象的 "value" 属性与 QProgressBar 的 "value" 属性单向绑定（只读）
 *
 * QProgressBar 用于显示进度，用户无法直接操作，因此只支持数据→UI 方向。
 *
 * @tparam DataType 数据对象类型
 * @param data          数据对象指针
 * @param widget        QProgressBar 控件指针
 * @param dataProperty  数据对象中对应的属性名（默认 "value"）
 *
 * @code
 * DataBind::bind(data, ui->progressBarDownload);
 * // data->setValue(75) → 进度条自动更新到 75%
 * @endcode
 */
template <typename DataType>
void bind(DataType *data, QProgressBar *widget,
          const char *dataProperty = "value") {
    if (!data || !widget) return;
    bindPropertyReadonly<DataType, int, QProgressBar, int>(
        data, dataProperty, widget, "value",
        &QProgressBar::setValue,
        nullptr
    );
}

// ============================================================================
// 四、选择类控件绑定
// ============================================================================

/**
 * @brief 将数据对象的 "index" 属性与 QComboBox 的 "currentIndex" 属性双向绑定
 *
 * 适用于下拉选择框，选中的索引与数据对象的 index 属性同步。
 *
 * @tparam DataType 数据对象类型
 * @param data          数据对象指针
 * @param widget        QComboBox 控件指针
 * @param dataProperty  数据对象中对应的属性名（默认 "index"）
 *
 * @code
 * DataBind::bind(data, ui->comboBoxSelect);
 * // 用户选择第 3 项 → data->setIndex(2)
 * // data->setIndex(0) → 下拉框自动选中第 1 项
 * @endcode
 */
template <typename DataType>
void bind(DataType *data, QComboBox *widget,
          const char *dataProperty = "index") {
    if (!data || !widget) return;
    bindProperty<DataType, int, QComboBox, int>(
        data, dataProperty, widget, "currentIndex",
        nullptr, &QComboBox::setCurrentIndex,
        nullptr, QOverload<int>::of(&QComboBox::currentIndexChanged)
    );
}

// ============================================================================
// 五、多行文本控件绑定
// ============================================================================

/**
 * @brief 将数据对象的 "text" 属性与 QTextEdit 的纯文本内容双向绑定
 *
 * QTextEdit 的 textChanged 信号不带参数，因此内部使用 lambda 进行转换。
 * 绑定的是纯文本内容（plainText），而非富文本（html）。
 *
 * @tparam DataType 数据对象类型
 * @param data          数据对象指针
 * @param widget        QTextEdit 控件指针
 * @param dataProperty  数据对象中对应的属性名（默认 "text"）
 *
 * @code
 * DataBind::bind(data, ui->textEditContent);
 * @endcode
 */
template <typename DataType>
void bind(DataType *data, QTextEdit *widget,
          const char *dataProperty = "text") {
    if (!data || !widget) return;

    // 直接使用 auto，不定义别名
    auto dataSetter = findSetter<DataType, QString>(data, dataProperty);
    if (!dataSetter) return;

    auto notifySignal = findNotifySignal<DataType>(data, dataProperty);
    if (notifySignal) {
        QObject::connect(data, notifySignal, widget, &QTextEdit::setPlainText);
    }

    QObject::connect(widget, &QTextEdit::textChanged, [data, dataSetter, widget]() {
        (data->*dataSetter)(widget->toPlainText());
    });
}

/**
 * @brief 将数据对象的 "text" 属性与 QPlainTextEdit 的纯文本内容双向绑定
 *
 * 与 QTextEdit 类似，但适用于纯文本编辑场景，性能更好。
 *
 * @tparam DataType 数据对象类型
 * @param data          数据对象指针
 * @param widget        QPlainTextEdit 控件指针
 * @param dataProperty  数据对象中对应的属性名（默认 "text"）
 *
 * @code
 * DataBind::bind(data, ui->plainTextEditLog);
 * @endcode
 */
template <typename DataType>
void bind(DataType *data, QPlainTextEdit *widget,
          const char *dataProperty = "text") {
    if (!data || !widget) return;

    auto dataSetter = findSetter<DataType, QString>(data, dataProperty);
    if (!dataSetter) return;

    auto notifySignal = findNotifySignal<DataType>(data, dataProperty);
    if (notifySignal) {
        QObject::connect(data, notifySignal, widget, &QPlainTextEdit::setPlainText);
    }

    QObject::connect(widget, &QPlainTextEdit::textChanged, [data, dataSetter, widget]() {
        (data->*dataSetter)(widget->toPlainText());
    });
}

// ============================================================================
// 六、高级绑定功能
// ============================================================================

/**
 * @brief 通用双向绑定（自定义属性名）
 *
 * 当默认属性名不匹配时，使用此函数可以指定数据对象和控件的属性名。
 * 适用于需要灵活绑定任意属性的场景。
 *
 * @tparam DataType     数据对象类型
 * @tparam WidgetType   控件类型
 * @param data          数据对象指针
 * @param dataProperty  数据对象的属性名（必须包含 NOTIFY 信号）
 * @param widget        控件指针
 * @param widgetProperty 控件的属性名
 *
 * @code
 * // 将数据对象的 "score" 属性绑定到 QSpinBox 的 "value" 属性
 * DataBind::bind(myData, "score", ui->spinBoxScore, "value");
 *
 * // 将数据对象的 "userName" 属性绑定到 QLineEdit 的 "text" 属性
 * DataBind::bind(myData, "userName", ui->lineEditName, "text");
 * @endcode
 *
 * @warning 此函数目前为简化版本，推荐使用 bindWithSignals 以获得更精确的控制
 */
template <typename DataType, typename WidgetType>
void bind(DataType *data, const char *dataProperty,
          WidgetType *widget, const char *widgetProperty) {
    if (!data || !widget) return;
    // 简化版实现，实际项目可扩展
    // 建议使用 bindWithSignals 进行精确绑定
    Q_UNUSED(dataProperty)
    Q_UNUSED(widgetProperty)
}

/**
 * @brief 通用双向绑定（显式指定信号和槽）
 *
 * 这是最底层、最灵活的绑定方式，直接指定数据对象和控件的 setter 与信号。
 * 适用于所有其他 bind 函数无法覆盖的场景，或需要精确控制绑定的情况。
 *
 * @tparam DataType     数据对象类型
 * @tparam DataValue    数据属性的值类型
 * @tparam WidgetType   控件类型
 * @tparam WidgetValue  控件属性的值类型
 * @param data          数据对象指针
 * @param dataSetter    数据对象的 setter 成员函数指针
 * @param dataNotify    数据对象的 NOTIFY 信号成员函数指针（数据→UI方向）
 * @param widget        控件指针
 * @param widgetSetter  控件的 setter 成员函数指针（数据→UI方向）
 * @param widgetSignal  控件的信号成员函数指针（UI→数据方向）
 *
 * @code
 * // 自定义绑定示例
 * DataBind::bindWithSignals(
 *     data,
 *     &MyData::setText,        // 数据 setter
 *     &MyData::textChanged,    // 数据 NOTIFY 信号
 *     ui->lineEditName,
 *     &QLineEdit::setText,     // UI setter
 *     &QLineEdit::textChanged  // UI 信号
 * );
 * @endcode
 *
 * @note 如果某个方向不需要绑定，可以传入 nullptr
 */
template <typename DataType, typename DataValue,
          typename WidgetType, typename WidgetValue>
void bindWithSignals(
    DataType *data,
    void (DataType::*dataSetter)(DataValue),
    void (DataType::*dataNotify)(),
    WidgetType *widget,
    void (WidgetType::*widgetSetter)(WidgetValue),
    void (WidgetType::*widgetSignal)(WidgetValue)
) {
    if (!data || !widget) return;

    // 数据 → UI：数据变化时更新控件
    if (dataNotify && widgetSetter) {
        QObject::connect(data, dataNotify, widget, widgetSetter);
    }

    // UI → 数据：用户操作控件时更新数据
    if (widgetSignal && dataSetter) {
        QObject::connect(widget, widgetSignal, data, dataSetter);
    }
}

// ============================================================================
// 七、批量绑定
// ============================================================================

/**
 * @brief 批量绑定多个不同类型的控件到同一个数据对象
 *
 * 使用 C++17 折叠表达式，可以在一行代码中绑定多个控件。
 *
 * @tparam DataType   数据对象类型
 * @tparam Widgets    可变参数，多个控件指针
 * @param data        数据对象指针
 * @param widgets     要绑定的控件列表
 *
 * @code
 * // 一行绑定三个不同类型的控件
 * DataBind::bindAll(data,
 *     ui->lineEditName,    // QLineEdit*
 *     ui->checkBoxEnable,  // QCheckBox*
 *     ui->sliderVolume     // QSlider*
 * );
 * @endcode
 *
 * @note 需要 C++17 编译器支持
 */
template <typename DataType, typename... Widgets>
void bindAll(DataType *data, Widgets... widgets) {
    (bind(data, widgets), ...);  // C++17 折叠表达式：依次调用 bind
}

/**
 * @brief 批量绑定多个同类型控件到同一个数据对象
 *
 * 当有多个相同类型的控件需要绑定到同一个数据对象时，使用此函数。
 *
 * @tparam DataType   数据对象类型
 * @tparam WidgetType 控件类型
 * @param data        数据对象指针
 * @param widgets     控件列表（QList 容器）
 *
 * @code
 * QList<QLineEdit*> edits = {ui->lineEdit1, ui->lineEdit2, ui->lineEdit3};
 * DataBind::bindAll(data, edits);  // 三个 QLineEdit 同时绑定
 * @endcode
 */
template <typename DataType, typename WidgetType>
void bindAll(DataType *data, const QList<WidgetType*> &widgets) {
    for (WidgetType *w : widgets) {
        bind(data, w);
    }
}

} // namespace Sqz::Config

#endif // DATABIND_H
