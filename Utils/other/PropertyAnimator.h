#ifndef PROPERTYANIMATOR_H
#define PROPERTYANIMATOR_H

#include <QObject>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QTimer>
#include <functional>
#include <QMetaProperty>
#include "SqzGlobal.h"

namespace Sqz::Utils {
// 前向声明
class AnimationBuilder;
class AnimationGroupBuilder;

// ============================================================
// 核心动画类：继承 QVariantAnimation，支持任意类型属性动画
// ============================================================
class SQZ_FRAMEWORK_API PropertyAnimator : public QVariantAnimation
{
    Q_OBJECT

private:
    QObject* m_target;                                // 动画目标对象
    std::function<void(const QVariant&)> m_setter;    // 自定义值设置器
    std::function<QVariant(qreal)> m_interpolator;    // 自定义插值器
    bool m_autoDelete;                                // 完成后自动销毁
    int m_delay;                                      // 启动延迟（毫秒）

public:
    /** 构造函数，target 为目标对象，parent 为父对象 */
    explicit PropertyAnimator(QObject* target = nullptr, QObject* parent = nullptr);

    // ---------- 配置方法 ----------
    /** 设置目标对象 */
    void setTarget(QObject* target);
    /** 设置自定义 setter（lambda） */
    void setSetter(std::function<void(const QVariant&)> setter);
    /** 通过属性名自动生成 setter（使用 setProperty） */
    void setPropertyName(const QString& name);
    /** 设置自定义插值器（参数 progress 0~1，返回对应值） */
    void setCustomInterpolator(std::function<QVariant(qreal)> interpolator);
    /** 设置动画完成后是否自动删除对象 */
    void setAutoDelete(bool on);
    /** 获取自动删除标志 */
    bool autoDelete() const { return m_autoDelete; }
    /** 设置启动延迟时间（毫秒） */
    void setDelay(int ms) { m_delay = ms; }
    /** 获取启动延迟时间 */
    int delay() const { return m_delay; }

    /** 立即应用结束值（用于 duration==0 时直接跳转） */
    void applyEndValue();

    // ---------- 工厂方法 ----------
    /** 静态工厂：返回链式构建器 */
    static AnimationBuilder animate(QObject* target);
    /** 一行启动动画（非链式，自动设置并启动） */
    static void animate(QObject* target, const QString& property,
                        const QVariant& from, const QVariant& to,
                        int duration = 300, QEasingCurve::Type easing = QEasingCurve::Linear);

protected:
    /** QVariantAnimation 虚函数：更新当前值（内部调用 setter） */
    void updateCurrentValue(const QVariant& value) override;
    /** 重写插值函数：若设置了自定义插值器则使用，否则调用父类 */
    QVariant interpolated(const QVariant& from, const QVariant& to, qreal progress) const override;

public slots:
    /** 延迟启动的槽函数（供 QTimer 调用） */
    void startWithDelay();

private slots:
    /** 动画完成时的处理（自动删除） */
    void onAnimationFinished();
};

// ============================================================
// 链式构建器：支持流式配置，最后 start() 启动
// ============================================================
class AnimationBuilder
{
private:
    PropertyAnimator* m_anim;    // 持有的动画对象
    bool m_ownAnim;              // 是否拥有所有权（用于析构时清理）

    friend class AnimationGroupBuilder;  // 允许组合构建器访问私有成员

public:
    /** 构造函数：创建内部 PropertyAnimator 实例 */
    explicit AnimationBuilder(QObject* target);
    /** 析构函数：若拥有所有权且动画未运行则释放 */
    ~AnimationBuilder();

    // ---------- 链式配置方法 ----------
    /** 设置属性名（自动生成 setter） */
    AnimationBuilder& property(const QString& name);
    /** 设置自定义 setter */
    AnimationBuilder& setter(std::function<void(const QVariant&)> setter);
    /** 设置起始值 */
    AnimationBuilder& from(const QVariant& from);
    /** 设置结束值 */
    AnimationBuilder& to(const QVariant& to);
    /** 设置持续时间（毫秒） */
    AnimationBuilder& duration(int ms);
    /** 设置缓动曲线类型（Qt 内置） */
    AnimationBuilder& easing(QEasingCurve::Type type);
    /** 设置自定义缓动曲线对象 */
    AnimationBuilder& easing(const QEasingCurve& curve);
    /** 设置循环次数（-1 为无限循环） */
    AnimationBuilder& loop(int count);
    /** 设置启动延迟（毫秒） */
    AnimationBuilder& delay(int ms);
    /** 设置自动删除标志 */
    AnimationBuilder& autoDelete(bool on = true);
    /** 设置自定义插值器 */
    AnimationBuilder& customInterpolator(std::function<QVariant(qreal)> interpolator);
    /** 设置动画开始回调 */
    AnimationBuilder& onStarted(std::function<void()> callback);
    /** 设置动画结束回调 */
    AnimationBuilder& onFinished(std::function<void()> callback);
    /** 设置动画更新回调（每帧变化） */
    AnimationBuilder& onUpdate(std::function<void(const QVariant&)> callback);

    /** 启动动画（根据延迟时间决定立即启动或定时启动） */
    void start();

    /** 转移所有权（供组合动画使用），调用后 builder 不再管理动画 */
    PropertyAnimator* take();

private:
    /** 内部回调连接（暂未使用，可扩展） */
    void connectCallbacks();
};

// ============================================================
// 组合动画构建器：支持并行 / 顺序执行多个动画
// ============================================================
class AnimationGroupBuilder
{
    Q_DISABLE_COPY(AnimationGroupBuilder)

public:
    enum Mode { Parallel, Sequential };   // 执行模式

private:
    Mode m_mode;                                 // 当前模式
    QList<PropertyAnimator*> m_animations;       // 包含的动画列表
    std::function<void()> m_onFinished;          // 组合完成回调

public:
    /** 构造函数：指定模式（默认并行） */
    explicit AnimationGroupBuilder(Mode mode = Parallel);
    /** 析构：清理未启动的动画 */
    ~AnimationGroupBuilder();

    /** 添加一个 AnimationBuilder（自动转移所有权） */
    AnimationGroupBuilder& add(AnimationBuilder& builder);
    /** 直接添加 PropertyAnimator 指针 */
    AnimationGroupBuilder& add(PropertyAnimator* anim);
    /** 设置组合完成回调 */
    AnimationGroupBuilder& onFinished(std::function<void()> callback);

    /** 启动组合动画（内部创建 QParallel 或 QSequential 组） */
    void start();
};

// ============================================================
// 静态工具类：提供预定义缓动和组合入口
// ============================================================
class PropertyAnimatorTools
{
public:
    /** 创建并行组合构建器 */
    static AnimationGroupBuilder parallel();
    /** 创建顺序组合构建器 */
    static AnimationGroupBuilder sequential();

    /** 返回 InOutQuad 缓动曲线 */
    static QEasingCurve easeInOut();
    /** 返回 OutBack 缓动曲线（弹性效果） */
    static QEasingCurve easeOutBack();
};
}
#endif // PROPERTYANIMATOR_H
