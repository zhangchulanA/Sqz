#ifndef BIZSANDBOX_H
#define BIZSANDBOX_H
#include <QDebug>
#include <algorithm>
#include <QVector>
#include <QVariant>
#include <optional>
#include <functional>
#include <QElapsedTimer>
#include <QMap>
#include <QSet>
#include <QWeakPointer>

namespace Sqz
{
// 沙盒数据三种模式
enum class SandboxMode
{
    SingleObject,   // 单个实体对象
    Container,      // 批量容器 QVector<T>
    LazyStream      // 惰性生成流，不缓存全量数据
};

// 算子阶段耗时记录
struct StageTime
{
    QString stageName;
    qint64 ms;
};

template<typename T>
class BizSandbox
{
public:
    using LazyFunc = std::function<std::optional<T>()>;
    using GlobalExceptHook = std::function<void(const QString& stage, const QString& errMsg)>;

    // 构造/拷贝/移动
    BizSandbox() = default;
    BizSandbox(const BizSandbox<T>& other)
    {
        m_mode = other.m_mode;
        m_single = other.m_single;
        m_container = other.m_container;
        m_lazySource = other.m_lazySource;
        m_debugEnable = other.m_debugEnable;
        m_timeList = other.m_timeList;
        m_lazyMaxRead = other.m_lazyMaxRead;
        m_globalExceptHook = other.m_globalExceptHook;
    }
    BizSandbox(BizSandbox<T>&& other) noexcept
    {
        m_mode = other.m_mode;
        m_single = std::move(other.m_single);
        m_container = std::move(other.m_container);
        m_lazySource = std::move(other.m_lazySource);
        m_debugEnable = other.m_debugEnable;
        m_timeList = std::move(other.m_timeList);
        m_lazyMaxRead = other.m_lazyMaxRead;
        m_globalExceptHook = std::move(other.m_globalExceptHook);
        other.reset();
    }

    // 重置沙盒全部状态
    BizSandbox& reset()
    {
        m_mode = SandboxMode::SingleObject;
        m_single = T{};
        m_container.clear();
        m_lazySource = nullptr;
        m_debugEnable = true;
        m_timeList.clear();
        m_lazyMaxRead = 100000;
        return *this;
    }

    // 生成独立数据分支，互不干扰
    BizSandbox<T> branch() const { return BizSandbox<T>(*this); }

    //==================== 数据加载 ====================
    // 加载单个对象
    BizSandbox& loadData(const T& obj)
    {
        reset();
        m_mode = SandboxMode::SingleObject;
        m_single = obj;
        return *this;
    }
    BizSandbox& loadData(T&& obj)
    {
        reset();
        m_mode = SandboxMode::SingleObject;
        m_single = std::move(obj);
        return *this;
    }

    // 加载任意STL/Qt容器
    template<typename Container>
    BizSandbox& loadData(const Container& cont)
    {
        reset();
        m_mode = SandboxMode::Container;
        m_container.assign(cont.begin(), cont.end());
        return *this;
    }
    template<typename Container>
    BizSandbox& loadData(Container&& cont)
    {
        reset();
        m_mode = SandboxMode::Container;
        m_container = QVector<T>(std::make_move_iterator(cont.begin()), std::make_move_iterator(cont.end()));
        return *this;
    }

    // 加载惰性数据源
    BizSandbox& loadLazySource(LazyFunc func)
    {
        reset();
        m_mode = SandboxMode::LazyStream;
        m_lazySource = std::move(func);
        return *this;
    }

    // 设置惰性流最大读取上限，防死循环
    BizSandbox& setLazyMaxRead(int limit)
    {
        m_lazyMaxRead = limit;
        return *this;
    }

    // 全局异常钩子，统一捕获所有算子异常
    BizSandbox& setGlobalExceptHook(GlobalExceptHook hook)
    {
        m_globalExceptHook = std::move(hook);
        return *this;
    }
    template<typename ExceptFunc>
    BizSandbox& catchException(ExceptFunc&& errCb)
    {
        m_globalExceptHook = [cb = std::forward<ExceptFunc>(errCb)](const QString& st, const QString& err)
        {
            cb(st, err);
        };
        return *this;
    }

    // 管道运算符，替代链式点调用
    BizSandbox& operator>>(std::function<BizSandbox&(BizSandbox&)> op)
    {
        return op(*this);
    }

