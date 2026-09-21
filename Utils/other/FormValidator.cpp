#include "FormValidator.h"
#include <QLineEdit>
#include <QComboBox>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QEvent>
#include <QRegExpValidator>
#include <QRegExp>
#include <QTimer>
#include <QDebug>
namespace Sqz {
// 静态成员初始化：默认错误样式（红色边框 + 淡红背景）
QString FormValidator::s_defaultErrorStyle = "border: 1px solid red; background-color: #ffeeee;";

// ---------- FormValidator 类实现 ----------

FormValidator::FormValidator(QObject *parent) : QObject(parent)
{
}

FormValidator::~FormValidator()
{
    // 清理所有控件的去抖定时器
    for (auto& val : m_validations) {
        if (val.debounceTimer) {
            val.debounceTimer->stop();
            delete val.debounceTimer;
            val.debounceTimer = nullptr;
        }
    }
}

void FormValidator::addRule(QWidget* widget, const ValidateFunc& rule)
{
    if (!widget) return;
    // 如果控件尚未注册，创建配置项并安装事件过滤器/信号连接
    if (!m_validations.contains(widget)) {
        ControlValidation cv;
        cv.widget = widget;
        cv.originalStyleSheet = widget->styleSheet();   // 保存原始样式，用于恢复
        cv.debounceTimer = nullptr;
        cv.realtime = false;
        m_validations[widget] = cv;
        widget->installEventFilter(this);                // 安装事件过滤器以捕获焦点事件

        // 根据控件类型连接相应信号
        if (auto lineEdit = qobject_cast<QLineEdit*>(widget)) {
            // 非实时模式：编辑完成时（按下回车或焦点离开）触发校验
            connect(lineEdit, &QLineEdit::editingFinished, [this, widget]() {
                if (!m_validations[widget].realtime) validateWidget(widget);
            });
            // 实时模式：文本改变时启动去抖定时器（定时器在 setRealtime 中创建）
            connect(lineEdit, &QLineEdit::textChanged, [this, widget](const QString&) {
                if (m_validations[widget].realtime) {
                    if (m_validations[widget].debounceTimer)
                        m_validations[widget].debounceTimer->start();
                }
            });
        } else if (auto combo = qobject_cast<QComboBox*>(widget)) {
            // 当下拉框当前项改变时触发校验（根据模式决定立即还是去抖）
            connect(combo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                    [this, widget](int) {
                        if (m_validations[widget].realtime) {
                            if (m_validations[widget].debounceTimer)
                                m_validations[widget].debounceTimer->start();
                        } else {
                            validateWidget(widget);
                        }
                    });
        }
        // 类似可添加 QTextEdit 等控件的支持，为简洁起见，此处仅支持上述两种。
        // 用户也可手动调用 validateWidget 来触发校验。
    }
    // 添加规则
    m_validations[widget].rules.append(rule);
}

void FormValidator::addRules(QWidget* widget, const QList<ValidateFunc>& rules)
{
    for (const auto& rule : rules)
        addRule(widget, rule);
}

void FormValidator::setRequired(QWidget* widget, bool required, const QString& errMsg)
{
    if (required) {
        addRule(widget, Rules::notNull(errMsg));
    }
    // 如果 required == false，本实现不做移除非空规则的操作，用户可以自行调用其它接口扩展。
}

void FormValidator::setRealtime(QWidget* widget, bool realtime)
{
    if (m_validations.contains(widget)) {
        auto& cv = m_validations[widget];
        cv.realtime = realtime;
        if (realtime && !cv.debounceTimer) {
            // 创建去抖定时器，超时时间 200ms，单次触发
            auto timer = new QTimer(this);
            timer->setSingleShot(true);
            timer->setInterval(200);
            connect(timer, &QTimer::timeout, [this, widget]() {
                onDebounceTimeout(widget);
            });
            cv.debounceTimer = timer;
        } else if (!realtime && cv.debounceTimer) {
            // 关闭实时模式时销毁定时器
            cv.debounceTimer->stop();
            delete cv.debounceTimer;
            cv.debounceTimer = nullptr;
        }
    }
}

void FormValidator::setErrorStyle(QWidget* widget, const QString& styleSheet)
{
    if (!widget) return;
    widget->setStyleSheet(styleSheet);
}

void FormValidator::clearErrorStyle(QWidget* widget)
{
    if (!widget) return;
    if (m_validations.contains(widget)) {
        widget->setStyleSheet(m_validations[widget].originalStyleSheet);
    } else {
        widget->setStyleSheet("");  // 未注册则清空样式
    }
}

bool FormValidator::validateWidget(QWidget* widget)
{
    if (!m_validations.contains(widget)) return true;   // 未注册的控件视为有效
    ValidationResult result = checkWidget(widget);
    applyValidationResult(widget, result);
    emit validityChanged(widget, result.ok);
    return result.ok;
}

bool FormValidator::validateAll()
{
    bool allOk = true;
    // 遍历所有注册的控件
    for (auto it = m_validations.begin(); it != m_validations.end(); ++it) {
        if (!validateWidget(it.key())) {
            allOk = false;
            // 如果需要自动聚焦到第一个失败控件，可以取消下面的注释
            // if (it.key()) it.key()->setFocus();
        }
    }
    return allOk;
}

QString FormValidator::lastErrorMessage(QWidget* widget) const
{
    // 当前简化实现未存储错误消息，可扩展为缓存每个控件的最后错误消息
    Q_UNUSED(widget);
    return QString();
}

void FormValidator::setDefaultErrorStyle(const QString& style)
{
    s_defaultErrorStyle = style;
}

bool FormValidator::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::FocusOut) {
        QWidget* widget = qobject_cast<QWidget*>(obj);
        if (widget && m_validations.contains(widget) && !m_validations[widget].realtime) {
            // 非实时模式下，焦点离开时触发校验
            validateWidget(widget);
        }
    }
    return QObject::eventFilter(obj, event);
}

