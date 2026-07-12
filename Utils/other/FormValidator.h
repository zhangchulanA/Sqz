#ifndef FORMVALIDATOR_H
#define FORMVALIDATOR_H

#include <QObject>
#include <QWidget>
#include <QPointer>
#include <QList>
#include <QHash>
#include <QString>
#include <QTimer>
#include <functional>
#include "SqzGlobal.h"

namespace Sqz {
class SQZ_FRAMEWORK_API FormValidator;

/**
 * @brief 校验结果结构体
 *
 * 包含校验是否通过（ok）以及失败时的错误提示信息（errorMessage）。
 */
struct SQZ_FRAMEWORK_API ValidationResult {
    bool ok;                // 是否通过校验
    QString errorMessage;   // 校验失败时的提示文字

    ValidationResult(bool ok = true, const QString& msg = QString())
        : ok(ok), errorMessage(msg) {}
};

/**
 * @brief 校验规则函数类型
 *
 * 接收控件的当前文本（或值），返回 ValidationResult。
 * 用户可以自定义 lambda 或函数对象来实现任意复杂的校验逻辑。
 */
typedef std::function<ValidationResult(const QString& text)> ValidateFunc;

/**
 * @brief 预定义规则工厂类（静态方法）
 *
 * 提供常用的校验规则，方便快速构建。
 */
class SQZ_FRAMEWORK_API Rules {
public:
    // 非空校验（不允许空白字符）
    static ValidateFunc notNull(const QString& errMsg = QString("此项不能为空"));
    // 最小长度校验
    static ValidateFunc minLength(int len, const QString& errMsg = QString());
    // 最大长度校验
    static ValidateFunc maxLength(int len, const QString& errMsg = QString());
    // 长度范围校验（包含两端）
    static ValidateFunc lengthRange(int min, int max, const QString& errMsg = QString());
    // 正则表达式匹配校验（精确匹配）
    static ValidateFunc regex(const QRegExp& rx, const QString& errMsg = QString());
    // 简单邮箱格式校验
    static ValidateFunc email(const QString& errMsg = QString("请输入有效的邮箱地址"));
    // 自定义布尔条件校验（方便将简单条件转为规则）
    static ValidateFunc custom(std::function<bool(const QString&)> check,
                               const QString& errMsg = QString());
};

/**
 * @brief 单个控件的校验配置信息
 *
 * 存储控件相关的校验规则列表、原始样式表、去抖定时器等。
 * 用于内部维护。
 */
struct SQZ_FRAMEWORK_API ControlValidation {
    QPointer<QWidget> widget;          // 待校验的控件指针（弱引用，防止悬空）
    QList<ValidateFunc> rules;          // 该控件的所有校验规则（按添加顺序执行）
    QString originalStyleSheet;         // 控件原始的样式表，用于恢复
    bool isRequired;                    // 是否为必填字段（快速设置非空校验，暂未完全使用）
    QTimer* debounceTimer;              // 实时校验时的去抖定时器（避免频繁校验）
    bool realtime;                      // 是否启用实时校验（true: 输入变化时校验；false: 焦点离开时校验）

