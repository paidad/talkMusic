#include "notes/notestore.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QSaveFile>
#include <QStringDecoder>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <functional>

namespace {
QString absolute(const QString &path) { return QDir::cleanPath(QFileInfo(path).absoluteFilePath()); }
QByteArray digest(const QByteArray &bytes) { return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256); }
bool samePath(const QString &a, const QString &b) {
#ifdef Q_OS_WIN
    return a.compare(b, Qt::CaseInsensitive) == 0;
#else
    return a == b;
#endif
}
}
NoteStore::NoteStore(QObject *parent) : QObject(parent) {
    // 合并一次文件操作触发的多条目录通知，避免文件树连续重建。
    m_timer.setSingleShot(true); m_timer.setInterval(150);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_timer.start(); });
    connect(&m_timer, &QTimer::timeout, this, [this] { watch(); emit changed(); });
}
bool NoteStore::available() const { return !m_root.isEmpty() && QFileInfo(m_root).isDir() && !QFileInfo(m_root).isSymLink() && !QFileInfo(m_root).isJunction(); }
void NoteStore::setRoot(const QString &path) {
    m_root = path.isEmpty() ? QString() : absolute(path); watch(); emit changed();
}
bool NoteStore::contains(const QString &path) const {
    // 先判断相对根目录是否越界，再逐级检查链接，不能仅用字符串 startsWith。
    if (!available()) return false;
    const auto candidate = absolute(path);
    const auto relative = QDir(m_root).relativeFilePath(candidate);
    if (relative == ".." || relative.startsWith("../") || QDir::isAbsolutePath(relative)) return false;
    QString cursor = candidate;
    while (!samePath(cursor, m_root)) {
        if (QFileInfo(cursor).isSymLink() || QFileInfo(cursor).isJunction()) return false;
        const auto parent = QFileInfo(cursor).absolutePath();
        if (parent == cursor) return false;
        cursor = parent;
    }
    return true;
}
bool NoteStore::validName(const QString &name) {
    // 同时排除 Windows 保留设备名、非法字符以及末尾空格/句点。
    const QRegularExpression invalid(R"([<>:"/\\|?*\x00-\x1f])");
    const QRegularExpression reserved(R"(^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$))", QRegularExpression::CaseInsensitiveOption);
    return !name.trimmed().isEmpty() && name != "." && name != ".." && !name.endsWith('.') && !name.endsWith(' ') &&
        !invalid.match(name).hasMatch() && !reserved.match(name).hasMatch();
}
QString NoteStore::create(const QString &parentPath, const QString &name, bool directory) {
    QString actual = name.trimmed();
    if (!directory && !actual.endsWith(".md", Qt::CaseInsensitive)) actual += ".md";
    const auto path = QDir(parentPath).filePath(actual);
    if (!validName(actual) || !contains(path) || !QFileInfo(parentPath).isDir() || QFileInfo::exists(path)) {
        emit error("名称无效、路径不可用或同名文件已经存在。"); return {};
    }
    if (directory) {
        if (!QDir().mkdir(path)) { emit error("无法创建文件夹。"); return {}; }
    } else {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) { emit error(file.errorString()); return {}; }
    }
    watch(); emit changed(); return absolute(path);
}
QString NoteStore::rename(const QString &path, const QString &name) {
    QString actual = name.trimmed(); const bool directory = QFileInfo(path).isDir();
    if (!directory && !actual.endsWith(".md", Qt::CaseInsensitive)) actual += ".md";
    const auto destination = QDir(QFileInfo(path).absolutePath()).filePath(actual);
    if (samePath(absolute(path), m_root) || !contains(path) || !contains(destination) || !validName(actual) || QFileInfo::exists(destination)) {
        emit error("无法重命名：名称无效、文件夹不可用或目标已存在。"); return {};
    }
    if (!QDir().rename(path, destination)) { emit error("重命名失败，请检查文件权限。"); return {}; }
    emit renamed(absolute(path), absolute(destination)); watch(); emit changed(); return absolute(destination);
}
bool NoteStore::trash(const QString &path) {
    if (samePath(absolute(path), m_root) || !contains(path) || !QFileInfo::exists(path)) { emit error("此路径无法删除。"); return false; }
    if (!QFile::moveToTrash(path)) { emit error("无法移入系统回收站，原文件仍然保留。"); return false; }
    emit removed(absolute(path)); watch(); emit changed(); return true;
}
void NoteStore::watch() {
    if (!m_watcher.directories().isEmpty()) m_watcher.removePaths(m_watcher.directories());
    QStringList paths;
    if (!m_root.isEmpty() && QFileInfo(QFileInfo(m_root).absolutePath()).isDir()) paths << QFileInfo(m_root).absolutePath();
    if (available()) {
        paths << m_root;
        std::function<void(const QString &)> append = [&](const QString &directory) {
            for (const auto &entry : QDir(directory).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks)) {
                if (entry.isJunction() || entry.isSymLink()) continue;
                paths << entry.absoluteFilePath(); append(entry.absoluteFilePath());
            }
        };
        append(m_root);
    }
    if (!paths.isEmpty()) m_watcher.addPaths(paths);
}

