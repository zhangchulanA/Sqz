#ifndef UIBUILDER_H
#define UIBUILDER_H

#include <QPointer>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QGroupBox>
#include <QComboBox>
#include <QProgressBar>
#include <QFont>
#include <Qt>

namespace UiBuilder
{
    // 文本标签
    class LabelBuilder
    {
    public:
        explicit LabelBuilder(QLabel* w);

        LabelBuilder& text(const QString& str);
        LabelBuilder& font(const QFont& ft);
        LabelBuilder& align(Qt::Alignment al);
        LabelBuilder& tooltip(const QString& tip);
        LabelBuilder& visible(bool b);
        LabelBuilder& enable(bool b);
        LabelBuilder& fixedSize(int w, int h);
        LabelBuilder& minSize(int w, int h);
        LabelBuilder& maxSize(int w, int h);
        LabelBuilder& wordWrap(bool on);
        LabelBuilder& setStyle(const QString& qss);

    private:
        QPointer<QLabel> m_target;
    };

    // 普通按钮
    class ButtonBuilder
    {
    public:
        explicit ButtonBuilder(QPushButton* w);

        ButtonBuilder& text(const QString& str);
        ButtonBuilder& font(const QFont& ft);
        ButtonBuilder& tooltip(const QString& tip);
        ButtonBuilder& visible(bool b);
        ButtonBuilder& enable(bool b);
        ButtonBuilder& fixedSize(int w, int h);
        ButtonBuilder& flat(bool on);
        ButtonBuilder& setStyle(const QString& qss);

    private:
        QPointer<QPushButton> m_target;
    };

    // 单行输入框
    class LineEditBuilder
    {
    public:
        explicit LineEditBuilder(QLineEdit* w);

        LineEditBuilder& text(const QString& str);
        LineEditBuilder& placeholder(const QString& str);
        LineEditBuilder& font(const QFont& ft);
        LineEditBuilder& readOnly(bool on);
        LineEditBuilder& visible(bool b);
        LineEditBuilder& enable(bool b);
        LineEditBuilder& fixedSize(int w, int h);
        LineEditBuilder& clearButton(bool show);
        LineEditBuilder& setStyle(const QString& qss);

    private:
        QPointer<QLineEdit> m_target;
    };

    // 复选框
    class CheckBoxBuilder
    {
    public:
        explicit CheckBoxBuilder(QCheckBox* w);

        CheckBoxBuilder& text(const QString& str);
        CheckBoxBuilder& font(const QFont& ft);
        CheckBoxBuilder& checked(bool b);
        CheckBoxBuilder& visible(bool b);
        CheckBoxBuilder& enable(bool b);
        CheckBoxBuilder& setStyle(const QString& qss);

    private:
        QPointer<QCheckBox> m_target;
    };

    // 分组容器
    class GroupBoxBuilder
    {
    public:
        explicit GroupBoxBuilder(QGroupBox* w);

        GroupBoxBuilder& title(const QString& str);
        GroupBoxBuilder& font(const QFont& ft);
        GroupBoxBuilder& visible(bool b);
        GroupBoxBuilder& enable(bool b);
        GroupBoxBuilder& setStyle(const QString& qss);

    private:
        QPointer<QGroupBox> m_target;
    };

    // 下拉选择框
    class ComboBoxBuilder
    {
    public:
        explicit ComboBoxBuilder(QComboBox* w);

        ComboBoxBuilder& font(const QFont& ft);
        ComboBoxBuilder& addItem(const QString& txt);
        ComboBoxBuilder& currentIndex(int idx);
        ComboBoxBuilder& editable(bool on);
        ComboBoxBuilder& visible(bool b);
        ComboBoxBuilder& enable(bool b);
        ComboBoxBuilder& setStyle(const QString& qss);

    private:
        QPointer<QComboBox> m_target;
    };

    // 进度条
    class ProgressBarBuilder
    {
    public:
        explicit ProgressBarBuilder(QProgressBar* w);

        ProgressBarBuilder& range(int min, int max);
        ProgressBarBuilder& value(int val);
        ProgressBarBuilder& textVisible(bool on);
        ProgressBarBuilder& visible(bool b);
        ProgressBarBuilder& enable(bool b);
        ProgressBarBuilder& fixedSize(int w, int h);
        ProgressBarBuilder& setStyle(const QString& qss);

    private:
        QPointer<QProgressBar> m_target;
    };

    // 入口函数，生成临时链式对象
    inline LabelBuilder build(QLabel* w)             { return LabelBuilder(w); }
    inline ButtonBuilder build(QPushButton* w)       { return ButtonBuilder(w); }
    inline LineEditBuilder build(QLineEdit* w)      { return LineEditBuilder(w); }
    inline CheckBoxBuilder build(QCheckBox* w)       { return CheckBoxBuilder(w); }
    inline GroupBoxBuilder build(QGroupBox* w)       { return GroupBoxBuilder(w); }
    inline ComboBoxBuilder build(QComboBox* w)       { return ComboBoxBuilder(w); }
    inline ProgressBarBuilder build(QProgressBar* w) { return ProgressBarBuilder(w); }
}

#endif // UIBUILDER_H
