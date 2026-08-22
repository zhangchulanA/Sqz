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
#include <QMargins>

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
                      int order = -1);

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
    explicit MenuButton(MenuNode* node, QWidget* parent = nullptr);
    MenuNode* node() const;

private:
    MenuNode* m_node;
};

// ============================ MenuEngine ============================
class MenuEngine : public QObject {
    Q_OBJECT
public:
    explicit MenuEngine(QObject* parent = nullptr);
    ~MenuEngine();

    void setRoot(MenuNode* root);
    void enter(MenuNode* node);
    void back();
    void reset();
    QList<MenuNode*> currentPath() const;
    QList<MenuNode*> getCurrentNodes() const;
    void triggerNode(MenuNode* node);

signals:
    void pathChanged(const QList<MenuNode*>& path);

private:
    void clearNode(MenuNode* node);
    MenuNode* m_root = nullptr;
    QList<MenuNode*> m_currentPath;
};

// ============================ MenuBar (手动布局) ============================
class MenuBar : public QWidget {
    Q_OBJECT
public:
    explicit MenuBar(QWidget* parent = nullptr);
    ~MenuBar();

    // ===== 引擎绑定 =====
    void setEngine(MenuEngine* engine);

    // ===== 布局配置 =====
    void setMaxCount(int count);                          // 每级最大按钮数
    void setBarHeight(int height);                        // 菜单栏高度
    void setButtonFixedWidth(int width);                  // 按钮固定宽度
    void setSpacing(int spacing);                         // 按钮间距
    void setMargins(int left, int top, int right, int bottom); // 边距

    // ===== 状态管理 =====
    void setNodeState(MenuNode* node, BtnState newState); // 设置节点状态（Off/On）

    // ===== 按钮查询 =====
    QList<MenuButton*> getButtons() const;                // 获取所有按钮
    MenuButton* getBtn(MenuNode* node) const;             // 根据节点获取按钮
    MenuButton* getBtn(const QString& text) const;        // 根据文本获取按钮

    // ===== 按钮操作 =====
    void setText(MenuButton* btn, const QString& newText); // 修改按钮文本（同步节点）
    void setText(MenuNode* node, const QString& newText);  // 通过节点修改文本
    void toggleState(MenuButton* btn);                     // 切换 Off ↔ On
    QString getStateStr(MenuButton* btn) const;            // 获取状态字符串
    BtnState getState(MenuButton* btn) const;              // 获取状态枚举

    // ===== 触发执行（带视觉反馈） =====
    void trigger(MenuNode* node);                          // 带视觉反馈触发节点

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onPathChanged(const QList<MenuNode*>& path);
    void onButtonClicked(MenuButton* btn);

private:
    // ----- 控件管理 -----
    void clearWidgets();
    MenuButton* createButton(MenuNode* node);
    void createMoreButton();

    // ----- 布局计算 -----
    void updateLayout();

    // ----- 样式更新 -----
    void updateAllStyles();
    void updateStyle(MenuButton* btn);
    void flash(MenuButton* btn);
    void simulateClick(MenuButton* btn);
    QString stateToString(BtnState state) const;

    // ----- 辅助 -----
    QList<MenuNode*> getCurrentNodes() const;
    void rebuild();

    // ----- 成员变量 -----
    MenuEngine* m_engine = nullptr;
    QList<QWidget*> m_widgets;          // 所有子控件（按钮或占位）
    MenuButton* m_backButton = nullptr;
    MenuButton* m_currentOn = nullptr;
    int m_maxCount = 6;
    int m_buttonFixedWidth = 100;
    int m_spacing = 0;
    QMargins m_margins;
};
