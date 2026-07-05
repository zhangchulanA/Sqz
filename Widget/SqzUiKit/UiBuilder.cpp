#include "UiBuilder.h"

namespace UiBuilder
{
    //===== LabelBuilder =====
    LabelBuilder::LabelBuilder(QLabel* w) : m_target(w) {}

    LabelBuilder& LabelBuilder::text(const QString& str)
    {
        if(m_target) m_target->setText(str);
        return *this;
    }
    LabelBuilder& LabelBuilder::font(const QFont& ft)
    {
        if(m_target) m_target->setFont(ft);
        return *this;
    }
    LabelBuilder& LabelBuilder::align(Qt::Alignment al)
    {
        if(m_target) m_target->setAlignment(al);
        return *this;
    }
    LabelBuilder& LabelBuilder::tooltip(const QString& tip)
    {
        if(m_target) m_target->setToolTip(tip);
        return *this;
    }
    LabelBuilder& LabelBuilder::visible(bool b)
    {
        if(m_target) m_target->setVisible(b);
        return *this;
    }
    LabelBuilder& LabelBuilder::enable(bool b)
    {
        if(m_target) m_target->setEnabled(b);
        return *this;
    }
    LabelBuilder& LabelBuilder::fixedSize(int w, int h)
    {
        if(m_target) m_target->setFixedSize(w, h);
        return *this;
    }
    LabelBuilder& LabelBuilder::minSize(int w, int h)
    {
        if(m_target) m_target->setMinimumSize(w, h);
        return *this;
    }
    LabelBuilder& LabelBuilder::maxSize(int w, int h)
    {
        if(m_target) m_target->setMaximumSize(w, h);
        return *this;
    }
    LabelBuilder& LabelBuilder::wordWrap(bool on)
    {
        if(m_target) m_target->setWordWrap(on);
        return *this;
    }
    LabelBuilder& LabelBuilder::setStyle(const QString& qss)
    {
        if(m_target) m_target->setStyleSheet(qss);
        return *this;
    }

    //===== ButtonBuilder =====
    ButtonBuilder::ButtonBuilder(QPushButton* w) : m_target(w) {}

    ButtonBuilder& ButtonBuilder::text(const QString& str)
    {
        if(m_target) m_target->setText(str);
        return *this;
    }
    ButtonBuilder& ButtonBuilder::font(const QFont& ft)
    {
        if(m_target) m_target->setFont(ft);
        return *this;
    }
    ButtonBuilder& ButtonBuilder::tooltip(const QString& tip)
    {
        if(m_target) m_target->setToolTip(tip);
        return *this;
    }
    ButtonBuilder& ButtonBuilder::visible(bool b)
    {
        if(m_target) m_target->setVisible(b);
        return *this;
    }
    ButtonBuilder& ButtonBuilder::enable(bool b)
    {
        if(m_target) m_target->setEnabled(b);
        return *this;
    }
    ButtonBuilder& ButtonBuilder::fixedSize(int w, int h)
    {
        if(m_target) m_target->setFixedSize(w, h);
        return *this;
    }
    ButtonBuilder& ButtonBuilder::flat(bool on)
    {
        if(m_target) m_target->setFlat(on);
        return *this;
    }
    ButtonBuilder& ButtonBuilder::setStyle(const QString& qss)
    {
        if(m_target) m_target->setStyleSheet(qss);
        return *this;
    }

    //===== LineEditBuilder =====
    LineEditBuilder::LineEditBuilder(QLineEdit* w) : m_target(w) {}

    LineEditBuilder& LineEditBuilder::text(const QString& str)
    {
        if(m_target) m_target->setText(str);
        return *this;
    }
    LineEditBuilder& LineEditBuilder::placeholder(const QString& str)
    {
        if(m_target) m_target->setPlaceholderText(str);
        return *this;
    }
    LineEditBuilder& LineEditBuilder::font(const QFont& ft)
    {
        if(m_target) m_target->setFont(ft);
        return *this;
    }
    LineEditBuilder& LineEditBuilder::readOnly(bool on)
    {
        if(m_target) m_target->setReadOnly(on);
        return *this;
    }
    LineEditBuilder& LineEditBuilder::visible(bool b)
    {
        if(m_target) m_target->setVisible(b);
        return *this;
    }
    LineEditBuilder& LineEditBuilder::enable(bool b)
    {
        if(m_target) m_target->setEnabled(b);
        return *this;
    }
    LineEditBuilder& LineEditBuilder::fixedSize(int w, int h)
    {
        if(m_target) m_target->setFixedSize(w, h);
        return *this;
    }
    LineEditBuilder& LineEditBuilder::clearButton(bool show)
    {
        if(m_target) m_target->setClearButtonEnabled(show);
        return *this;
    }
    LineEditBuilder& LineEditBuilder::setStyle(const QString& qss)
    {
        if(m_target) m_target->setStyleSheet(qss);
        return *this;
    }

