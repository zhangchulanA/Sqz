#ifndef UIBUILDER_H
#define UIBUILDER_H

// 直接包含所使用的基础类型，避免依赖传递包含导致的脆弱性
#include <QPointer>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QGroupBox>
#include <QComboBox>
#include <QProgressBar>
#include <QFont>
#include <QString>
#include <Qt>
#include "SqzGlobal.h"

namespace Sqz::Widget
{
    /**
     * @brief 通用控件 Builder 模板基类（CRTP 模式）
     *
     * 封装所有 QWidget 派生类的公共配置方法。通过 CRTP（Curiously Recurring
     * Template Pattern）使链式调用返回派生类引用，保证任意顺序的链式配置均可用。
     *
     * 注意：Builder 不接管控件所有权，调用方需自行通过 parent 或智能指针管理生命周期。
     *
     * @tparam Derived 派生 Builder 类型（如 LabelBuilder）
     * @tparam T       目标控件类型（如 QLabel）
     */
    template <typename Derived, typename T>
    class WidgetBuilderBase
    {
    public:
        /** @brief 构造函数，绑定目标控件指针（弱引用，不接管所有权） */
        explicit WidgetBuilderBase(T* w) : m_target(w) {}

        /** @brief 设置控件字体 */
        Derived& font(const QFont& ft)
        {
            if (m_target) m_target->setFont(ft);
            return static_cast<Derived&>(*this);
        }

        /** @brief 设置控件可见性 */
        Derived& visible(bool b)
        {
            if (m_target) m_target->setVisible(b);
            return static_cast<Derived&>(*this);
        }

        /** @brief 设置控件可用（启用/禁用）状态 */
        Derived& enable(bool b)
        {
            if (m_target) m_target->setEnabled(b);
            return static_cast<Derived&>(*this);
        }

        /** @brief 设置控件样式表（QSS 字符串） */
        Derived& setStyle(const QString& qss)
        {
            if (m_target) m_target->setStyleSheet(qss);
            return static_cast<Derived&>(*this);
        }

        /** @brief 设置控件固定尺寸（参数为负值时告警并忽略） */
        Derived& fixedSize(int w, int h)
        {
            // 校验尺寸参数：负值无效，避免 Qt 控件布局异常
            if (m_target && w >= 0 && h >= 0) {
                m_target->setFixedSize(w, h);
            } else if (w < 0 || h < 0) {
                qWarning("WidgetBuilder::fixedSize: invalid size(%d, %d), skipped", w, h);
            }
            return static_cast<Derived&>(*this);
        }

        /** @brief 获取底层控件指针（控件已被销毁时返回 nullptr） */
        T* widget() const { return m_target; }

    protected:
        QPointer<T> m_target;  ///< 目标控件弱引用，控件销毁时自动置空
    };

    // ===== 文本标签 Builder =====
    class SQZ_FRAMEWORK_API LabelBuilder : public WidgetBuilderBase<LabelBuilder, QLabel>
    {
        using Base = WidgetBuilderBase<LabelBuilder, QLabel>;
    public:
        /** @brief 构造函数，绑定 QLabel */
        explicit LabelBuilder(QLabel* w);

        /** @brief 设置标签文本 */
        LabelBuilder& text(const QString& str);
        /** @brief 设置文本对齐方式 */
        LabelBuilder& align(Qt::Alignment al);
        /** @brief 设置悬浮提示 */
        LabelBuilder& tooltip(const QString& tip);
        /** @brief 设置最小尺寸（负值告警并忽略） */
        LabelBuilder& minSize(int w, int h);
        /** @brief 设置最大尺寸（负值告警并忽略） */
        LabelBuilder& maxSize(int w, int h);
        /** @brief 设置是否自动换行 */
        LabelBuilder& wordWrap(bool on);

    protected:
        using Base::m_target;  ///< 使模板基类成员在派生类中可直接访问
    };

    // ===== 普通按钮 Builder =====
    class SQZ_FRAMEWORK_API ButtonBuilder : public WidgetBuilderBase<ButtonBuilder, QPushButton>
    {
        using Base = WidgetBuilderBase<ButtonBuilder, QPushButton>;
    public:
        /** @brief 构造函数，绑定 QPushButton */
        explicit ButtonBuilder(QPushButton* w);

        /** @brief 设置按钮文本 */
        ButtonBuilder& text(const QString& str);
        /** @brief 设置悬浮提示 */
        ButtonBuilder& tooltip(const QString& tip);
        /** @brief 设置是否为扁平按钮 */
        ButtonBuilder& flat(bool on);

    protected:
        using Base::m_target;
    };

    // ===== 单行输入框 Builder =====
    class SQZ_FRAMEWORK_API LineEditBuilder : public WidgetBuilderBase<LineEditBuilder, QLineEdit>
    {
        using Base = WidgetBuilderBase<LineEditBuilder, QLineEdit>;
    public:
        /** @brief 构造函数，绑定 QLineEdit */
        explicit LineEditBuilder(QLineEdit* w);

