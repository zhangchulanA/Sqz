/*****************************************************************************
** 文件名:  CustomSearchBox.cpp
** 功能描述: 搜索框控件的完整实现，包含所有接口函数的具体逻辑。
** 说明:     配合 CustomSearchBox.h 使用，无需额外依赖。
** 修正:     1. 修复 setFixedSize 时的递归异常（添加 m_resizing 保护）
**           2. 极端尺寸下自动缩放字体，保证文字完整显示
**           3. 设置最小尺寸 (80,25) 避免过度缩小
**           4. 按钮样式强制 white-space: nowrap 防止文字换行
*****************************************************************************/

#include "CustomSearchBox.h"
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QStringListModel>
#include <QResizeEvent>
#include <QDebug>

// 常量定义
static const int MAX_HISTORY_COUNT = 20;          // 最大历史记录条数
static const int DEFAULT_BUTTON_MIN_WIDTH = 40;   // 默认按钮最小宽度（像素）
static const int DEFAULT_LINEEDIT_STRETCH = 5;    // 默认输入框拉伸因子
static const int DEFAULT_BUTTON_STRETCH = 1;      // 默认按钮拉伸因子
static const int DEFAULT_SPACING = 2;             // 默认间距
namespace Sqz {
/*! 构造函数实现 */
CustomSearchBox::CustomSearchBox(QWidget *parent, const QString &historyKey)
    : QWidget(parent)
    , m_lineEdit(nullptr)
    , m_searchButton(nullptr)
    , m_settings(QSettings::IniFormat, QSettings::UserScope,
                  "YourAppName", "CustomSearchBox")  // 组织名和应用名可按需修改
    , m_historyKey(historyKey.isEmpty() ? "searchHistory" : historyKey)
    , m_historyCompleter(nullptr)
    , m_customCompleter(nullptr)
    , m_useCustomCompleter(false)
    , m_autoAdjustFont(true)
    , m_resizing(false)
    , m_lineEditStretch(DEFAULT_LINEEDIT_STRETCH)
    , m_buttonStretch(DEFAULT_BUTTON_STRETCH)
    , m_spacing(DEFAULT_SPACING)
    , m_buttonMinWidth(DEFAULT_BUTTON_MIN_WIDTH)
    , m_primaryColor(Qt::blue)
    , m_backgroundColor(Qt::white)
{
    setupUI();                  // 创建布局和子控件
    applyDefaultStyle();       // 应用默认样式（清新风格）
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding); // 水平和垂直均可拉伸
    setMinimumSize(80, 25);    // 设置合理的最小尺寸，避免极端缩小导致布局崩溃

    loadHistory();             // 从 QSettings 加载历史记录
    if (m_historyCompleter) {
        m_lineEdit->setCompleter(m_historyCompleter);
        m_useCustomCompleter = false;
    }

    // 信号连接
    connect(m_lineEdit, &QLineEdit::returnPressed,
            this, &CustomSearchBox::onLineEditReturnPressed);
    connect(m_lineEdit, &QLineEdit::textChanged,
            this, &CustomSearchBox::onTextChanged);
    connect(m_searchButton, &QPushButton::clicked,
            this, &CustomSearchBox::onSearchButtonClicked);
}

CustomSearchBox::~CustomSearchBox()
{
    // Qt 对象树会自动清理子控件，无需额外代码
}

/*! 创建 UI 布局（水平布局，比例可调） */
void CustomSearchBox::setupUI()
{
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(m_spacing);

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setClearButtonEnabled(true);         // 显示清空按钮
    m_lineEdit->setPlaceholderText(tr("Search..."));
    m_lineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_searchButton = new QPushButton(this);
    m_searchButton->setText(tr("搜索"));
    m_searchButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_searchButton->setMinimumWidth(m_buttonMinWidth);
    // 强制按钮文字不换行（极端窄时也不换行，配合自动缩小字体）
    m_searchButton->setStyleSheet("QPushButton { white-space: nowrap; }");

    // 按拉伸因子添加控件，实现宽度比例
    layout->addWidget(m_lineEdit, m_lineEditStretch);
    layout->addWidget(m_searchButton, m_buttonStretch);

    setFocusProxy(m_lineEdit);   // 让输入框成为焦点代理
}

