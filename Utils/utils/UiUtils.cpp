#include "UiUtils.h"
#include <QByteArray>
#include <QBuffer>
#include <QPixmap>
#include <QGuiApplication>
#include <QPainter>
#include <QList>
#include <QVBoxLayout>
#include <QRegExp>
#include <QFileDialog>
#include <QDateTime>

namespace Sqz {
// ===================== ToastWidget 实现 =====================
ToastWidget::ToastWidget(const QString &msg, TipType type, ToastPos pos, int showMs)
    : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool),
      m_label(nullptr),
      m_pos(pos),
      m_type(type),
      m_showTime(showMs),
      m_opacityEffect(nullptr),
      m_fadeTimer(nullptr),
      m_delayTimer(nullptr)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowOpacity(1.0);
    setFixedSize(QSize(320, 60));

    m_label = new QLabel(msg, this);
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setWordWrap(true);
    m_label->setStyleSheet("color:#ffffff;font-size:14px;");
    m_label->setContentsMargins(10, 5, 10, 5);

    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(m_label);

    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(1.0);
    setGraphicsEffect(m_opacityEffect);

    m_delayTimer = new QTimer(this);
    m_delayTimer->setSingleShot(true);
    connect(m_delayTimer, &QTimer::timeout, this, &ToastWidget::startFadeOut);

    m_fadeTimer = new QTimer(this);
    m_fadeTimer->setInterval(30);
    connect(m_fadeTimer, &QTimer::timeout, this, [this](){
        qreal opacity = m_opacityEffect->opacity() - 0.05;
        if (opacity <= 0)
        {
            m_fadeTimer->stop();
            this->close();
            this->deleteLater();
        }
        m_opacityEffect->setOpacity(opacity);
    });
    m_delayTimer->start(m_showTime);
}

void ToastWidget::showToast()
{
    calcToastPos();
    this->show();
    this->raise();
}

void ToastWidget::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QRect rect = this->rect();
    rect.adjust(2, 2, -2, -2);
    painter.setBrush(QColor(getBgColor(m_type)));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect, 8, 8);
}

QString ToastWidget::getBgColor(TipType type)
{
    switch (type)
    {
    case TipType::Success: return "#00b42a";
    case TipType::Warn:    return "#ff7d00";
    case TipType::Error:   return "#f53f3f";
    case TipType::Info:    return "#1677ff";
    default: return "#1677ff";
    }
}

void ToastWidget::calcToastPos()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenRect = screen->availableGeometry();
    QSize toastSize = this->size();
    const int margin = 20;
    int x = 0, y = 0;
    switch (m_pos)
    {
    case ToastPos::TopLeft:
        x = screenRect.x() + margin;
        y = screenRect.y() + margin;
        break;
    case ToastPos::TopRight:
        x = screenRect.right() - toastSize.width() - margin;
        y = screenRect.y() + margin;
        break;
    case ToastPos::BottomLeft:
        x = screenRect.x() + margin;
        y = screenRect.bottom() - toastSize.height() - margin;
        break;
    case ToastPos::BottomRight:
        x = screenRect.right() - toastSize.width() - margin;
        y = screenRect.bottom() - toastSize.height() - margin;
        break;
    case ToastPos::TopCenter:
        x = screenRect.left() + (screenRect.width() - toastSize.width()) / 2;
        y = screenRect.y() + margin;
        break;
    case ToastPos::BottomCenter:
        x = screenRect.left() + (screenRect.width() - toastSize.width()) / 2;
        y = screenRect.bottom() - toastSize.height() - margin;
        break;
    }
    this->move(x, y);
}

void ToastWidget::startFadeOut()
{
    m_fadeTimer->start();
}

// ===================== MovableHelper 拖动辅助类 =====================
MovableHelper::MovableHelper(QWidget *target)
    : QObject(target), m_target(target)
{
    m_target->installEventFilter(this);
}

bool MovableHelper::eventFilter(QObject *obj, QEvent *event)
{
    if (obj != m_target)
        return QObject::eventFilter(obj, event);
    if (event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *e = static_cast<QMouseEvent*>(event);
        if (e->button() == Qt::LeftButton)
        {
            m_mousePressPos = e->globalPos();
            m_widgetOriginPos = m_target->pos();
        }
    }
    else if (event->type() == QEvent::MouseMove)
    {
        QMouseEvent *e = static_cast<QMouseEvent*>(event);
        if (e->buttons() & Qt::LeftButton)
        {
            QPoint offset = e->globalPos() - m_mousePressPos;
            m_target->move(m_widgetOriginPos + offset);
        }
    }
    return false;
}

