/*****************************************************************************
** 文件名:  CustomSearchBox.h
** 功能描述: 一个功能强大、样式可定制的搜索框控件。
**           支持搜索历史记录、自动补全、自适应字体、自定义样式表、
**           宽度比例可调、按钮最小宽度保护等高级特性。
** 作者:     Generated
** 版本:     2.1 (修复固定尺寸异常与极端尺寸文字显示问题)
** 日期:     2026-05-22
** 说明:     基于 Qt 5.12，跨平台支持 Windows / Linux。
**           使用时只需包含该头文件，并连接到 searchTriggered 信号即可。
*****************************************************************************/

#ifndef CUSTOMSEARCHBOX_H
#define CUSTOMSEARCHBOX_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QStringList>
#include <QSettings>
#include <QCompleter>
#include <QFont>
#include "SqzGlobal.h"
/*!
 * \class CustomSearchBox
 * \brief 一个功能完整的搜索框控件，集成历史记录、自动补全、自适应样式。
 *
 * 主要功能：
 * - 自动保存/加载搜索历史（最近20条，自动去重，基于 QSettings 持久化）
 * - 支持外部设置自动补全词条（例如热门搜索建议）
 * - 内部输入框与按钮宽度比例可调（默认 5:1）
 * - 字体自适应：可自动跟随父控件字体，也可手动指定字体
 * - 样式定制：支持设置整体样式表、单独设置输入框/按钮样式表，或快速设置主题色
 * - 键盘快捷操作：回车触发搜索，Esc 清空输入框
 * - 高度自适应拉伸：控件整体可在水平和垂直方向填充分配的空间
 * - 修复：当调用 setFixedSize 或极端尺寸时，文字自动缩放，保证完整显示，避免递归崩溃
 *
 * 使用示例：
 * @code
 * CustomSearchBox *searchBox = new CustomSearchBox(this, "docSearch");
 * searchBox->setPlaceholderText("搜索文档...");
 * connect(searchBox, &CustomSearchBox::searchTriggered, this, [](const QString& kw){
 *     qDebug() << "搜索关键词:" << kw;
 * });
 * @endcode
 */
namespace Sqz {
class SQZ_FRAMEWORK_API CustomSearchBox : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText)
    Q_PROPERTY(bool autoAdjustFont READ autoAdjustFont WRITE setAutoAdjustFont)

