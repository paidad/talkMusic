#pragma once
#include <QWidget>
#include <QMap>
#include "todostore.h"
class QLabel;
class QLineEdit;
class QScrollArea;
class QPushButton;
class TodoWidget : public QWidget {
    Q_OBJECT
public:
    explicit TodoWidget(TodoStore *store, QWidget *parent = nullptr);
    void select(const QString &id);
    QString activeId() const { return m_active; }
    void refresh();
signals:
    void activeChanged();
private:
    void add();
    void edit(const QString &taskId);
    TodoStore *m_store;
    QString m_active;
    QLabel *m_title, *m_count;
    QLineEdit *m_add;
    QScrollArea *m_scroll;
    QPushButton *m_completed, *m_undo;
    QMap<QString, int> m_positions;
    QMap<QString, bool> m_showCompleted;
};