    //===== CheckBoxBuilder =====
    CheckBoxBuilder::CheckBoxBuilder(QCheckBox* w) : m_target(w) {}

    CheckBoxBuilder& CheckBoxBuilder::text(const QString& str)
    {
        if(m_target) m_target->setText(str);
        return *this;
    }
    CheckBoxBuilder& CheckBoxBuilder::font(const QFont& ft)
    {
        if(m_target) m_target->setFont(ft);
        return *this;
    }
    CheckBoxBuilder& CheckBoxBuilder::checked(bool b)
    {
        if(m_target) m_target->setChecked(b);
        return *this;
    }
    CheckBoxBuilder& CheckBoxBuilder::visible(bool b)
    {
        if(m_target) m_target->setVisible(b);
        return *this;
    }
    CheckBoxBuilder& CheckBoxBuilder::enable(bool b)
    {
        if(m_target) m_target->setEnabled(b);
        return *this;
    }
    CheckBoxBuilder& CheckBoxBuilder::setStyle(const QString& qss)
    {
        if(m_target) m_target->setStyleSheet(qss);
        return *this;
    }

    //===== GroupBoxBuilder =====
    GroupBoxBuilder::GroupBoxBuilder(QGroupBox* w) : m_target(w) {}

    GroupBoxBuilder& GroupBoxBuilder::title(const QString& str)
    {
        if(m_target) m_target->setTitle(str);
        return *this;
    }
    GroupBoxBuilder& GroupBoxBuilder::font(const QFont& ft)
    {
        if(m_target) m_target->setFont(ft);
        return *this;
    }
    GroupBoxBuilder& GroupBoxBuilder::visible(bool b)
    {
        if(m_target) m_target->setVisible(b);
        return *this;
    }
    GroupBoxBuilder& GroupBoxBuilder::enable(bool b)
    {
        if(m_target) m_target->setEnabled(b);
        return *this;
    }
    GroupBoxBuilder& GroupBoxBuilder::setStyle(const QString& qss)
    {
        if(m_target) m_target->setStyleSheet(qss);
        return *this;
    }

    //===== ComboBoxBuilder =====
    ComboBoxBuilder::ComboBoxBuilder(QComboBox* w) : m_target(w) {}

    ComboBoxBuilder& ComboBoxBuilder::font(const QFont& ft)
    {
        if(m_target) m_target->setFont(ft);
        return *this;
    }
    ComboBoxBuilder& ComboBoxBuilder::addItem(const QString& txt)
    {
        if(m_target) m_target->addItem(txt);
        return *this;
    }
    ComboBoxBuilder& ComboBoxBuilder::currentIndex(int idx)
    {
        if(m_target) m_target->setCurrentIndex(idx);
        return *this;
    }
    ComboBoxBuilder& ComboBoxBuilder::editable(bool on)
    {
        if(m_target) m_target->setEditable(on);
        return *this;
    }
    ComboBoxBuilder& ComboBoxBuilder::visible(bool b)
    {
        if(m_target) m_target->setVisible(b);
        return *this;
    }
    ComboBoxBuilder& ComboBoxBuilder::enable(bool b)
    {
        if(m_target) m_target->setEnabled(b);
        return *this;
    }
    ComboBoxBuilder& ComboBoxBuilder::setStyle(const QString& qss)
    {
        if(m_target) m_target->setStyleSheet(qss);
        return *this;
    }

    //===== ProgressBarBuilder =====
    ProgressBarBuilder::ProgressBarBuilder(QProgressBar* w) : m_target(w) {}

    ProgressBarBuilder& ProgressBarBuilder::range(int min, int max)
    {
        if(m_target) m_target->setRange(min, max);
        return *this;
    }
    ProgressBarBuilder& ProgressBarBuilder::value(int val)
    {
        if(m_target) m_target->setValue(val);
        return *this;
    }
    ProgressBarBuilder& ProgressBarBuilder::textVisible(bool on)
    {
        if(m_target) m_target->setTextVisible(on);
        return *this;
    }
    ProgressBarBuilder& ProgressBarBuilder::visible(bool b)
    {
        if(m_target) m_target->setVisible(b);
        return *this;
    }
    ProgressBarBuilder& ProgressBarBuilder::enable(bool b)
    {
        if(m_target) m_target->setEnabled(b);
        return *this;
    }
    ProgressBarBuilder& ProgressBarBuilder::fixedSize(int w, int h)
    {
        if(m_target) m_target->setFixedSize(w, h);
        return *this;
    }
    ProgressBarBuilder& ProgressBarBuilder::setStyle(const QString& qss)
    {
        if(m_target) m_target->setStyleSheet(qss);
        return *this;
    }
}
