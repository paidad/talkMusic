#include "todos/todowidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QTimer>
#include <QSettings>

TodoWidget::TodoWidget(TodoStore *store, QWidget *parent) : QWidget(parent), m_store(store) {
    setObjectName("todoWorkspace"); auto *layout = new QVBoxLayout(this); layout->setContentsMargins(24, 24, 24, 18); layout->setSpacing(14);
    m_title = new QLabel("把今天，安排得从容一点"); m_title->setProperty("role", "heading"); m_title->setWordWrap(true); m_title->setTextFormat(Qt::PlainText); layout->addWidget(m_title);
    m_count = new QLabel; m_count->setProperty("role", "muted"); layout->addWidget(m_count);
    auto *compose = new QHBoxLayout; m_add = new QLineEdit; m_add->setObjectName("newTaskTitle"); m_add->setPlaceholderText("添加一件想完成的事…"); m_add->setMaxLength(1000);
    auto *add = new QPushButton("添加"); add->setObjectName("addTask"); add->setProperty("accent", true); compose->addWidget(m_add, 1); compose->addWidget(add); layout->addLayout(compose);
    connect(m_add, &QLineEdit::returnPressed, this, &TodoWidget::add); connect(add, &QPushButton::clicked, this, &TodoWidget::add);
    auto *controls = new QHBoxLayout; m_completed = new QPushButton("显示已完成"); m_completed->setObjectName("showCompletedTasks"); m_completed->setCheckable(true);
    m_undo = new QPushButton("撤销删除"); m_undo->setObjectName("undoTaskRemoval"); controls->addWidget(m_completed); controls->addStretch(); controls->addWidget(m_undo); layout->addLayout(controls);
    connect(m_completed, &QPushButton::clicked, this, [this](bool checked) { m_showCompleted[m_active] = checked; refresh(); });
    connect(m_undo, &QPushButton::clicked, this, [this] { m_store->undoRemoval(m_active); });
    m_scroll = new QScrollArea; m_scroll->setObjectName("todoTasks"); m_scroll->setWidgetResizable(true); m_scroll->setFrameShape(QFrame::NoFrame); layout->addWidget(m_scroll, 1);
    connect(store, &TodoStore::changed, this, &TodoWidget::refresh); refresh();
}
void TodoWidget::select(const QString &id) {
    if (id == m_active) return;
    m_positions[m_active] = m_scroll->verticalScrollBar()->value();
    if (!m_positions.contains(id)) m_positions[id] = 0;
    m_active = m_store->find(id) ? id : QString(); refresh();
    QSettings().setValue("todos/lastList", m_active); emit activeChanged();
}
void TodoWidget::add() { if (m_store->addTask(m_active, m_add->text())) { m_add->clear(); m_add->setFocus(); } }
void TodoWidget::refresh() {
    const auto *list = m_store->find(m_active);
    if (!list && !m_active.isEmpty()) { m_active.clear(); emit activeChanged(); }
    m_title->setText(list ? list->title : "把今天，安排得从容一点"); m_add->setEnabled(list); m_completed->setEnabled(list); m_undo->setEnabled(list && m_store->canUndo(m_active));
    const int oldPosition = m_scroll->verticalScrollBar()->value();
    int completed = 0; if (list) for (const auto &task : list->tasks) if (task.completed) ++completed;
    m_count->setText(list ? QString("%1 / %2 已完成 · 任务按创建顺序排列").arg(completed).arg(list->tasks.size()) : "在右侧创建或选择一份待办清单。");
    m_completed->setChecked(m_showCompleted.value(m_active, false)); m_completed->setText(QString("显示已完成（%1）").arg(completed));
    auto *container = new QWidget; auto *layout = new QVBoxLayout(container); layout->setContentsMargins(0, 4, 0, 4); layout->setSpacing(10);
    int visible = 0;
    if (list) for (const auto &task : list->tasks) {
        if (task.completed && !m_completed->isChecked()) continue;
        ++visible; auto *row = new QFrame; row->setObjectName("taskCard"); auto *rowLayout = new QHBoxLayout(row); rowLayout->setContentsMargins(12, 12, 12, 12); rowLayout->setSpacing(10);
        auto *check = new QCheckBox; check->setObjectName("taskCheck"); check->setChecked(task.completed); check->setAccessibleName(task.title); rowLayout->addWidget(check, 0, Qt::AlignVCenter);
        auto *texts = new QVBoxLayout; auto *title = new QLabel(task.title); title->setTextFormat(Qt::PlainText); title->setWordWrap(true);
        if (task.completed) { auto font = title->font(); font.setStrikeOut(true); title->setFont(font); title->setProperty("role", "muted"); }
        texts->addWidget(title);
        if (!task.notes.isEmpty()) { auto *notes = new QLabel(task.notes); notes->setTextFormat(Qt::PlainText); notes->setWordWrap(true); notes->setProperty("role", "muted"); texts->addWidget(notes); }
        rowLayout->addLayout(texts, 1);
        auto *editButton = new QPushButton("编辑"); editButton->setObjectName("editTask"); auto *remove = new QPushButton("×"); remove->setObjectName("removeTask"); remove->setToolTip("删除任务，可撤销"); remove->setFixedWidth(32);
        rowLayout->addWidget(editButton, 0, Qt::AlignTop); rowLayout->addWidget(remove, 0, Qt::AlignTop);
        const auto listId = m_active; const auto taskId = task.id;
        connect(check, &QCheckBox::toggled, this, [this, listId, taskId](bool checked) { QTimer::singleShot(0, this, [this, listId, taskId, checked] { if (!m_store->completeTask(listId, taskId, checked)) refresh(); }); });
        connect(editButton, &QPushButton::clicked, this, [this, taskId] { edit(taskId); });
        connect(remove, &QPushButton::clicked, this, [this, listId, taskId] { QTimer::singleShot(0, this, [this, listId, taskId] { m_store->removeTask(listId, taskId); }); });
        layout->addWidget(row);
    }
    if (!visible) { auto *empty = new QLabel(list ? "这里暂时没有未完成的任务。\n添加第一项任务，或展开已完成列表。" : "一份清单，一个清晰的开始。"); empty->setAlignment(Qt::AlignCenter); empty->setWordWrap(true); empty->setProperty("role", "muted"); layout->addWidget(empty); }
    layout->addStretch(); delete m_scroll->takeWidget(); m_scroll->setWidget(container);
    const int position = m_positions.contains(m_active) ? m_positions.take(m_active) : oldPosition;
    QTimer::singleShot(0, this, [this, position] { m_scroll->verticalScrollBar()->setValue(position); });
}
void TodoWidget::edit(const QString &taskId) {
    const auto *list = m_store->find(m_active); if (!list) return;
    TodoTask task; bool found = false; for (const auto &candidate : list->tasks) if (candidate.id == taskId) { task = candidate; found = true; break; } if (!found) return;
    QDialog dialog(this); dialog.setWindowTitle("编辑任务"); dialog.resize(440, 300); auto *layout = new QVBoxLayout(&dialog);
    auto *title = new QLineEdit(task.title); title->setMaxLength(1000); auto *notes = new QPlainTextEdit(task.notes); notes->setPlaceholderText("备注（可选）"); layout->addWidget(title); layout->addWidget(notes, 1);
    auto *error = new QLabel; error->setWordWrap(true); error->setStyleSheet("color: #bf4658;"); layout->addWidget(error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel); layout->addWidget(buttons);
    connect(m_store, &TodoStore::error, &dialog, [error](const QString &message) { error->setText(message); });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [this, &dialog, title, notes, error, taskId] {
        if (title->text().trimmed().isEmpty()) { error->setText("任务标题不能为空。"); return; }
        if (m_store->editTask(m_active, taskId, title->text(), notes->toPlainText())) dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject); dialog.exec();
}