/*! 应用默认的现代样式表（未自定义时使用） */
void CustomSearchBox::applyDefaultStyle()
{
    QString defaultStyle =
        "CustomSearchBox { background-color: transparent; }"
        "QLineEdit {"
        "   border: 1px solid #d0d0d0;"
        "   border-radius: 4px;"
        "   padding: 4px 8px;"
        "   background-color: white;"
        "   selection-background-color: #3399ff;"
        "}"
        "QLineEdit:focus {"
        "   border: 1px solid #3399ff;"
        "   outline: none;"
        "}"
        "QPushButton {"
        "   border: 1px solid #d0d0d0;"
        "   border-radius: 4px;"
        "   background-color: #f0f0f0;"
        "   padding: 4px 12px;"
        "   color: #333333;"
        "   white-space: nowrap;"
        "}"
        "QPushButton:hover {"
        "   background-color: #e0e0e0;"
        "   border-color: #3399ff;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #d0d0d0;"
        "}"
        "QPushButton:focus {"
        "   outline: none;"
        "   border-color: #3399ff;"
        "}";

    // 仅在没有任何自定义样式表时应用
    if (m_customStyleSheet.isEmpty() && m_lineEditStyleSheet.isEmpty() && m_buttonStyleSheet.isEmpty()) {
        this->setStyleSheet(defaultStyle);
    } else {
        updateChildStyles();
    }
}

/*! 更新子控件样式，遵循优先级：专用样式 > 整体样式 > 默认样式 */
void CustomSearchBox::updateChildStyles()
{
    QString finalLineEditStyle;
    QString finalButtonStyle;

    if (!m_lineEditStyleSheet.isEmpty()) {
        finalLineEditStyle = m_lineEditStyleSheet;
    } else if (!m_customStyleSheet.isEmpty()) {
        finalLineEditStyle = m_customStyleSheet;
    } else {
        finalLineEditStyle =
            "QLineEdit {"
            "   border: 1px solid #d0d0d0;"
            "   border-radius: 4px;"
            "   padding: 4px 8px;"
            "   background-color: white;"
            "}"
            "QLineEdit:focus { border-color: #3399ff; }";
    }

    if (!m_buttonStyleSheet.isEmpty()) {
        finalButtonStyle = m_buttonStyleSheet;
    } else if (!m_customStyleSheet.isEmpty()) {
        finalButtonStyle = m_customStyleSheet;
    } else {
        finalButtonStyle =
            "QPushButton {"
            "   border: 1px solid #d0d0d0;"
            "   border-radius: 4px;"
            "   background-color: #f0f0f0;"
            "   padding: 4px 12px;"
            "   white-space: nowrap;"
            "}"
            "QPushButton:hover { background-color: #e0e0e0; }"
            "QPushButton:pressed { background-color: #d0d0d0; }";
    }

    // 分别应用到具体控件，避免样式冲突
    m_lineEdit->setStyleSheet(finalLineEditStyle);
    m_searchButton->setStyleSheet(finalButtonStyle);
}

/*! 字体与样式的统一更新函数（响应自动适应或手动设置） */
void CustomSearchBox::updateFontAndStyle()
{
    if (m_autoAdjustFont) {
        // 继承父控件的字体（如果父控件有字体设置）
        QFont parentFont = this->font();
        if (!parentFont.isCopyOf(QFont())) {
            m_lineEdit->setFont(parentFont);
            m_searchButton->setFont(parentFont);
        } else {
            // 若无，则使用系统默认字体并稍微加大一点（美观）
            QFont defaultFont = QFont();
            defaultFont.setPointSize(defaultFont.pointSize() + 1);
            m_lineEdit->setFont(defaultFont);
            m_searchButton->setFont(defaultFont);
        }
    }

    // 如果有主题色设置，重新应用主题（颜色可能已变）
    if (m_primaryColor.isValid() && m_backgroundColor.isValid()) {
        setTheme(m_primaryColor, m_backgroundColor);
    } else {
        updateChildStyles();
    }
}

// ==================== 基础功能接口实现 ====================

QString CustomSearchBox::text() const
{
    return m_lineEdit->text();
}

void CustomSearchBox::setText(const QString &text)
{
    if (m_lineEdit->text() == text)
        return;
    m_lineEdit->setText(text);
    emit textChanged(text);
}

QString CustomSearchBox::placeholderText() const
{
    return m_lineEdit->placeholderText();
}

void CustomSearchBox::setPlaceholderText(const QString &text)
{
    m_lineEdit->setPlaceholderText(text);
}

void CustomSearchBox::setCompletionWords(const QStringList &words)
{
    if (words.isEmpty()) {
        // 清空自定义补全，恢复历史补全
        if (m_useCustomCompleter) {
            m_lineEdit->setCompleter(m_historyCompleter);
            m_useCustomCompleter = false;
        }
        return;
    }

    if (!m_customCompleter) {
        m_customCompleter = new QCompleter(this);
        m_customCompleter->setCaseSensitivity(Qt::CaseInsensitive);
        m_customCompleter->setFilterMode(Qt::MatchContains); // 包含匹配
    }
    QStringListModel *model = new QStringListModel(words, m_customCompleter);
    m_customCompleter->setModel(model);
    m_lineEdit->setCompleter(m_customCompleter);
    m_useCustomCompleter = true;
}

