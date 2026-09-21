#ifndef RESOURCEHUB_H
#define RESOURCEHUB_H

#include <QObject>
#include <QPixmap>
#include <QImage>
#include <QFont>
#include <QSize>
#include <QString>
#include <QHash>
#include <QTimer>
#include <QFileSystemWatcher>
#include <QJsonObject>
#include <functional>

// 图片缓存单元
struct ImageCacheItem
{
    QImage img;
    qint64 lastUseTime = 0;
    qint64 byteSize = 0;
};

// 字体缓存单元
struct FontCacheItem
{
    int fontId = -1;
    int refCount = 0;
    QString filePath;
};

class ResourceHub : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString skinName READ skinName NOTIFY skinChanged)
public:
    // 单例
    static ResourceHub* instance();
    ~ResourceHub() override;

    //==================== 路径自定义接口 ====================
    void setSkinRootDir(const QString& dir);
    void setAssetRootDir(const QString& dir);
    QString getSkinRootDir() const;
    QString getAssetRootDir() const;

    //==================== 皮肤控制 ====================
    QString skinName() const;
    void switchSkin(const QString& skinName);
    QString getCurrentSkinDir() const;

    //==================== 标准目录 ====================
    QString getCacheDir() const;
    QString getFontDir() const;
    QString getTempDir() const;

    //==================== 图片加载 Widget C++ ====================
    QPixmap getPixmap(const QString& resKey, const QSize& limit = QSize());
    void asyncLoadImage(const QString& resKey, std::function<void(QImage)> callback);

    //==================== QML 调用接口 ====================
    Q_INVOKABLE QString getImageSource(const QString& resKey);

    //==================== 字体 ====================
    QFont getFont(const QString& family, int pointSize, int weight = QFont::Normal);

    //==================== 文件资源(qss/json) ====================
    QString loadAssetText(const QString& resKey);
    QJsonObject loadAssetJson(const QString& resKey);

    //==================== 缓存清理 ====================
    void gc();
    void clearImageCache();
    void clearFontCache();

signals:
    void skinChanged();
    void resourceModified(const QString& filePath);

private:
    explicit ResourceHub(QObject* parent = nullptr);
    static ResourceHub* m_inst;

    // 核心路径解析器
    bool parseResKey(const QString& key, QString& realPath, bool& isQrc, bool& isNet);
    // LRU缓存淘汰
    void trimImageCache();
    qint64 calcImageByte(const QImage& img);
    // 网络图片下载
    void downloadNetImage(const QString& url, const QString& cachePath, std::function<void(QImage)> cb);
    // 递归监听目录文件改动
    void watchDir(const QString& dir);

    // 路径配置
    QString m_skinRoot;
    QString m_assetRoot;
    QString m_curSkin;

    // 图片缓存
    QHash<QString, ImageCacheItem> m_imgCache;
    qint64 m_maxMemLimit = 64 * 1024 * 1024;
    qint64 m_currentMemUsed = 0;

    // 字体缓存
    QHash<QString, FontCacheItem> m_fontPool;
    // 文本文件缓存(qss/json)
    QHash<QString, QString> m_textAssetCache;

    QFileSystemWatcher m_watcher;
    QTimer m_gcTimer;
};

#endif // RESOURCEHUB_H
