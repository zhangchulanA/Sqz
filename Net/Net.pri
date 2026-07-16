include(Base/Base.pri)
include(Channel/Channel.pri)
include(Core/Core.pri)
include(Request/Request.pri)
include(Server/Server.pri)

HEADERS += \
    $$PWD/NetDef.h \
    $$PWD/NetGlobal.h

INCLUDEPATH += $$PWD/Net/ \
               $$PWD/Net/Base/ \
               $$PWD/Net/Channel \
               $$PWD/Net/Core \
               $$PWD/Net/Request \
               $$PWD/Net/Server
