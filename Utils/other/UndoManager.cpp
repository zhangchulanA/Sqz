#include "UndoManager.h"
#include <QApplication>
#include <QStyle>
#include <QMessageBox>

// ==================== UndoManager 实现 ====================

UndoManager* UndoManager::instance() {
    static UndoManager manager;
    return &manager;
}

void UndoManager::push(QUndoCommand* cmd) {
    if (!cmd) return;
    m_stack.push(cmd);
    emit commandExecuted(cmd->text());
    updateActions();
}

void UndoManager::undo() {
    if (m_stack.canUndo()) {
        m_stack.undo();
        updateActions();
    }
}

void UndoManager::redo() {
    if (m_stack.canRedo()) {
        m_stack.redo();
        updateActions();
    }
}

void UndoManager::clear() {
    if (m_stack.count() > 0) {
        m_stack.clear();
        updateActions();
    }
}

void UndoManager::setClean() {
    m_stack.setClean();
}

void UndoManager::beginMacro(const QString& text) {
    if (m_macroActive) {
        qWarning() << "Already in a macro";
        return;
    }
    m_macroActive = true;
    m_stack.beginMacro(text);
}

void UndoManager::endMacro() {
    if (!m_macroActive) {
        qWarning() << "Not in a macro";
        return;
    }
    m_macroActive = false;
    m_stack.endMacro();
    updateActions();
}

void UndoManager::setupMenu(QMenu* menu) {
    if (!menu) return;

    m_undoAction = m_stack.createUndoAction(this);
    m_undoAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowBack));
    m_undoAction->setShortcut(QKeySequence::Undo);

    m_redoAction = m_stack.createRedoAction(this);
    m_redoAction->setIcon(QApplication::style()->standardIcon(QStyle::SP_ArrowForward));
    m_redoAction->setShortcut(QKeySequence::Redo);

    m_clearAction = new QAction(tr("清空历史"), this);
    connect(m_clearAction, &QAction::triggered, this, [this]() {
        if (m_stack.count() == 0) return;
        if (QMessageBox::question(nullptr, "确认", "清空所有历史？")
            == QMessageBox::Yes) {
            clear();
        }
    });

    menu->addAction(m_undoAction);
    menu->addAction(m_redoAction);
    menu->addSeparator();
    menu->addAction(m_clearAction);

    connect(&m_stack, &QUndoStack::cleanChanged, this, &UndoManager::cleanChanged);
    connect(&m_stack, &QUndoStack::canUndoChanged, this, &UndoManager::canUndoChanged);
    connect(&m_stack, &QUndoStack::canRedoChanged, this, &UndoManager::canRedoChanged);
    connect(&m_stack, &QUndoStack::indexChanged, this, &UndoManager::onStackChanged);

    updateActions();
}

void UndoManager::setupToolBar(QToolBar* toolbar) {
    if (!toolbar) return;
    if (!m_undoAction) {
        QMenu tempMenu;
        setupMenu(&tempMenu);
    }
    if (m_undoAction) toolbar->addAction(m_undoAction);
    if (m_redoAction) toolbar->addAction(m_redoAction);
}

void UndoManager::setUndoLimit(int limit) {
    m_stack.setUndoLimit(limit);
}

QUndoView* UndoManager::createUndoView(QWidget* parent) {
    return new QUndoView(&m_stack, parent);
}

void UndoManager::onStackChanged() {
    updateActions();
    emit indexChanged(m_stack.index());
    emit countChanged(m_stack.count());
}

void UndoManager::updateActions() {
    if (m_undoAction) {
        QString text = m_stack.undoText();
        m_undoAction->setText(text.isEmpty() ? tr("撤销") : tr("撤销: %1").arg(text));
        m_undoAction->setEnabled(m_stack.canUndo());
    }
    if (m_redoAction) {
        QString text = m_stack.redoText();
        m_redoAction->setText(text.isEmpty() ? tr("重做") : tr("重做: %1").arg(text));
        m_redoAction->setEnabled(m_stack.canRedo());
    }
    if (m_clearAction) {
        m_clearAction->setEnabled(m_stack.count() > 0);
    }
}
