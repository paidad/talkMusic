#pragma once
#include <QWidget>
#include <QMap>
#include "notes/notestore.h"
#include "notes/markdownoutline.h"
class QPlainTextEdit;
class QTextBrowser;
class QLabel;
class QStackedWidget;
class QPushButton;
class QSplitter;
class WebNoteEditor;

// 笔记工作区：协调源码 / 即时预览、打开页面缓存和保存冲突提示。
class NoteEditor : public QWidget {
    Q_OBJECT
public:
    explicit NoteEditor(NoteStore *store, QWidget *parent = nullptr);
    ~NoteEditor() override;
    QString activePath() const;
    bool openFile(const QString &path);
    // 切文档、切模式及关闭前的同步屏障：网页快照与落盘都成功才返回 true。
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
private:
    struct Page;
    void render(Page *page);
    void refresh();
    void resolveSaveAs();
    NoteStore *m_store;
    // 按路径缓存页面以保留撤销、光标和滚动；Page 本身由本类显式释放。
    QMap<QString, Page *> m_pages;
    Page *m_active = nullptr;
    QStackedWidget *m_stack;
    QLabel *m_title, *m_status;
    QWidget *m_conflict;
    QPushButton *m_modes[2];
    QPushButton *m_help;
    QWidget *m_helpPopup = nullptr;
    // 0 为原生源码，1 为可编辑的即时预览；不再提供分屏模式。
    int m_mode = 0;
    // 嵌套事件循环等待网页时可能重入，防止重复发起整批保存。
    bool m_flushing = false;
};
