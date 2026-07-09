#ifndef SQZ_CORE_SQZGLOBAL_H
#define SQZ_CORE_SQZGLOBAL_H

#include <QtCore/QtGlobal>

// Windows 需要导出/导入标记，Linux/macOS 空宏
#if defined(Q_OS_WIN)
    // 编译当前库时定义 SQZ_FRAMEWORK_BUILD_LIB → 导出符号
    #if defined(SQZ_FRAMEWORK_BUILD_LIB)
        #define SQZ_FRAMEWORK_API Q_DECL_EXPORT
    // 第三方使用本库 → 导入符号
    #else
        #define SQZ_FRAMEWORK_API Q_DECL_IMPORT
    #endif
#else
    // Linux/macOS 动态库默认全部导出，宏置空
    #define SQZ_FRAMEWORK_API
#endif

#endif // SQZ_CORE_SQZGLOBAL_H
