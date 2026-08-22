// main.cpp - 底部菜单栏完整使用示例 (手动布局版本)
#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QStatusBar>
#include <QTextEdit>
#include <QGroupBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QSlider>
#include <QDateTime>
#include <QDebug>
#include "MenuBar.h"

// ========== 模拟业务函数 ==========
inline void radioStart() {
    qDebug() << "📻 电台启动";
}

inline void radioStop() {
    qDebug() << "📻 电台关闭";
}

inline void radioSetFreq() {
    static double freq = 88.5;
    freq += 0.5;
    if (freq > 108.0) freq = 88.0;
    qDebug() << "📻 频率设为" << freq << "MHz";
}

inline void reportShow() {
    qDebug() << "📄 显示战场报告";
}

inline void networkConfig() {
    qDebug() << "🌐 打开网络配置";
}

inline void logShow() {
    qDebug() << "📋 查看系统日志";
}

inline void cryptoEnable() {
    qDebug() << "🔐 加密已启用";
}

inline void cryptoDisable() {
    qDebug() << "🔓 加密已禁用";
}

inline void gpsStatus() {
    qDebug() << "🛰️ GPS信号正常，定位成功";
}

inline void systemInfo() {
    qDebug() << "💻 系统信息: CPU 80%, 内存 4.2GB/8GB";
}

inline void powerSave() {
    qDebug() << "⚡ 省电模式已开启";
}

inline void highPerformance() {
    qDebug() << "🚀 高性能模式已开启";
}

// ========== 主窗口 ==========
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("底部菜单栏 - 手动布局示例");
        resize(1100, 750);

        // ===== 1. 构建菜单树 =====
        buildMenuTree();

        // ===== 2. 创建引擎 =====
        m_engine = new MenuEngine(this);
        m_engine->setRoot(m_root);

        // ===== 3. 创建菜单栏并配置 =====
        m_bar = new MenuBar(this);
        m_bar->setEngine(m_engine);
        m_bar->setMaxCount(8);                    // 每级最多8个按钮
        m_bar->setBarHeight(55);                  // 高度55px
        m_bar->setButtonFixedWidth(100);          // 固定宽度100px
        m_bar->setSpacing(0);                     // 间距0px
        m_bar->setMargins(8, 3, 8, 3);            // 边距 left, top, right, bottom

        // ===== 4. 创建控制面板 =====
        createControlPanel();

        // ===== 5. 连接信号 =====
        setupConnections();

        // ===== 6. 应用样式 =====
        applyStyle();

        statusBar()->showMessage("✅ 就绪 | 间距=0，按钮紧贴排列");
    }

