#include "UiBuilder.h"

namespace Sqz::Widget
{
    //===== LabelBuilder =====

    // 构造函数，转发给 CRTP 基类绑定目标控件
    LabelBuilder::LabelBuilder(QLabel* w) : Base(w) {}

    // 设置标签文本
    LabelBuilder& LabelBuilder::text(const QString& str)
    {
        if (m_target) m_target->setText(str);
        return *this;
    }

    // 设置文本对齐方式
    LabelBuilder& LabelBuilder::align(Qt::Alignment al)
    {
        if (m_target) m_target->setAlignment(al);
        return *this;
    }

    // 设置悬浮提示
    LabelBuilder& LabelBuilder::tooltip(const QString& tip)
    {
        if (m_target) m_target->setToolTip(tip);
        return *this;
    }

    // 设置最小尺寸（负值告警并忽略）
    LabelBuilder& LabelBuilder::minSize(int w, int h)
    {
        // 校验尺寸参数：负值无效，避免最小尺寸变为非法值
        if (m_target && w >= 0 && h >= 0) {
            m_target->setMinimumSize(w, h);
        } else if (w < 0 || h < 0) {
            qWarning("LabelBuilder::minSize: invalid size(%d, %d), skipped", w, h);
        }
        return *this;
    }

    // 设置最大尺寸（负值告警并忽略）
    LabelBuilder& LabelBuilder::maxSize(int w, int h)
    {
        // 校验尺寸参数：负值无效，避免最大尺寸变为非法值
        if (m_target && w >= 0 && h >= 0) {
            m_target->setMaximumSize(w, h);
        } else if (w < 0 || h < 0) {
            qWarning("LabelBuilder::maxSize: invalid size(%d, %d), skipped", w, h);
        }
        return *this;
    }

    // 设置是否自动换行
    LabelBuilder& LabelBuilder::wordWrap(bool on)
    {
        if (m_target) m_target->setWordWrap(on);
        return *this;
    }

    //===== ButtonBuilder =====

    // 构造函数，转发给 CRTP 基类绑定目标控件
    ButtonBuilder::ButtonBuilder(QPushButton* w) : Base(w) {}

    // 设置按钮文本
    ButtonBuilder& ButtonBuilder::text(const QString& str)
    {
        if (m_target) m_target->setText(str);
        return *this;
    }

    // 设置悬浮提示
    ButtonBuilder& ButtonBuilder::tooltip(const QString& tip)
    {
        if (m_target) m_target->setToolTip(tip);
        return *this;
    }

    // 设置是否为扁平按钮
    ButtonBuilder& ButtonBuilder::flat(bool on)
    {
        if (m_target) m_target->setFlat(on);
        return *this;
    }

    //===== LineEditBuilder =====

    // 构造函数，转发给 CRTP 基类绑定目标控件
    LineEditBuilder::LineEditBuilder(QLineEdit* w) : Base(w) {}

    // 设置文本内容
    LineEditBuilder& LineEditBuilder::text(const QString& str)
    {
        if (m_target) m_target->setText(str);
        return *this;
    }

    // 设置占位提示文本
    LineEditBuilder& LineEditBuilder::placeholder(const QString& str)
    {
        if (m_target) m_target->setPlaceholderText(str);
        return *this;
    }

    // 设置是否只读
    LineEditBuilder& LineEditBuilder::readOnly(bool on)
    {
        if (m_target) m_target->setReadOnly(on);
        return *this;
    }

    // 设置是否显示清除按钮
    LineEditBuilder& LineEditBuilder::clearButton(bool show)
    {
        if (m_target) m_target->setClearButtonEnabled(show);
        return *this;
    }

    //===== CheckBoxBuilder =====

    // 构造函数，转发给 CRTP 基类绑定目标控件
    CheckBoxBuilder::CheckBoxBuilder(QCheckBox* w) : Base(w) {}

    // 设置复选框文本
    CheckBoxBuilder& CheckBoxBuilder::text(const QString& str)
    {
        if (m_target) m_target->setText(str);
        return *this;
    }

    // 设置勾选状态
    CheckBoxBuilder& CheckBoxBuilder::checked(bool b)
    {
        if (m_target) m_target->setChecked(b);
        return *this;
    }

    //===== GroupBoxBuilder =====

    // 构造函数，转发给 CRTP 基类绑定目标控件
    GroupBoxBuilder::GroupBoxBuilder(QGroupBox* w) : Base(w) {}

    // 设置分组标题
    GroupBoxBuilder& GroupBoxBuilder::title(const QString& str)
    {
        if (m_target) m_target->setTitle(str);
        return *this;
    }

    //===== ComboBoxBuilder =====

    // 构造函数，转发给 CRTP 基类绑定目标控件
    ComboBoxBuilder::ComboBoxBuilder(QComboBox* w) : Base(w) {}

    // 追加一个选项
    ComboBoxBuilder& ComboBoxBuilder::addItem(const QString& txt)
    {
        if (m_target) m_target->addItem(txt);
        return *this;
    }

    // 设置当前选中索引（越界告警并忽略）
    ComboBoxBuilder& ComboBoxBuilder::currentIndex(int idx)
    {
        // -1 表示无选中项，合法范围 [-1, count-1]；越界时静默失败易引发逻辑 bug
        if (m_target && idx >= -1 && idx < m_target->count()) {
            m_target->setCurrentIndex(idx);
        } else if (m_target) {
            qWarning("ComboBoxBuilder::currentIndex: index %d out of range [-1, %d), skipped",
                     idx, m_target->count());
        }
        return *this;
    }

    // 设置是否可编辑
    ComboBoxBuilder& ComboBoxBuilder::editable(bool on)
    {
        if (m_target) m_target->setEditable(on);
        return *this;
    }

    //===== ProgressBarBuilder =====

    // 构造函数，转发给 CRTP 基类绑定目标控件
    ProgressBarBuilder::ProgressBarBuilder(QProgressBar* w) : Base(w) {}

    // 设置数值范围（min>max 时告警并忽略）
    ProgressBarBuilder& ProgressBarBuilder::range(int min, int max)
    {
        // 校验 min <= max，避免进度条行为异常（min==max==0 仍允许，表示忙碌指示器）
        if (m_target && min <= max) {
            m_target->setRange(min, max);
        } else if (min > max) {
            qWarning("ProgressBarBuilder::range: min(%d) > max(%d), skipped", min, max);
        }
        return *this;
    }

    // 设置当前值
    ProgressBarBuilder& ProgressBarBuilder::value(int val)
    {
        if (m_target) m_target->setValue(val);
        return *this;
    }

    // 设置是否显示文本
    ProgressBarBuilder& ProgressBarBuilder::textVisible(bool on)
    {
        if (m_target) m_target->setTextVisible(on);
        return *this;
    }
}