// ===================== UiUtils 静态工具实现（原有代码完整保留） =====================
void UiUtils::centerWindow(QWidget *w, QWidget *parent)
{
    if (!w) return;
    QRect targetRect;
    if (parent && parent->isVisible())
        targetRect = parent->frameGeometry();
    else
        targetRect = QGuiApplication::primaryScreen()->availableGeometry();

    int x = targetRect.x() + (targetRect.width() - w->width()) / 2;
    int y = targetRect.y() + (targetRect.height() - w->height()) / 2;
    w->move(x, y);
}

void UiUtils::setWindowLimit(QWidget *w, int minW, int minH, int maxW, int maxH)
{
    if (!w) return;
    w->setMinimumSize(minW, minH);
    if (maxW > 0 && maxH > 0)
        w->setMaximumSize(maxW, maxH);
}

void UiUtils::showTip(const QString &msg, TipType type, QWidget *parent)
{
    QMessageBox::Icon icon = QMessageBox::Information;
    switch (type)
    {
    case TipType::Success: icon = QMessageBox::Information; break;
    case TipType::Warn:    icon = QMessageBox::Warning; break;
    case TipType::Error:   icon = QMessageBox::Critical; break;
    case TipType::Info:    icon = QMessageBox::Information; break;
    }
    QMessageBox msgBox(parent);
    msgBox.setIcon(icon);
    msgBox.setText(msg);
    msgBox.setWindowTitle("提示");
    msgBox.exec();
}

bool UiUtils::showConfirm(const QString &msg, const QString &title, QWidget *parent)
{
    auto ret = QMessageBox::question(parent, title, msg, QMessageBox::Yes | QMessageBox::No);
    return ret == QMessageBox::Yes;
}

void UiUtils::setWidgetStyle(QWidget *w, int radius, const QString &bgColor, int borderW, const QString &borderColor)
{
    if (!w) return;
    QString qss = QString("background-color:%1;border-radius:%2px;").arg(bgColor).arg(radius);
    if (borderW > 0)
        qss += QString("border:%1px solid %2;").arg(borderW).arg(borderColor);
    w->setStyleSheet(qss);
}

void UiUtils::setWidgetFont(QWidget *w, int fontSize, bool bold)
{
    if (!w) return;
    QFont f = w->font();
    f.setPointSize(fontSize);
    f.setBold(bold);
    w->setFont(f);
}

void UiUtils::clearStyle(QWidget *w)
{
    if (w) w->setStyleSheet("");
}

QImage UiUtils::scaleImage(const QImage &src, int w, int h)
{
    return src.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QString UiUtils::imageToBase64(const QImage &img, const QString &fmt)
{
    QByteArray buf;
    QBuffer buffer(&buf);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, fmt.toLatin1().data());
    return buf.toBase64();
}

QImage UiUtils::base64ToImage(const QString &base64Str)
{
    QByteArray data = QByteArray::fromBase64(base64Str.toUtf8());
    QImage img;
    img.loadFromData(data);
    return img;
}

