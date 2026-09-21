#include "EntityCopy.h"
#include <QMetaProperty>
#include <QVariant>
namespace Sqz {
void EntityCopy::copy(QObject *src, QObject *dst, const QStringList &ignoreFields)
{
    if(!src || !dst) return;
    const QMetaObject* srcMeta = src->metaObject();
    const QMetaObject* dstMeta = dst->metaObject();

    // 遍历源实体所有属性
    for(int i = 0; i < srcMeta->propertyCount(); ++i)
    {
        copyProp(srcMeta, dstMeta, src, dst, i, ignoreFields);
    }
}

void EntityCopy::copyProp(const QMetaObject *srcMeta, const QMetaObject *dstMeta,
                          QObject *src, QObject *dst, int propIdx, const QStringList &ignore)
{
    QMetaProperty srcProp = srcMeta->property(propIdx);
    QString propName = srcProp.name();

    // 跳过忽略字段
    if(ignore.contains(propName)) return;

    // 目标不存在该属性则跳过
    int dstIdx = dstMeta->indexOfProperty(propName.toUtf8());
    if(dstIdx < 0) return;
    QMetaProperty dstProp = dstMeta->property(dstIdx);

    // 目标属性不可写则跳过
    if(!dstProp.isWritable()) return;

    QVariant val = srcProp.read(src);

    // 如果是嵌套QObject子对象，递归复制
    if(val.canConvert<QObject*>())
    {
        QObject* subSrc = val.value<QObject*>();
        QVariant subDstVar = dstProp.read(dst);
        if(subDstVar.canConvert<QObject*>())
        {
            QObject* subDst = subDstVar.value<QObject*>();
            copy(subSrc, subDst, ignore);
        }
        return;
    }

    // 普通值直接赋值
    dstProp.write(dst, val);
}
}
