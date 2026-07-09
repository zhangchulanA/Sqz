#ifndef VARIANTUTIL_H
#define VARIANTUTIL_H

#include <QVariant>
#include <QDebug>
#include "SqzGlobal.h"

namespace Sqz::Utils
{
    // 安全转换，返回是否成功，输出参数接收结果
    template<typename T>
    inline bool tryConvert(const QVariant& var, T& out)
    {
        if (!var.canConvert<T>())
        {
            qWarning() << "[VariantUtil] convert failed, target type:" << typeid(T).name();
            return false;
        }
        out = var.value<T>();
        return true;
    }

    // 安全取值，失败返回默认空对象
    template<typename T>
    inline T safeVal(const QVariant& var)
    {
        T res{};
        tryConvert(var, res);
        return res;
    }

    // 对象转QVariant 内部封装
    template<typename T>
    inline QVariant toVariant(const T& data)
    {
        return QVariant::fromValue(data);
    }
}

// ===================== 转出宏：对象 → QVariant =====================
// VAR_TO_VAR(类型, 变量名, 源对象)
// 示例：VAR_TO_VAR(TestData, vData, dataObj);
#define VAR_TO_VAR(Type, VarName, SourceObj) \
    QVariant VarName = VariantUtil::toVariant<Type>(SourceObj)

// 极简一行临时QVariant，无需定义变量
#define VAR_TMP(Type, SourceObj) VariantUtil::toVariant<Type>(SourceObj)

// ===================== 转入宏：QVariant → 对象 =====================
// 1. 直接取值，不判断，失败返回空对象
#define VAR_GET_SAFE(Type, VariantObj) VariantUtil::safeVal<Type>(VariantObj)

// 2. 定义变量+自动生成ok标记，手动if判断
#define VAR_GET_CHECK(Type, VarName, VariantObj) \
    Type VarName; \
    bool VarName##_ok = VariantUtil::tryConvert<Type>(VariantObj, VarName)

// 3. 转换成功自动执行代码块，省去if
#define VAR_GET_IF(Type, VarName, VariantObj, Code) \
{ \
    VAR_GET_CHECK(Type, VarName, VariantObj); \
    if (VarName##_ok) \
    { Code } \
}

#endif // VARIANTUTIL_H
