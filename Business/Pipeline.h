/**
 * @file Pipeline.h
 * @brief 管道过滤器 - 像Unix管道一样处理数据
 *
 * 核心思想：数据从一头进，经过一系列"过滤器"处理，从另一头出
 * 每个过滤器只做一件事，组合起来完成复杂任务
 *
 * 核心功能：
 * 1. 过滤（filter）：保留符合条件的元素
 * 2. 转换（map）：变换数据格式
 * 3. 聚合（reduce）：归约汇总（求和、求平均等）
 * 4. 排序（sort）：排序
 * 5. 去重（distinct）：去重
 * 6. 分组（groupBy）：按条件分组
 * 7. 分页（page）：分页
 * 8. 惰性求值：调用collect()时才真正执行
 *
 * 使用场景：
 * 1. 数据清洗：过滤脏数据→转换格式→排序→输出
 * 2. 日志分析：读取日志→过滤关键词→统计次数
 * 3. 集合操作：用户列表→过滤VIP→按年龄排序
 * 4. 实时数据处理：传感器数据→滤波→转换→显示
 * 5. 图像处理：原图→裁剪→滤镜→压缩
 */

#ifndef PIPELINE_H
#define PIPELINE_H

#include <QList>
#include <QHash>
#include <QSet>
#include <QDebug>
#include <functional>
#include <algorithm>
#include <optional>

/**
 * @brief 管道流 - 数据流的起点
 * @tparam T 数据类型
 *
 * 用法示例：
 * @code
 * auto result = Stream::from(data)
 *     .filter([](int x) { return x > 0; })
 *     .map([](int x) { return x * 2; })
 *     .sort()
 *     .collect();
 * @endcode
 */
template<typename T>
class Stream {
public:
    // ========== 构造函数 ==========

    /**
     * @brief 从QList创建流
     */
    explicit Stream(const QList<T>& data) : m_data(data), m_isExecuted(false) {}

    /**
     * @brief 从QVector创建流
     */
    explicit Stream(const QVector<T>& data) : m_data(data.toList()), m_isExecuted(false) {}

    /**
     * @brief 从QSet创建流
     */
    explicit Stream(const QSet<T>& data) : m_data(data.toList()), m_isExecuted(false) {}

    /**
     * @brief 从std::initializer_list创建流
     */
    explicit Stream(const std::initializer_list<T>& data)
        : m_data(data.begin(), data.end()), m_isExecuted(false) {}

    /**
     * @brief 静态工厂方法：从数据创建流
     */
    static Stream<T> from(const QList<T>& data) {
        return Stream<T>(data);
    }

    static Stream<T> from(const std::initializer_list<T>& data) {
        return Stream<T>(data);
    }

    // ========== 过滤器操作 ==========

    /**
     * @brief 过滤：保留满足条件的元素
     * @param predicate 判断函数，返回true的元素保留
     * @return 返回自身引用，支持链式调用
     *
     * 示例：只保留偶数
     * stream.filter([](int x) { return x % 2 == 0; });
     */
    Stream<T>& filter(std::function<bool(const T&)> predicate) {
        m_filters.append(predicate);
        return *this;
    }

    /**
     * @brief 排除：移除满足条件的元素（filter的反向）
     */
    Stream<T>& exclude(std::function<bool(const T&)> predicate) {
        m_filters.append([predicate](const T& item) {
            return !predicate(item);
        });
        return *this;
    }

    /**
     * @brief 空值过滤：移除空值（指针类型）
     */
    Stream<T>& nonNull() {
        m_filters.append([](const T& item) {
            return item != nullptr;
        });
        return *this;
    }

    // ========== 转换操作 ==========

    /**
     * @brief 映射：将数据转换为另一种类型
     * @tparam R 目标类型
     * @param mapper 转换函数
     * @return 新的Stream（类型已变）
     *
     * 示例：将int转成字符串
     * stream.map<QString>([](int x) { return QString::number(x); });
     */
    template<typename R>
    Stream<R> map(std::function<R(const T&)> mapper) {
        // 先执行当前流，获得数据
        QList<T> executedData = execute();

        // 转换为新类型
        QList<R> mappedData;
        for (const auto& item : executedData) {
            mappedData.append(mapper(item));
        }

        return Stream<R>(mappedData);
    }