    //==================== 算子计时统一包装（内置异常捕获） ====================
    template<typename Func, typename ExceptFunc = GlobalExceptHook>
    BizSandbox& runStage(const QString& stageName, Func&& work, ExceptFunc&& exceptCb = {})
    {
        auto errHandler = exceptCb ? exceptCb : m_globalExceptHook;
        if (!m_debugEnable)
        {
            try { work(); }
            catch (const std::exception& e)
            {
                if (errHandler) errHandler(stageName, e.what());
            }
            catch (...)
            {
                if (errHandler) errHandler(stageName, "unknown exception");
            }
            return *this;
        }
        QElapsedTimer t;
        t.start();
        try
        {
            work();
        }
        catch (const std::exception& e)
        {
            qDebug() << "Stage err:" << stageName << e.what();
            if (errHandler) errHandler(stageName, e.what());
        }
        catch (...)
        {
            qDebug() << "Stage unknown err:" << stageName;
            if (errHandler) errHandler(stageName, "unknown exception");
        }
        qint64 cost = t.elapsed();
        m_timeList.push_back({stageName, cost});
        return *this;
    }

    //==================== 基础流式算子 ====================
    // 过滤数据
    template<typename Func>
    BizSandbox& filter(Func&& predicate)
    {
        return runStage("filter", [&]()
        {
            if (m_mode == SandboxMode::SingleObject)
            {
                if (!predicate(m_single)) m_single = T{};
            }
            else if (m_mode == SandboxMode::Container)
            {
                QVector<T> out;
                out.reserve(m_container.size());
                for (auto& item : m_container)
                    if (predicate(item)) out.push_back(std::move(item));
                m_container.swap(out);
            }
            else if (m_mode == SandboxMode::LazyStream)
            {
                auto oldSource = m_lazySource;
                int maxLimit = m_lazyMaxRead;
                m_lazySource = [oldSource, pred = std::forward<Func>(predicate), maxLimit]() mutable -> std::optional<T>
                {
                    int readCnt = 0;
                    while (readCnt < maxLimit)
                    {
                        auto opt = oldSource();
                        if (!opt) return std::nullopt;
                        readCnt++;
                        if (pred(opt.value())) return opt;
                    }
                    return std::nullopt;
                };
            }
        });
    }

    // 字段原地转换
    template<typename Func>
    BizSandbox& convertField(Func&& convertFunc)
    {
        return runStage("convertField", [&]()
        {
            if (m_mode == SandboxMode::SingleObject)
                convertFunc(m_single);
            else if (m_mode == SandboxMode::Container)
                for (auto& item : m_container) convertFunc(item);
            else if (m_mode == SandboxMode::LazyStream)
            {
                auto oldSource = m_lazySource;
                m_lazySource = [oldSource, func = std::forward<Func>(convertFunc)]() -> std::optional<T>
                {
                    auto opt = oldSource();
                    if (opt.has_value())
                    {
                        auto val = opt.value();
                        func(val);
                        return val;
                    }
                    return std::nullopt;
                };
            }
        });
    }

    // 数据校验，语义等价filter
    template<typename Func>
    BizSandbox& validate(Func&& checkFunc)
    {
        return runStage("validate", [&]() { filter(std::forward<Func>(checkFunc)); });
    }

    // 跳过前N条（支持惰性流）
    BizSandbox& skip(int count)
    {
        return runStage("skip", [&]()
        {
            if (m_mode == SandboxMode::Container)
            {
                if (count < m_container.size()) m_container = m_container.mid(count);
                else m_container.clear();
            }
            else if (m_mode == SandboxMode::LazyStream)
            {
                auto oldSrc = m_lazySource;
                int skipNum = count;
                m_lazySource = [oldSrc, skipNum, read = 0]() mutable -> std::optional<T>
                {
                    while (read < skipNum)
                    {
                        auto opt = oldSrc();
                        if (!opt) return std::nullopt;
                        read++;
                    }
                    return oldSrc();
                };
            }
        });
    }

    // 仅保留前N条（支持惰性流）
    BizSandbox& take(int count)
    {
        return runStage("take", [&]()
        {
            if (m_mode == SandboxMode::Container)
            {
                if (m_container.size() > count) m_container.resize(count);
            }
            else if (m_mode == SandboxMode::LazyStream)
            {
                auto oldSrc = m_lazySource;
                int takeNum = count;
                m_lazySource = [oldSrc, takeNum, read = 0]() mutable -> std::optional<T>
                {
                    if (read >= takeNum) return std::nullopt;
                    auto opt = oldSrc();
                    if (opt) read++;
                    return opt;
                };
            }
        });
    }