NoteDocument::NoteDocument(NoteStore *store, QObject *parent) : QObject(parent), m_store(store) {
    m_saveTimer.setSingleShot(true); m_saveTimer.setInterval(1000);
    m_poll.setInterval(1000);
    connect(&m_saveTimer, &QTimer::timeout, this, [this] { save(); });
    connect(&m_poll, &QTimer::timeout, this, [this] { checkDisk(false); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { checkDisk(); });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { checkDisk(); });
}
void NoteDocument::setStatus(const QString &text) { m_status = text; emit statusChanged(); }
void NoteDocument::markConflict(const QString &message) { m_saveTimer.stop(); m_conflict = true; setStatus(message); }
bool NoteDocument::read(QString &text, QByteArray &hash) {
    // 解码失败不拿替代字符覆盖原件；内存统一用 LF，另记原文件 BOM 与换行类型。
    if (!m_store->contains(m_path)) { setStatus("笔记目录不可用或文件已变为符号链接，编辑内容仍保留。"); return false; }
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) { setStatus("无法读取笔记：" + file.errorString()); return false; }
    const auto bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) { setStatus("无法完整读取笔记。"); return false; }
    QStringDecoder decoder(QStringConverter::Utf8);
    text = decoder(bytes);
    if (decoder.hasError()) { setStatus("此笔记不是有效的 UTF-8 文本，请先在其他编辑器中转换编码。"); return false; }
    m_bom = bytes.startsWith("\xef\xbb\xbf"); m_crlf = bytes.contains("\r\n");
    if (text.startsWith(QChar::ByteOrderMark)) text.remove(0, 1);
    text.replace("\r\n", "\n"); text.replace('\r', '\n'); hash = digest(bytes);
    const QFileInfo info(m_path); m_diskTime = info.lastModified(); m_diskSize = info.size(); return true;
}
bool NoteDocument::open(const QString &path) { m_path = absolute(path); return reload(); }
bool NoteDocument::reload() {
    QString text; QByteArray hash;
    if (!read(text, hash)) return false;
    m_saveTimer.stop(); m_text = text; m_hash = hash; m_dirty = false; m_conflict = false;
    watch(); m_poll.start(); setStatus("已保存"); emit reloaded(); return true;
}
void NoteDocument::watch() {
    // 原子替换文件可能使旧监听失效，重新绑定文件和父目录两个入口。
    if (!m_watcher.files().isEmpty()) m_watcher.removePaths(m_watcher.files());
    if (!m_watcher.directories().isEmpty()) m_watcher.removePaths(m_watcher.directories());
    if (QFileInfo(m_path).isFile()) m_watcher.addPath(m_path);
    if (QFileInfo(QFileInfo(m_path).absolutePath()).isDir()) m_watcher.addPath(QFileInfo(m_path).absolutePath());
}
void NoteDocument::setText(const QString &text) {
    // 实际内容改变才标脏；冲突期间暂停自动保存，等待用户选择保留哪个版本。
    if (text == m_text) return;
    m_text = text; m_dirty = true;
    if (!m_conflict) { setStatus("保存中…"); m_saveTimer.start(); }
}
void NoteDocument::checkDisk(bool force) {
    // 定时轮询可用时间/大小快速筛选；保存前强制读取摘要，以磁盘基线识别变化。
    if (m_path.isEmpty()) return;
    if (m_conflict) return;
    const QFileInfo info(m_path);
    if (!force && m_store->contains(m_path) && info.isFile() && info.lastModified() == m_diskTime && info.size() == m_diskSize) return;
    QFile file(m_path);
    if (!m_store->contains(m_path) || !file.open(QIODevice::ReadOnly)) {
        markConflict("文件已移动、删除或暂时无法读取，编辑内容仍保留。"); return;
    }
    const auto bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) { markConflict("文件读取失败，编辑内容仍保留。"); return; }
    // 无本地修改时自动重载；有本地修改时进入冲突，不能静默覆盖任一版本。
    if (digest(bytes) != m_hash) {
        if (m_dirty) markConflict("检测到外部修改，请选择保留哪个版本。");
        else if (!reload()) markConflict("外部文件无法加载，原内容仍保留。");
    }
    if (!m_conflict) { m_diskTime = info.lastModified(); m_diskSize = info.size(); }
    watch();
}
bool NoteDocument::write(const QString &path) {
    // 此处是统一落盘入口：再次验证目标范围，恢复编码标记，再提交原子替换。
    if (!m_store->contains(path) || !path.endsWith(".md", Qt::CaseInsensitive)) { setStatus("保存失败：目标必须位于笔记目录内，且不能是符号链接。"); return false; }
    QString text = m_text; if (m_crlf) text.replace("\n", "\r\n");
    const auto bytes = (m_bom ? QByteArray("\xef\xbb\xbf") : QByteArray()) + text.toUtf8();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        setStatus("保存失败：" + file.errorString()); return false;
    }
    // 仅在提交成功后更新比较基线和已保存状态，失败仍保留内存草稿。
    m_path = absolute(path); m_hash = digest(bytes); m_dirty = false; m_conflict = false;
    const QFileInfo info(m_path); m_diskTime = info.lastModified(); m_diskSize = info.size();
    m_saveTimer.stop(); watch(); setStatus("已保存"); return true;
}
bool NoteDocument::save() {
    // 检查与写入不是跨进程事务，仍存在外部程序在二者之间修改文件的竞争窗口。
    if (m_conflict) return false;
    checkDisk();
    if (m_conflict) return false;
    return !m_dirty || write(m_path);
}
bool NoteDocument::overwrite() { return write(m_path); }
bool NoteDocument::saveAs(const QString &path) { return write(path); }
void NoteDocument::relocate(const QString &path) { m_path = absolute(path); watch(); }
