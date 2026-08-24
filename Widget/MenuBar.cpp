
// MenuBar.cpp - 底部多级菜单栏组件实现

#include "MenuBar.h"

// ============================ MenuNode ============================
MenuNode::MenuNode(const QString& text,
                   BtnState state,
                   std::function<void()> callback,
                   const QList<MenuNode*>& children,
                   int order,
                   bool isBack,
                   bool exclusive)
    : text(text), state(state), callback(callback), children(children),
      order(order), isBack(isBack), exclusive(exclusive) {

    if(isBack == true){
        state = BtnState::Bk;
    }
}

// ============================ MenuButton ============================
MenuButton::MenuButton(MenuNode* node, QWidget* parent)
    : QPushButton(parent), m_node(node) {
    setText(node->text);
}

MenuNode* MenuButton::node() const {
    return m_node;
}

// ============================ MenuEngine ============================
MenuEngine::MenuEngine(QObject* parent) : QObject(parent) {}

MenuEngine::~MenuEngine() {
    clearNode(m_root);
}

void MenuEngine::setRoot(MenuNode* root) {
    m_root = root;
    reset();
}

void MenuEngine::enter(MenuNode* node) {
    if (!node) return;
    if (!node->children.isEmpty()) {
        m_currentPath.append(node);
        emit pathChanged(m_currentPath);
    } else {
        if (node->callback) node->callback();
    }
}

void MenuEngine::back() {
    if (m_currentPath.size() <= 1) return;
    m_currentPath.removeLast();
    emit pathChanged(m_currentPath);
}

void MenuEngine::reset() {
    m_currentPath.clear();
    if (m_root) m_currentPath.append(m_root);
    emit pathChanged(m_currentPath);
}

QList<MenuNode*> MenuEngine::currentPath() const {
    return m_currentPath;
}

QList<MenuNode*> MenuEngine::getCurrentNodes() const {
    if (m_currentPath.isEmpty()) return {};
    return m_currentPath.last()->children;
}

void MenuEngine::triggerNode(MenuNode* node) {
    if (node && node->callback) node->callback();
}

void MenuEngine::clearNode(MenuNode* node) {
    if (!node) return;
    for (auto* child : node->children) clearNode(child);
    delete node;
}

// ============================ MenuBar ============================
MenuBar::MenuBar(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(50);
    setMaximumHeight(80);
    m_buttonFixedWidth = 100;
    m_spacing = 0;
    m_margins = QMargins(10, 5, 10, 5);
}

MenuBar::~MenuBar() {
    clearWidgets();
}

// ===== 引擎绑定 =====
void MenuBar::setEngine(MenuEngine* engine) {
    if (m_engine) {
        disconnect(m_engine, &MenuEngine::pathChanged, this, &MenuBar::onPathChanged);
    }
    m_engine = engine;
    connect(m_engine, &MenuEngine::pathChanged, this, &MenuBar::onPathChanged);
    onPathChanged(m_engine->currentPath());
}

// ===== 布局配置 =====
void MenuBar::setMaxCount(int count) {
    m_maxCount = count;
    rebuild();
}

void MenuBar::setBarHeight(int height) {
    setFixedHeight(height);
    updateLayout();
}

void MenuBar::setButtonFixedWidth(int width) {
    m_buttonFixedWidth = qMax(30, width);
    updateLayout();
}

void MenuBar::setSpacing(int spacing) {
    m_spacing = qMax(0, spacing);
    updateLayout();
}

void MenuBar::setMargins(int left, int top, int right, int bottom) {
    m_margins = QMargins(left, top, right, bottom);
    updateLayout();
}

// ===== 状态管理 =====
void MenuBar::setNodeState(MenuNode* node, BtnState newState) {
    if (!node || node->state == newState) return;

    if (newState == BtnState::On) {
        if (m_currentOn && m_currentOn->node() != node) {
            m_currentOn->node()->state = BtnState::Of;
            updateStyle(m_currentOn);
            m_currentOn = nullptr;
        }
    }

    node->state = newState;

    for (auto* w : m_widgets) {
        MenuButton* btn = qobject_cast<MenuButton*>(w);
        if (btn && btn->node() == node) {
            updateStyle(btn);
            if (newState == BtnState::On) m_currentOn = btn;
            else if (newState == BtnState::Of && m_currentOn == btn) m_currentOn = nullptr;
            break;
        }
    }
}

// ===== 按钮查询 =====
QList<MenuButton*> MenuBar::getButtons() const {
    QList<MenuButton*> result;
    for (auto* w : m_widgets) {
        MenuButton* btn = qobject_cast<MenuButton*>(w);
        if (btn) result.append(btn);
    }
    return result;
}

MenuButton* MenuBar::getBtn(MenuNode* node) const {
    if (!node) return nullptr;
    for (auto* w : m_widgets) {
        MenuButton* btn = qobject_cast<MenuButton*>(w);
        if (btn && btn->node() == node) return btn;
    }
    return nullptr;
}