    // 容器排序
    template<typename Func>
    BizSandbox& sort(Func&& cmp)
    {
        return runStage("sort", [&]()
        {
            if (m_mode == SandboxMode::Container)
                std::sort(m_container.begin(), m_container.end(), cmp);
        });
    }

    // 遍历回调，不修改数据
    template<typename Func>
    BizSandbox& each(Func&& func)
    {
        return runStage("each", [&]()
        {
            if (m_mode == SandboxMode::SingleObject)
                func(m_single);
            else if (m_mode == SandboxMode::Container)
                for (auto& v : m_container) func(v);
        });
    }

    // QVariant键去重（兼容旧业务）
    template<typename KeyFunc>
    BizSandbox& distinct(KeyFunc&& keyFunc)
    {
        return runStage("distinct", [&]()
        {
            if (m_mode != SandboxMode::Container) return;
            QVector<T> out;
            out.reserve(m_container.size());
            QSet<QVariant> existKeys;
            for (auto& row : m_container)
            {
                QVariant key = keyFunc(row);
                if (!existKeys.contains(key))
                {
                    existKeys.insert(key);
                    out.push_back(std::move(row));
                }
            }
            m_container.swap(out);
        });
    }

    //==================== 条件分支算子 ====================
    // 任意一条满足条件则执行分支
    template<typename CondFunc>
    BizSandbox& whenAny(CondFunc&& cond, std::function<void(BizSandbox&)> trueChain)
    {
        return runStage("whenAny", [&]()
        {
            bool hit = false;
            if (m_mode == SandboxMode::SingleObject)
                hit = cond(m_single);
            else if (m_mode == SandboxMode::Container)
            {
                for (auto& item : m_container)
                {
                    if (cond(item)) { hit = true; break; }
                }
            }
            if (hit) trueChain(*this);
        });
    }

    // 全部数据满足条件才执行分支
    template<typename CondFunc>
    BizSandbox& whenAll(CondFunc&& cond, std::function<void(BizSandbox&)> trueChain)
    {
        return runStage("whenAll", [&]()
        {
            bool hit = true;
            if (m_mode == SandboxMode::SingleObject)
                hit = cond(m_single);
            else if (m_mode == SandboxMode::Container)
            {
                for (auto& item : m_container)
                {
                    if (!cond(item)) { hit = false; break; }
                }
            }
            if (hit) trueChain(*this);
        });
    }

    //==================== 查询判断接口（const只读） ====================
    std::optional<T> findFirst() const
    {
        if (m_mode == SandboxMode::SingleObject) return m_single;
        if (m_mode == SandboxMode::Container && !m_container.empty()) return m_container.front();
        return std::nullopt;
    }

    template<typename Pred>
    bool any(Pred&& pred) const
    {
        if (m_mode == SandboxMode::SingleObject) return pred(m_single);
        for (const auto& v : m_container) if (pred(v)) return true;
        return false;
    }

    template<typename Pred>
    bool all(Pred&& pred) const
    {
        if (m_mode == SandboxMode::SingleObject) return pred(m_single);
        for (const auto& v : m_container) if (!pred(v)) return false;
        return true;
    }

    // 数据总数
    qsizetype count() const
    {
        if (m_mode == SandboxMode::SingleObject) return 1;
        if (m_mode == SandboxMode::Container) return m_container.size();
        return 0;
    }

    // 求和快捷聚合
    template<typename ValFunc, typename R>
    R sum(ValFunc&& getVal) const
    {
        R total{};
        if (m_mode == SandboxMode::SingleObject)
            total += getVal(m_single);
        else
            for (const auto& item : m_container) total += getVal(item);
        return total;
    }

    //==================== 类型映射 ====================
    template<typename B, typename MapFunc>
    BizSandbox<B> mapTo(MapFunc&& mapper)
    {
        BizSandbox<B> target;
        target.setGlobalExceptHook(m_globalExceptHook);
        target.setDebug(m_debugEnable);
        target.setLazyMaxRead(m_lazyMaxRead);
        if (m_mode == SandboxMode::SingleObject)
        {
            B res = mapper(m_single);
            target.loadData(std::move(res));
        }
        else if (m_mode == SandboxMode::Container)
        {
            QVector<B> list;
            list.reserve(m_container.size());
            for (auto& item : m_container)
                list.push_back(mapper(std::move(item)));
            target.loadData(std::move(list));
        }
        return target;
    }

