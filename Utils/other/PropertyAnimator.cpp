#include "PropertyAnimator.h"
#include <QMetaObject>
#include <QMetaProperty>
#include <QDebug>
#include <QParallelAnimationGroup>
#include <QSequentialAnimationGroup>
#include <QPropertyAnimation>

// ============================================================
// PropertyAnimator 实现
// ============================================================
namespace Sqz::Utils {
/** 构造函数：初始化成员变量，连接 finished 信号到内部槽 */
PropertyAnimator::PropertyAnimator(QObject* target, QObject* parent)
    : QVariantAnimation(parent)
    , m_target(target)
    , m_autoDelete(false)
    , m_delay(0)
{
    connect(this, &QAbstractAnimation::finished,
            this, &PropertyAnimator::onAnimationFinished);
}

/** 设置目标对象 */
void PropertyAnimator::setTarget(QObject* target)
{
    m_target = target;
}

/** 设置自定义值设置器（lambda） */
void PropertyAnimator::setSetter(std::function<void(const QVariant&)> setter)
{
    m_setter = std::move(setter);
}

/** 通过属性名自动生成 setter（使用 Qt 的 setProperty） */
void PropertyAnimator::setPropertyName(const QString& name)
{
    if (!m_target) {
        qWarning() << "PropertyAnimator: target is null, cannot set property name";
        return;
    }
    QByteArray propName = name.toUtf8();
    m_setter = [this, propName](const QVariant& value) {
        if (m_target) {
            m_target->setProperty(propName.constData(), value);
        }
    };
}

/** 设置自定义插值器 */
void PropertyAnimator::setCustomInterpolator(std::function<QVariant(qreal)> interpolator)
{
    m_interpolator = std::move(interpolator);
}

/** 设置自动删除标志 */
void PropertyAnimator::setAutoDelete(bool on)
{
    m_autoDelete = on;
}

/** 立即应用结束值（用于 duration=0 的场景） */
void PropertyAnimator::applyEndValue()
{
    if (m_setter && m_target) {
        m_setter(endValue());
    }
}

/** QVariantAnimation 虚函数：每帧更新值时调用 setter */
void PropertyAnimator::updateCurrentValue(const QVariant& value)
{
    if (m_setter && m_target) {
        m_setter(value);
    }
}

/** 重写插值函数：若存在自定义插值器则使用，否则使用父类默认 */
QVariant PropertyAnimator::interpolated(const QVariant& from, const QVariant& to, qreal progress) const
{
    if (m_interpolator) {
        return m_interpolator(progress);
    }
    return QVariantAnimation::interpolated(from, to, progress);
}

/** 动画完成时若 autoDelete 为 true 则 deleteLater */
void PropertyAnimator::onAnimationFinished()
{
    if (m_autoDelete) {
        this->deleteLater();
    }
}

/** 延迟启动槽：直接调用父类 start() */
void PropertyAnimator::startWithDelay()
{
    QVariantAnimation::start();
}

// ---------- 静态工厂 ----------

/** 返回一个 AnimationBuilder 实例，用于链式配置 */
AnimationBuilder PropertyAnimator::animate(QObject* target)
{
    return AnimationBuilder(target);
}

/** 一键启动动画：创建 builder 并立即 start() */
void PropertyAnimator::animate(QObject* target, const QString& property,
                               const QVariant& from, const QVariant& to,
                               int duration, QEasingCurve::Type easing)
{
    AnimationBuilder builder(target);
    builder.property(property)
           .from(from)
           .to(to)
           .duration(duration)
           .easing(easing)
           .autoDelete(true)
           .start();
}

// ============================================================
// AnimationBuilder 实现
// ============================================================

/** 构造函数：创建内部 PropertyAnimator 并设定目标 */
AnimationBuilder::AnimationBuilder(QObject* target)
    : m_ownAnim(true)
{
    m_anim = new PropertyAnimator(target);
    m_anim->setTarget(target);
}

/** 析构函数：若拥有所有权且动画未运行，则释放内存 */
AnimationBuilder::~AnimationBuilder()
{
    if (m_ownAnim && m_anim) {
        if (m_anim->state() != QAbstractAnimation::Running) {
            delete m_anim;
        }
        // 若正在运行，由 autoDelete 处理
    }
}

/** 设置属性名（内部生成 setter） */
AnimationBuilder& AnimationBuilder::property(const QString& name)
{
    m_anim->setPropertyName(name);
    return *this;
}

/** 设置自定义 setter */
AnimationBuilder& AnimationBuilder::setter(std::function<void(const QVariant&)> setter)
{
    m_anim->setSetter(std::move(setter));
    return *this;
}

/** 设置起始值 */
AnimationBuilder& AnimationBuilder::from(const QVariant& from)
{
    m_anim->setStartValue(from);
    return *this;
}

/** 设置结束值 */
AnimationBuilder& AnimationBuilder::to(const QVariant& to)
{
    m_anim->setEndValue(to);
    return *this;
}

/** 设置持续时间（毫秒） */
AnimationBuilder& AnimationBuilder::duration(int ms)
{
    m_anim->setDuration(ms);
    return *this;
}

/** 设置缓动曲线类型 */
AnimationBuilder& AnimationBuilder::easing(QEasingCurve::Type type)
{
    m_anim->setEasingCurve(QEasingCurve(type));
    return *this;
}

/** 设置自定义缓动曲线对象 */
AnimationBuilder& AnimationBuilder::easing(const QEasingCurve& curve)
{
    m_anim->setEasingCurve(curve);
    return *this;
}

/** 设置循环次数（-1 表示无限） */
AnimationBuilder& AnimationBuilder::loop(int count)
{
    m_anim->setLoopCount(count);
    return *this;
}

/** 设置启动延迟（毫秒） */
AnimationBuilder& AnimationBuilder::delay(int ms)
{
    m_anim->setDelay(ms);
    return *this;
}

/** 设置自动删除标志 */
AnimationBuilder& AnimationBuilder::autoDelete(bool on)
{
    m_anim->setAutoDelete(on);
    return *this;
}

/** 设置自定义插值器 */
AnimationBuilder& AnimationBuilder::customInterpolator(std::function<QVariant(qreal)> interpolator)
{
    m_anim->setCustomInterpolator(std::move(interpolator));
    return *this;
}

/** 设置动画开始回调（通过 stateChanged 检测启动） */
AnimationBuilder& AnimationBuilder::onStarted(std::function<void()> callback)
{
    QObject::connect(m_anim, &QAbstractAnimation::stateChanged,
        [callback](QAbstractAnimation::State newState, QAbstractAnimation::State oldState) {
            if (newState == QAbstractAnimation::Running && oldState == QAbstractAnimation::Stopped) {
                callback();
            }
        });
    return *this;
}

/** 设置动画结束回调 */
AnimationBuilder& AnimationBuilder::onFinished(std::function<void()> callback)
{
    QObject::connect(m_anim, &QAbstractAnimation::finished, callback);
    return *this;
}

/** 设置动画更新回调（每帧值变化） */
AnimationBuilder& AnimationBuilder::onUpdate(std::function<void(const QVariant&)> callback)
{
    QObject::connect(m_anim, &QVariantAnimation::valueChanged, callback);
    return *this;
}

/** 启动动画：若 duration==0 直接应用结束值；否则根据延迟启动 */
void AnimationBuilder::start()
{
    if (!m_anim) return;

    int dur = m_anim->duration();
    if (dur == 0) {
        m_anim->applyEndValue();
        emit m_anim->finished();
        if (m_anim->autoDelete()) m_anim->deleteLater();
        return;
    }

    int delayMs = m_anim->delay();
    if (delayMs > 0) {
        QTimer::singleShot(delayMs, m_anim, &PropertyAnimator::startWithDelay);
    } else {
        m_anim->start();
    }
}

/** 转移所有权：调用后 builder 不再管理动画对象 */
PropertyAnimator* AnimationBuilder::take()
{
    m_ownAnim = false;
    return m_anim;
}

// ============================================================
// AnimationGroupBuilder 实现
// ============================================================

/** 构造函数：指定执行模式（并行/顺序） */
AnimationGroupBuilder::AnimationGroupBuilder(Mode mode)
    : m_mode(mode)
{
}

/** 析构函数：清理未使用的动画对象 */
AnimationGroupBuilder::~AnimationGroupBuilder()
{
    for (auto anim : m_animations) {
        anim->deleteLater();
    }
}

/** 添加由 AnimationBuilder 构建的动画（自动转移所有权） */
AnimationGroupBuilder& AnimationGroupBuilder::add(AnimationBuilder& builder)
{
    PropertyAnimator* anim = builder.take();
    if (anim) {
        m_animations.append(anim);
    }
    return *this;
}

/** 直接添加 PropertyAnimator 指针 */
AnimationGroupBuilder& AnimationGroupBuilder::add(PropertyAnimator* anim)
{
    if (anim) m_animations.append(anim);
    return *this;
}

/** 设置组合完成回调 */
AnimationGroupBuilder& AnimationGroupBuilder::onFinished(std::function<void()> callback)
{
    m_onFinished = callback;
    return *this;
}

/** 启动组合动画：创建 QParallel 或 QSequential 组并启动 */
void AnimationGroupBuilder::start()
{
    if (m_animations.isEmpty()) return;

    QAnimationGroup* group = nullptr;
    if (m_mode == Parallel) {
        group = new QParallelAnimationGroup();
    } else {
        group = new QSequentialAnimationGroup();
    }

    for (auto anim : m_animations) {
        group->addAnimation(anim);
    }

    if (m_onFinished) {
        QObject::connect(group, &QAnimationGroup::finished, m_onFinished);
    }

    group->start(QAbstractAnimation::DeleteWhenStopped);
    m_animations.clear(); // group 已接管所有权
}

// ============================================================
// 静态工具实现
// ============================================================

/** 返回并行组合构建器 */
AnimationGroupBuilder PropertyAnimatorTools::parallel()
{
    return AnimationGroupBuilder(AnimationGroupBuilder::Parallel);
}

/** 返回顺序组合构建器 */
AnimationGroupBuilder PropertyAnimatorTools::sequential()
{
    return AnimationGroupBuilder(AnimationGroupBuilder::Sequential);
}

/** 返回 InOutQuad 缓动曲线 */
QEasingCurve PropertyAnimatorTools::easeInOut()
{
    return QEasingCurve(QEasingCurve::InOutQuad);
}

/** 返回 OutBack 缓动曲线（弹性效果） */
QEasingCurve PropertyAnimatorTools::easeOutBack()
{
    return QEasingCurve(QEasingCurve::OutBack);
}
}
