#include "FileUtils.h"
#include <QFile>
#include <QDirIterator>
#include <QDebug>
#include <QTextStream>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QFileInfo>
#include <QTextCodec>
#include <QStack>

// ========================== 创建 ==========================
namespace Sqz::Utils {
bool FileUtils::createFile(const QString &filePath)
{
    if (isFileExist(filePath)) {
        // 文件已存在，进一步检查是否可写（避免后续操作失败）
        QFile f(filePath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "createFile: 文件已存在但不可写" << filePath;
            return false;
        }
        f.close();
        return true;
    }

    QFileInfo info(filePath);
    if (!createDir(info.path())) {
        qWarning() << "createFile: 无法创建父目录" << info.path();
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "createFile: 创建文件失败" << file.errorString();
        return false;
    }
    file.close();
    return true;
}

bool FileUtils::createDir(const QString &dirPath)
{
    if (isDirExist(dirPath)) return true;
    QDir dir;
    return dir.mkpath(dirPath);
}

// ========================== 拷贝 ==========================

bool FileUtils::copyFile(const QString &srcFile, const QString &destFile, bool overwrite)
{
    if (!isFileExist(srcFile)) {
        qWarning() << "copyFile: 源文件不存在" << srcFile;
        return false;
    }

    QFileInfo destInfo(destFile);
    if (!createDir(destInfo.path())) {
        qWarning() << "copyFile: 无法创建目标目录" << destInfo.path();
        return false;
    }

    if (overwrite && isFileExist(destFile)) {
        if (!QFile::remove(destFile)) {
            qWarning() << "copyFile: 无法删除已存在的目标文件" << destFile;
            return false;
        }
    }

    return QFile::copy(srcFile, destFile);
}

bool FileUtils::copyDir(const QString &srcDir, const QString &destDir, bool overwrite)
{
    if (!isDirExist(srcDir)) {
        qWarning() << "copyDir: 源目录不存在" << srcDir;
        return false;
    }

    if (!createDir(destDir)) {
        qWarning() << "copyDir: 无法创建目标目录" << destDir;
        return false;
    }

    return copyDirContent(srcDir, destDir, overwrite);
}

// 内部实现：递归复制目录内容（不复制顶层目录自身）
bool FileUtils::copyDirContent(const QString &srcDir, const QString &destDir, bool overwrite)
{
    // 使用 QDirIterator，不跟随符号链接（避免循环）
    QDirIterator it(srcDir,
                    QDir::AllEntries | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString srcPath = it.next();
        // 计算相对路径，避免字符串偏移错误
        QString relPath = QDir(srcDir).relativeFilePath(srcPath);
        QString dstPath = destDir + "/" + relPath;

        QFileInfo info = it.fileInfo();
        if (info.isDir()) {
            if (!createDir(dstPath)) {
                qWarning() << "copyDirContent: 创建子目录失败" << dstPath;
                return false;
            }
        } else {
            if (!copyFile(srcPath, dstPath, overwrite)) {
                qWarning() << "copyDirContent: 复制文件失败" << srcPath;
                return false;
            }
        }
    }
    return true;
}

// ========================== 移动（支持跨设备） ==========================

// 跨设备移动文件：复制+删除源
bool FileUtils::moveFileCrossDevice(const QString &src, const QString &dst, bool overwrite)
{
    if (!copyFile(src, dst, overwrite))
        return false;
    if (!deleteFile(src)) {
        qWarning() << "moveFileCrossDevice: 复制成功但删除源失败" << src;
        // 此时目标文件可能已创建，可以尝试回滚删除目标（但可能失败）
        QFile::remove(dst);
        return false;
    }
    return true;
}

// 跨设备移动目录：递归复制+删除源
bool FileUtils::moveDirCrossDevice(const QString &src, const QString &dst, bool overwrite)
{
    if (!copyDir(src, dst, overwrite))
        return false;
    if (!deleteDir(src)) {
        qWarning() << "moveDirCrossDevice: 复制成功但删除源失败" << src;
        // 回滚：删除已复制的目标目录
        deleteDir(dst);
        return false;
    }
    return true;
}

bool FileUtils::moveFile(const QString &srcFile, const QString &destFile, bool overwrite)
{
    if (!isFileExist(srcFile)) {
        qWarning() << "moveFile: 源文件不存在" << srcFile;
        return false;
    }

    QFileInfo destInfo(destFile);
    if (!createDir(destInfo.path())) {
        qWarning() << "moveFile: 无法创建目标目录" << destInfo.path();
        return false;
    }

    // 如果目标文件存在且需要覆盖，先删除
    if (overwrite && isFileExist(destFile)) {
        if (!QFile::remove(destFile)) {
            qWarning() << "moveFile: 无法删除已存在的目标文件" << destFile;
            return false;
        }
    }

    // 先尝试直接 rename（同设备快速移动）
    if (QFile::rename(srcFile, destFile))
        return true;

    // rename 失败（可能跨设备或权限问题），回退到复制+删除
    qDebug() << "moveFile: rename 失败，尝试跨设备复制+删除" << srcFile << "->" << destFile;
    return moveFileCrossDevice(srcFile, destFile, overwrite);
}

bool FileUtils::moveDir(const QString &srcDir, const QString &destDir, bool overwrite)
{
    if (!isDirExist(srcDir)) {
        qWarning() << "moveDir: 源目录不存在" << srcDir;
        return false;
    }

    // 防止将目录移动到自身内部（造成无限循环或数据丢失）
    QDir src(srcDir);
    QDir dst(destDir);
    if (dst.absolutePath().startsWith(src.absolutePath() + QDir::separator()) &&
        dst.absolutePath() != src.absolutePath()) {
        qWarning() << "moveDir: 不能将目录移动到自身子目录内" << srcDir << "->" << destDir;
        return false;
    }

    // 如果目标目录存在且需要覆盖，先删除
    if (overwrite && isDirExist(destDir)) {
        if (!deleteDir(destDir)) {
            qWarning() << "moveDir: 无法删除已存在的目标目录" << destDir;
            return false;
        }
    }

    // 尝试直接 rename
    QDir dir;
    if (dir.rename(srcDir, destDir))
        return true;

    // rename 失败，尝试跨设备复制+删除
    qDebug() << "moveDir: rename 失败，尝试跨设备复制+删除" << srcDir << "->" << destDir;
    return moveDirCrossDevice(srcDir, destDir, overwrite);
}

// ========================== 重命名 ==========================

bool FileUtils::rename(const QString &oldPath, const QString &newName)
{
    if (!isPathExist(oldPath)) {
        qWarning() << "rename: 原路径不存在" << oldPath;
        return false;
    }

    // 检查新名称是否包含路径分隔符
    if (newName.contains('/') || newName.contains('\\')) {
        qWarning() << "rename: 新名称不能包含路径分隔符" << newName;
        return false;
    }

    QFileInfo info(oldPath);
    QString newPath = info.path() + "/" + newName;

    // 对目录使用 QDir::rename，对文件使用 QFile::rename
    if (info.isDir()) {
        QDir dir;
        return dir.rename(oldPath, newPath);
    } else {
        return QFile::rename(oldPath, newPath);
    }
}

// ========================== 删除（迭代实现，避免栈溢出） ==========================

bool FileUtils::deleteFile(const QString &filePath)
{
    if (!isFileExist(filePath)) return true;   // 不存在视为成功
    return QFile::remove(filePath);
}

// 迭代删除目录内容（不删除目录自身）
bool FileUtils::clearDirContent(const QString &dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists()) return true;