void CustomSearchBox::clearHistory()
{
    m_historyList.clear();
    saveHistory();
    if (!m_useCustomCompleter) {
        updateHistoryCompleter();
    }
}

// ==================== 样式定制接口实现 ====================

void CustomSearchBox::setCustomFont(const QFont &font)
{
    m_autoAdjustFont = false;   // 手动设置字体后，关闭自动适应
    m_lineEdit->setFont(font);
    m_searchButton->setFont(font);
}

void CustomSearchBox::setAutoAdjustFont(bool autoAdjust)
{
    m_autoAdjustFont = autoAdjust;
    if (autoAdjust) {
        updateFontAndStyle();
    }
}

void CustomSearchBox::setCustomStyleSheet(const QString &styleSheet)
{
    m_customStyleSheet = styleSheet;
    this->setStyleSheet(styleSheet);
    // 如果有专用的子控件样式，则保持其优先级
    if (!m_lineEditStyleSheet.isEmpty()) {
        m_lineEdit->setStyleSheet(m_lineEditStyleSheet);
    }
    if (!m_buttonStyleSheet.isEmpty()) {
        m_searchButton->setStyleSheet(m_buttonStyleSheet);
    }
}

void CustomSearchBox::setLineEditStyleSheet(const QString &styleSheet)
{
    m_lineEditStyleSheet = styleSheet;
    m_lineEdit->setStyleSheet(styleSheet);
}

void CustomSearchBox::setButtonStyleSheet(const QString &styleSheet)
{
    m_buttonStyleSheet = styleSheet;
    m_searchButton->setStyleSheet(styleSheet);
}

void CustomSearchBox::setTheme(const QColor &primaryColor, const QColor &backgroundColor)
{
    m_primaryColor = primaryColor;
    m_backgroundColor = backgroundColor;

    QString themeStyle = QString(
        "QLineEdit {"
        "   border: 1px solid %1;"
        "   border-radius: 4px;"
        "   padding: 4px 8px;"
        "   background-color: %2;"
        "}"
        "QLineEdit:focus {"
        "   border: 2px solid %1;"
        "}"
        "QPushButton {"
        "   border: 1px solid %1;"
        "   border-radius: 4px;"
        "   background-color: %3;"
        "   padding: 4px 12px;"
        "   color: %1;"
        "   white-space: nowrap;"
        "}"
        "QPushButton:hover {"
        "   background-color: %4;"
        "}"
        "QPushButton:pressed {"
        "   background-color: %5;"
        "}"
    ).arg(primaryColor.name())
     .arg(backgroundColor.name())
     .arg(backgroundColor.lighter(110).name())
     .arg(backgroundColor.lighter(90).name())
     .arg(backgroundColor.darker(110).name());

    setCustomStyleSheet(themeStyle);
}

void CustomSearchBox::setSpacing(int spacing)
{
    m_spacing = spacing;
    if (layout()) {
        layout()->setSpacing(spacing);
    }
}

void CustomSearchBox::setWidthRatio(int lineEditRatio, int buttonRatio)
{
    m_lineEditStretch = lineEditRatio;
    m_buttonStretch = buttonRatio;
    QHBoxLayout *lay = qobject_cast<QHBoxLayout*>(layout());
    if (lay && lay->count() >= 2) {
        lay->setStretchFactor(m_lineEdit, lineEditRatio);
        lay->setStretchFactor(m_searchButton, buttonRatio);
    }
}

void CustomSearchBox::setButtonMinimumWidth(int minWidth)
{
    m_buttonMinWidth = minWidth;
    m_searchButton->setMinimumWidth(minWidth);
}

// ==================== 搜索触发与键盘事件 ====================

void CustomSearchBox::triggerSearch()
{
    QString currentText = m_lineEdit->text().trimmed();
    if (!currentText.isEmpty()) {
        addToHistory(currentText);
        emit searchTriggered(currentText);
    }
}

void CustomSearchBox::onSearchButtonClicked()
{
    triggerSearch();
}

void CustomSearchBox::onLineEditReturnPressed()
{
    triggerSearch();
}

void CustomSearchBox::onTextChanged(const QString &text)
{
    emit textChanged(text);
}

void CustomSearchBox::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        m_lineEdit->clear();   // Esc 清空输入框
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}

// ==================== 历史记录管理 ====================

