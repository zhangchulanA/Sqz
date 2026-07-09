#ifndef TIMEOUTKEEPER_H
#define TIMEOUTKEEPER_H

#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QHash>
#include <QMutex>
#include <functional>
#include "Singleton.h"
#include "SqzGlobal.h"
/**
 * @brief 超时守护器 - 管理多个独立 key 的数据接收超时检测
 *
 * 适用场景：
 *   - 同时监听多个 TCP 连接、串口、传感器或业务会话
 *   - 每个 key 可以设置不同的超时阈值和超时回调
 *   - 只需一个定时器轮询所有 key，资源占用极低
 *
 * 核心用法：
 *   1. 创建 TimeoutKeeper 实例，调用 start() 启动内部轮询定时器
 *   2. 每次成功收到数据时，调用 refresh(key) 刷新该 key 的最后活动时间
 *   3. 当某个 key 超过阈值未刷新，将触发其超时回调（或发出信号）
 *   4. 超时后若再次收到该 key 的数据，超时状态自动清除并重新计时
 *
 * 线程安全：所有公共函数均已加锁，可在任意线程调用
 */


namespace Sqz::Utils {
class SQZ_FRAMEWORK_API TimeoutKeeper : public QObject , public Singleton<TimeoutKeeper>
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent Qt 父对象
     */
    explicit TimeoutKeeper(QObject *parent = nullptr);

    /**
     * @brief 析构函数，自动停止定时器并释放资源
     */
    ~TimeoutKeeper();

    /**
     * @brief 启动全局超时检测（只需调用一次）
     * @param globalCheckIntervalMs 轮询间隔（毫秒），建议设为所有超时阈值中最短值的 1/2 ~ 1/5
     *
     * 例如：最短超时阈值为 2000ms，则检查间隔可设为 500ms ~ 1000ms。
     * 间隔过大会导致超时响应延迟，过小则增加 CPU 负担。
     */
    void start(int globalCheckIntervalMs = 1000);

    /**
     * @brief 停止全局检测，并清空所有已注册的 key
     */
    void stop();

    /**
     * @brief 刷新某个 key 的最后活动时间（每次收到该 key 的有效数据时调用）
     * @param key 唯一标识（如连接ID、会话ID、传感器名称等）
     *
     * 行为：
     *   - 如果 key 尚未注册，则自动注册并使用默认超时阈值（5000ms）
     *   - 如果 key 之前处于超时状态，收到数据后自动清除超时标志并重新计时
     */
    void refresh(const QString &key);

    /**
     * @brief 显式添加一个监视 key，并指定超时阈值和回调
     * @param key 唯一标识
     * @param timeoutMs 超时毫秒数（必须 > 0）
     * @param onTimeout 超时回调函数（可为空，后续可通过 setTimeoutHandler 设置）
     */
    void addMonitor(const QString &key, int timeoutMs, std::function<void()> onTimeout = nullptr);

    /**
     * @brief 移除监视的 key，不再检测其超时
     * @param key 唯一标识
     */
    void removeMonitor(const QString &key);

    /**
     * @brief 为已存在的 key 设置或修改超时回调
     * @param key 唯一标识（必须先通过 addMonitor 或 refresh 注册）
     * @param handler 超时回调函数
     */
    void setTimeoutHandler(const QString &key, std::function<void()> handler);

    /**
     * @brief 查询某个 key 当前是否处于超时状态
     * @param key 唯一标识
     * @return true-已超时，false-未超时或 key 不存在
     */
    bool isTimeout(const QString &key) const;

    /**
     * @brief 动态修改某个 key 的超时阈值
     * @param key 唯一标识
     * @param ms 新的超时毫秒数（仅对已存在的 key 生效）
     */
    void setTimeoutMs(const QString &key, int ms);

    /**
     * @brief 获取默认超时阈值（用于自动注册的 key）
     * @return 默认超时毫秒数
     */
    int defaultTimeoutMs() const;

    /**
     * @brief 设置默认超时阈值（仅影响之后自动注册的 key，已存在的 key 不受影响）
     * @param ms 新的默认超时毫秒数
     */
    void setDefaultTimeoutMs(int ms);

    /**
     * @brief 动态修改全局轮询检查间隔（需要先调用 start 启动）
     * @param ms 新的检查间隔（毫秒）
     */
    void setCheckInterval(int ms);

signals:
    /**
     * @brief 超时信号，携带发生超时的 key
     * @param key 超时的标识
     *
     * 可连接此信号实现统一处理，也可以为每个 key 单独设置回调。
     * 两种方式可同时使用，互不影响。
     */
    void timeoutOccurred(const QString &key);

private slots:
    /** @brief 定时器槽函数，轮询所有 key 检查超时 */
    void checkAllTimeouts();

private:
    /** @brief 存储每个 key 的超时信息 */
    struct TimeoutInfo
    {
        QElapsedTimer lastReceivedTimer;  // 最后一次收到数据的高精度计时器
        int timeoutMs;                    // 该 key 的超时阈值（毫秒）
        bool isTimeout;                   // 当前是否已处于超时状态
        std::function<void()> callback;   // 超时回调函数

        TimeoutInfo() : timeoutMs(5000), isTimeout(false) {}
    };

    QTimer m_checkTimer;                  // 统一轮询定时器
    QHash<QString, TimeoutInfo> m_infos;  // key -> 超时信息映射表
    mutable QMutex m_mutex;               // 保护 m_infos 的互斥锁（线程安全）
    int m_defaultTimeoutMs;               // 自动注册时的默认超时阈值（毫秒）
    bool m_started;                       // 是否已启动检测
};
}
#endif // TIMEOUTKEEPER_H