MenuButton* MenuBar::getBtn(const QString& text) const {
    for (auto* w : m_widgets) {
        MenuButton* btn = qobject_cast<MenuButton*>(w);
        if (btn && btn->text() == text) return btn;
    }
    return nullptr;
}

// ===== 按钮操作 =====
void MenuBar::setText(MenuButton* btn, const QString& newText) {
    if (!btn) return;
    btn->setText(newText);
    MenuNode* node = btn->node();
    if (node) node->text = newText;
}

void MenuBar::setText(MenuNode* node, const QString& newText) {
    if (!node) return;
    node->text = newText;
    MenuButton* btn = getBtn(node);
    if (btn) btn->setText(newText);
}

void MenuBar::toggleState(MenuButton* btn) {
    if (!btn) return;
    MenuNode* node = btn->node();
    if (!node) return;
    if (node->state == BtnState::Bk) {
        setNodeState(node, BtnState::On);
        return;
    }
    BtnState newState = (node->state == BtnState::Of) ? BtnState::On : BtnState::Of;
    setNodeState(node, newState);
}

QString MenuBar::getStateStr(MenuButton* btn) const {
    if (!btn) return "unknown";
    MenuNode* node = btn->node();
    if (!node) return "unknown";
    return stateToString(node->state);
}

BtnState MenuBar::getState(MenuButton* btn) const {
    if (!btn) return BtnState::Of;
    MenuNode* node = btn->node();
    if (!node) return BtnState::Of;
    return node->state;
}

// ===== 触发执行（带视觉反馈） =====
void MenuBar::trigger(MenuNode* node) {
    if (!node) return;

    MenuButton* targetBtn = getBtn(node);
    if (targetBtn) {
        if (node->state == BtnState::Bk) {
            flash(targetBtn);
            if (node->callback) node->callback();
        } else {
            simulateClick(targetBtn);
        }
    } else {
        if (node->callback) node->callback();
    }
}

// ===== 事件 =====
void MenuBar::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateLayout();
}

// ===== 槽函数 =====
void MenuBar::onPathChanged(const QList<MenuNode*>& path) {
    clearWidgets();
    if (path.isEmpty()) return;

    MenuNode* current = path.last();
    QList<MenuNode*> items = current->children;

    // 互斥处理
    bool exclusiveEnabled = true;
    for (auto* node : items) {
        if (!node->exclusive) { exclusiveEnabled = false; break; }
    }
    if (exclusiveEnabled) {
        bool hasOn = false;
        for (auto* node : items) {
            if (node->state == BtnState::On) {
                if (!hasOn) hasOn = true;
                else node->state = BtnState::Of;
            }
        }
    }

    // 按 order 排序
    QList<MenuNode*> orderedNodes;
    QMap<int, MenuNode*> orderMap;
    QList<MenuNode*> noOrderNodes;
    bool hasOrder = false;
    //分离有order和没order的节点
    for (auto* node : items) {
        if (node->order >= 0 ) {
            orderMap.insert(node->order, node);
            hasOrder = true;
        } else {
            noOrderNodes.append(node);
        }
    }

    if (hasOrder) {
        int maxOrder = orderMap.isEmpty() ? -1 : orderMap.lastKey();
        int noOrderIndex = 0;
        for (int i = 0; i <= maxOrder; ++i) {
            if(orderMap.contains(i))
                orderedNodes.append(orderMap.value(i));
            else{
                if(noOrderIndex < noOrderNodes.size()){
                    orderedNodes.append(noOrderNodes[noOrderIndex]);
                    noOrderIndex++;
                }else{
                    orderedNodes.append(nullptr);
                }
            }
        }
        while (noOrderIndex < noOrderNodes.size()) {
            orderedNodes.append(noOrderNodes[noOrderIndex]);
            noOrderIndex++;
        }
    }else{
        orderedNodes = noOrderNodes;
    }


    // 创建控件（最多 m_maxCount 个）
    int maxDisplay = m_maxCount;
    int created = 0;
    int totalWidgets = 0;
    for (auto* node : orderedNodes) {
        if (totalWidgets >= maxDisplay) break;
        if (node) {
            MenuButton* btn = createButton(node);
            m_widgets.append(btn);
            if (node->state == BtnState::On) m_currentOn = btn;
            created++;
        } else {
            QWidget* placeholder = new QWidget(this);
            placeholder->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
            placeholder->setStyleSheet("background: transparent;");
            placeholder->show();
            m_widgets.append(placeholder);
        }
        totalWidgets++;
    }

    updateAllStyles();
    updateLayout();
}

