#pragma once
#include <QWidget>
#include <QVariantMap>
class QWebEngineView;
class NoteStore;

// Each open document owns a browser page, including its selection and undo stack.
class WebNoteEditor : public QWidget {
    Q_OBJECT
public:
    WebNoteEditor(NoteStore *store, const QString &path, const QString &text, QWidget *parent = nullptr);
    void setDocument(const QString &path, const QString &text, bool resetUndo = false);
    void setReadOnly(bool readOnly);
    bool synchronize();
    bool ready() const { return m_ready; }
    QString text() const { return m_text; }
    void insertMarkdown(const QString &text);
    void jumpToHeading(int ordinal);
    QWebEngineView *view() const { return m_view; }
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
    int m_generation = 1, m_revision = 0;
    bool m_ready = false, m_readOnly = false, m_syncing = false, m_failed = false;
};