    //==================== 分组聚合 ====================
    template<typename KeyFunc, typename AggFunc>
    BizSandbox& groupBy(KeyFunc&& getKey, AggFunc&& aggregator)
    {
        return runStage("groupBy", [&]()
        {
            if (m_mode != SandboxMode::Container) return;
            QMap<QVariant, QVector<T>> groups;
            groups.reserve(m_container.size() / 10);
            for (auto& row : m_container)
                groups[getKey(row)].push_back(std::move(row));
            QVector<T> result;
            result.reserve(m_container.size());
            for (auto& arr : groups.values())
                aggregator(arr, result);
            m_container.swap(result);
        });
    }

    //==================== 关联查询（性能优化索引版） ====================
    template<typename OtherT, typename KeyL, typename KeyR, typename JoinFunc>
    BizSandbox& leftJoin(const QVector<OtherT>& otherArr, KeyL&& getLKey, KeyR&& getRKey, JoinFunc&& combine)
    {
        return runStage("leftJoin", [&]()
        {
            if (m_mode != SandboxMode::Container) return;
            QMap<QVariant, QVector<OtherT>> rightIndex;
            rightIndex.reserve(otherArr.size());
            for (auto& r : otherArr) rightIndex[getRKey(r)].push_back(r);
            QVector<T> out;
            out.reserve(m_container.size());
            for (auto& leftRow : m_container)
            {
                auto lk = getLKey(leftRow);
                if (rightIndex.contains(lk))
                {
                    for (auto& match : rightIndex[lk])
                        out.push_back(combine(leftRow, match));
                }
                else
                    out.push_back(std::move(leftRow));
            }
            m_container.swap(out);
        });
    }

    template<typename OtherT, typename KeyL, typename KeyR, typename JoinFunc>
    BizSandbox& innerJoin(const QVector<OtherT>& otherArr, KeyL&& getLKey, KeyR&& getRKey, JoinFunc&& combine)
    {
        return runStage("innerJoin", [&]()
        {
            if (m_mode != SandboxMode::Container) return;
            QMap<QVariant, QVector<OtherT>> rightIndex;
            rightIndex.reserve(otherArr.size());
            for (auto& r : otherArr) rightIndex[getRKey(r)].push_back(r);
            QVector<T> out;
            out.reserve(m_container.size());
            for (auto& leftRow : m_container)
            {
                auto lk = getLKey(leftRow);
                if (rightIndex.contains(lk))
                {
                    for (auto& match : rightIndex[lk])
                        out.push_back(combine(leftRow, match));
                }
            }
            m_container.swap(out);
        });
    }

    //==================== 多沙盒合并 ====================
    BizSandbox& concat(const BizSandbox<T>& other)
    {
        return runStage("concat", [&]()
        {
            if (m_mode != SandboxMode::Container) return;
            const auto& otherCont = other.getContainer();
            m_container.reserve(m_container.size() + otherCont.size());
            std::copy(otherCont.cbegin(), otherCont.cend(), std::back_inserter(m_container));
        });
    }

    template<typename KeyFunc>
    BizSandbox& unionWith(const BizSandbox<T>& other, KeyFunc&& key)
    {
        concat(other);
        distinct(std::forward<KeyFunc>(key));
        return *this;
    }

    //==================== 惰性流转为内存容器 ====================
    QVector<T> lazyToContainer() const
    {
        QVector<T> res;
        if (m_mode != SandboxMode::LazyStream || !m_lazySource) return res;
        res.reserve(m_lazyMaxRead);
        while (auto opt = m_lazySource())
        {
            res.push_back(std::move(opt.value()));
        }
        return res;
    }

    // 惰性流转容器模式，用于sort/groupBy/distinct
    BizSandbox& materialize()
    {
        if (m_mode == SandboxMode::LazyStream)
        {
            m_container = lazyToContainer();
            m_lazySource = nullptr;
            m_mode = SandboxMode::Container;
        }
        return *this;
    }

    //==================== 生命周期钩子 ====================
    BizSandbox& beforeProcess(std::function<void()> cb)
    {
        return runStage("beforeProcess", [&](){ cb(); });
    }

