#pragma once
#include <QObject>
#include <QMap>
#include <QVector>
#include <QDateTime>

struct ChatMessage {
    QString sender;
    QString text;
    QString songPath;
    bool outgoing = false;
    QDateTime time;
};

class DemoChat : public QObject {
    Q_OBJECT
public:
    explicit DemoChat(QObject *parent = nullptr);
    QStringList conversations() const;
    QString activeConversation() const { return m_active; }
    QVector<ChatMessage> messages() const { return m_messages.value(m_active); }
    void selectConversation(const QString &name);
    void sendText(const QString &text);
    void shareSong(const QString &path);
signals:
    void changed();
private:
    QString m_active = QStringLiteral("一起听 · 音乐客厅");
    QMap<QString, QVector<ChatMessage>> m_messages;
};
