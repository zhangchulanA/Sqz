#include "ResourceHub.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QFontDatabase>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSvgRenderer>
#include <QDateTime>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QPainter>

ResourceHub* ResourceHub::m_inst = nullptr;

ResourceHub* ResourceHub::instance()
{
    if (!m_inst)
        m_inst = new ResourceHub();
    return m_inst;
}

ResourceHub::ResourceHub(QObject *parent)
    : QObject(parent)
{
    // 默认路径，外部可覆盖修改
    m_skinRoot = QDir::currentPath() + "/res/skin";
    m_assetRoot = QDir::currentPath() + "/assets";
    m_curSkin = "default";

    // 30秒自动GC清理闲置图片
    m_gcTimer.setInterval(30000);
    connect(&m_gcTimer, &QTimer::timeout, this, &ResourceHub::gc);
    m_gcTimer.start();

    // 文件改动监听
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString& f){
        emit resourceModified(f);
        m_textAssetCache.remove(f);
        m_imgCache.remove(f);
    });
    watchDir(getCurrentSkinDir());
}

ResourceHub::~ResourceHub()
{
    m_gcTimer.stop();
    clearImageCache();
    clearFontCache();
}

// 自定义皮肤根目录
void ResourceHub::setSkinRootDir(const QString &dir)
{
    m_skinRoot = dir;
    m_watcher.removePaths(m_watcher.directories());
    watchDir(getCurrentSkinDir());
    clearImageCache();
}

// 自定义静态资源根目录(不受皮肤切换影响)
void ResourceHub::setAssetRootDir(const QString &dir)
{
    m_assetRoot = dir;
    QDir().mkpath(m_assetRoot);
}

QString ResourceHub::getSkinRootDir() const
{
    return m_skinRoot;
}

QString ResourceHub::getAssetRootDir() const
{
    return m_assetRoot;
}

QString ResourceHub::skinName() const
{
    return m_curSkin;
}

// 切换皮肤
void ResourceHub::switchSkin(const QString &skinDirName)
{
    QString fullPath = m_skinRoot + "/" + skinDirName;
    if (!QDir(fullPath).exists()) return;

    m_curSkin = skinDirName;
    clearImageCache();
    m_textAssetCache.clear();
    m_watcher.removePaths(m_watcher.directories());
    watchDir(fullPath);
    emit skinChanged();
}

QString ResourceHub::getCurrentSkinDir() const
{
    return m_skinRoot + "/" + m_curSkin;
}

// 缓存目录(网络图片自动存这里)
QString ResourceHub::getCacheDir() const
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/res_cache";
    QDir().mkpath(path);
    return path;
}

QString ResourceHub::getFontDir() const
{
    QString path = QDir::currentPath() + "/res/font";
    QDir().mkpath(path);
    return path;
}

QString ResourceHub::getTempDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation);
}