    // 使用栈进行深度优先迭代，避免递归栈溢出
    QStack<QString> stack;
    stack.push(dirPath);

    while (!stack.isEmpty()) {
        QString current = stack.pop();
        QDir curDir(current);
        // 获取所有条目（排除 . 和 ..）
        QFileInfoList list = curDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
        for (const QFileInfo &info : list) {
            if (info.isDir()) {
                // 子目录先压栈，稍后处理
                stack.push(info.filePath());
            } else {
                // 删除文件
                if (!QFile::remove(info.filePath())) {
                    qWarning() << "clearDirContent: 删除文件失败" << info.filePath();
                    return false;
                }
            }
        }
    }

    // 此时所有子目录都已处理，需要反向删除空目录（从最深层开始）
    // 上面栈处理时没有直接删除目录，重新遍历一次删除空目录
    QDirIterator it(dirPath, QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    // 收集所有子目录路径（从深到浅需要排序）
    QStringList dirs;
    while (it.hasNext()) {
        it.next();
        dirs.prepend(it.filePath());  // 先插入子路径，保证删除时从深层开始
    }
    for (const QString &d : dirs) {
        QDir(d).rmdir(d);
    }
    return true;
}

bool FileUtils::clearDir(const QString &dirPath)
{
    if (!isDirExist(dirPath)) return true;
    return clearDirContent(dirPath);
}

bool FileUtils::deleteDir(const QString &dirPath)
{
    if (!isDirExist(dirPath)) return true;
    if (!clearDirContent(dirPath)) return false;
    return QDir(dirPath).rmdir(dirPath);
}

// ========================== 遍历 ==========================

QFileInfoList FileUtils::getFileList(const QString &dirPath, bool recursive, QDir::Filters filters)
{
    QFileInfoList list;
    if (!isDirExist(dirPath)) return list;

    if (recursive) {
        QDirIterator it(dirPath, filters, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            list.append(it.fileInfo());
        }
    } else {
        list = QDir(dirPath).entryInfoList(filters);
    }
    return list;
}

QFileInfoList FileUtils::getFilesBySuffix(const QString &dirPath, const QStringList &suffixes, bool recursive)
{
    QFileInfoList all = getFileList(dirPath, recursive, QDir::Files | QDir::NoDotAndDotDot);
    QFileInfoList result;
    for (const QFileInfo &info : all) {
        if (suffixes.contains(info.suffix().toLower()))
            result.append(info);
    }
    return result;
}

qint64 FileUtils::getDirSize(const QString &dirPath)
{
    qint64 size = 0;
    // 不跟随符号链接，避免循环
   QDirIterator it(dirPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        size += it.fileInfo().size();
    }
    return size;
}

// ========================== 读写 ==========================

bool FileUtils::writeTextFile(const QString &filePath, const QString &content,
                              const QString &codec, bool append)
{
    if (!createFile(filePath)) return false;

    QFile file(filePath);
    QIODevice::OpenMode mode = QIODevice::WriteOnly | QIODevice::Text;
    if (append) mode |= QIODevice::Append;

    if (!file.open(mode)) {
        qWarning() << "writeTextFile: 无法打开文件" << file.errorString();
        return false;
    }

    QTextStream out(&file);
    QTextCodec *c = QTextCodec::codecForName(codec.toUtf8());
    if (c) out.setCodec(c);
    else qWarning() << "writeTextFile: 未知编码" << codec << "使用系统默认";

    out << content;
    file.close();
    return true;
}

QString FileUtils::readTextFile(const QString &filePath, const QString &codec)
{
    if (!isFileExist(filePath)) return "";
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "readTextFile: 无法打开文件" << file.errorString();
        return "";
    }