void FormValidator::onDebounceTimeout(QWidget* widget)
{
    if (widget && m_validations.contains(widget)) {
        validateWidget(widget);
    }
}

QString FormValidator::getWidgetText(QWidget* widget) const
{
    if (auto lineEdit = qobject_cast<QLineEdit*>(widget)) {
        return lineEdit->text();
    } else if (auto combo = qobject_cast<QComboBox*>(widget)) {
        return combo->currentText();
    } else if (auto textEdit = qobject_cast<QTextEdit*>(widget)) {
        return textEdit->toPlainText();
    } else if (auto plainEdit = qobject_cast<QPlainTextEdit*>(widget)) {
        return plainEdit->toPlainText();
    }
    return QString();
}

ValidationResult FormValidator::checkWidget(QWidget* widget) const
{
    if (!m_validations.contains(widget))
        return ValidationResult(true);
    const auto& cv = m_validations[widget];
    QString text = getWidgetText(widget);
    // 依次执行规则
    for (const auto& rule : cv.rules) {
        ValidationResult res = rule(text);
        if (!res.ok) {
            return res;   // 返回第一个失败的结果
        }
    }
    return ValidationResult(true);
}

void FormValidator::applyValidationResult(QWidget* widget, const ValidationResult& result)
{
    if (!widget) return;
    if (result.ok) {
        clearErrorStyle(widget);
        widget->setToolTip("");   // 清除错误提示
    } else {
        setErrorStyle(widget, s_defaultErrorStyle);
        widget->setToolTip(result.errorMessage);   // 显示错误信息
    }
}

// ---------- Rules 预定义规则实现 ----------

ValidateFunc Rules::notNull(const QString& errMsg)
{
    QString msg = errMsg.isEmpty() ? "此项不能为空" : errMsg;
    return [msg](const QString& text) -> ValidationResult {
        if (text.trimmed().isEmpty())
            return ValidationResult(false, msg);
        return ValidationResult(true);
    };
}

ValidateFunc Rules::minLength(int len, const QString& errMsg)
{
    QString msg = errMsg.isEmpty() ? QString("长度不能少于 %1 个字符").arg(len) : errMsg;
    return [len, msg](const QString& text) -> ValidationResult {
        if (text.length() < len)
            return ValidationResult(false, msg);
        return ValidationResult(true);
    };
}

ValidateFunc Rules::maxLength(int len, const QString& errMsg)
{
    QString msg = errMsg.isEmpty() ? QString("长度不能超过 %1 个字符").arg(len) : errMsg;
    return [len, msg](const QString& text) -> ValidationResult {
        if (text.length() > len)
            return ValidationResult(false, msg);
        return ValidationResult(true);
    };
}

ValidateFunc Rules::lengthRange(int min, int max, const QString& errMsg)
{
    QString msg = errMsg.isEmpty() ? QString("长度应为 %1~%2 个字符").arg(min).arg(max) : errMsg;
    return [min, max, msg](const QString& text) -> ValidationResult {
        int len = text.length();
        if (len < min || len > max)
            return ValidationResult(false, msg);
        return ValidationResult(true);
    };
}

ValidateFunc Rules::regex(const QRegExp& rx, const QString& errMsg)
{
    QString msg = errMsg.isEmpty() ? QString("格式不正确") : errMsg;
    return [rx, msg](const QString& text) -> ValidationResult {
        if (rx.exactMatch(text))
            return ValidationResult(true);
        return ValidationResult(false, msg);
    };
}

ValidateFunc Rules::email(const QString& errMsg)
{
    // 简单的邮箱正则表达式
    QRegExp emailRx("^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$");
    return regex(emailRx, errMsg.isEmpty() ? "请输入有效的邮箱地址" : errMsg);
}

ValidateFunc Rules::custom(std::function<bool(const QString&)> check, const QString& errMsg)
{
    QString msg = errMsg.isEmpty() ? "校验失败" : errMsg;
    return [check, msg](const QString& text) -> ValidationResult {
        if (check(text))
            return ValidationResult(true);
        return ValidationResult(false, msg);
    };
}
}