// C++同步取图，仅本地/qrc，网络图用asyncLoadImage
QPixmap ResourceHub::getPixmap(const QString &resKey, const QSize &limit)
{
    QString realPath;
    bool isQrc, isNet;
    if (!parseResKey(resKey, realPath, isQrc, isNet))
        return QPixmap("qrc://img/placeholder"); // 兜底占位图

    if (isNet) return QPixmap();

    // 命中缓存直接返回
    if (m_imgCache.contains(realPath))
    {
        ImageCacheItem& item = m_imgCache[realPath];
        item.lastUseTime = QDateTime::currentMSecsSinceEpoch();
        QImage img = item.img;
        if (!limit.isEmpty())
            img = img.scaled(limit, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        return QPixmap::fromImage(img);
    }

    QImage img;
    if (realPath.endsWith(".svg", Qt::CaseInsensitive))
    {
        QSvgRenderer render(realPath);
        img = QImage(render.viewBox().size(), QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        render.render(&p);
    }
    else
    {
        img.load(realPath);
    }

    if (img.isNull())
        return QPixmap("qrc://img/placeholder");

    // 写入LRU缓存
    ImageCacheItem newItem;
    newItem.img = img;
    newItem.lastUseTime = QDateTime::currentMSecsSinceEpoch();
    newItem.byteSize = calcImageByte(img);
    m_imgCache[realPath] = newItem;
    m_currentMemUsed += newItem.byteSize;
    trimImageCache();

    if (!limit.isEmpty())
        img = img.scaled(limit, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return QPixmap::fromImage(img);
}

// 异步加载大图/网络图片
void ResourceHub::asyncLoadImage(const QString &resKey, std::function<void (QImage)> callback)
{
    QString realPath;
    bool isQrc, isNet;
    parseResKey(resKey, realPath, isQrc, isNet);

    if (!isNet)
    {
        QPixmap pix = getPixmap(resKey);
        callback(pix.toImage());
        return;
    }

    QString cacheFile = getCacheDir() + "/" + QString::number(qHash(realPath)) + ".cache";
    if (QFileInfo(cacheFile).exists())
    {
        QImage img(cacheFile);
        callback(img);
        return;
    }
    downloadNetImage(realPath, cacheFile, callback);
}

// QML使用
QString ResourceHub::getImageSource(const QString &resKey)
{
    QString realPath;
    bool isQrc, isNet;
    parseResKey(resKey, realPath, isQrc, isNet);
    if (isQrc) return realPath;
    if (isNet) return realPath;
    return "file://" + realPath;
}

// 获取全局字体
QFont ResourceHub::getFont(const QString &family, int pointSize, int weight)
{
    QString key = QString("%1_%2").arg(family).arg(pointSize);
    if (m_fontPool.contains(key))
    {
        FontCacheItem& item = m_fontPool[key];
        item.refCount++;
        QFont f(family, pointSize, weight);
        // 移除报错的styleString调用
        return f;
    }

    QDir fontDir(getFontDir());
    QStringList fonts = fontDir.entryList(QStringList() << "*.ttf" << "*.ttc");
    for (const QString& fn : fonts)
    {
        QString fp = fontDir.filePath(fn);
        int id = QFontDatabase::addApplicationFont(fp);
        if (id < 0) continue;
        QStringList families = QFontDatabase::applicationFontFamilies(id);
        for (const QString& fam : families)
        {
            QString fkey = QString("%1_%2").arg(fam).arg(pointSize);
            FontCacheItem it;
            it.fontId = id;
            it.filePath = fp;
            it.refCount = 1;
            m_fontPool[fkey] = it;
        }
    }
    return QFont(family, pointSize, weight);
}

// 读取文本资源 qss/txt
QString ResourceHub::loadAssetText(const QString &resKey)
{
    QString realPath;
    bool isQrc, isNet;
    if (!parseResKey(resKey, realPath, isQrc, isNet))
        return "";

    if (m_textAssetCache.contains(realPath))
        return m_textAssetCache[realPath];

    QFile f(realPath);
    if (!f.open(QIODevice::ReadOnly)) return "";
    QString text = f.readAll();
    f.close();
    m_textAssetCache[realPath] = text;
    return text;
}

// 读取json配置
QJsonObject ResourceHub::loadAssetJson(const QString &resKey)
{
    QString text = loadAssetText(resKey);
    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
    return doc.object();
}

void ResourceHub::gc()
{
    trimImageCache();
}

void ResourceHub::clearImageCache()
{
    m_imgCache.clear();
    m_currentMemUsed = 0;
}

void ResourceHub::clearFontCache()
{
    for (auto& item : m_fontPool)
    {
        if (item.fontId >=0)
            QFontDatabase::removeApplicationFont(item.fontId);
    }
    m_fontPool.clear();
}

// 核心路径解析，支持5种格式
bool ResourceHub::parseResKey(const QString &key, QString &realPath, bool &isQrc, bool &isNet)
{
    isQrc = false;
    isNet = false;
    realPath.clear();

    // 1 内置qrc://
    if (key.startsWith("qrc://"))
    {
        isQrc = true;
        realPath = ":" + key.mid(5);
        return QFileInfo(realPath).exists();
    }
    // 2 网络http/https
    if (key.startsWith("http://") || key.startsWith("https://"))
    {
        isNet = true;
        realPath = key;
        return true;
    }
    // 3 本地绝对file://
    if (key.startsWith("file://"))
    {
        realPath = key.mid(7);
        return QFileInfo(realPath).exists();
    }
    // 4 静态资源 asset:xxx 不受皮肤切换影响
    if (key.startsWith("asset:"))
    {
        QString sub = key.mid(6);
        realPath = m_assetRoot + "/" + sub;
        return QFileInfo(realPath).exists();
    }
    // 5 皮肤资源 skin:xxx 跟随当前皮肤目录
    if (key.startsWith("skin:"))
    {
        QString sub = key.mid(5);
        realPath = getCurrentSkinDir() + "/" + sub;
        return QFileInfo(realPath).exists();
    }
    // 兼容旧简写：纯文件名默认走皮肤目录
    realPath = getCurrentSkinDir() + "/" + key;
    return QFileInfo(realPath).exists();
}

// 图片缓存超上限，淘汰很久不用的图
void ResourceHub::trimImageCache()
{
    if (m_currentMemUsed < m_maxMemLimit) return;

    QList<QString> keys = m_imgCache.keys();
    std::sort(keys.begin(), keys.end(), [this](const QString& a, const QString& b){
        return m_imgCache[a].lastUseTime < m_imgCache[b].lastUseTime;
    });

    for (const QString& k : keys)
    {
        ImageCacheItem item = m_imgCache.take(k);
        m_currentMemUsed -= item.byteSize;
        if (m_currentMemUsed < m_maxMemLimit * 0.75) break;
    }
}

qint64 ResourceHub::calcImageByte(const QImage &img)
{
    return img.byteCount();
}

// 网络图片异步下载
void ResourceHub::downloadNetImage(const QString &url, const QString &cachePath, std::function<void (QImage)> cb)
{
    QNetworkAccessManager* mgr = new QNetworkAccessManager(this);
    QNetworkReply* rep = mgr->get(QNetworkRequest(url));
    connect(rep, &QNetworkReply::finished, this, [=](){
        QByteArray data = rep->readAll();
        QImage img;
        if (!data.isEmpty())
        {
            img.loadFromData(data);
            QFile f(cachePath);
            if (f.open(QIODevice::WriteOnly))
            {
                f.write(data);
                f.close();
            }
        }
        cb(img);
        rep->deleteLater();
        mgr->deleteLater();
    });
}

// 递归监听文件夹改动，实现热更新
void ResourceHub::watchDir(const QString &dir)
{
    QDir d(dir);
    QStringList subs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    m_watcher.addPath(dir);
    for (const QString& sub : subs)
        watchDir(d.filePath(sub));
}
