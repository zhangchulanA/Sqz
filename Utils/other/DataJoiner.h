#ifndef DATAJOINER_H
#define DATAJOINER_H

#include <QMap>
#include <QVariant>
#include <functional>
#include "SqzGlobal.h"
/* ============================================================
   DataJoiner：多路数据合并工具（Namespace 风格，无需实例化）

   核心功能：
   - 等待 N 路数据全部到齐后，统一触发合并回调
   - 支持同时管理多个独立合并任务（通过任务 ID 区分）
   - 支持超时保护，超时后触发部分数据回调
   - 线程安全，可在任意线程调用

   使用流程：
   1. Begin(count)          -> 获取任务 ID
   2. OnReady(id, callback) -> 设置凑齐回调
   3. OnTimeout(id, cb)     -> 设置超时回调（可选）
   4. SetTimeout(id, ms)    -> 设置超时时间（可选）
   5. Feed(id, tag, data)   -> 喂入各路数据
   6. 凑齐后自动触发 OnReady，任务自动销毁
   ============================================================ */

namespace Sqz {

// 回调函数类型
using ReadyCallback = std::function<void(const QMap<QString, QVariant>&)>;
using TimeoutCallback = std::function<void(const QMap<QString, QVariant>&)>;

/* ---------- 任务管理 ---------- */

// 开始一个合并任务：需要等待 expectedCount 路数据
// 返回任务 ID（>0），失败返回 -1
SQZ_FRAMEWORK_API int Begin(int expectedCount);

// 取消任务（立即停止，不再触发任何回调）
SQZ_FRAMEWORK_API void Cancel(int taskId);

// 重置任务（清空已收数据，重新开始等待）
SQZ_FRAMEWORK_API void Reset(int taskId);

/* ---------- 回调设置 ---------- */

// 设置"凑齐"回调：当所有路数到齐时触发
SQZ_FRAMEWORK_API void OnReady(int taskId, ReadyCallback callback);

// 设置"超时"回调：超过设定时间未凑齐时触发
SQZ_FRAMEWORK_API void OnTimeout(int taskId, TimeoutCallback callback);

// 设置超时时间（毫秒），默认无限等待
SQZ_FRAMEWORK_API void SetTimeout(int taskId, int ms);

/* ---------- 核心操作 ---------- */

// 喂入一路数据（tag 是来源标识，如 "GPS"、"ACCEL"）
SQZ_FRAMEWORK_API void Feed(int taskId, const QString& tag, const QVariant& data);

/* ---------- 状态查询（调试用） ---------- */

// 查询任务是否已凑齐
SQZ_FRAMEWORK_API bool IsReady(int taskId);

// 查询任务当前已收到几路数据
SQZ_FRAMEWORK_API int ReceivedCount(int taskId);

// 查询当前有多少个活跃任务
SQZ_FRAMEWORK_API int ActiveCount();

// 重置所有任务（清空全部状态，慎用）
SQZ_FRAMEWORK_API void ResetAll();

} // namespace Sqz

#endif // DATAJOINER_H
