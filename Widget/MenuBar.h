// MenuBar.h - 底部多级菜单栏组件 (Qt 5.12)
// 手动几何布局：无布局管理器，间距精确控制
// 样式只影响视觉，不影响间距

#pragma once

#include <QWidget>
#include <QPushButton>
#include <QList>
#include <QTimer>
#include <QPointer>
#include <QStyle>
#include <functional>
#include <QDebug>
#include <QMap>

// ============================ 枚举 ============================
enum class BtnState {
    Off,      // 普通（暗）
    On,       // 常亮（亮）
    Blink     // 瞬时闪烁
};

// ============================ MenuNode ============================
class MenuNode {
public:
    explicit MenuNode(const QString& text,
                      BtnState state = BtnState::Off,
                      std::function<void()> callback = nullptr,
                      const QList<MenuNode*>& children = {},
                      bool exclusive = true,
                      int order = -1)
        : text(text), state(state), callback(callback), children(children),
          exclusive(exclusive), order(order) {}

    QString text;
    BtnState state;
    std::function<void()> callback;
    QList<MenuNode*> children;
    bool exclusive;
    int order;
};

// ============================ MenuButton ============================
class MenuButton : public QPushButton {
    Q_OBJECT
public:
    explicit MenuButton(MenuNode* node, QWidget* parent = nullptr)
        : QPushButton(parent), m_node(node) {
        setText(node->text);
        // 由父布局控制大小，此处不设置策略
    }

    MenuNode* node() const { return m_node; }

private:
    MenuNode* m_node;
};

// ============================ MenuEngine ============================
class MenuEngine : public QObject {
    Q_OBJECT
public:
    explicit MenuEngine(QObject* parent = nullptr) : QObject(parent) {}
    ~MenuEngine() { clearNode(m_root); }

    void setRoot(MenuNode* root) { m_root = root; reset(); }

    void enter(MenuNode* node) {
        if (!node) return;
        if (!node->children.isEmpty()) {
            m_currentPath.append(node);
            emit pathChanged(m_currentPath);
        } else {
            if (node->callback) node->callback();
        }
    }

    void back() {
        if (m_currentPath.size() <= 1) return;
        m_currentPath.removeLast();
        emit pathChanged(m_currentPath);
    }

    void reset() {
        m_currentPath.clear();
        if (m_root) m_currentPath.append(m_root);
        emit pathChanged(m_currentPath);
    }

    QList<MenuNode*> currentPath() const { return m_currentPath; }
    QList<MenuNode*> getCurrentNodes() const {
        if (m_currentPath.isEmpty()) return {};
        return m_currentPath.last()->children;
    }

    void triggerNode(MenuNode* node) {
        if (node && node->callback) node->callback();
    }

signals:
    void pathChanged(const QList<MenuNode*>& path);

private:
    void clearNode(MenuNode* node) {
        if (!node) return;
        for (auto* child : node->children) clearNode(child);
        delete node;
    }

    MenuNode* m_root = nullptr;
    QList<MenuNode*> m_currentPath;
};

// ============================ MenuBar (手动布局) ============================
class MenuBar : public QWidget {
    Q_OBJECT
public:
    explicit MenuBar(QWidget* parent = nullptr) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMinimumHeight(50);
        setMaximumHeight(80);
        m_buttonFixedWidth = 100;
        m_spacing = 0;
        m_margins = QMargins(10, 5, 10, 5);
    }

    ~MenuBar() { clearWidgets(); }

    void setEngine(MenuEngine* engine) {
        if (m_engine) disconnect(m_engine, &MenuEngine::pathChanged, this, &MenuBar::onPathChanged);
        m_engine = engine;
        connect(m_engine, &MenuEngine::pathChanged, this, &MenuBar::onPathChanged);
        onPathChanged(m_engine->currentPath());
    }

    void setMaxCount(int count) { m_maxCount = count; rebuild(); }
    void setBarHeight(int height) { setFixedHeight(height); updateLayout(); }
    void setButtonFixedWidth(int width) { m_buttonFixedWidth = qMax(30, width); updateLayout(); }
    void setSpacing(int spacing) { m_spacing = qMax(0, spacing); updateLayout(); }
    void setMargins(int left, int top, int right, int bottom) {
        m_margins = QMargins(left, top, right, bottom);
        updateLayout();
    }

    // 单独设置节点状态
    void setNodeState(MenuNode* node, BtnState newState) {
        if (!node || node->state == newState) return;

        if (newState == BtnState::On) {
            if (m_currentOn && m_currentOn->node() != node) {
                m_currentOn->node()->state = BtnState::Off;
                updateButtonStyle(m_currentOn);
                m_currentOn = nullptr;
            }
        }

        node->state = newState;

        for (auto* w : m_widgets) {
            MenuButton* btn = qobject_cast<MenuButton*>(w);
            if (btn && btn->node() == node) {
                updateButtonStyle(btn);
                if (newState == BtnState::On) m_currentOn = btn;
                else if (newState == BtnState::Off && m_currentOn == btn) m_currentOn = nullptr;
                break;
            }
        }
    }

    QList<MenuButton*> getButtons() const {
        QList<MenuButton*> result;
        for (auto* w : m_widgets) {
            MenuButton* btn = qobject_cast<MenuButton*>(w);
            if (btn) result.append(btn);
        }
        return result;
    }