    /**
     * @brief 扁平映射：将每个元素映射为列表，然后展平
     */
    template<typename R>
    Stream<R> flatMap(std::function<QList<R>(const T&)> mapper) {
        QList<T> executedData = execute();

        QList<R> result;
        for (const auto& item : executedData) {
            result.append(mapper(item));
        }

        return Stream<R>(result);
    }

    // ========== 聚合操作 ==========

    /**
     * @brief 归约：将数据归约为单个值
     * @param reducer 归约函数 (累计值, 当前值) -> 新累计值
     * @param initial 初始值
     * @return 归约结果
     *
     * 示例：求和
     * int sum = stream.reduce([](int acc, int x) { return acc + x; }, 0);
     */
    T reduce(std::function<T(const T&, const T&)> reducer, const T& initial) {
        QList<T> data = execute();
        T result = initial;
        for (const auto& item : data) {
            result = reducer(result, item);
        }
        return result;
    }

    /**
     * @brief 求和（仅对数值类型有效）
     */
    template<typename U = T>
    typename std::enable_if<std::is_arithmetic<U>::value, U>::type
    sum() {
        QList<T> data = execute();
        T result = 0;
        for (const auto& item : data) {
            result += item;
        }
        return result;
    }

    /**
     * @brief 求平均值（仅对数值类型有效）
     */
    template<typename U = T>
    typename std::enable_if<std::is_arithmetic<U>::value, double>::type
    avg() {
        QList<T> data = execute();
        if (data.isEmpty()) return 0.0;

        T sum = 0;
        for (const auto& item : data) {
            sum += item;
        }
        return static_cast<double>(sum) / data.size();
    }

    /**
     * @brief 获取最大值
     */
    std::optional<T> max() {
        QList<T> data = execute();
        if (data.isEmpty()) return std::nullopt;

        T maxVal = data[0];
        for (const auto& item : data) {
            if (item > maxVal) maxVal = item;
        }
        return maxVal;
    }

    /**
     * @brief 获取最小值
     */
    std::optional<T> min() {
        QList<T> data = execute();
        if (data.isEmpty()) return std::nullopt;

        T minVal = data[0];
        for (const auto& item : data) {
            if (item < minVal) minVal = item;
        }
        return minVal;
    }

    /**
     * @brief 计数
     */
    int count() {
        return execute().size();
    }

    /**
     * @brief 检查是否所有元素满足条件
     */
    bool allMatch(std::function<bool(const T&)> predicate) {
        QList<T> data = execute();
        for (const auto& item : data) {
            if (!predicate(item)) return false;
        }
        return true;
    }

    /**
     * @brief 检查是否有元素满足条件
     */
    bool anyMatch(std::function<bool(const T&)> predicate) {
        QList<T> data = execute();
        for (const auto& item : data) {
            if (predicate(item)) return true;
        }
        return false;
    }

    /**
     * @brief 检查是否没有元素满足条件
     */
    bool noneMatch(std::function<bool(const T&)> predicate) {
        return !anyMatch(predicate);
    }

    // ========== 排序操作 ==========

    /**
     * @brief 排序（升序）
     */
    Stream<T>& sort() {
        m_sortAscending = true;
        m_hasSort = true;
        return *this;
    }

    /**
     * @brief 排序（降序）
     */
    Stream<T>& sortDescending() {
        m_sortAscending = false;
        m_hasSort = true;
        return *this;
    }

    /**
     * @brief 自定义排序
     * @param comparator 比较函数，返回true表示a排在b前面
     */
    Stream<T>& sort(std::function<bool(const T&, const T&)> comparator) {
        m_sortComparator = comparator;
        m_hasSort = true;
        return *this;
    }

