#include "MsgBox.h"
#include <QMouseEvent>
#include <QStyle>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QApplication>
#include <QScreen>
#include <QCloseEvent>

// ==================== 构造 / 析构 ====================
namespace Sqz {
MsgBox::MsgBox(QWidget *parent)
    : QDialog(parent),
      m_iconLabel(nullptr),
      m_textLabel(nullptr),
      m_okButton(nullptr),
      m_timer(new QTimer(this)),
      m_isDragging(false)
{
    // 窗口属性（无边框 + 置顶）
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
//    setAttribute(Qt::WA_TranslucentBackground);

    // 样式（温和圆角）
    setStyleSheet(
        "QDialog {"
        "    background: #ffffff;"
        "    border-radius: 10px;"
        "    border: 1px solid #cccccc;"
        "}"
        "QPushButton {"
        "    padding: 6px 24px;"
        "    border-radius: 5px;"
        "    background: #f5f5f5;"
        "    border: 1px solid #bbbbbb;"
        "}"
        "QPushButton:hover { background: #e8e8e8; }"
        "QPushButton:pressed { background: #d5d5d5; }"
    );

    // ---- 主布局 ----
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(12);

    // ---- 图标 + 文本（水平） ----
    QHBoxLayout *topLayout = new QHBoxLayout();
    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(36, 36);
    m_iconLabel->setScaledContents(false);
    topLayout->addWidget(m_iconLabel);

    m_textLabel = new QLabel(this);
    m_textLabel->setWordWrap(true);
    m_textLabel->setMinimumWidth(240);
    m_textLabel->setStyleSheet("font-size: 13px; color: #222222;");
    topLayout->addWidget(m_textLabel);

    mainLayout->addLayout(topLayout);

    // ---- 确定按钮 ----
    m_okButton = new QPushButton("确定", this);
    m_okButton->setFixedWidth(90);
    connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(m_okButton);
    mainLayout->addLayout(btnLayout);

    // ---- 定时器 ----
    connect(m_timer, &QTimer::timeout, this, &MsgBox::onTimeout);

    // 关闭时自动删除（用于静态方法）
    setAttribute(Qt::WA_DeleteOnClose);
}

MsgBox::~MsgBox()
{
    if (m_timer->isActive())
        m_timer->stop();
}

// ==================== 静态接口 ====================

void MsgBox::info(const QString &title, const QString &text,
                  int timeout, QWidget *parent)
{
    MsgBox *box = new MsgBox(parent);
    box->setupUI(Information, title, text, true);
    box->setModal(true);
    if (timeout > 0) box->startTimer(timeout);
    box->exec();   // 模态阻塞，关闭后自动 delete（由于 WA_DeleteOnClose）
}

void MsgBox::warning(const QString &title, const QString &text,
                     int timeout, QWidget *parent)
{
    MsgBox *box = new MsgBox(parent);
    box->setupUI(Warning, title, text, true);
    box->setModal(true);
    if (timeout > 0) box->startTimer(timeout);
    box->exec();
}

void MsgBox::critical(const QString &title, const QString &text,
                      int timeout, QWidget *parent)
{
    MsgBox *box = new MsgBox(parent);
    box->setupUI(Critical, title, text, true);
    box->setModal(true);
    if (timeout > 0) box->startTimer(timeout);
    box->exec();
}

void MsgBox::success(const QString &title, const QString &text,
                     int timeout, QWidget *parent)
{
    MsgBox *box = new MsgBox(parent);
    box->setupUI(Success, title, text, true);
    box->setModal(true);
    if (timeout > 0) box->startTimer(timeout);
    box->exec();
}

void MsgBox::toast(const QString &text, int duration, QWidget *parent)
{
    MsgBox *toast = new MsgBox(parent);
    toast->setupUI(Information, "", text, false); // 无标题、无按钮
    toast->setModal(false);

    // 定位右下角
    QRect screenRect = QApplication::primaryScreen()->geometry();
    toast->move(screenRect.right() - toast->width() - 30,
                screenRect.bottom() - toast->height() - 30);

    toast->startTimer(duration);
    toast->show();   // 非模态，自动删除
}

// ==================== UI 组装 ====================

void MsgBox::setupUI(IconType icon, const QString &title,
                     const QString &text, bool hasButton)
{
    setWindowTitle(title);
    m_textLabel->setText(text);

    // 获取图标（若标准图标不可用，使用备用方案）
    QPixmap pixmap;
    QStyle::StandardPixmap sp;
    switch (icon) {
    case Information: sp = QStyle::SP_MessageBoxInformation; break;
    case Warning:     sp = QStyle::SP_MessageBoxWarning;     break;
    case Critical:    sp = QStyle::SP_MessageBoxCritical;    break;
    case Success:     sp = QStyle::SP_DialogApplyButton;     break;
    default:          sp = QStyle::SP_MessageBoxInformation;
    }
    pixmap = style()->standardPixmap(sp);
    if (pixmap.isNull()) {
        // 备用：如果标准图标无效，用文字图标（极少发生）
        pixmap = QPixmap(36, 36);
        pixmap.fill(Qt::transparent);
    }
    m_iconLabel->setPixmap(pixmap.scaled(36, 36, Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation));

    // 按钮显隐
    m_okButton->setVisible(hasButton);
    if (!hasButton) {
        layout()->setContentsMargins(20, 12, 20, 12);
        // Toast 无按钮，调整背景为半透明黑（更现代）
        setStyleSheet(
            "QDialog {"
            "    background: rgba(40, 40, 40, 210);"
            "    border-radius: 8px;"
            "    border: none;"
            "}"
            "QLabel { color: white; }"
        );
    }

    adjustSize();
    setFixedSize(size()); // 固定尺寸防变形
}

// ==================== 定时器 ====================

void MsgBox::startTimer(int ms)
{
    if (ms > 0 && !m_timer->isActive())
        m_timer->start(ms);
}

void MsgBox::onTimeout()
{
    m_timer->stop();
    if (isModal()) {
        accept();   // 模态框关闭
    } else {
        addFadeOutAnimation(); // Toast 淡出
    }
}

// ==================== 淡出动画（Toast） ====================

void MsgBox::addFadeOutAnimation()
{
    QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(effect);
    QPropertyAnimation *anim = new QPropertyAnimation(effect, "opacity");
    anim->setDuration(350);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    connect(anim, &QPropertyAnimation::finished, this, &QDialog::close);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

// ==================== 窗口拖动 ====================

void MsgBox::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_dragPos = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}

void MsgBox::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPos() - m_dragPos);
        event->accept();
    }
}

void MsgBox::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        event->accept();
    }
}

// ==================== 关闭事件 ====================

void MsgBox::closeEvent(QCloseEvent *event)
{
    if (m_timer->isActive())
        m_timer->stop();
    event->accept();
}
}