protected:
    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        updateLayout();
    }

private slots:
    void onPathChanged(const QList<MenuNode*>& path) {
        clearWidgets();
        if (path.isEmpty()) return;

        MenuNode* current = path.last();
        QList<MenuNode*> items = current->children;

        // 互斥处理
        bool exclusiveEnabled = true;
        for (auto* node : items) if (!node->exclusive) { exclusiveEnabled = false; break; }
        if (exclusiveEnabled) {
            bool hasOn = false;
            for (auto* node : items) {
                if (node->state == BtnState::On) {
                    if (!hasOn) hasOn = true;
                    else node->state = BtnState::Off;
                }
            }
        }

        // 按 order 排序并生成顺序列表（含占位）
        QList<MenuNode*> orderedNodes;
        QMap<int, MenuNode*> orderMap;
        QList<MenuNode*> tailNodes;
        bool hasOrder = false;
        for (auto* node : items) {
            if (node->order >= 0) {
                orderMap.insert(node->order, node);
                hasOrder = true;
            } else {
                tailNodes.append(node);
            }
        }
        if (hasOrder) {
            int maxOrder = orderMap.isEmpty() ? -1 : orderMap.lastKey();
            for (int i = 0; i <= maxOrder; ++i) {
                orderedNodes.append(orderMap.value(i, nullptr));
            }
        }
        orderedNodes.append(tailNodes);

        // 创建普通控件（按钮或占位）
        int maxDisplay = m_maxCount - (path.size() > 1 ? 1 : 0);
        int created = 0;
        bool moreAdded = false;

        for (auto* node : orderedNodes) {
            if (created >= maxDisplay) {
                if (!moreAdded) {
                    createMoreButton();
                    moreAdded = true;
                }
                break;
            }
            if (node) {
                MenuButton* btn = createButton(node);
                m_widgets.append(btn);
                if (node->state == BtnState::On) m_currentOn = btn;
            } else {
                QWidget* placeholder = new QWidget(this);
                placeholder->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
                placeholder->setStyleSheet("background: transparent;");
                placeholder->show();
                m_widgets.append(placeholder);
            }
            created++;
        }

        // 返回按钮（固定在最后）
        if (path.size() > 1) {
            static MenuNode backNode("← 返回", BtnState::Blink, nullptr, {}, false, -1);
            MenuButton* backBtn = createButton(&backNode);
            backBtn->setProperty("isBack", true);
            connect(backBtn, &MenuButton::clicked, this, [=]() {
                flashButton(backBtn);
                if (m_engine) m_engine->back();
            });
            m_widgets.append(backBtn);
            m_backButton = backBtn;
        }

        // 更新所有样式
        updateAllStyles();
        // 计算并设置几何位置
        updateLayout();
    }

    void onButtonClicked(MenuButton* btn) {
        MenuNode* node = btn->node();
        if (!node || btn == m_backButton) return;

        if (!node->children.isEmpty()) {
            if (m_engine) m_engine->enter(node);
            return;
        }

        if (node->state == BtnState::Blink) {
            flashButton(btn);
            if (node->callback) node->callback();
            return;
        }

        bool exclusiveEnabled = true;
        QList<MenuNode*> items = getCurrentNodes();
        for (auto* n : items) if (!n->exclusive) { exclusiveEnabled = false; break; }

        if (exclusiveEnabled) {
            if (node->state == BtnState::Off) {
                if (m_currentOn) {
                    m_currentOn->node()->state = BtnState::Off;
                    updateButtonStyle(m_currentOn);
                    m_currentOn = nullptr;
                }
                node->state = BtnState::On;
                m_currentOn = btn;
                updateButtonStyle(btn);
            } else if (node->state == BtnState::On) {
                node->state = BtnState::Off;
                m_currentOn = nullptr;
                updateButtonStyle(btn);
            }
        } else {
            node->state = (node->state == BtnState::Off) ? BtnState::On : BtnState::Off;
            updateButtonStyle(btn);
        }
        if (node->callback) node->callback();
    }