public:
    /*!
     * \brief 构造函数
     * \param parent 父窗口指针
     * \param historyKey 历史记录在 QSettings 中的键名，用于区分不同搜索框的历史记录
     *                   （例如 "mainSearch", "docSearch" 等）
     */
    explicit CustomSearchBox(QWidget *parent = nullptr,
                             const QString &historyKey = "searchHistory");
    ~CustomSearchBox();

    // ========== 基础功能接口 ==========

    /*! 获取当前输入框的文本内容 */
    QString text() const;

    /*! 设置输入框的文本内容（不会触发搜索，仅改变显示） */
    void setText(const QString &text);

    /*! 获取占位提示文字 */
    QString placeholderText() const;

    /*! 设置占位提示文字（例如 "请输入搜索关键词..."） */
    void setPlaceholderText(const QString &text);

    /*!
     * \brief 设置自动补全的词条列表
     * \param words 补全词条（例如 QStringList{"Qt", "C++", "Python"}）
     * 调用后会优先使用自定义补全词条，清空列表（words.isEmpty()）则恢复为历史记录补全。
     */
    void setCompletionWords(const QStringList &words);

    /*! 清空所有搜索历史记录（持久化存储也会一并清除） */
    void clearHistory();

    // ========== 样式与外观定制接口 ==========

    /*!
     * \brief 设置自定义字体（会同时应用于输入框和按钮）
     * \param font 要设置的字体对象
     * 注意：调用此函数后会自动关闭“自动适应字体”模式（autoAdjustFont 设为 false）。
     */
    void setCustomFont(const QFont &font);

    /*!
     * \brief 设置是否自动适应父控件的字体变化
     * \param autoAdjust true: 自动继承父控件字体，并响应系统字体设置变化；
     *                   false: 不再自动调整，使用当前字体。
     * 默认值为 true。
     */
    void setAutoAdjustFont(bool autoAdjust);

    /*! 获取当前是否处于自动字体适应模式 */
    bool autoAdjustFont() const { return m_autoAdjustFont; }

    /*!
     * \brief 设置整个 CustomSearchBox 的样式表（会同时影响输入框和按钮）
     * \param styleSheet 合法的 Qt 样式表字符串
     * 优先级低于单独设置的输入框/按钮样式表。
     */
    void setCustomStyleSheet(const QString &styleSheet);

    /*!
     * \brief 单独设置输入框的样式表（优先级最高）
     * \param styleSheet QLineEdit 的样式表
     */
    void setLineEditStyleSheet(const QString &styleSheet);

    /*!
     * \brief 单独设置按钮的样式表（优先级最高）
     * \param styleSheet QPushButton 的样式表
     */
    void setButtonStyleSheet(const QString &styleSheet);

    /*!
     * \brief 快速设置主题色（便捷接口）
     * \param primaryColor 主色调（用于边框、焦点高亮、按钮文字等）
     * \param backgroundColor 背景色（输入框背景）
     * 内部会自动生成一套协调的样式表。
     */
    void setTheme(const QColor &primaryColor, const QColor &backgroundColor = Qt::white);

    /*!
     * \brief 设置输入框与按钮之间的间距（像素）
     * \param spacing 间距值，默认 2 像素
     */
    void setSpacing(int spacing);

    /*!
     * \brief 设置输入框与按钮的宽度比例（基于水平布局的拉伸因子）
     * \param lineEditRatio 输入框的拉伸因子，默认 5
     * \param buttonRatio   按钮的拉伸因子，默认 1
     * 例如 5:1 表示输入框宽度占总宽度的 5/6，按钮占 1/6。
     */
    void setWidthRatio(int lineEditRatio, int buttonRatio);

    /*!
     * \brief 设置按钮的最小宽度（像素），防止控件缩得太小时按钮消失
     * \param minWidth 最小宽度值，默认 40 像素
     */
    void setButtonMinimumWidth(int minWidth);

signals:
    /*!
     * \brief 当用户触发搜索时发出的信号
     * \param text 当前输入框的文本（已去除首尾空白）
     * 触发时机：点击搜索按钮、在输入框中按下回车键、或调用 triggerSearch()。
     */
    void searchTriggered(const QString &text);

    /*! 当输入框文本内容发生变化时发出 */
    void textChanged(const QString &text);

public slots:
    /*!
     * \brief 手动触发搜索（使用当前输入框内容）
     * 效果：自动将当前文本加入历史记录，并发出 searchTriggered 信号。
     * 若文本为空，则不做任何操作。
     */
    void triggerSearch();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool event(QEvent *event) override;

private slots:
    void onSearchButtonClicked();
    void onLineEditReturnPressed();
    void onTextChanged(const QString &text);
    void updateHistoryCompleter();
    void updateFontAndStyle();

private:
    void setupUI();
    void loadHistory();
    void saveHistory();
    void addToHistory(const QString &text);
    void applyDefaultStyle();
    void updateChildStyles();

    // UI 组件
    QLineEdit   *m_lineEdit;
    QPushButton *m_searchButton;

    // 历史记录相关
    QStringList  m_historyList;
    QSettings    m_settings;          // 持久化存储对象
    QString      m_historyKey;
    QCompleter  *m_historyCompleter;  // 历史记录自动补全器
    QCompleter  *m_customCompleter;   // 外部自定义补全器
    bool         m_useCustomCompleter;

    // 样式定制成员
    bool         m_autoAdjustFont;     // 自动适应字体标志
    bool         m_resizing;           // 防止 resizeEvent 递归
    QString      m_customStyleSheet;   // 整体自定义样式表
    QString      m_lineEditStyleSheet; // 输入框专用样式表
    QString      m_buttonStyleSheet;   // 按钮专用样式表
    int          m_lineEditStretch;    // 输入框宽度拉伸因子
    int          m_buttonStretch;      // 按钮宽度拉伸因子
    int          m_spacing;            // 间距
    int          m_buttonMinWidth;     // 按钮最小宽度

    // 主题记忆（用于重新应用）
    QColor       m_primaryColor;
    QColor       m_backgroundColor;
};
}
#endif // CUSTOMSEARCHBOX_H