        /** @brief 设置文本内容 */
        LineEditBuilder& text(const QString& str);
        /** @brief 设置占位提示文本 */
        LineEditBuilder& placeholder(const QString& str);
        /** @brief 设置是否只读 */
        LineEditBuilder& readOnly(bool on);
        /** @brief 设置是否显示清除按钮 */
        LineEditBuilder& clearButton(bool show);

    protected:
        using Base::m_target;
    };

    // ===== 复选框 Builder =====
    class SQZ_FRAMEWORK_API CheckBoxBuilder : public WidgetBuilderBase<CheckBoxBuilder, QCheckBox>
    {
        using Base = WidgetBuilderBase<CheckBoxBuilder, QCheckBox>;
    public:
        /** @brief 构造函数，绑定 QCheckBox */
        explicit CheckBoxBuilder(QCheckBox* w);

        /** @brief 设置复选框文本 */
        CheckBoxBuilder& text(const QString& str);
        /** @brief 设置勾选状态 */
        CheckBoxBuilder& checked(bool b);

    protected:
        using Base::m_target;
    };

    // ===== 分组容器 Builder =====
    class SQZ_FRAMEWORK_API GroupBoxBuilder : public WidgetBuilderBase<GroupBoxBuilder, QGroupBox>
    {
        using Base = WidgetBuilderBase<GroupBoxBuilder, QGroupBox>;
    public:
        /** @brief 构造函数，绑定 QGroupBox */
        explicit GroupBoxBuilder(QGroupBox* w);

        /** @brief 设置分组标题 */
        GroupBoxBuilder& title(const QString& str);

    protected:
        using Base::m_target;
    };

    // ===== 下拉选择框 Builder =====
    class SQZ_FRAMEWORK_API ComboBoxBuilder : public WidgetBuilderBase<ComboBoxBuilder, QComboBox>
    {
        using Base = WidgetBuilderBase<ComboBoxBuilder, QComboBox>;
    public:
        /** @brief 构造函数，绑定 QComboBox */
        explicit ComboBoxBuilder(QComboBox* w);

        /** @brief 追加一个选项 */
        ComboBoxBuilder& addItem(const QString& txt);
        /** @brief 设置当前选中索引（越界告警并忽略） */
        ComboBoxBuilder& currentIndex(int idx);
        /** @brief 设置是否可编辑 */
        ComboBoxBuilder& editable(bool on);

    protected:
        using Base::m_target;
    };

    // ===== 进度条 Builder =====
    class SQZ_FRAMEWORK_API ProgressBarBuilder : public WidgetBuilderBase<ProgressBarBuilder, QProgressBar>
    {
        using Base = WidgetBuilderBase<ProgressBarBuilder, QProgressBar>;
    public:
        /** @brief 构造函数，绑定 QProgressBar */
        explicit ProgressBarBuilder(QProgressBar* w);

        /** @brief 设置数值范围（min>max 时告警并忽略） */
        ProgressBarBuilder& range(int min, int max);
        /** @brief 设置当前值 */
        ProgressBarBuilder& value(int val);
        /** @brief 设置是否显示文本 */
        ProgressBarBuilder& textVisible(bool on);

    protected:
        using Base::m_target;
    };

    // ===== 入口函数：生成临时链式 Builder 对象 =====
    // 注意：内联函数不应使用 DLL 导出宏（SQZ_FRAMEWORK_API），内联函数在调用点展开，
    //       不会作为符号进入共享库，导出无意义且可能触发编译告警。
    /** @brief 构建 QLabel 的链式配置入口 */
    inline LabelBuilder build(QLabel* w)             { return LabelBuilder(w); }
    /** @brief 构建 QPushButton 的链式配置入口 */
    inline ButtonBuilder build(QPushButton* w)       { return ButtonBuilder(w); }
    /** @brief 构建 QLineEdit 的链式配置入口 */
    inline LineEditBuilder build(QLineEdit* w)       { return LineEditBuilder(w); }
    /** @brief 构建 QCheckBox 的链式配置入口 */
    inline CheckBoxBuilder build(QCheckBox* w)       { return CheckBoxBuilder(w); }
    /** @brief 构建 QGroupBox 的链式配置入口 */
    inline GroupBoxBuilder build(QGroupBox* w)       { return GroupBoxBuilder(w); }
    /** @brief 构建 QComboBox 的链式配置入口 */
    inline ComboBoxBuilder build(QComboBox* w)       { return ComboBoxBuilder(w); }
    /** @brief 构建 QProgressBar 的链式配置入口 */
    inline ProgressBarBuilder build(QProgressBar* w) { return ProgressBarBuilder(w); }
}

#endif // UIBUILDER_H