    /**
     * @brief 按指定字段排序（需要提供获取比较键的函数）
     */
    template<typename Key>
    Stream<T>& sortBy(std::function<Key(const T&)> keyExtractor, bool ascending = true) {
        m_sortAscending = ascending;
        m_hasSort = true;
        m_sortComparator = [keyExtractor, ascending](const T& a, const T& b) {
            Key ka = keyExtractor(a);
            Key kb = keyExtractor(b);
            if (ascending) {
                return ka < kb;
            } else {
                return ka > kb;
            }
        };
        return *this;
    }

    // ========== 去重操作 ==========

    /**
     * @brief 去重（使用operator==比较）
     */
    Stream<T>& distinct() {
        m_hasDistinct = true;
        return *this;
    }

    /**
     * @brief 按指定键去重
     */
    template<typename Key>
    Stream<T>& distinctBy(std::function<Key(const T&)> keyExtractor) {
        m_distinctKeyExtractor = [keyExtractor](const T& item) -> QVariant {
            return QVariant::fromValue(keyExtractor(item));
        };
        m_hasDistinct = true;
        return *this;
    }

    // ========== 分页操作 ==========

    /**
     * @brief 分页
     * @param pageIndex 页码（从0开始）
     * @param pageSize 每页大小
     */
    Stream<T>& page(int pageIndex, int pageSize) {
        m_pageIndex = pageIndex;
        m_pageSize = pageSize;
        m_hasPagination = true;
        return *this;
    }

    /**
     * @brief 限制数量
     */
    Stream<T>& limit(int count) {
        m_limit = count;
        m_hasPagination = true;
        return *this;
    }

    /**
     * @brief 跳过前N个
     */
    Stream<T>& skip(int count) {
        m_skip = count;
        m_hasPagination = true;
        return *this;
    }

    // ========== 分组操作 ==========

    /**
     * @brief 分组
     * @param keyExtractor 提取分组的键
     * @return QHash<键, QList<值>>
     */
    template<typename Key>
    QHash<Key, QList<T>> groupBy(std::function<Key(const T&)> keyExtractor) {
        QList<T> data = execute();
        QHash<Key, QList<T>> groups;

        for (const auto& item : data) {
            Key key = keyExtractor(item);
            groups[key].append(item);
        }

        return groups;
    }

    /**
     * @brief 分组并聚合（类似SQL的GROUP BY + COUNT）
     */
    template<typename Key>
    QHash<Key, int> groupCount(std::function<Key(const T&)> keyExtractor) {
        QList<T> data = execute();
        QHash<Key, int> counts;

        for (const auto& item : data) {
            Key key = keyExtractor(item);
            counts[key] = counts.value(key, 0) + 1;
        }

        return counts;
    }

    // ========== 收集操作（触发真正执行） ==========

    /**
     * @brief 收集结果（触发执行）
     * @return QList<T>
     */
    QList<T> collect() {
        return execute();
    }

    /**
     * @brief 收集为QSet（自动去重）
     */
    QSet<T> collectToSet() {
        QList<T> data = execute();
        return QSet<T>(data.begin(), data.end());
    }

    /**
     * @brief 收集为QHash（需要提供键提取函数）
     */
    template<typename Key>
    QHash<Key, T> collectToHash(std::function<Key(const T&)> keyExtractor) {
        QList<T> data = execute();
        QHash<Key, T> result;

        for (const auto& item : data) {
            result[keyExtractor(item)] = item;
        }

        return result;
    }

    /**
     * @brief 收集为QMap（按键排序）
     */
    template<typename Key>
    QMap<Key, T> collectToMap(std::function<Key(const T&)> keyExtractor) {
        QList<T> data = execute();
        QMap<Key, T> result;

        for (const auto& item : data) {
            result[keyExtractor(item)] = item;
        }

        return result;
    }

    /**
     * @brief 遍历每个元素（不改变数据）
     */
    Stream<T>& forEach(std::function<void(const T&)> action) {
        m_forEachActions.append(action);
        return *this;
    }

