#ifndef FRAMELESSATTACHER_H
#define FRAMELESSATTACHER_H

#include <QObject>
#include <QWidget>
#include <QPointer>
#include <QPoint>
#include "SqzGlobal.h"
namespace Sqz{
class SQZ_FRAMEWORK_API FramelessAttacher : public QObject
{
    Q_OBJECT
public:
    // 附加无边框能力
    static FramelessAttacher* attach(QWidget* win);

    // 设置拖拽标题栏
    FramelessAttacher* dragBar(QWidget* bar);
    // 开启双击最大化
    FramelessAttacher* dbClickMax(bool on);

protected:
    // 事件过滤器
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    explicit FramelessAttacher(QWidget* win);

    QPointer<QWidget> m_win;
    QPointer<QWidget> m_bar;
    QPoint m_offset;
    bool m_dbClickMax = true;
};
}
#endif // FRAMELESSATTACHER_H
