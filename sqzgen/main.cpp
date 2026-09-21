#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDebug>
#include <QSet>
#include <QHash>
#include <QMap>
#include <QDateTime>
#include <QStringList>
#include <algorithm>

const QString g_cacheFileName = ".codegen_cache.json";

// ============================================================
// 源文件扫描
// ============================================================
void scanSourceFiles(const QDir& dir, QList<QFileInfo>& cppFiles, QList<QFileInfo>& hFiles)
{
    QFileInfoList list = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for(const QFileInfo& fi : list)
    {
        if(fi.isDir())
            scanSourceFiles(QDir(fi.absoluteFilePath()), cppFiles, hFiles);
        else
        {
            QString suf = fi.suffix().toLower();
            if(suf == "cpp")
                cppFiles.append(fi);
            else if(suf == "h")
                hFiles.append(fi);
        }
    }
}

// ============================================================
// 解析 cpp：提取 SQZ_REG(Xxx)
// ============================================================
QSet<QString> parseCppForReg(const QString& filePath)
{
    QSet<QString> regSet;
    QFile f(filePath);
    if(!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return regSet;
    QString content = QString::fromUtf8(f.readAll());
    f.close();

    QRegularExpression reReg("SQZ_REG\\s*\\(\\s*(\\w+)\\s*\\)");
    auto iter = reReg.globalMatch(content);
    while(iter.hasNext())
        regSet.insert(iter.next().captured(1));
    return regSet;
}

// ============================================================
// 解析 h：提取 class Xxx : public SqzWidget / SqzService
// ============================================================
QHash<QString, QString> parseHeaderForInherit(const QString& filePath)
{
    QHash<QString, QString> map;
    QFile f(filePath);
    if(!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return map;
    QString content = QString::fromUtf8(f.readAll());
    f.close();

    QRegularExpression reClassWidget("class\\s+(\\w+)\\s*:\\s*public\\s+SqzWidget");
    QRegularExpression reClassService("class\\s+(\\w+)\\s*:\\s*public\\s+SqzService");

    auto iterW = reClassWidget.globalMatch(content);
    while(iterW.hasNext())
        map.insert(iterW.next().captured(1), "widget");

    auto iterS = reClassService.globalMatch(content);
    while(iterS.hasNext())
        map.insert(iterS.next().captured(1), "service");

    return map;
}

// ============================================================
// 缓存读写
// ============================================================
QHash<QString, qint64> readCache(const QString& workDir)
{
    QHash<QString,qint64> emptyRet;
    QFile cache(QDir(workDir).filePath(g_cacheFileName));
    if(!cache.exists() || !cache.open(QIODevice::ReadOnly))
        return emptyRet;
    QJsonDocument doc = QJsonDocument::fromJson(cache.readAll());
    cache.close();
    if(!doc.isObject())
        return emptyRet;
    QJsonObject obj = doc.object();
    QHash<QString,qint64> ret;
    for(auto it=obj.begin();it!=obj.end();++it)
        ret.insert(it.key(), it.value().toVariant().toLongLong());
    return ret;
}

bool writeCache(const QString& workDir, const QHash<QString, qint64>& cacheMap)
{
    QJsonObject obj;
    for(auto it=cacheMap.begin();it!=cacheMap.end();++it)
        obj.insert(it.key(), QJsonValue(qint64(it.value())));
    QJsonDocument doc(obj);
    QFile f(QDir(workDir).filePath(g_cacheFileName));
    if(!f.open(QIODevice::WriteOnly))
        return false;
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

// ============================================================
// JSON 清洗：状态机扫描，只删除字符串外的注释和尾随逗号
// ============================================================
QString cleanJsonForQt(const QString& raw)
{
    QString out;
    out.reserve(raw.size());

    const int n = raw.size();
    int i = 0;
    bool inString = false;
    bool escaped = false;

    while(i < n)
    {
        QChar c = raw[i];

        if(inString)
        {
            out.append(c);
            if(escaped) escaped = false;
            else if(c == '\\') escaped = true;
            else if(c == '"') inString = false;
            ++i;
            continue;
        }

        if(c == '"')
        {
            inString = true;
            out.append(c);
            ++i;
            continue;
        }

        if(c == '/' && i + 1 < n && raw[i+1] == '/')
        {
            i += 2;
            while(i < n && raw[i] != '\n') ++i;
            continue;
        }

        if(c == '/' && i + 1 < n && raw[i+1] == '*')
        {
            i += 2;
            while(i + 1 < n && !(raw[i] == '*' && raw[i+1] == '/')) ++i;
            i += 2;
            continue;
        }

        if(c == ',')
        {
            int j = i + 1;
            while(j < n && (raw[j] == ' ' || raw[j] == '\t' || raw[j] == '\n' || raw[j] == '\r')) ++j;
            if(j < n && (raw[j] == '}' || raw[j] == ']'))
            {
                ++i;
                continue;
            }
        }

        out.append(c);
        ++i;
    }

    return out;
}

// ============================================================
// 读取旧 JSON（容错）
// ============================================================
bool readOldJson(const QString& path, QJsonObject& outRoot, bool& fileExisted)
{
    outRoot = QJsonObject();
    fileExisted = false;

    QFile f(path);
    if(!f.exists())
        return true;

    fileExisted = true;
    if(!f.open(QIODevice::ReadOnly))
    {
        qDebug() << "错误：无法打开旧 JSON 文件：" << path;
        return false;
    }
    QString raw = QString::fromUtf8(f.readAll());
    f.close();

    if(raw.trimmed().isEmpty())
        return true;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(cleanJsonForQt(raw).toUtf8(), &err);
    if(err.error != QJsonParseError::NoError)
    {
        qDebug() << "错误：旧 JSON 解析失败，位置：" << err.offset
                 << "错误：" << err.errorString();
        return false;
    }
    if(!doc.isObject())
    {
        qDebug() << "错误：旧 JSON 根节点不是对象";
        return false;
    }
    outRoot = doc.object();
    return true;
}

// ============================================================
// 手动按指定 key 顺序序列化一个 JSON 对象（缩进 4 空格）
// ============================================================
QString escapeJsonString(const QString& s)
{
    QString out;
    out.reserve(s.size() + 2);
    out.append('"');
    for(QChar c : s)
    {
        switch(c.unicode())
        {
        case '"':  out.append("\\\""); break;
        case '\\': out.append("\\\\"); break;
        case '\b': out.append("\\b");  break;
        case '\f': out.append("\\f");  break;
        case '\n': out.append("\\n");  break;
        case '\r': out.append("\\r");  break;
        case '\t': out.append("\\t");  break;
        default:
            if(c.unicode() < 0x20)
                out.append(QString("\\u%1").arg(c.unicode(), 4, 16, QChar('0')));
            else
                out.append(c);
        }
    }
    out.append('"');
    return out;
}

// 序列化 JSON 值（递归），indent 为当前缩进层级
QString serializeValue(const QJsonValue& v, int indent);

QString serializeObject(const QJsonObject& obj, const QStringList& keyOrder, int indent)
{
    QString pad(indent * 4, ' ');
    QString innerPad((indent + 1) * 4, ' ');

    // 先按指定顺序输出，再输出剩余的 key（保持字母序）
    QStringList keys;
    QSet<QString> used;
    for(const QString& k : keyOrder)
    {
        if(obj.contains(k))
        {
            keys.append(k);
            used.insert(k);
        }
    }
    QStringList rest = obj.keys();
    std::sort(rest.begin(), rest.end());
    for(const QString& k : rest)
    {
        if(!used.contains(k))
            keys.append(k);
    }

    if(keys.isEmpty())
        return "{}";

    QString out = "{\n";
    for(int i = 0; i < keys.size(); ++i)
    {
        const QString& k = keys[i];
        out += innerPad + escapeJsonString(k) + ": "
             + serializeValue(obj.value(k), indent + 1);
        if(i + 1 < keys.size())
            out += ",";
        out += "\n";
    }
    out += pad + "}";
    return out;
}

QString serializeArray(const QJsonArray& arr, int indent)
{
    QString pad(indent * 4, ' ');
    QString innerPad((indent + 1) * 4, ' ');

    if(arr.isEmpty())
        return "[]";

    QString out = "[\n";
    for(int i = 0; i < arr.size(); ++i)
    {
        out += innerPad + serializeValue(arr[i], indent + 1);
        if(i + 1 < arr.size())
            out += ",";
        out += "\n";
    }
    out += pad + "]";
    return out;
}

QString serializeValue(const QJsonValue& v, int indent)
{
    if(v.isObject())
        return serializeObject(v.toObject(), {}, indent);
    if(v.isArray())
        return serializeArray(v.toArray(), indent);
    if(v.isString())
        return escapeJsonString(v.toString());
    if(v.isBool())
        return v.toBool() ? "true" : "false";
    if(v.isDouble())
    {
        double d = v.toDouble();
        // 整数不输出小数点
        if(d == qint64(d))
            return QString::number(qint64(d));
        return QString::number(d, 'g', 17);
    }
    if(v.isNull())
        return "null";
    return "null";
}

// ============================================================
// 按指定 key 顺序序列化 Service 条目
// ============================================================
QString serializeService(const QJsonObject& obj, int indent)
{
    static const QStringList order = {
        "ClassName", "Async", "Auto", "Order", "Props"
    };
    return serializeObject(obj, order, indent);
}

// ============================================================
// 按指定 key 顺序序列化 View 条目
// ============================================================
QString serializeView(const QJsonObject& obj, int indent)
{
    static const QStringList order = {
        "ViewType", "ClassName", "QmlSource", "Auto", "Main", "Props"
    };
    return serializeObject(obj, order, indent);
}

// ============================================================
// 主函数
// ============================================================
int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    if(argc < 3)
    {
        fprintf(stderr,"用法：SqzCodeGen <src目录> <SqzAppConfig.json输出路径>\n");
        return 1;
    }
    QString srcDirPath = argv[1];
    QString outJsonPath = argv[2];

    QDir srcDir(srcDirPath);
    if(!srcDir.exists())
    {
        fprintf(stderr,"错误：源码目录不存在 %s\n", srcDirPath.toUtf8().constData());
        return 2;
    }

    // ---------- 1. 扫描源文件 ----------
    QList<QFileInfo> cppFiles, hFiles;
    scanSourceFiles(srcDir, cppFiles, hFiles);

    QString workDir = QFileInfo(outJsonPath).absoluteDir().absolutePath();
    auto oldCache = readCache(workDir);
    QHash<QString, qint64> newCache;

    auto checkFile = [&](const QFileInfo& fi){
        qint64 mtime = fi.lastModified().toMSecsSinceEpoch();
        QString absPath = fi.absoluteFilePath();
        newCache[absPath] = mtime;
    };
    for(const auto& fi : cppFiles) checkFile(fi);
    for(const auto& fi : hFiles)   checkFile(fi);

    // ---------- 2. 解析源文件得到扫描结果 ----------
    QSet<QString> regAllClasses;
    for(const auto& fi : cppFiles)
        regAllClasses.unite(parseCppForReg(fi.absoluteFilePath()));

    QHash<QString, QString> classNameToType;
    for(const auto& fi : hFiles)
        classNameToType.unite(parseHeaderForInherit(fi.absoluteFilePath()));

    QSet<QString> widgetScanSet, serviceScanSet;
    for(const QString& cls : regAllClasses)
    {
        if(!classNameToType.contains(cls)) continue;
        QString t = classNameToType[cls];
        if(t == "widget")       widgetScanSet.insert(cls);
        else if(t == "service") serviceScanSet.insert(cls);
    }

    // ---------- 3. 读取旧 JSON ----------
    QJsonObject oldRoot;
    bool fileExisted = false;
    if(!readOldJson(outJsonPath, oldRoot, fileExisted))
    {
        fprintf(stderr, "错误：旧 JSON 解析失败，已中止以避免覆盖用户配置。请检查文件内容。\n");
        return 4;
    }

    // ---------- 4. 增量更新 ----------
    QJsonObject newRoot = oldRoot;

    // AppMeta
    if(!newRoot.contains("AppMeta") || !newRoot["AppMeta"].isObject())
    {
        QJsonObject meta;
        meta.insert("AppName", "XXX");
        meta.insert("Version", "1.0.0");
        newRoot.insert("AppMeta", meta);
    }

    // ---- Services 增量更新 ----
    {
        QJsonArray oldArr = newRoot["Services"].toArray();
        QHash<QString, QJsonObject> oldMap;   // ClassName -> 原始对象
        for(auto item : oldArr)
        {
            QJsonObject o = item.toObject();
            QString cls = o["ClassName"].toString();
            if(!cls.isEmpty())
                oldMap.insert(cls, o);
        }

        // 按 ClassName 升序排序的类名列表
        QStringList sortedClasses = serviceScanSet.values();
        std::sort(sortedClasses.begin(), sortedClasses.end());

        QJsonArray newArr;
        for(const QString& cls : sortedClasses)
        {
            if(oldMap.contains(cls))
            {
                newArr.append(oldMap.value(cls));
            }
            else
            {
                QJsonObject o;
                o.insert("ClassName", cls);
                o.insert("Async", false);
                o.insert("Auto", false);
                o.insert("Order", 99);
                o.insert("Props", QJsonObject());
                newArr.append(o);
            }
        }
        newRoot["Services"] = newArr;
    }

    // ---- Views 增量更新 ----
    {
        QJsonArray oldArr = newRoot["Views"].toArray();

        // 分类旧数据
        QHash<QString, QJsonObject> oldWidgetMapByQml;   // QmlSource -> 旧对象
        QList<QJsonObject> oldWidgetList;                // 顺序列表，用于兜底
        QList<QJsonObject> nonWidgetList;                // 非 SqzWidget 原样保留

        for(auto item : oldArr)
        {
            QJsonObject o = item.toObject();
            if(o["ViewType"].toString() != "SqzWidget")
            {
                nonWidgetList.append(o);
                continue;
            }
            oldWidgetList.append(o);
            QString qml = o["QmlSource"].toString();
            if(!qml.isEmpty() && !oldWidgetMapByQml.contains(qml))
                oldWidgetMapByQml.insert(qml, o);
        }

        // 用于标记 oldWidgetList 中已被使用的索引
        QSet<int> usedIndices;
        QList<QJsonObject> newWidgetList;

        // 按 ClassName 升序排序扫描到的 Widget 类
        QStringList sortedWidgets = widgetScanSet.values();
        std::sort(sortedWidgets.begin(), sortedWidgets.end());

        for(const QString& cls : sortedWidgets)
        {
            QJsonObject matched;
            bool found = false;

            // 1) 按 QmlSource 匹配（QmlSource 包含类名，忽略大小写）
            for(int i = 0; i < oldWidgetList.size(); ++i)
            {
                if(usedIndices.contains(i)) continue;
                QJsonObject o = oldWidgetList[i];
                QString qml = o["QmlSource"].toString();
                if(qml.isEmpty()) continue;
                if(qml.contains(cls, Qt::CaseInsensitive))
                {
                    matched = o;
                    usedIndices.insert(i);
                    found = true;
                    break;
                }
            }

            // 2) 顺序兜底：取第一个未被使用的旧 SqzWidget
            if(!found)
            {
                for(int i = 0; i < oldWidgetList.size(); ++i)
                {
                    if(usedIndices.contains(i)) continue;
                    matched = oldWidgetList[i];
                    usedIndices.insert(i);
                    found = true;
                    break;
                }
            }

            if(found)
            {
                // SqzWidget 不写 ClassName，移除可能存在的 ClassName
                newWidgetList.append(matched);
            }
            else
            {
                QJsonObject o;
                o.insert("ViewType", "SqzWidget");
                o.insert("ClassName", cls);
                o.insert("QmlSource", "");
                o.insert("Auto", false);
                o.insert("Main", false);
                o.insert("Props", QJsonObject());
                newWidgetList.append(o);
            }
        }

        // 非 SqzWidget 的 View 也按 ClassName 排序后追加
        std::sort(nonWidgetList.begin(), nonWidgetList.end(),
                  [](const QJsonObject& a, const QJsonObject& b){
            return a["ClassName"].toString() < b["ClassName"].toString();
        });

        QJsonArray newArr;
        // 先放非 SqzWidget（已按 ClassName 排序）
        for(const auto& o : nonWidgetList)
            newArr.append(o);
        // 再放 SqzWidget（已按扫描类名升序）
        for(const auto& o : newWidgetList)
            newArr.append(o);

        newRoot["Views"] = newArr;
    }

    // ---------- 5. 手动拼装最终 JSON，保证 key 顺序 ----------
    QString finalJson;
    {
        // AppMeta
        QString appMetaStr = serializeObject(newRoot["AppMeta"].toObject(), {}, 1);

        // Services
        QJsonArray servicesArr = newRoot["Services"].toArray();
        QString servicesStr;
        if(servicesArr.isEmpty())
        {
            servicesStr = "[]";
        }
        else
        {
            servicesStr = "[\n";
            for(int i = 0; i < servicesArr.size(); ++i)
            {
                servicesStr += "        " + serializeService(servicesArr[i].toObject(), 2);
                if(i + 1 < servicesArr.size()) servicesStr += ",";
                servicesStr += "\n";
            }
            servicesStr += "    ]";
        }

        // Views
        QJsonArray viewsArr = newRoot["Views"].toArray();
        QString viewsStr;
        if(viewsArr.isEmpty())
        {
            viewsStr = "[]";
        }
        else
        {
            viewsStr = "[\n";
            for(int i = 0; i < viewsArr.size(); ++i)
            {
                viewsStr += "        " + serializeView(viewsArr[i].toObject(), 2);
                if(i + 1 < viewsArr.size()) viewsStr += ",";
                viewsStr += "\n";
            }
            viewsStr += "    ]";
        }

        finalJson = QString("{\n"
                            "    \"AppMeta\": %1,\n"
                            "    \"Services\": %2,\n"
                            "    \"Views\": %3\n"
                            "}\n")
                        .arg(appMetaStr, servicesStr, viewsStr);
    }

    // ---------- 6. 写回文件 ----------
    QFile outFile(outJsonPath);
    if(!outFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        fprintf(stderr,"错误：无法写入输出文件 %s\n", outJsonPath.toUtf8().constData());
        return 3;
    }
    outFile.write(finalJson.toUtf8());
    outFile.close();

    writeCache(workDir, newCache);

    qDebug() << QString("扫描完成：Widget:%1个 Service:%2个；已增量更新 SqzAppConfig.json")
                .arg(widgetScanSet.size()).arg(serviceScanSet.size());
    return 0;
}