    ControlValidation() : isRequired(false), debounceTimer(nullptr), realtime(false) {}
};

/**
 * @class FormValidator
 * @brief 表单字段校验器，支持多规则、实时/失焦校验、错误样式和全局批量校验。
 *
 * 【主要功能】
 * - 为 QLineEdit、QComboBox、QTextEdit 等控件添加一条或多条校验规则。
 * - 支持实时校验（输入时去抖校验）和焦点离开校验两种模式，可针对每个控件独立配置。
 * - 校验失败时自动应用错误样式（默认红色边框）并显示 ToolTip 错误消息。
 * - 提供 validateAll() 方法一次性校验所有已注册控件，适合表单提交场景。
 * - 支持自定义校验规则（lambda），可轻松实现字段联动校验（如“确认密码”）。
 * - 内置常用规则：非空、长度限制、正则、邮箱等，并可扩展。
 *
 * 【使用方式】
 * 1. 创建 FormValidator 实例。
 * 2. 调用 addRule() 或 addRules() 为特定控件添加校验规则。
 * 3. 可选：调用 setRealtime() 开启实时校验模式。
 * 4. 在需要提交表单时调用 validateAll() 判断是否全部通过。
 * 5. 也可以单独调用 validateWidget() 校验某个控件。
 *
 * 示例：
 * @code
 * FormValidator validator;
 * QLineEdit *nameEdit = new QLineEdit;
 * validator.addRules(nameEdit, {
 *     Rules::notNull("请输入姓名"),
 *     Rules::maxLength(20, "姓名不能超过20个字符")
 * });
 * validator.setRealtime(nameEdit, true);
 *
 * QPushButton *submitBtn = new QPushButton("提交");
 * connect(submitBtn, &QPushButton::clicked, [&]{
 *     if (validator.validateAll()) {
 *         // 所有字段有效，执行提交
 *     }
 * });
 * @endcode
 *
 * 【注意事项】
 * - 控件被销毁时，校验器会自动忽略该控件（使用 QPointer），不会崩溃。
 * - 实时校验模式使用了 200ms 的去抖定时器，避免每次按键都触发校验。
 * - 错误样式可以通过 setErrorStyle() 单独设置，或通过静态方法 setDefaultErrorStyle() 全局修改。
 * - 对于 QComboBox，校验基于 currentText()，需要确保有有效的文本。
 * - 联动校验（如确认密码）建议通过自定义规则访问其他控件的值来实现。
 */
class SQZ_FRAMEWORK_API FormValidator : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param parent 父对象，用于 Qt 对象树管理
     */
    explicit FormValidator(QObject *parent = nullptr);

    /**
     * @brief 析构函数，清理定时器等资源
     */
    ~FormValidator();

    /**
     * @brief 为控件添加一条校验规则
     * @param widget 待校验的控件指针（支持 QLineEdit, QComboBox, QTextEdit 等）
     * @param rule  校验规则函数（ValidateFunc 类型）
     *
     * 如果该控件尚未注册，会自动注册并安装必要的事件过滤器/信号连接。
     * 可以多次调用为同一个控件添加多条规则，校验时按添加顺序依次执行，
     * 遇到第一个失败规则即停止并返回该规则的错误信息。
     */
    void addRule(QWidget* widget, const ValidateFunc& rule);

    /**
     * @brief 为控件批量添加多条校验规则
     * @param widget 待校验的控件
     * @param rules  校验规则列表
     *
     * 循环调用 addRule() 实现。
     */
    void addRules(QWidget* widget, const QList<ValidateFunc>& rules);

    /**
     * @brief 快速设置控件的必填（非空）校验
     * @param widget  控件
     * @param required 是否为必填（true 添加非空规则，false 不做任何操作，暂不支持移除）
     * @param errMsg  自定义的错误消息（可选）
     *
     * 实际上调用了 addRule(widget, Rules::notNull(errMsg))。
     */
    void setRequired(QWidget* widget, bool required = true,
                     const QString& errMsg = QString("此项不能为空"));

    /**
     * @brief 设置控件是否启用实时校验模式
     * @param widget   控件
     * @param realtime true：输入变化时自动校验（带去抖）；false：焦点离开时校验
     *
     * 实时模式下，控件的内容发生变化（如 QLineEdit 的 textChanged 信号）会触发一个 200ms 的去抖定时器，
     * 停止输入后 200ms 才执行校验，避免频繁校验影响性能。
     * 非实时模式下，将在控件失去焦点（FocusOut）或 editingFinished 时校验。
     */
    void setRealtime(QWidget* widget, bool realtime);

    /**
     * @brief 手动为控件设置错误样式（覆盖默认样式）
     * @param widget     控件
     * @param styleSheet 样式表字符串
     *
     * 通常不需要调用此方法，因为校验器在校验失败时会自动应用默认错误样式。
     * 此方法可用于特殊情况下的自定义样式。
     */
    void setErrorStyle(QWidget* widget, const QString& styleSheet);

