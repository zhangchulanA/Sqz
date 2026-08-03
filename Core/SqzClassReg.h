#ifndef SQZCLASSREG_H
#define SQZCLASSREG_H

#include <QHash>
#include <QString>
#include <functional>
#include <QVariantList>
#include <type_traits>
#include "SqzGlobal.h"
#include "SqzHub.h"
namespace Sqz
{
class SqzQuick;
// 编译期判断是否是SqzQuick子类
template<typename T>
constexpr bool IsSqzQuickClass()
{
    return std::is_base_of_v<Sqz::SqzQuick, T>;
}

struct SQZ_FRAMEWORK_API ClassFactory
{
    std::function<void*()> NoArgCreator = nullptr;
    std::function<void*(const QVariantList&)> ArgCreator = nullptr;
    bool IsQuick = false;    // 编译期直接赋值，不用运行判断
    bool IsQObject = false;  // 是否为 QObject 派生，用于带参类的销毁方式判断
};

inline QHash<QString,ClassFactory>& GlobalClassTable()
{
    static QHash<QString, ClassFactory> table;
    return table;
}

#define SQZ_REG_NOARG(Cls) \
namespace { \
    struct AutoReg_##Cls { \
        AutoReg_##Cls() { \
            auto& table = Sqz::GlobalClassTable(); \
            Sqz::ClassFactory info; \
            info.NoArgCreator = []()->void*{ return new Cls(); }; \
            info.IsQuick = Sqz::IsSqzQuickClass<Cls>(); \
            table[#Cls] = info; \
        } \
    }; \
    static AutoReg_##Cls Reg_##Cls; \
}

#define SQZ_REG_ARG(Cls) \
namespace { \
    struct AutoReg_##Cls { \
        AutoReg_##Cls() { \
            auto& table = Sqz::GlobalClassTable(); \
            Sqz::ClassFactory info; \
            info.ArgCreator = [](const QVariantList& args)->void*{ return new Cls(args); }; \
            info.IsQuick = Sqz::IsSqzQuickClass<Cls>(); \
            info.IsQObject = std::is_base_of_v<QObject, Cls>; \
            table[#Cls] = info; \
        } \
    }; \
    static AutoReg_##Cls Reg_##Cls; \
}
#define SqzIn  SqzHub::Instance()
}
#endif