private:
    // ===== 构建菜单树 =====
    void buildMenuTree() {
        m_root = new MenuNode("");

        // ---------- 一级：作战指挥 (order=0, 常亮) ----------
        auto* command = new MenuNode(
                    "⚔️ 作战",
                    BtnState::On,
                    nullptr,
        {
                        // 二级：车内指挥 (order=0, 常亮)
                        new MenuNode("🚗 车内指挥", BtnState::On, nullptr,
                        {
                            // 三级：具体操作 (order 跳跃排列，演示占位)
                            new MenuNode("📡 启动电台", BtnState::Off, radioStart, {}, true, 1),
                            new MenuNode("📡 关闭电台", BtnState::Blink, radioStop, {}, true, 3),
                            new MenuNode("📻 频率设置", BtnState::Off, radioSetFreq, {}, true, 0),   // 排最前面
                            new MenuNode("🔐 加密启用", BtnState::Blink, cryptoEnable, {}, true, 2),
                            new MenuNode("🔓 加密禁用", BtnState::Blink, cryptoDisable, {}, true, 4),
                        }
                        ),
                        // 二级：战场报告 (order=1, 普通)
                        new MenuNode("📄 战场报告", BtnState::Off, reportShow, {}, false, 1),
                        // 二级：GPS定位 (order=2, 普通)
                        new MenuNode("🛰️ GPS定位", BtnState::Off, gpsStatus, {}, true, 2),
                    },
                    true,
                    0
                    );

        // ---------- 一级：系统管理 (order=1, 普通) ----------
        auto* system = new MenuNode(
                    "⚙️ 系统",
                    BtnState::Off,
                    nullptr,
        {
                        new MenuNode("🌐 网络设置", BtnState::Off, networkConfig, {}, true, 0),
                        new MenuNode("📋 系统日志", BtnState::Off, logShow, {}, true, 2),
                        new MenuNode("💻 系统信息", BtnState::Blink, systemInfo, {}, true, 1),
                        new MenuNode("⚡ 省电模式", BtnState::Off, powerSave, {}, true, 3),
                        new MenuNode("🚀 高性能", BtnState::Off, highPerformance, {}, true, 4),
                    },
                    true,
                    1
                    );

        // ---------- 一级：更多功能 (order=2, 普通) ----------
        auto* more = new MenuNode(
                    "📦 更多",
                    BtnState::Off,
                    nullptr,
        {
                        new MenuNode("🔧 工具A", BtnState::Off, [](){ qDebug() << "🔧 工具A执行"; }),
                        new MenuNode("🔧 工具B", BtnState::Off, [](){ qDebug() << "🔧 工具B执行"; }),
                        new MenuNode("🔧 工具C", BtnState::Blink, [](){ qDebug() << "🔧 工具C执行"; }),
                        new MenuNode("🔧 工具D", BtnState::Off, [](){ qDebug() << "🔧 工具D执行"; }),
                        new MenuNode("🔧 工具E", BtnState::Off, [](){ qDebug() << "🔧 工具E执行"; }),
                        new MenuNode("🔧 工具F", BtnState::Off, [](){ qDebug() << "🔧 工具F执行"; }),
                    },
                    true,
                    2
                    );

        m_root->children = {command, system, more};
    }

    // ===== 创建控制面板 =====
    void createControlPanel() {
        QWidget* center = new QWidget(this);
        QVBoxLayout* mainLayout = new QVBoxLayout(center);
        mainLayout->setSpacing(8);
        mainLayout->setContentsMargins(10, 10, 10, 10);

        // ---- 控制按钮区域 ----
        QGroupBox* ctrlGroup = new QGroupBox("🎮 控制面板", this);
        QHBoxLayout* ctrlLayout = new QHBoxLayout(ctrlGroup);

        auto* btnGetNodes = new QPushButton("📋 获取当前节点", this);
        auto* btnGetBtns = new QPushButton("🔘 获取当前按钮", this);
        auto* btnTrigger = new QPushButton("🎯 触发'启动电台'", this);
        auto* btnReset = new QPushButton("🏠 重置到根", this);
        auto* btnSetOn = new QPushButton("💡 点亮'频率设置'", this);
        auto* btnClearLog = new QPushButton("🗑️ 清空日志", this);

        ctrlLayout->addWidget(btnGetNodes);
        ctrlLayout->addWidget(btnGetBtns);
        ctrlLayout->addWidget(btnTrigger);
        ctrlLayout->addWidget(btnReset);
        ctrlLayout->addWidget(btnSetOn);
        ctrlLayout->addWidget(btnClearLog);
        ctrlLayout->addStretch();

        // ---- 配置区域 ----
        QGroupBox* configGroup = new QGroupBox("⚙️ 配置 (即时生效)", this);
        QHBoxLayout* configLayout = new QHBoxLayout(configGroup);

        // 按钮宽度
        QLabel* labelWidth = new QLabel("宽度:", this);
        QSpinBox* spinWidth = new QSpinBox(this);
        spinWidth->setRange(50, 200);
        spinWidth->setValue(100);
        spinWidth->setSuffix(" px");
        spinWidth->setFixedWidth(80);

        // 间距
        QLabel* labelSpacing = new QLabel("间距:", this);
        QSpinBox* spinSpacing = new QSpinBox(this);
        spinSpacing->setRange(0, 20);
        spinSpacing->setValue(0);
        spinSpacing->setSuffix(" px");
        spinSpacing->setFixedWidth(80);

        // 边距
        QLabel* labelMargin = new QLabel("边距:", this);
        QSpinBox* spinMargin = new QSpinBox(this);
        spinMargin->setRange(0, 20);
        spinMargin->setValue(8);
        spinMargin->setSuffix(" px");
        spinMargin->setFixedWidth(80);

        // 显示路径
        QCheckBox* chkShowPath = new QCheckBox("显示路径", this);
        chkShowPath->setChecked(true);

        configLayout->addWidget(labelWidth);
        configLayout->addWidget(spinWidth);
        configLayout->addSpacing(15);
        configLayout->addWidget(labelSpacing);
        configLayout->addWidget(spinSpacing);
        configLayout->addSpacing(15);
        configLayout->addWidget(labelMargin);
        configLayout->addWidget(spinMargin);
        configLayout->addSpacing(15);
        configLayout->addWidget(chkShowPath);
        configLayout->addStretch();

        // ---- 日志显示区域 ----
        QGroupBox* logGroup = new QGroupBox("📝 操作日志", this);
        QVBoxLayout* logLayout = new QVBoxLayout(logGroup);
        m_logText = new QTextEdit(this);
        m_logText->setReadOnly(true);
        m_logText->setMaximumHeight(160);
        m_logText->setFontFamily("Consolas");
        m_logText->setFontPointSize(11);
        logLayout->addWidget(m_logText);

        // ---- 状态提示 ----
        QHBoxLayout* statusLayout = new QHBoxLayout();
        QLabel* tipLabel = new QLabel("💡 提示: 间距设为0时按钮紧贴排列，可手动调整间距观察效果", this);
        tipLabel->setStyleSheet("color: #8888aa; font-size: 12px;");
        statusLayout->addWidget(tipLabel);
        statusLayout->addStretch();

        // ---- 添加到主布局 ----
        mainLayout->addWidget(ctrlGroup);
        mainLayout->addWidget(configGroup);
        mainLayout->addWidget(logGroup);
        mainLayout->addLayout(statusLayout);
        mainLayout->addStretch();
        mainLayout->addWidget(m_bar);  // 底部菜单栏

        setCentralWidget(center);

        // ---- 连接配置信号 ----
        connect(spinWidth, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int val) {
            m_bar->setButtonFixedWidth(val);
            appendLog("⚙️ 按钮宽度设为 " + QString::number(val) + "px");
        });

        connect(spinSpacing, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int val) {
            m_bar->setSpacing(val);
            appendLog("⚙️ 间距设为 " + QString::number(val) + "px");
        });

        connect(spinMargin, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int val) {
            m_bar->setMargins(val, 3, val, 3);
            appendLog("⚙️ 边距设为 " + QString::number(val) + "px");
        });

        connect(chkShowPath, &QCheckBox::toggled,
                this, [this](bool checked) {
            if (!checked) statusBar()->clearMessage();
        });

        // ---- 连接控制按钮 ----
        connect(btnGetNodes, &QPushButton::clicked, this, &MainWindow::onGetNodes);
        connect(btnGetBtns, &QPushButton::clicked, this, &MainWindow::onGetButtons);
        connect(btnTrigger, &QPushButton::clicked, this, &MainWindow::onTriggerRadio);
        connect(btnReset, &QPushButton::clicked, this, [this]() {
            m_engine->reset();
            appendLog("🏠 已重置到根菜单");
        });
        connect(btnSetOn, &QPushButton::clicked, this, &MainWindow::onSetFrequencyOn);
        connect(btnClearLog, &QPushButton::clicked, this, [this]() {
            m_logText->clear();
        });

        // 保存引用
        m_spinWidth = spinWidth;
        m_spinSpacing = spinSpacing;
        m_spinMargin = spinMargin;
        m_chkShowPath = chkShowPath;
    }

    // ===== 连接信号 =====
    void setupConnections() {
        connect(m_engine, &MenuEngine::pathChanged,
                this, [this](const QList<MenuNode*>& path) {
            if (!m_chkShowPath->isChecked()) return;

            QStringList names;
            for (int i = 1; i < path.size(); ++i) {
                // 去除表情符号，保留纯文本
                QString text = path[i]->text;
                // 简单清理：移除常见表情符号前缀
                text.remove(QRegExp("^[\\x{1F300}-\\x{1FAFF}\\x{2600}-\\x{27BF}\\x{2700}-\\x{27BF}]"));
                text = text.trimmed();
                if (!text.isEmpty()) names << text;
            }
            QString pathStr = names.join(" → ");
            if (pathStr.isEmpty()) pathStr = "根菜单";
            statusBar()->showMessage("📍 " + pathStr);
            appendLog("📂 切换到: " + pathStr);
        });
    }

    // ===== 样式 =====
    void applyStyle() {
        setStyleSheet(R"(
                      QMainWindow {
                      background-color: #1a1a2e;
                      }
                      QGroupBox {
                      color: #e0e0e0;
                      border: 1px solid #4a4a6a;
                      border-radius: 6px;
                      margin-top: 10px;
                      padding-top: 10px;
                      font-weight: bold;
                      }
                      QGroupBox::title {
                      subcontrol-origin: margin;
                      left: 10px;
                      padding: 0 5px 0 5px;
                      color: #8a8aaa;
                      }
                      QPushButton {
                      padding: 4px 10px;
                      background-color: #2a2a4a;
                      color: #d0d0e0;
                      border: 1px solid #4a4a6a;
                      border-radius: 4px;
                      font-size: 12px;
                      }
                      QPushButton:hover {
                      background-color: #3a3a5a;
                      border-color: #6a6a8a;
                      }
                      QPushButton:pressed {
                      background-color: #1a1a3a;
                      }
                      QLabel {
                      color: #b0b0c0;
                      font-size: 12px;
                      }
                      QSpinBox {
                      background-color: #2a2a4a;
                      color: #d0d0e0;
                      border: 1px solid #4a4a6a;
                      border-radius: 4px;
                      padding: 2px 4px;
                      }
                      QSpinBox::up-button, QSpinBox::down-button {
                      background-color: #3a3a5a;
                      border: none;
                      }
                      QCheckBox {
                      color: #b0b0c0;
                      font-size: 12px;
                      }
                      QCheckBox::indicator {
                      width: 16px;
                      height: 16px;
                      }
                      QTextEdit {
                      background-color: #0d0d1a;
                      color: #a0d0a0;
                      border: 1px solid #3a3a5a;
                      border-radius: 4px;
                      font-family: Consolas;
                      font-size: 11px;
                      }

                      /* ===== 菜单栏按钮样式 (纯视觉，不影响间距) ===== */
                      MenuButton {
                      border-radius: 0px;
                      font-size: 13px;
                      font-weight: 500;
                      }
                      MenuButton[state="off"] {
                      background-color: red;
                      color: #a0a0b0;
                      border: 1px solid white;
                      }
                      MenuButton[state="off"]:hover:pressed {
                      background-color: gray;
                      color: white;
                      border: 1px solid white;
                      }
                      MenuButton[state="on"] {
                      background-color: blue;
                      color: white;
                      border: 1px solid white;
                      }
                      MenuButton[state="on"]:hover:pressed {
                      background-color: gray;
                      color: white;
                      border: 1px solid white;
                      }
                      MenuButton[state="blink"] {
                      background-color: red;
                      color: white;
                      border: 1px solid #ffa726;
                      }
                      MenuButton[state="blink"]:hover:pressed {
                      background-color: blue;
                      color: white;
                      border: 1px solid #ffa726;
                      }
                      MenuButton[flashing="true"] {
                      background-color: #FFC107 !important;
                      color: black !important;
                      border: 1px solid #ffca28 !important;
                      font-weight: bold;
                      }
                      QPushButton[type="more"] {
                      background-color: #4a4a6a;
                      color: #b0b0c0;
                      border: 1px solid #5a5a7a;
                      border-radius: 3px;
                      font-size: 13px;
                      margin: 0px;
                      padding: 0px;
                      }
                      QPushButton[type="more"]:hover {
                      background-color: #5a5a7a;
                      }
                      )");
    }

    // ===== 日志辅助 =====
    void appendLog(const QString& msg) {
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
        m_logText->append("[" + timestamp + "] " + msg);
        QTextCursor cursor = m_logText->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_logText->setTextCursor(cursor);
    }

    // ===== 演示接口 =====

    void onGetNodes() {
        auto nodes = m_engine->getCurrentNodes();
        appendLog("📋 当前层级节点数: " + QString::number(nodes.size()));
        for (int i = 0; i < nodes.size(); ++i) {
            auto* n = nodes[i];
            const char* stateName[] = {"Off", "On", "Blink"};
            int idx = static_cast<int>(n->state);
            appendLog(QString("  [%1] %2 | state=%3 | exclusive=%4 | children=%5 | order=%6")
                      .arg(i)
                      .arg(n->text)
                      .arg(stateName[idx])
                      .arg(n->exclusive ? "true" : "false")
                      .arg(n->children.size())
                      .arg(n->order));
        }
    }

    void onGetButtons() {
        auto btns = m_bar->getButtons();
        appendLog("🔘 当前按钮数: " + QString::number(btns.size()));
        for (int i = 0; i < btns.size(); ++i) {
            auto* btn = btns[i];
            if (btn) {
                appendLog(QString("  [%1] %2 | state=%3")
                          .arg(i)
                          .arg(btn->text())
                          .arg(btn->property("state").toString()));
            }
        }
    }

    void onTriggerRadio() {
        MenuNode* target = findNodeRecursive(m_root, "📡 启动电台");
        if (target) {
//            m_engine->triggerNode(target);
            m_bar->trigger(target);
            appendLog("🎯 已触发 '启动电台' 回调");
        } else {
            appendLog("❌ 未找到 '启动电台' 节点");
        }
    }

    void onSetFrequencyOn() {
        MenuNode* target = findNodeRecursive(m_root, "📻 频率设置");
        if (target) {
            m_bar->setNodeState(target, BtnState::On);
            appendLog("💡 强制点亮 '频率设置' (常亮状态)");
        } else {
            appendLog("❌ 未找到 '频率设置' 节点");
        }
    }

    MenuNode* findNodeRecursive(MenuNode* root, const QString& text) {
        if (!root) return nullptr;
        if (root->text == text) return root;
        for (auto* child : root->children) {
            auto* result = findNodeRecursive(child, text);
            if (result) return result;
        }
        return nullptr;
    }

private:
    MenuNode* m_root = nullptr;
    MenuEngine* m_engine = nullptr;
    MenuBar* m_bar = nullptr;
    QTextEdit* m_logText = nullptr;
    QSpinBox* m_spinWidth = nullptr;
    QSpinBox* m_spinSpacing = nullptr;
    QSpinBox* m_spinMargin = nullptr;
    QCheckBox* m_chkShowPath = nullptr;
};


