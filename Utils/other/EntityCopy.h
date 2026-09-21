#ifndef ENTITYCOPY_H
#define ENTITYCOPY_H

#include <QObject>
#include <QStringList>
#include <QMetaObject>

/**
 * @brief 实体反射复制工具
 * 自动复制两个Q_OBJECT实体同名Q_PROPERTY属性
 * 支持忽略指定字段、递归复制嵌套子对象
 * 可跨不同实体类复制，弥补拷贝构造只能同类复制的缺陷
 */
namespace Sqz {
class EntityCopy
{
public:
    /**
     * @brief 复制源实体到目标实体
     * @param src 源对象
     * @param dst 目标对象
     * @param ignoreFields 需要跳过不复制的字段名列表
     */
    static void copy(QObject* src, QObject* dst, const QStringList& ignoreFields = {});

private:
    // 递归单属性复制
    static void copyProp(const QMetaObject* srcMeta, const QMetaObject* dstMeta,
                         QObject* src, QObject* dst, int propIdx, const QStringList& ignore);
};
}
#endif // ENTITYCOPY_H