void UiUtils::setLabelImage(QLabel *lab, const QString &imgPath, bool keepRatio)
{
    if (!lab) return;
    QPixmap pix(imgPath);
    if (pix.isNull()) return;
    if (keepRatio)
        lab->setPixmap(pix.scaled(lab->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    else
        lab->setPixmap(pix.scaled(lab->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}

QSize UiUtils::getScreenSize()
{
    return QGuiApplication::primaryScreen()->availableSize();
}

qreal UiUtils::getScreenScale(QWidget *w)
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!w)
        return screen->devicePixelRatio();

    QWidget* topWin = w->window();
    if (!topWin)
        return screen->devicePixelRatio();

    QRect winGeo = topWin->frameGeometry();
    QList<QScreen*> screenList = QGuiApplication::screens();
    for (QScreen* scr : screenList)
    {
        if (scr->geometry().intersects(winGeo))
        {
            screen = scr;
            break;
        }
    }
    return screen->devicePixelRatio();
}

void UiUtils::showToast(const QString &msg, TipType type, ToastPos pos, int showMs)
{
    ToastWidget *toast = new ToastWidget(msg, type, pos, showMs);
    toast->showToast();
}

// ==================== 控件递归查找实现 ====================
QWidget* UiUtils::findWidgetByName(QWidget *parent, const QString &objName)
{
    if (!parent || objName.isEmpty())
        return nullptr;
    if (parent->objectName() == objName)
        return parent;
    const QObjectList &children = parent->children();
    for (QObject *child : children)
    {
        QWidget *childWidget = qobject_cast<QWidget*>(child);
        if (childWidget)
        {
            QWidget *found = findWidgetByName(childWidget, objName);
            if (found)
                return found;
        }
    }
    return nullptr;
}

void UiUtils::recursiveFindByName(QWidget *parent, const QString &objName, QList<QWidget*> &list)
{
    if (!parent) return;
    if (parent->objectName() == objName)
        list.append(parent);
    const QObjectList &children = parent->children();
    for (QObject *child : children)
    {
        QWidget *cw = qobject_cast<QWidget*>(child);
        if (cw) recursiveFindByName(cw, objName, list);
    }
}

QList<QWidget*> UiUtils::findWidgetsByName(QWidget *parent, const QString &objName)
{
    QList<QWidget*> res;
    if (!parent || objName.isEmpty()) return res;
    recursiveFindByName(parent, objName, res);
    return res;
}

// ==================== 窗口扩展功能实现 ====================
void UiUtils::setMovableWidget(QWidget *w, bool enable)
{
    if (!w) return;
    if (enable)
    {
        new MovableHelper(w);
    }
}

void UiUtils::fadeIn(QWidget *w, int duration)
{
    if (!w) return;
    w->show();
    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(w);
    eff->setOpacity(0);
    w->setGraphicsEffect(eff);
    QPropertyAnimation *anim = new QPropertyAnimation(eff, "opacity", w);
    anim->setDuration(duration);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void UiUtils::fadeOut(QWidget *w, int duration)
{
    if (!w) return;
    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(w);
    eff->setOpacity(1);
    w->setGraphicsEffect(eff);
    QPropertyAnimation *anim = new QPropertyAnimation(eff, "opacity", w);
    anim->setDuration(duration);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);
    QObject::connect(anim, &QPropertyAnimation::finished, w, &QWidget::close);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void UiUtils::setWindowTopmost(QWidget *w, bool top)
{
    if (!w) return;
    if (top)
        w->setWindowFlags(w->windowFlags() | Qt::WindowStaysOnTopHint);
    else
        w->setWindowFlags(w->windowFlags() & ~Qt::WindowStaysOnTopHint);
    w->show();
}

void UiUtils::preventClose(QWidget *w, bool prevent)
{
    if (!w) return;
    if (prevent)
        w->setWindowFlags(w->windowFlags() | Qt::CustomizeWindowHint | Qt::WindowMinimizeButtonHint);
    else
        w->setWindowFlags(w->windowFlags() & ~(Qt::CustomizeWindowHint | Qt::WindowMinimizeButtonHint));
}

QPixmap UiUtils::grabWindow(QWidget *w)
{
    if (!w) return QPixmap();
    return w->grab();
}

void UiUtils::saveWindowScreenshot(QWidget *w, const QString &savePath)
{
    QPixmap pix = grabWindow(w);
    pix.save(savePath);
}

// ==================== 样式扩展实现 ====================
void UiUtils::setShadow(QWidget *w, int blur, const QColor &color)
{
    if (!w) return;
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(w);
    shadow->setBlurRadius(blur);
    shadow->setColor(color);
    shadow->setOffset(3,3);
    w->setGraphicsEffect(shadow);
}

void UiUtils::setOpacity(QWidget *w, qreal opacity)
{
    if (!w) return;
    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(w);
    eff->setOpacity(opacity);
    w->setGraphicsEffect(eff);
}

void UiUtils::setFlatButton(QPushButton *btn)
{
    if (!btn) return;
    btn->setStyleSheet("QPushButton{border:none;background:transparent;}QPushButton:hover{background:#e8f3ff;}");
}

void UiUtils::debugBorder(QWidget *w, const QColor &color)
{
    if (!w) return;
    w->setStyleSheet(QString("border:1px solid %1;").arg(color.name()));
}

// ==================== DPI屏幕扩展 ====================
bool UiUtils::isHighDpi()
{
    return QGuiApplication::primaryScreen()->devicePixelRatio() > 1;
}

int UiUtils::dpiScale(int pixel)
{
    qreal scale = QGuiApplication::primaryScreen()->devicePixelRatio();
    return qRound(pixel * scale);
}

QScreen* UiUtils::currentScreen()
{
    QPoint mousePos = QCursor::pos();
    for (QScreen *scr : QGuiApplication::screens())
    {
        if (scr->geometry().contains(mousePos))
            return scr;
    }
    return QGuiApplication::primaryScreen();
}

// ==================== Toast快捷封装 ====================
void UiUtils::showSuccessToast(const QString &msg, int ms)
{
    showToast(msg, TipType::Success, ToastPos::BottomRight, ms);
}

void UiUtils::showErrorToast(const QString &msg, int ms)
{
    showToast(msg, TipType::Error, ToastPos::BottomRight, ms);
}

// ==================== 批量控件操作 ====================
void UiUtils::recursiveClearInput(QWidget *parent)
{
    if (!parent) return;
    if (QLineEdit *e = qobject_cast<QLineEdit*>(parent)) e->clear();
    if (QTextEdit *t = qobject_cast<QTextEdit*>(parent)) t->clear();
    if (QComboBox *c = qobject_cast<QComboBox*>(parent)) c->setCurrentIndex(0);
    if (QSpinBox *s = qobject_cast<QSpinBox*>(parent)) s->setValue(0);

    const QObjectList &children = parent->children();
    for (QObject *ch : children)
    {
        QWidget *cw = qobject_cast<QWidget*>(ch);
        if (cw) recursiveClearInput(cw);
    }
}

void UiUtils::clearAllInputs(QWidget *parent)
{
    recursiveClearInput(parent);
}

void UiUtils::recursiveSetEnable(QWidget *parent, bool enabled)
{
    if (!parent) return;
    parent->setEnabled(enabled);
    const QObjectList &children = parent->children();
    for (QObject *ch : children)
    {
        QWidget *cw = qobject_cast<QWidget*>(ch);
        if (cw) recursiveSetEnable(cw, enabled);
    }
}

void UiUtils::setEnabledAll(QWidget *parent, bool enabled)
{
    recursiveSetEnable(parent, enabled);
}

void UiUtils::recursiveSetVisible(QWidget *parent, bool visible)
{
    if (!parent) return;
    parent->setVisible(visible);
    const QObjectList &children = parent->children();
    for (QObject *ch : children)
    {
        QWidget *cw = qobject_cast<QWidget*>(ch);
        if (cw) recursiveSetVisible(cw, visible);
    }
}

void UiUtils::setVisibleAll(QWidget *parent, bool visible)
{
    recursiveSetVisible(parent, visible);
}

void UiUtils::setPlaceHolder(QWidget *parent, const QString &objName, const QString &text)
{
    QLineEdit *edit = qobject_cast<QLineEdit*>(findWidgetByName(parent, objName));
    if (edit) edit->setPlaceholderText(text);
}

// ==================== 输入框限制 ====================
void UiUtils::setDigitOnly(QLineEdit *edit)
{
    if (!edit) return;
    QRegExp rx("^[0-9]*$");
    edit->setValidator(new QRegExpValidator(rx, edit));
}

void UiUtils::setDoubleOnly(QLineEdit *edit)
{
    if (!edit) return;
    QRegExp rx("^-?\\d+(\\.\\d+)?$");
    edit->setValidator(new QRegExpValidator(rx, edit));
}

void UiUtils::setNoChinese(QLineEdit *edit)
{
    if (!edit) return;
    QRegExp rx("[^\u4e00-\u9fa5]*");
    edit->setValidator(new QRegExpValidator(rx, edit));
}

void UiUtils::setPasswordMode(QLineEdit *edit, bool enable)
{
    if (!edit) return;
    edit->setEchoMode(enable ? QLineEdit::Password : QLineEdit::Normal);
}

// ==================== 布局快捷操作 ====================
void UiUtils::clearLayout(QLayout *layout, bool deleteWidget)
{
    if (!layout) return;
    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr)
    {
        if (item->widget() && deleteWidget)
            item->widget()->deleteLater();
        delete item;
    }
}

void UiUtils::destroyLayout(QWidget *w)
{
    if (!w) return;
    QLayout *lay = w->layout();
    if (lay)
    {
        clearLayout(lay, true);
        delete lay;
        w->setLayout(nullptr);
    }
}

void UiUtils::adjustWindowSize(QWidget *w)
{
    if (!w) return;
    w->adjustSize();
}

// ==================== 控件动画（修复QGraphicsScaleEffect不存在问题） ====================
void UiUtils::moveAnimation(QWidget *w, const QPoint &endPos, int duration)
{
    if (!w) return;
    QPropertyAnimation *anim = new QPropertyAnimation(w, "pos", w);
    anim->setDuration(duration);
    anim->setStartValue(w->pos());
    anim->setEndValue(endPos);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void UiUtils::scaleAnimation(QWidget *w, qreal startScale, qreal endScale, int duration)
{
    if (!w) return;
    // Qt5.12无QGraphicsScaleEffect，改用transform矩阵缩放
    QPropertyAnimation *anim = new QPropertyAnimation(w, "transform", w);
    anim->setDuration(duration);

    QTransform startTrans;
    startTrans.scale(startScale, startScale);
    QTransform endTrans;
    endTrans.scale(endScale, endScale);

    anim->setStartValue(startTrans);
    anim->setEndValue(endTrans);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void UiUtils::blinkWidget(QWidget *w, int times, int interval)
{
    if (!w) return;
    int *blinkCnt = new int(0);
    QTimer *timer = new QTimer(w);
    QObject::connect(timer, &QTimer::timeout, w, [=](){
        w->setVisible(!w->isVisible());
        *blinkCnt = *blinkCnt + 1;
        if (*blinkCnt >= times * 2)
        {
            timer->stop();
            timer->deleteLater();
            w->setVisible(true);
            delete blinkCnt;
        }
    });
    timer->setInterval(interval);
    timer->start();
}

// ==================== 输入弹窗 ====================
QString UiUtils::getInputText(const QString &title, const QString &label, const QString &defaultText, QWidget *parent)
{
    QString res;
    bool ok = false;
    res = QInputDialog::getText(parent, title, label, QLineEdit::Normal, defaultText, &ok);
    return ok ? res : "";
}

// ==================== 表单状态持久化（移除goto，改用if判断） ====================
void UiUtils::recursiveSaveInput(QWidget *parent, QSettings &settings, const QString &prefix)
{
    if (!parent) return;
    QString objName = parent->objectName();
    if (!objName.isEmpty())
    {
        QString key = prefix + "/" + objName;
        if (QLineEdit *e = qobject_cast<QLineEdit*>(parent))
            settings.setValue(key, e->text());
        if (QTextEdit *t = qobject_cast<QTextEdit*>(parent))
            settings.setValue(key, t->toPlainText());
        if (QComboBox *c = qobject_cast<QComboBox*>(parent))
            settings.setValue(key, c->currentIndex());
        if (QSpinBox *s = qobject_cast<QSpinBox*>(parent))
            settings.setValue(key, s->value());
        if (QCheckBox *cb = qobject_cast<QCheckBox*>(parent))
            settings.setValue(key, cb->isChecked());
    }

    const QObjectList &children = parent->children();
    for (QObject *ch : children)
    {
        QWidget *cw = qobject_cast<QWidget*>(ch);
        if (cw) recursiveSaveInput(cw, settings, prefix);
    }
}

void UiUtils::saveInputsState(QWidget *parent, const QString &key)
{
    QSettings set;
    recursiveSaveInput(parent, set, key);
}

void UiUtils::recursiveRestoreInput(QWidget *parent, QSettings &settings, const QString &prefix)
{
    if (!parent) return;
    QString objName = parent->objectName();
    if (!objName.isEmpty())
    {
        QString key = prefix + "/" + objName;
        if (settings.contains(key))
        {
            if (QLineEdit *e = qobject_cast<QLineEdit*>(parent))
                e->setText(settings.value(key).toString());
            if (QTextEdit *t = qobject_cast<QTextEdit*>(parent))
                t->setPlainText(settings.value(key).toString());
            if (QComboBox *c = qobject_cast<QComboBox*>(parent))
                c->setCurrentIndex(settings.value(key).toInt());
            if (QSpinBox *s = qobject_cast<QSpinBox*>(parent))
                s->setValue(settings.value(key).toInt());
            if (QCheckBox *cb = qobject_cast<QCheckBox*>(parent))
                cb->setChecked(settings.value(key).toBool());
        }
    }

    const QObjectList &children = parent->children();
    for (QObject *ch : children)
    {
        QWidget *cw = qobject_cast<QWidget*>(ch);
        if (cw) recursiveRestoreInput(cw, settings, prefix);
    }
}

void UiUtils::restoreInputsState(QWidget *parent, const QString &key)
{
    QSettings set;
    recursiveRestoreInput(parent, set, key);
}

// ==================== 调试打印控件树 ====================
void UiUtils::dumpWidgetTree(QWidget *w, int indent)
{
    if (!w) return;
    QString space(indent * 2, ' ');
    qDebug() << space << w->metaObject()->className() << "objName:" << w->objectName();
    const QObjectList &children = w->children();
    for (QObject *ch : children)
    {
        QWidget *cw = qobject_cast<QWidget*>(ch);
        if (cw) dumpWidgetTree(cw, indent + 1);
    }
}
}
