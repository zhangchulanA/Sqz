
QT       += core gui
#QT       += core gui network sql xml concurrent qml quick quickwidgets svg
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17


INCLUDEPATH += /opt/QtFluentWidgets/include/Fluent
LIBS += -L/opt/QtFluentWidgets/lib -lQtFluentWidgets

VERSION = 1.0.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"
DEFINES += SQZ_Prefix=\\\"Sqz\\\"
CONFIG(debug,debug|release){
    #debug专属配置
    TEMPLATE = app
    TARGET = SqzApp
    DEFINES += DEBUG_MODE
    message("Debug模式：生成可执行程序")
}else{
    #release 专属配置
    DEFINES += RELEASE_MODE
    TEMPLATE = lib
    CONFIG += staticlib
    TARGET = Sqz
    DESTDIR = $$PWD/SqzLib
    message("Release模式：生成库文件,在当前目录SqzLib文件夹下")
}

OBJECTS_DIR     = $$OUT_PWD/obj
MOC_DIR         = $$OUT_PWD/moc
RCC_DIR         = $$OUT_PWD/rcc
UI_DIR          = $$OUT_PWD/ui

#LIBS += -lasound
LIBS += -lX11
DEFINES += QT_X11EXTRAS_LIB
INCLUDEPATH +=  $$PWD/Log \
                $$PWD/Core \
                $$PWD/Database \
                $$PWD/Utils \
                $$PWD/Widget \
                $$PWD/Config \
                $$PWD/NetWork \
                $$PWD/Business \
                $$PWD/Global

include(Business/Business.pri)
include(Config/Config.pri)
include(Core/Core.pri)
include(Widget/Widget.pri)
include(Database/Database.pri)
include(DataManager/DataManager.pri)
include(Log/Log.pri)
include(Style/Style.pri)
include(Utils/Utils.pri)
include(NetWork/NetWork.pri)
include(Global/Global.pri)

SOURCES += \
#    TestWidget.cpp \
#    RetransmitManager.cpp \
    SqzTest.cpp \
    main.cpp

HEADERS += \
#    RetransmitManager.h \
 \#    TestWidget.h
    SqzTest.h

FORMS += \
#    MainWindowTest.ui \
 \#    TestWidget.ui
    SqzTest.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

win32 {
#    QMAKE_CXXFLAGS += /utf-8
    LIBS += -ldbghelp

}

unix {
    QMAKE_CXXFLAGS += -Wall -Wextra
    QMAKE_LFLAGS += -rdynamic


    QMAKE_POST_LINK += $$PWD/MakeRun.sh $$VERSION $$PWD
    # 可选：强制UTF-8
    # QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8
}

RESOURCES +=


DISTFILES += \
    SqzData/config/app_config.ini \
    SqzData/translator/English.json \
    SqzData/translator/简体中文.json
#复制配置文件到输出目录

    config.files += $$PWD/SqzData/config/app_config.ini
    config.path = $$OUT_PWD/SqzData/config
    COPIES += config

    translators.files += $$PWD/SqzData/translator/简体中文.json \
                          $$PWD/SqzData/translator/English.json
    translators.path = $$OUT_PWD/SqzData/translator
    COPIES += translators


QMAKE_CLEAN += \
        $$OBJECTS_DIR/* \
        $$MOC_DIR/* \
        $$RCC_DIR/* \
        $$UI_DIR/*

