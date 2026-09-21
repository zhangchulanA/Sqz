#ifndef TESTMODEL_H
#define TESTMODEL_H
#include "SqzModel.h"

BEGIN_MODEL(AddressModel)
    SQZ_FIELD_QSTRING(City)        // 城市 QString
    SQZ_FIELD_QSTRING(Street)      // 街道 QString
    SQZ_FIELD_INT(HouseNumber)     // 门牌号 int
    SQZ_FIELD_UCHAR(ZipCode)       // 邮编简写 unsigned char
END_MODEL

// --------------------------
// 2. 定义主模型：用户模型，里面嵌套 AddressModel
// --------------------------
BEGIN_MODEL(UserModel)
    SQZ_FIELD_INT(Id)                     // 用户ID int
    SQZ_FIELD_QSTRING(UserName)           // 用户名 QString
    SQZ_FIELD_LLONG(CreateTime)           // 创建时间 long long 时间戳
    SQZ_FIELD_BOOL(IsVip)                 // 是否VIP bool
    SQZ_FIELD_STRING(Note)                // 备注 std::string
    SQZ_FIELD_QBYTEARRAY(AvatarBin)       // 头像二进制，自动base64
    SQZ_FIELD_SQZMODEL(address, AddressModel) // 嵌套地址模型
END_MODEL
#endif // TESTMODEL_H