    QTextStream in(&file);
    QTextCodec *c = QTextCodec::codecForName(codec.toUtf8());
    if (c) in.setCodec(c);
    return in.readAll();
}

bool FileUtils::writeBinaryFile(const QString &filePath, const QByteArray &data)
{
    if (!createFile(filePath)) return false;
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "writeBinaryFile: 无法打开文件" << file.errorString();
        return false;
    }
    file.write(data);
    file.close();
    return true;
}

QByteArray FileUtils::readBinaryFile(const QString &filePath)
{
    if (!isFileExist(filePath)) return {};
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "readBinaryFile: 无法打开文件" << file.errorString();
        return {};
    }
    return file.readAll();
}

// ========================== 路径工具 ==========================

QString FileUtils::formatPath(const QString &path)
{
    return QDir::cleanPath(path);
}

QString FileUtils::getFileDir(const QString &filePath)
{
    return QFileInfo(filePath).path();
}

QString FileUtils::getFileName(const QString &filePath)
{
    return QFileInfo(filePath).fileName();
}

QString FileUtils::getFileNameWithoutSuffix(const QString &filePath)
{
    return QFileInfo(filePath).baseName();
}

QString FileUtils::getFileSuffix(const QString &filePath)
{
    return QFileInfo(filePath).suffix();
}

