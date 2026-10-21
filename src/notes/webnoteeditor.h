#pragma once
#include <QWidget>
#include <QVariantMap>
class QWebEngineView;
class NoteStore;

// 每篇已打开文档持有独立网页页面，保留选区和撤销栈；文件保存仍由 NoteDocument 负责。
class WebNoteEditor : public QWidget {
    Q_OBJECT
public:
    WebNoteEditor(NoteStore *store, const QString &path, const QString &text, QWidget *parent = nullptr);
    void setDocument(const QString &path, const QString &text, bool resetUndo = false);
    void setReadOnly(bool readOnly);
    // 有界等待最新网页快照；输入法组合中、超时或页面失败时返回 false。
    bool synchronize();
    bool ready() const { return m_ready; }
    QString text() const { return m_text; }
    void insertMarkdown(const QString &text);
    void jumpToHeading(int ordinal);
    QWebEngineView *view() const { return m_view; }
    // 供窄桥接对象转发的协议入口，页面不得直接获得整个 QWidget 的能力。
    Q_INVOKABLE QVariantMap configuration() const;
    Q_INVOKABLE void loaded(int generation);
    Q_INVOKABLE void changed(int generation, int revision, const QString &text);
    Q_INVOKABLE void saveRequested();
    Q_INVOKABLE void chooseImage();
    Q_INVOKABLE void clipboardWrite(const QString &text);
    Q_INVOKABLE QString clipboardRead() const;
    void openLink(const QString &url);
signals:
    void textChanged(const QString &text);
    void saveRequest();
    void error(const QString &message);
    void becameReady();
private:
    void load(bool resetUndo);
    QWebEngineView *m_view;
    NoteStore *m_store;
    QString m_path, m_text;
    // generation 区分内容加载，revision 区分同次加载的编辑，拒绝迟到和重复消息。
    int m_generation = 1, m_revision = 0;
    bool m_ready = false, m_readOnly = false, m_syncing = false, m_failed = false;
};
