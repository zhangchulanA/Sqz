#include "FramelessAttacher.h"
#include <QEvent>
#include <QMouseEvent>

FramelessAttacher::FramelessAttacher(QWidget* win)
    : QObject(win), m_win(win)
{
    m_win->installEventFilter(this);
    // 无边框标记
    m_win->setWindowFlags(m_win->windowFlags() | Qt::FramelessWindowHint);
}

FramelessAttacher* FramelessAttacher::attach(QWidget* win)
{
    if (!win) return nullptr;
    return new FramelessAttacher(win);
}

FramelessAttacher* FramelessAttacher::dragBar(QWidget* bar)
{
    m_bar = bar;
    return this;
}

FramelessAttacher* FramelessAttacher::dbClickMax(bool on)
{
    m_dbClickMax = on;
    return this;
}

bool FramelessAttacher::eventFilter(QObject* obj, QEvent* event)
{
    if (obj != m_win.data())
        return QObject::eventFilter(obj, event);

    // 鼠标按下：记录偏移
    if (event->type() == QEvent::MouseButtonPress)
    {
        auto me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton && m_bar && m_bar->underMouse())
        {
            m_offset = me->globalPos() - m_win->frameGeometry().topLeft();
            event->accept();
        }
    }

    // 鼠标拖动：移动窗口
    if (event->type() == QEvent::MouseMove)
    {
        auto me = static_cast<QMouseEvent*>(event);
        if (me->buttons() & Qt::LeftButton && m_bar && m_bar->underMouse())
        {
            m_win->move(me->globalPos() - m_offset);
            event->accept();
        }
    }

    // 双击切换最大化/正常
    if (event->type() == QEvent::MouseButtonDblClick && m_dbClickMax)
    {
        auto me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton && m_bar && m_bar->underMouse())
        {
            m_win->isMaximized() ? m_win->showNormal() : m_win->showMaximized();
            event->accept();
        }
    }

    return false;
}
