#ifndef MSGBOX_H
#define MSGBOX_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "SqzGlobal.h"
/*!
 * \brief 轻量级增强消息框
 *
 * 支持模态/非模态，自动关闭，窗口拖动，Toast 提示。
 * 所有静态方法均自动管理内存，无需担心内存泄漏。
 */
namespace Sqz {
class SQZ_FRAMEWORK_API MsgBox : public QDialog
{
    Q_OBJECT

public:
    enum IconType { Information, Warning, Critical, Success };
    explicit MsgBox(QWidget *parent = nullptr);
    ~MsgBox();
    /*!
     * \brief 信息框（模态）
     * \param title   标题
     * \param text    正文
     * \param timeout 自动关闭毫秒数（0=不自动关闭）
     * \param parent  父窗口
     */
    static void info(const QString &title, const QString &text,
                     int timeout = 0, QWidget *parent = nullptr);

    static void warning(const QString &title, const QString &text,
                        int timeout = 0, QWidget *parent = nullptr);

    static void critical(const QString &title, const QString &text,
                         int timeout = 0, QWidget *parent = nullptr);

    static void success(const QString &title, const QString &text,
                        int timeout = 0, QWidget *parent = nullptr);

    /*!
     * \brief Toast 提示（非模态，右下角弹出，自动淡出）
     * \param text     显示文字
     * \param duration 显示时长（毫秒）
     * \param parent   父窗口
     */
    static void toast(const QString &text, int duration = 2000,
                      QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    // 重写 closeEvent 确保定时器停止
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onTimeout();

private:


    void setupUI(IconType icon, const QString &title,
                 const QString &text, bool hasButton);
    void startTimer(int ms);
    void addFadeOutAnimation();

    QLabel      *m_iconLabel;
    QLabel      *m_textLabel;
    QPushButton *m_okButton;
    QTimer      *m_timer;

    bool         m_isDragging;
    QPoint       m_dragPos;
};
}
#endif // MSGBOX_H
