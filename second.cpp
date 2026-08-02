// second.cpp —— 独立翻译单元，再次包含 SERIALIZE.h
// 目的：验证头文件中 inline 重载的 ODR 正确性（原版的非 inline 显式特化
//       在多 TU 链接时会触发 "multiple definition" 错误）。
#include "SERIALIZE.h"

// 跨 TU 的辅助函数，强制引用 QByteArray 内联重载
QJsonValue crossTuSerializeByteArray(const QByteArray& ba)
{
    return toJsonValue(ba);
}

bool crossTuDeserializeByteArray(const QJsonValue& v, QByteArray& out)
{
    return fromJsonValue(v, out);
}