    /**
     * @brief 清除控件的错误样式，恢复原始样式
     * @param widget 控件
     *
     * 内部自动调用，通常在控件校验通过时执行。
     */
    void clearErrorStyle(QWidget* widget);

    /**
     * @brief 单独校验某个控件
     * @param widget 控件
     * @return 是否通过校验
     *
     * 执行该控件的所有规则，并自动应用/清除错误样式，同时发射 validityChanged 信号。
     * 如果控件尚未注册，返回 true。
     */
    bool validateWidget(QWidget* widget);

    /**
     * @brief 校验所有已注册的控件
     * @return 是否全部通过校验
     *
     * 遍历所有控件，依次调用 validateWidget()。
     * 只要有一个控件校验失败，返回 false。
     * 常用于表单提交按钮的点击处理。
     */
    bool validateAll();

    /**
     * @brief 获取某个控件最后一次校验失败时的错误消息
     * @param widget 控件
     * @return 错误消息字符串（当前实现简化，未存储）
     *
     * 【注意】当前版本未实现存储历史错误消息，此函数预留接口，暂返回空字符串。
     * 实际错误消息可以通过捕获 validityChanged 信号获得，或直接使用控件的 toolTip。
     */
    QString lastErrorMessage(QWidget* widget) const;

    /**
     * @brief 设置全局默认的错误样式
     * @param style 样式表字符串
     *
     * 静态方法，影响所有 FormValidator 实例。
     * 默认样式为： "border: 1px solid red; background-color: #ffeeee;"
     */
    static void setDefaultErrorStyle(const QString& style);

signals:
    /**
     * @brief 当任一控件的校验状态改变时发出
     * @param widget 控件指针
     * @param valid  是否有效（true: 通过校验）
     *
     * 在 validateWidget 内部调用 applyValidationResult 后发射。
     * 可用于界面联动，例如根据表单整体有效性启用/禁用提交按钮。
     */
    void validityChanged(QWidget* widget, bool valid);

protected:
    /**
     * @brief 事件过滤器，用于捕获控件的焦点离开等事件
     * @param obj  事件接收对象
     * @param event 事件对象
     * @return 是否已处理
     *
     * 在非实时模式下，监听 FocusOut 事件触发校验。
     */
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    /**
     * @brief 去抖定时器超时槽函数
     * @param widget 触发校验的控件
     *
     * 由每个控件的 debounceTimer 定时器触发，调用 validateWidget(widget)。
     */
    void onDebounceTimeout(QWidget* widget);

private:
    // 存储所有已注册控件的校验配置，键为控件指针
    QHash<QWidget*, ControlValidation> m_validations;

    // 全局默认错误样式（静态成员）
    static QString s_defaultErrorStyle;

    /**
     * @brief 获取控件的当前文本内容
     * @param widget 控件
     * @return 文本内容
     *
     * 支持 QLineEdit、QComboBox、QTextEdit、QPlainTextEdit。
     * 对于其他控件类型，返回空字符串。
     */
    QString getWidgetText(QWidget* widget) const;

    /**
     * @brief 执行具体的校验逻辑（不改变控件样式）
     * @param widget 控件
     * @return ValidationResult 包含是否通过和错误消息
     *
     * 遍历该控件的所有规则，依次执行，返回第一个失败的结果；
     * 如果全部通过，返回 ok=true。
     */
    ValidationResult checkWidget(QWidget* widget) const;

    /**
     * @brief 应用校验结果到控件（设置/清除样式，设置 ToolTip，发射信号）
     * @param widget 控件
     * @param result 校验结果
     *
     * 内部调用 setErrorStyle 或 clearErrorStyle，并设置控件的 toolTip。
     * 最后发射 validityChanged 信号。
     */
    void applyValidationResult(QWidget* widget, const ValidationResult& result);
};
}
#endif // FORMVALIDATOR_H