void CustomSearchBox::loadHistory()
{
    QStringList saved = m_settings.value(m_historyKey).toStringList();
    // 去重且保持最近顺序（saved 本身是时间倒序，但再处理一次更安全）
    QStringList unique;
    for (const QString &item : saved) {
        if (!unique.contains(item)) {
            unique.append(item);
        }
    }
    m_historyList = unique;
    while (m_historyList.size() > MAX_HISTORY_COUNT) {
        m_historyList.removeLast();
    }
    updateHistoryCompleter();
}

void CustomSearchBox::saveHistory()
{
    m_settings.setValue(m_historyKey, m_historyList);
    m_settings.sync();   // 立即写入磁盘
}

void CustomSearchBox::addToHistory(const QString &text)
{
    if (text.isEmpty())
        return;

    // 如果已存在，先移除旧的（保持最近搜索在最前面）
    int index = m_historyList.indexOf(text);
    if (index != -1) {
        m_historyList.removeAt(index);
    }
    m_historyList.prepend(text);
    while (m_historyList.size() > MAX_HISTORY_COUNT) {
        m_historyList.removeLast();
    }
    saveHistory();

    // 如果当前没有使用自定义补全，则刷新历史补全器
    if (!m_useCustomCompleter) {
        updateHistoryCompleter();
    }
}

void CustomSearchBox::updateHistoryCompleter()
{
    if (m_historyList.isEmpty()) {
        m_lineEdit->setCompleter(nullptr);
        return;
    }

    if (!m_historyCompleter) {
        m_historyCompleter = new QCompleter(this);
        m_historyCompleter->setCaseSensitivity(Qt::CaseInsensitive);
        m_historyCompleter->setFilterMode(Qt::MatchContains);
    }
    QStringListModel *model = new QStringListModel(m_historyList, m_historyCompleter);
    m_historyCompleter->setModel(model);
    // 只有当前未使用自定义补全时才更新
    if (!m_useCustomCompleter) {
        m_lineEdit->setCompleter(m_historyCompleter);
    }
}

// ==================== 事件重写 ====================

/*!
 * \brief 重写 resizeEvent 实现字体自适应，并防止递归。
 * 当控件尺寸变化时，根据按钮实际宽度和文字所需宽度动态调整字体大小，
 * 确保文字完整显示。使用 m_resizing 标志避免因字体调整触发布局递归。
 */
void CustomSearchBox::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!m_autoAdjustFont || m_resizing) {
        return;   // 无需自适应或正在递归中，直接返回
    }

    m_resizing = true;   // 防止递归

    int w = event->size().width();
    int h = event->size().height();

    // 估算按钮文字所需最小宽度（文字宽度 + 左右 padding 约 20 像素）
    QFontMetrics fm(m_searchButton->font());
    int buttonTextWidth = fm.horizontalAdvance(m_searchButton->text()) + 20;
    // 计算按钮在当前布局比例下的实际宽度
    int buttonActualWidth = w * m_buttonStretch / (m_lineEditStretch + m_buttonStretch);
    int lineEditActualWidth = w * m_lineEditStretch / (m_lineEditStretch + m_buttonStretch);

    int newFontSize = m_lineEdit->font().pointSize();
    // 当按钮实际宽度不足以显示完整文字，或者高度太小时，缩小字体
    if (buttonActualWidth < buttonTextWidth || h < 25) {
        // 按比例缩小，最低不小于 6 点
        newFontSize = qMax(6, newFontSize - 2);
        QFont newFont = m_lineEdit->font();
        newFont.setPointSize(newFontSize);
        m_lineEdit->setFont(newFont);
        m_searchButton->setFont(newFont);
    } else if (h > 40 && newFontSize < 12) {
        // 空间充足时可适当放大字体，最大不超过 14 点
        newFontSize = qMin(14, newFontSize + 1);
        QFont newFont = m_lineEdit->font();
        newFont.setPointSize(newFontSize);
        m_lineEdit->setFont(newFont);
        m_searchButton->setFont(newFont);
    }

    // 强制刷新布局，确保新字体立即生效
    if (layout()) {
        layout()->update();
    }

    m_resizing = false;
}

void CustomSearchBox::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange) {
        if (m_autoAdjustFont && !m_resizing) {
            updateFontAndStyle();
        }
    } else if (event->type() == QEvent::StyleChange) {
        updateChildStyles();
    }
}

bool CustomSearchBox::event(QEvent *event)
{
    if (event->type() == QEvent::StyleChange || event->type() == QEvent::DynamicPropertyChange) {
        if (!m_customStyleSheet.isEmpty()) {
            this->setStyleSheet(m_customStyleSheet);
        }
    }
    return QWidget::event(event);
}
}
