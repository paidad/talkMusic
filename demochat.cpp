#include "demochat.h"

DemoChat::DemoChat(QObject *parent) : QObject(parent) {
    for (const auto &name : QStringList{m_active, QStringLiteral("林间"), QStringLiteral("小满")}) {
        m_messages.insert(name, {{name == m_active ? QStringLiteral("林间") : name,
            QStringLiteral("欢迎来到音乐客厅。导入一首喜欢的歌，试试把它分享在这里吧。"), {}, false, QDateTime::currentDateTime()}});
    }
}
QStringList DemoChat::conversations() const { return {QStringLiteral("一起听 · 音乐客厅"), QStringLiteral("林间"), QStringLiteral("小满")}; }
void DemoChat::selectConversation(const QString &name) {
    if (!m_messages.contains(name) || name == m_active) return;
    m_active = name; emit changed();
}
void DemoChat::sendText(const QString &text) {
    if (text.trimmed().isEmpty()) return;
    m_messages[m_active].append({QStringLiteral("我"), text.trimmed(), {}, true, QDateTime::currentDateTime()}); emit changed();
}
void DemoChat::shareSong(const QString &path) {
    if (path.isEmpty()) return;
    m_messages[m_active].append({QStringLiteral("我"), {}, path, true, QDateTime::currentDateTime()}); emit changed();
}