    //==================== 调试快照 & 调试开关 ====================
    BizSandbox& setDebug(bool enable)
    {
        m_debugEnable = enable;
#ifdef QT_NO_DEBUG
        m_debugEnable = false;
#endif
        return *this;
    }

    // 打印快照，惰性流采样N条
    BizSandbox& dumpSnapshot(const QString& tag, int lazySampleCnt = 20)
    {
        if (!m_debugEnable) return *this;
        qDebug() << "\n====== BizSandbox[" << tag << "] ======";
        if (m_mode == SandboxMode::SingleObject)
        {
            qDebug() << "[Single]" << m_single;
        }
        else if (m_mode == SandboxMode::Container)
        {
            qDebug() << "[Container size]" << m_container.size();
            for (int i = 0; i < m_container.size(); ++i)
                qDebug() << i << " > " << m_container[i];
        }
        else if (m_mode == SandboxMode::LazyStream)
        {
            qDebug() << "[LazyStream sample " << lazySampleCnt << "]";
            auto tempSrc = m_lazySource;
            for (int i = 0; i < lazySampleCnt; ++i)
            {
                auto opt = tempSrc();
                if (!opt) break;
                qDebug() << i << " > " << opt.value();
            }
        }
        qDebug() << "-------- Stage Time --------";
        for (auto& st : m_timeList)
            qDebug() << st.stageName << ":" << st.ms << "ms";
        return *this;
    }

    // 自定义打印回调快照
    template<typename PrintFunc>
    BizSandbox& dumpSnapshot(const QString& tag, PrintFunc&& printCb)
    {
        if (!m_debugEnable) return *this;
        qDebug() << "\n====== BizSandbox[" << tag << "] ======";
        if (m_mode == SandboxMode::SingleObject)
            printCb(m_single);
        else if (m_mode == SandboxMode::Container)
        {
            int idx = 0;
            for (auto& item : m_container)
            {
                qDebug() << idx << " > ";
                printCb(item);
                idx++;
            }
        }
        qDebug() << "-------- Stage Time --------";
        for (auto& st : m_timeList)
            qDebug() << st.stageName << ":" << st.ms << "ms";
        return *this;
    }

    //==================== 入库接口 ====================
    std::optional<bool> saveToDb(std::function<bool(const T&)> singleInsert,
                                 std::function<bool(const QVector<T>&)> batchInsert)
    {
        runStage("saveToDb", [&](){ dumpSnapshot("save_db_preview"); });
        if (m_mode == SandboxMode::SingleObject)
            return singleInsert(m_single);
        else if (m_mode == SandboxMode::Container)
            return batchInsert(m_container);
        return std::nullopt;
    }

    //==================== 数据导出接口 ====================
    T getSingle() const { return m_single; }
    QVector<T>& getContainer() { return m_container; }
    const QVector<T>& getContainer() const { return m_container; }
    SandboxMode getMode() { return m_mode; }

    bool isEmpty()
    {
        if (m_mode == SandboxMode::SingleObject) return T{} == m_single;
        else if (m_mode == SandboxMode::Container) return m_container.empty();
        else if (m_mode == SandboxMode::LazyStream)
        {
            auto probe = m_lazySource();
            return !probe.has_value();
        }
        return false;
    }

    QVariantList toVariantList()
    {
        QVariantList list;
        list.reserve(m_container.size());
        for (auto& v : m_container) list << v;
        return list;
    }

    //==================== 工具：弱指针捕获惰性源，防循环引用 ====================
    template<typename TObj, typename Func>
    static auto weakLazyCapture(TObj* obj, Func&& func)
    {
        QWeakPointer<TObj> weak(obj);
        return [weak, f = std::forward<Func>(func)]() -> std::optional<T>
        {
            auto strong = weak.lock();
            if (!strong) return std::nullopt;
            return f(strong.data());
        };
    }

private:
    SandboxMode m_mode = SandboxMode::SingleObject;
    T m_single{};
    QVector<T> m_container;
    LazyFunc m_lazySource = nullptr;
    bool m_debugEnable = true;
    QVector<StageTime> m_timeList;
    int m_lazyMaxRead = 100000;
    GlobalExceptHook m_globalExceptHook;
};

// 旧业务兼容别名
using VariantMapSandbox = BizSandbox<QVariantMap>;
}
#endif // BIZSANDBOX_H