void MenuBar::onButtonClicked(MenuButton* btn) {
    MenuNode* node = btn->node();
    if (!node) return;

    // 返回按钮：自动返回上一级，并执行用户回调
    if (node->isBack) {
        if (node->state == BtnState::Bk) {
            flash(btn);
        }
        if (m_engine) m_engine->back();
        if (node->callback) node->callback();
        return;
    }

    // 有子节点：进入下一级
    if (!node->children.isEmpty()) {
        if (m_engine) m_engine->enter(node);
        return;
    }

    // 叶子节点：切换状态并执行回调
    if (node->state == BtnState::Bk) {
        flash(btn);
        if (node->callback) node->callback();
        return;
    }

    bool exclusiveEnabled = true;
    QList<MenuNode*> items = getCurrentNodes();
    for (auto* n : items) {
        if (!n->exclusive) { exclusiveEnabled = false; break; }
    }

    if (exclusiveEnabled) {
        if (node->state == BtnState::Of) {
            if (m_currentOn) {
                m_currentOn->node()->state = BtnState::Of;
                updateStyle(m_currentOn);
                m_currentOn = nullptr;
            }
            node->state = BtnState::On;
            m_currentOn = btn;
            updateStyle(btn);
        } else if (node->state == BtnState::On) {
            node->state = BtnState::Of;
            m_currentOn = nullptr;
            updateStyle(btn);
        }
    } else {
        node->state = (node->state == BtnState::Of) ? BtnState::On : BtnState::Of;
        updateStyle(btn);
    }
    if (node->callback) node->callback();
}

// ===== 控件管理 =====
void MenuBar::clearWidgets() {
    for (auto* w : m_widgets) {
        w->hide();
        w->deleteLater();
    }
    m_widgets.clear();
    m_currentOn = nullptr;
}

MenuButton* MenuBar::createButton(MenuNode* node) {
    auto* btn = new MenuButton(node, this);
    btn->setProperty("state", stateToString(node->state));
    if (node->isBack) {
        btn->setProperty("isBack", true);
    }
    connect(btn, &MenuButton::clicked, this, [=]() { onButtonClicked(btn); });
    btn->show();
    return btn;
}

// ===== 布局计算 =====
void MenuBar::updateLayout() {
    if (m_widgets.isEmpty()) return;

    int left = m_margins.left();
    int top = m_margins.top();
    int right = m_margins.right();
    int bottom = m_margins.bottom();
    int availWidth = width() - left - right;
    int availHeight = height() - top - bottom;

    int btnWidth = m_buttonFixedWidth;
    int spacing = m_spacing;

    int x = left;
    for (auto* w : m_widgets) {
        if (!w) continue;
        w->setGeometry(x, top, btnWidth, availHeight);
        x += btnWidth + spacing;
    }
}

// ===== 样式更新 =====
void MenuBar::updateAllStyles() {
    for (auto* w : m_widgets) {
        MenuButton* btn = qobject_cast<MenuButton*>(w);
        if (btn) updateStyle(btn);
    }
}

void MenuBar::updateStyle(MenuButton* btn) {
    if (!btn) return;
    MenuNode* node = btn->node();
    if (!node) return;
    btn->setProperty("state", stateToString(node->state));
    btn->style()->polish(btn);
}

void MenuBar::flash(MenuButton* btn) {
    if (!btn) return;
    btn->setProperty("state", "Bk");
    btn->style()->polish(btn);
    QPointer<MenuButton> safeBtn(btn);
    QTimer::singleShot(200, this, [=]() {
        if (!safeBtn.isNull()) {
            updateStyle(safeBtn);
        }
    });
}

void MenuBar::simulateClick(MenuButton* btn) {
    MenuNode* node = btn->node();
    if (!node) return;

    bool exclusiveEnabled = true;
    QList<MenuNode*> items = getCurrentNodes();
    for (auto* n : items) {
        if (!n->exclusive) { exclusiveEnabled = false; break; }
    }

    if (exclusiveEnabled) {
        if (node->state == BtnState::Of) {
            if (m_currentOn) {
                m_currentOn->node()->state = BtnState::Of;
                updateStyle(m_currentOn);
                m_currentOn = nullptr;
            }
            node->state = BtnState::On;
            m_currentOn = btn;
            updateStyle(btn);
        } else if (node->state == BtnState::On) {
            node->state = BtnState::Of;
            m_currentOn = nullptr;
            updateStyle(btn);
        }
    } else {
        node->state = (node->state == BtnState::Of) ? BtnState::On : BtnState::Of;
        updateStyle(btn);
    }

    if (node->callback) node->callback();
}

QString MenuBar::stateToString(BtnState state) const {
    switch (state) {
    case BtnState::Of:   return "Of";
    case BtnState::On:   return "On";
    case BtnState::Bk:   return "Bk";
    default: return "Of";
    }
}

// ===== 辅助 =====
QList<MenuNode*> MenuBar::getCurrentNodes() const {
    if (!m_engine) return {};
    auto path = m_engine->currentPath();
    if (path.isEmpty()) return {};
    return path.last()->children;
}

void MenuBar::rebuild() {
    if (m_engine) onPathChanged(m_engine->currentPath());
}
