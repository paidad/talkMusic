#pragma once
#include <QWidget>
#include <QMap>
#include "todos/todostore.h"
class QLabel;
class QLineEdit;
class QScrollArea;
class QPushButton;
// 待办工作区只处理展示和输入，数据修改与持久化统一经过 TodoStore。
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
    // 每份清单分别记住本次运行中的滚动位置和“显示已完成”状态。
    QMap<QString, int> m_positions;
    QMap<QString, bool> m_showCompleted;
};