private:
    // ----- 控件管理 -----
    void clearWidgets() {
        for (auto* w : m_widgets) {
            w->hide();
            w->deleteLater();
        }
        m_widgets.clear();
        m_backButton = nullptr;
        m_currentOn = nullptr;
    }

    MenuButton* createButton(MenuNode* node) {
        auto* btn = new MenuButton(node, this);
        btn->setProperty("state", stateToString(node->state));
        connect(btn, &MenuButton::clicked, this, [=]() { onButtonClicked(btn); });
        btn->show();
        return btn;
    }

    void createMoreButton() {
        auto* moreBtn = new QPushButton("更多...", this);
        moreBtn->setProperty("type", "more");
        connect(moreBtn, &QPushButton::clicked, this, [=]() {
            qDebug() << "更多按钮点击，请实现弹出菜单或进入下一级";
        });
        moreBtn->show();
        m_widgets.append(moreBtn);
    }

    // ----- 几何布局计算 -----
    void updateLayout() {
        if (m_widgets.isEmpty()) return;

        int totalWidth = width();
        int totalHeight = height();

        // 可用区域（除去边距）
        int left = m_margins.left();
        int top = m_margins.top();
        int right = m_margins.right();
        int bottom = m_margins.bottom();
        int availWidth = totalWidth - left - right;
        int availHeight = totalHeight - top - bottom;

        // 计算每个控件的宽度（固定宽度，但最后一个可能自适应？这里统一固定）
        int btnWidth = m_buttonFixedWidth;
        int spacing = m_spacing;
        int count = m_widgets.size();

        // 如果所有控件总宽度超过可用宽度，需要按比例缩小？这里简单截断或保持固定，用户需要保证宽度足够
        // 也可以不做限制，超出则部分不可见（但可通过scroll解决，暂不考虑）
        // 这里我们让控件宽度固定，如果超出就超出（用户需调整窗口或按钮宽度）

        int x = left;
        int y = top + (availHeight - btnWidth) / 2; // 垂直居中（假设按钮高度等于宽度？实际高度应固定）
        int btnHeight = availHeight; // 填满可用高度

        // 但为了美观，高度可以设为可用高度，宽度固定
        // 遍历所有控件
        for (auto* w : m_widgets) {
            if (!w) continue;
            // 如果是返回按钮，我们将其放在最后，但已经在列表最后，所以顺序OK
            w->setGeometry(x, top, btnWidth, availHeight);
            x += btnWidth + spacing;
        }
    }

    // ----- 样式更新 -----
    void updateAllStyles() {
        for (auto* w : m_widgets) {
            MenuButton* btn = qobject_cast<MenuButton*>(w);
            if (btn) updateButtonStyle(btn);
        }
    }

    void updateButtonStyle(MenuButton* btn) {
        if (!btn) return;
        MenuNode* node = btn->node();
        if (!node) return;
        btn->setProperty("state", stateToString(node->state));
        btn->style()->polish(btn);
    }

    void flashButton(MenuButton* btn) {
        if (!btn) return;
        btn->setProperty("state", "blink");
        btn->style()->polish(btn);
        QPointer<MenuButton> safeBtn(btn);
        QTimer::singleShot(200, this, [=]() {
            if (!safeBtn.isNull()) {
                updateButtonStyle(safeBtn);
            }
        });
    }

    QString stateToString(BtnState state) {
        switch (state) {
            case BtnState::Off:   return "off";
            case BtnState::On:    return "on";
            case BtnState::Blink: return "blink";
            default: return "off";
        }
    }

    QList<MenuNode*> getCurrentNodes() const {
        if (!m_engine) return {};
        auto path = m_engine->currentPath();
        if (path.isEmpty()) return {};
        return path.last()->children;
    }

    void rebuild() {
        if (m_engine) onPathChanged(m_engine->currentPath());
    }

private:
    MenuEngine* m_engine = nullptr;
    QList<QWidget*> m_widgets;      // 所有子控件（按钮或占位）
    MenuButton* m_backButton = nullptr;
    MenuButton* m_currentOn = nullptr;
    int m_maxCount = 6;
    int m_buttonFixedWidth = 100;
    int m_spacing = 0;
    QMargins m_margins;
};