    /**
     * @brief 打印每个元素（调试用）
     */
    Stream<T>& print(const QString& prefix = "") {
        m_forEachActions.append([prefix](const T& item) {
            qDebug() << prefix << item;
        });
        return *this;
    }

    // ========== 工具方法 ==========

    /**
     * @brief 检查流是否为空
     */
    bool isEmpty() {
        return execute().isEmpty();
    }

    /**
     * @brief 获取第一个元素
     */
    std::optional<T> first() {
        QList<T> data = execute();
        if (data.isEmpty()) return std::nullopt;
        return data[0];
    }

    /**
     * @brief 获取最后一个元素
     */
    std::optional<T> last() {
        QList<T> data = execute();
        if (data.isEmpty()) return std::nullopt;
        return data.last();
    }

    /**
     * @brief 获取指定索引的元素
     */
    std::optional<T> at(int index) {
        QList<T> data = execute();
        if (index < 0 || index >= data.size()) return std::nullopt;
        return data[index];
    }

private:
    /**
     * @brief 执行所有操作（惰性求值）
     */
    QList<T> execute() {
        if (m_isExecuted) {
            return m_resultCache;
        }

        // 复制原始数据
        QList<T> result = m_data;

        // 应用过滤器
        for (const auto& filter : m_filters) {
            QList<T> filtered;
            for (const auto& item : result) {
                if (filter(item)) {
                    filtered.append(item);
                }
            }
            result = filtered;
        }

        // 应用排序
        if (m_hasSort) {
            if (m_sortComparator) {
                std::sort(result.begin(), result.end(), m_sortComparator);
            } else if (m_sortAscending) {
                std::sort(result.begin(), result.end());
            } else {
                std::sort(result.begin(), result.end(), std::greater<T>());
            }
        }

        // 应用去重
        if (m_hasDistinct) {
            if (m_distinctKeyExtractor) {
                QHash<QVariant, T> seen;
                QList<T> distinct;
                for (const auto& item : result) {
                    QVariant key = m_distinctKeyExtractor(item);
                    if (!seen.contains(key)) {
                        seen[key] = item;
                        distinct.append(item);
                    }
                }
                result = distinct;
            } else {
                QSet<T> seen;
                QList<T> distinct;
                for (const auto& item : result) {
                    if (!seen.contains(item)) {
                        seen.insert(item);
                        distinct.append(item);
                    }
                }
                result = distinct;
            }
        }

        // 应用分页
        if (m_hasPagination) {
            int start = m_skip;
            if (m_pageIndex >= 0 && m_pageSize > 0) {
                start = m_pageIndex * m_pageSize + m_skip;
            }

            int end = result.size();
            if (m_limit > 0) {
                end = start + m_limit;
            } else if (m_pageIndex >= 0 && m_pageSize > 0) {
                end = (m_pageIndex + 1) * m_pageSize + m_skip;
            }

            if (start < result.size()) {
                end = qMin(end, result.size());
                result = result.mid(start, end - start);
            } else {
                result.clear();
            }
        }

        // 执行forEach操作
        for (const auto& action : m_forEachActions) {
            for (const auto& item : result) {
                action(item);
            }
        }

        // 缓存结果
        m_resultCache = result;
        m_isExecuted = true;

        return result;
    }

private:
    QList<T> m_data;                           // 原始数据
    QList<std::function<bool(const T&)>> m_filters;  // 过滤器
    QList<std::function<void(const T&)>> m_forEachActions; // forEach回调

    // 排序相关
    bool m_hasSort = false;
    bool m_sortAscending = true;
    std::function<bool(const T&, const T&)> m_sortComparator;

    // 去重相关
    bool m_hasDistinct = false;
    std::function<QVariant(const T&)> m_distinctKeyExtractor;

    // 分页相关
    bool m_hasPagination = false;
    int m_pageIndex = -1;
    int m_pageSize = 0;
    int m_limit = -1;
    int m_skip = 0;

    // 执行状态
    bool m_isExecuted;
    QList<T> m_resultCache;
};

#endif // PIPELINE_H
