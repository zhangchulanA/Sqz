#ifndef TESTMODEL_H
#define TESTMODEL_H
#include "SqzModel.h"

using namespace Sqz;

BEGIN_MODEL(AddressModel)
    SQZ_FIELD_QSTRING(city)
    SQZ_FIELD_INT(number)
END_MODEL

BEGIN_MODEL(UserModel)
    SQZ_FIELD_INT(id)
    SQZ_FIELD_QSTRING(name)
    SQZ_FIELD_SQZMODEL(address, AddressModel)
END_MODEL
#endif // TESTMODEL_H
