QT       += svg concurrent

HEADERS += \
    $$PWD/QHotkey/KeyManager.h \
    $$PWD/QHotkey/qhotkey.h \
    $$PWD/QHotkey/qhotkey_p.h \
    $$PWD/kits/ByteView.h \
    $$PWD/kits/DynCall.h \
    $$PWD/other/Async.h \
    $$PWD/other/ChainBranch.h \
    $$PWD/other/DataJoiner.h \
    $$PWD/other/Entangler.h \
    $$PWD/other/EntityCopy.h \
    $$PWD/other/EventAggregator.h \
    $$PWD/other/FormValidator.h \
    $$PWD/other/NumberClip.h \
    $$PWD/other/ObjectPool.h \
    $$PWD/other/PluginInterface.h \
    $$PWD/other/PluginManager.h \
    $$PWD/other/PropertyAnimator.h \
    $$PWD/other/Random.h \
    $$PWD/other/ResourceHub.h \
    $$PWD/other/SafeTimer.h \
    $$PWD/other/ShortcutManager.h \
    $$PWD/other/Singleton.h \
    $$PWD/other/ThreadPool.h \
    $$PWD/other/TimeoutKeeper.h \
    $$PWD/other/Toolbox.h \
    $$PWD/translator/Translator.h \
    $$PWD/translator/TranslatorMask.h \
    $$PWD/utils/FileUtils.h \
    $$PWD/utils/IniUtils.h \
    $$PWD/utils/JsonUtils.h \
    $$PWD/utils/StringUtils.h \
    $$PWD/utils/SystemUtils.h \
    $$PWD/utils/TimeUtils.h \
    $$PWD/utils/TimerUtils.h \
    $$PWD/utils/UiUtils.h \
    $$PWD/utils/VariantUtil.h

SOURCES += \
#    $$PWD/other/Async.cpp \
    $$PWD/QHotkey/KeyManager.cpp \
    $$PWD/QHotkey/qhotkey.cpp \
#    $$PWD/QHotkey/qhotkey_mac.cpp \
#    $$PWD/QHotkey/qhotkey_win.cpp \
    $$PWD/QHotkey/qhotkey_x11.cpp \
    $$PWD/other/ChainBranch.cpp \
    $$PWD/other/DataJoiner.cpp \
    $$PWD/other/Entangler.cpp \
    $$PWD/other/EntityCopy.cpp \
    $$PWD/other/EventAggregator.cpp \
    $$PWD/other/FormValidator.cpp \
    $$PWD/other/NumberClip.cpp \
    $$PWD/other/PluginInterface.cpp \
    $$PWD/other/PluginManager.cpp \
    $$PWD/other/PropertyAnimator.cpp \
    $$PWD/other/Random.cpp \
    $$PWD/other/ResourceHub.cpp \
    $$PWD/other/ShortcutManager.cpp \
    $$PWD/other/Singleton.cpp \
    $$PWD/other/ThreadPool.cpp \
    $$PWD/other/TimeoutKeeper.cpp \
    $$PWD/translator/Translator.cpp \
    $$PWD/translator/TranslatorMask.cpp \
    $$PWD/utils/FileUtils.cpp \
    $$PWD/utils/IniUtils.cpp \
    $$PWD/utils/JsonUtils.cpp \
    $$PWD/utils/StringUtils.cpp \
    $$PWD/utils/SystemUtils.cpp \
    $$PWD/utils/TimeUtils.cpp \
    $$PWD/utils/TimerUtils.cpp \
    $$PWD/utils/UiUtils.cpp

INCLUDEPATH +=  $$PWD/translator\
                $$PWD/utils \
                $$PWD/other \
                $$PWD/QHotkey \
                $$PWD/Kits


