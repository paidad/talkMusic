#pragma once
#include <QWidget>
#include <QMap>
#include "notestore.h"
#include "markdownoutline.h"
class QPlainTextEdit;
class QTextBrowser;
class QLabel;
class QStackedWidget;
class QPushButton;
class QSplitter;
class WebNoteEditor;

class NoteEditor : public QWidget {
    Q_OBJECT
public:
    explicit NoteEditor(NoteStore *store, QWidget *parent = nullptr);
    ~NoteEditor() override;
    QString activePath() const;
    bool openFile(const QString &path);
    bool flushAll();
    void clearDocuments();
    void insertSong(const QString &title, const QString &artist, const QString &path);
    QVector<MarkdownHeading> headings() const;
    void jumpToHeading(int sourceLine);
    void setMode(int mode);
    int mode() const { return m_mode; }
    QPlainTextEdit *activeEditor() const;
    WebNoteEditor *activeWebEditor() const;
signals:
    void activeChanged();
    void outlineChanged();
    void error(const QString &message);
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    struct Page;
    void render(Page *page);
    void refresh();
    void resolveSaveAs();
    NoteStore *m_store;
    QMap<QString, Page *> m_pages;
    Page *m_active = nullptr;
    QStackedWidget *m_stack;
    QLabel *m_title, *m_status;
    QWidget *m_conflict;
    QPushButton *m_modes[3];
    int m_mode = 0;
    bool m_flushing = false;
};