// ========================== 校验 ==========================

bool FileUtils::isFileExist(const QString &filePath)
{
    return QFile::exists(filePath) && QFileInfo(filePath).isFile();
}

bool FileUtils::isDirExist(const QString &dirPath)
{
    QDir dir(dirPath);
    return dir.exists();
}

bool FileUtils::isPathExist(const QString &path)
{
    return QFile::exists(path);
}

QString FileUtils::getFileMd5(const QString &filePath)
{
    if (!isFileExist(filePath)) return "";

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "getFileMd5: 无法打开文件" << filePath;
        return "";
    }

    QCryptographicHash hash(QCryptographicHash::Md5);
    const qint64 bufferSize = 8192;
    QByteArray buffer;

    while (!file.atEnd()) {
        buffer = file.read(bufferSize);
        if (buffer.isEmpty()) break;
        hash.addData(buffer);
    }
    file.close();
    return hash.result().toHex().toUpper();
}

// ========================== 桌面快捷方式（跨平台实用方案） ==========================

bool FileUtils::createDesktopLink(const QString &filePath, const QString &linkName)
{
    QString desk = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (desk.isEmpty()) {
        qWarning() << "createDesktopLink: 无法获取桌面路径";
        return false;
    }

    // 确保目标文件存在
    if (!isPathExist(filePath)) {
        qWarning() << "createDesktopLink: 目标文件不存在" << filePath;
        return false;
    }

#ifdef Q_OS_WIN
    // Windows: 生成 .bat 批处理文件
    QString batPath = desk + "/" + linkName + ".bat";
    QFile batFile(batPath);
    if (!batFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "createDesktopLink: 无法创建批处理文件" << batPath;
        return false;
    }
    QTextStream out(&batFile);
    // 使用 start 命令打开文件，自动关联默认程序
    QString escapedPath = filePath;
    escapedPath.replace("\"", "\"\"");  // 简单转义双引号
    out << "@echo off\n";
    out << "start \"\" \"" << escapedPath << "\"\n";
    out << "exit\n";
    batFile.close();
    return true;
#else
    // Linux / Unix: 生成 .desktop 文件
    QString desktopPath = desk + "/" + linkName + ".desktop";
    QFile desktopFile(desktopPath);
    if (!desktopFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "createDesktopLink: 无法创建 .desktop 文件" << desktopPath;
        return false;
    }
    QTextStream out(&desktopFile);
    out << "[Desktop Entry]\n";
    out << "Version=1.0\n";
    out << "Type=Application\n";
    out << "Name=" << linkName << "\n";
    out << "Exec=\"" << filePath << "\"\n";
    out << "Terminal=false\n";
    out << "StartupNotify=false\n";
    desktopFile.close();

    // 给 .desktop 文件添加可执行权限
    QFile::setPermissions(desktopPath,
                          QFile::permissions(desktopPath) |
                          QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther);
    return true;
#endif
}
}
