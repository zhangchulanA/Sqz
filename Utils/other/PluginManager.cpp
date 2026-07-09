#include "PluginManager.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>

namespace Sqz::Utils {
PluginManager& PluginManager::GetInstance()
{
    static PluginManager ins;
    return ins;
}

PluginManager::PluginManager(QObject *parent)
    : QObject(parent)
{

}

PluginManager::~PluginManager()
{
    unloadAllPlugins();
}

void PluginManager::setPluginDir(const QString &dir)
{
    m_pluginDir = dir;
}

// 预扫描：只读取插件名字，不常驻加载
bool PluginManager::scanPluginMeta()
{
    QDir dir(m_pluginDir);
    if(!dir.exists())
    {
        qDebug() << "插件目录不存在：" << m_pluginDir;
        return false;
    }

    // 跨平台筛选后缀
    QStringList filters;
#ifdef Q_OS_WIN
    filters << "*.dll";
#endif
#ifdef Q_OS_LINUX
    filters << "*.so";
#endif
#ifdef Q_OS_MAC
    filters << "*.dylib";
#endif

    dir.setNameFilters(filters);
    QFileInfoList fileList = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);

    // 清空旧映射
    m_nameToFilePathMap.clear();

    for(const auto& info : fileList)
    {
        QString filePath = info.absoluteFilePath();
        // 临时加载一下，只读元信息，读完就卸载
        QPluginLoader tempLoader(filePath);
        if(!tempLoader.load())
        {
            qDebug() << "扫描跳过无效插件：" << filePath;
            continue;
        }

        QObject* obj = tempLoader.instance();
        PluginInterface* plug = qobject_cast<PluginInterface*>(obj);
        if(!plug)
        {
            tempLoader.unload();
            continue;
        }

        // 关键：拿到【你自己定义的业务插件名】
        QString bizName = plug->getPluginName();
        // 建立映射：自定义名字 → 真实文件路径
        m_nameToFilePathMap[bizName] = filePath;

        // 立刻卸载，不占内存
        tempLoader.unload();
    }

    qDebug() << "插件预扫描完成，共识别到：" << m_nameToFilePathMap.size() << " 个插件";
    return m_nameToFilePathMap.size() > 0;
}

// 核心：按【自定义业务名】按需加载
PluginInterface* PluginManager::loadPluginByBizName(const QString &bizPluginName)
{
    // 1. 已经加载过，直接返回
    if(m_loadedPluginMap.contains(bizPluginName))
    {
        return m_loadedPluginMap[bizPluginName].second;
    }

    // 2. 检查扫描表里有没有这个插件名
    if(!m_nameToFilePathMap.contains(bizPluginName))
    {
        qDebug() << "不存在该插件：" << bizPluginName;
        return nullptr;
    }

    // 3. 取出对应的真实物理文件路径
    QString realFilePath = m_nameToFilePathMap[bizPluginName];

    // 4. 正式按需加载
    QPluginLoader* loader = new QPluginLoader(realFilePath);
    if(!loader->load())
    {
        qDebug() << "插件加载失败：" << realFilePath << loader->errorString();
        delete loader;
        return nullptr;
    }

    QObject* obj = loader->instance();
    PluginInterface* plug = qobject_cast<PluginInterface*>(obj);
    if(!plug)
    {
        loader->unload();
        delete loader;
        return nullptr;
    }

    // 5. 存入已加载容器
    m_loadedPluginMap[bizPluginName] = {loader, plug};
    qDebug() << "按需加载成功 ==> " << bizPluginName;

    return plug;
}

// 按业务名卸载单个
bool PluginManager::unloadPluginByBizName(const QString &bizPluginName)
{
    if(!m_loadedPluginMap.contains(bizPluginName))
        return false;

    auto pair = m_loadedPluginMap[bizPluginName];
    if(pair.first)
    {
        pair.first->unload();
        delete pair.first;
    }
    m_loadedPluginMap.remove(bizPluginName);
    qDebug() << "已卸载插件：" << bizPluginName;
    return true;
}

// 卸载全部
void PluginManager::unloadAllPlugins()
{
    auto iter = m_loadedPluginMap.begin();
    for(; iter != m_loadedPluginMap.end(); ++iter)
    {
        auto pair = iter.value();
        if(pair.first)
        {
            pair.first->unload();
            delete pair.first;
        }
    }
    m_loadedPluginMap.clear();
    qDebug() << "所有已加载插件全部释放";
}

// 查询已加载
PluginInterface* PluginManager::getLoadedPluginByBizName(const QString &bizPluginName)
{
    if(m_loadedPluginMap.contains(bizPluginName))
        return m_loadedPluginMap[bizPluginName].second;
    return nullptr;
}
}
