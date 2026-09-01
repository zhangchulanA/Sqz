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
#include "Logger.h"

//using namespace Sqz;
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
        m_bar->setMaxCount(9);                    // 每级最多8个按钮
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

        // ---------- 一级：车内指挥 (order=0, 常亮) ----------
        auto* InCar_Com = new MenuNode("车内指挥",BtnState::On,nullptr,
        {
                                           new MenuNode("路线设置", BtnState::Of, nullptr,{},0),
                                           new MenuNode("偏航设置", BtnState::Of, nullptr, {},2),
                                           new MenuNode("返回", BtnState::Of, [](){ qDebug() << "返回"; },{},8,true)
                                       },0);

        // ---------- 一级：车际指挥 (order=1, 普通) ----------
        auto* InterVehicle_Com = new MenuNode("车际指挥",BtnState::Of,nullptr,
        {
                                                  new MenuNode("文电处理", BtnState::Of, nullptr,
                                                  {
                                                      new MenuNode("命令", BtnState::Of,nullptr,{},0),
                                                      new MenuNode("请求", BtnState::Of, [](){ qDebug() << "🔧 工具B执行"; },{},1),
                                                      new MenuNode("报告", BtnState::Of, [](){ qDebug() << "🔧 工具C执行"; },{},2),
                                                      new MenuNode("自由文电", BtnState::Of, [](){ qDebug() << "🔧 工具D执行"; },{},3),
                                                      new MenuNode("图片浏览", BtnState::Of, [](){ qDebug() << "🔧 工具E执行"; },{},5),
                                                      new MenuNode("收件箱", BtnState::Of, [](){ qDebug() << "🔧 工具F执行"; },{},6),
                                                      new MenuNode("发件箱", BtnState::Of, [](){ qDebug() << "🔧 工具F执行"; },{},7),
                                                      new MenuNode("返回", BtnState::Of, [](){ qDebug() << "返回"; },{},8,true)
                                                  }),
                                                  new MenuNode("情况处理", BtnState::Of, nullptr, {
                                                      new MenuNode("目标标注", BtnState::Of,nullptr,{},0),
                                                      new MenuNode("情况管理", BtnState::Of, [](){ qDebug() << "🔧 工具B执行"; },{},1),
                                                      new MenuNode("图层设置", BtnState::Of, [](){ qDebug() << "🔧 工具C执行"; },{},2),
                                                      new MenuNode("态势清除", BtnState::Of, [](){ qDebug() << "🔧 工具D执行"; },{},3),
                                                      new MenuNode("返回", BtnState::Of, [](){ qDebug() << "返回"; },{},8,true)
                                                  }),
                                                  new MenuNode("技况报告", BtnState::Of, nullptr,{
                                                      new MenuNode("技术状况", BtnState::Of, [](){ qDebug() << "🔧 工具B执行"; },{},0),
                                                      new MenuNode("人员统计", BtnState::Of, [](){ qDebug() << "🔧 工具B执行"; },{},1),
                                                      new MenuNode("弹药统计", BtnState::Of, [](){ qDebug() << "🔧 工具C执行"; },{},2),
                                                      new MenuNode("燃油统计", BtnState::Of, [](){ qDebug() << "🔧 工具D执行"; },{},3),
                                                      new MenuNode("故障统计", BtnState::Of, [](){ qDebug() << "🔧 工具D执行"; },{},4),
                                                      new MenuNode("弹量输入", BtnState::Of, [](){ qDebug() << "🔧 工具D执行"; },{},7),
                                                      new MenuNode("返回", BtnState::Of, [](){ qDebug() << "返回"; },{},8,true)
                                                  }),
                                                  new MenuNode("导航处理", BtnState::Of, nullptr, {
                                                      new MenuNode("导航线", BtnState::Of,nullptr,{},0),
                                                      new MenuNode("定位发送", BtnState::Of, [](){ qDebug() << "🔧 工具B执行"; },{},1),
                                                      new MenuNode("导航管理", BtnState::Of, [](){ qDebug() << "🔧 工具C执行"; },{},2),
                                                      new MenuNode("返回", BtnState::Of, [](){ qDebug() << "返回"; },{},8,true)
                                                  }),
                                                  new MenuNode("通信配置", BtnState::Of, nullptr, {
                                                      new MenuNode("接入方式", BtnState::Of, [](){ qDebug() << "🔧 工具B执行"; },{},0),
                                                      new MenuNode("时隙设置", BtnState::Of, [](){ qDebug() << "🔧 工具B执行"; },{},1),
                                                      new MenuNode("网络设置", BtnState::Of, [](){ qDebug() << "🔧 工具C执行"; },{},2),
                                                      new MenuNode("速率设置", BtnState::Of, [](){ qDebug() << "🔧 工具D执行"; },{},3),
                                                      new MenuNode("网络维护", BtnState::Of, [](){ qDebug() << "🔧 工具D执行"; },{
                                                          new MenuNode("节点列表", BtnState::Of, [](){ qDebug() << "🔧 工具B执行"; },{}),
                                                          new MenuNode("主动退网", BtnState::Of, [](){ qDebug() << "🔧 工具B执行"; },{}),
                                                          new MenuNode("主动入网", BtnState::Of, [](){ qDebug() << "🔧 工具C执行"; },{}),
                                                          new MenuNode("路由查询", BtnState::Of, [](){ qDebug() << "🔧 工具D执行"; },{}),
                                                          new MenuNode("返回", BtnState::Of, [](){ qDebug() << "返回"; },{},8,true)
                                                      },4),
                                                      new MenuNode("指挥设置", BtnState::Of, [](){ qDebug() << "🔧 工具D执行"; },{
                                                          new MenuNode("转信设置", BtnState::Of, [](){ qDebug() << "🔧 工具B执行"; },{}),
                                                          new MenuNode("喇叭音量", BtnState::Of, [](){ qDebug() << "🔧 工具B执行"; },{}),
                                                          new MenuNode("网际开关", BtnState::Of, [](){ qDebug() << "🔧 工具C执行"; },{}),
                                                          new MenuNode("链路切换", BtnState::Of, [](){ qDebug() << "🔧 工具D执行"; },{}),
                                                          new MenuNode("返回", BtnState::Of, [](){ qDebug() << "返回"; },{},8,true)
                                                      },5),
                                                      new MenuNode("电台状态", BtnState::Of, [](){ qDebug() << "🔧 工具D执行"; },{},6),
                                                      new MenuNode("高级设置", BtnState::Of, [](){ qDebug() << "🔧 工具D执行"; },{
                                                          new MenuNode("时延设置", BtnState::Of, [](){ qDebug() << "🔧 工具B执行"; },{},0),
                                                          new MenuNode("恢复出场", BtnState::Of, [](){ qDebug() << "🔧 工具C执行"; },{},1),
                                                          new MenuNode("返回", BtnState::Of, [](){ qDebug() << "返回"; },{},8,true)
                                                      },7),
                                                      new MenuNode("返回", BtnState::Of, [](){ qDebug() << "返回"; },{},8,true)
                                                  }),
                                                  new MenuNode("系统设置", BtnState::Of, nullptr, {
                                                      new MenuNode("席位设置", BtnState::Of, [](){ qDebug() << "🔧 工具B执行"; },{},0),
                                                      new MenuNode("名录查看", BtnState::Of, [](){ qDebug() << "🔧 工具B执行"; },{},2),
                                                      new MenuNode("数据管理", BtnState::Of, [](){ qDebug() << "🔧 工具C执行"; },{},3),
                                                      new MenuNode("通信测试", BtnState::Of, [](){ qDebug() << "🔧 工具D执行"; },{},4),
                                                      new MenuNode("返回", BtnState::Of, [](){ qDebug() << "返回"; },{},8,true)
                                                  }),
                                                  new MenuNode("返回", BtnState::Of, [](){ qDebug() << "返回"; },{},8,true)
                                              },1);

        // ---------- 一级：系统管理 (order=2, 普通) ----------
        auto* SystemMgnt = new MenuNode("系统管理",BtnState::Of,nullptr,
        {
                                            new MenuNode("语言设置", BtnState::Of, [](){ qDebug() << "🔧 工具A执行"; },{},0),
                                            new MenuNode("时区时间", BtnState::Of, [](){ qDebug() << "🔧 工具B执行"; },{},1),
                                            new MenuNode("屏幕除雾", BtnState::Of, [](){ qDebug() << "🔧 工具C执行"; },{},3),
                                            new MenuNode("屏幕亮度", BtnState::Of, [](){ qDebug() << "🔧 工具D执行"; },{},4),
                                            new MenuNode("屏幕校准", BtnState::Of, [](){ qDebug() << "🔧 工具E执行"; },{},5),
                                            new MenuNode("本机信息", BtnState::Of, [](){ qDebug() << "🔧 工具F执行"; },{},6),
                                            new MenuNode("账户管理", BtnState::Of, [](){ qDebug() << "🔧 工具F执行"; },{},7),
                                            new MenuNode("返回", BtnState::Of, [](){ qDebug() << "返回"; },{},8,true)
                                        },2);
        // ---------- 一级：导航控制 (order=3, 普通) ----------
        auto* NavControl = new MenuNode("导航控制",BtnState::Of,nullptr,
        {
                                            new MenuNode("归位", BtnState::Of, [](){ qDebug() << "🔧 工具A执行"; },{},0),
                                            new MenuNode("航迹关闭", BtnState::Of, [](){ qDebug() << "🔧 工具B执行"; },{},1),
                                            new MenuNode("航迹清除", BtnState::Of, [](){ qDebug() << "🔧 工具C执行"; },{},2),
                                            new MenuNode("UTM经纬度", BtnState::Of, [](){ qDebug() << "🔧 工具D执行"; },{},3),
                                            new MenuNode("全屏地图", BtnState::Of, [](){ qDebug() << "🔧 工具E执行"; },{},4),
                                            new MenuNode("地图设置", BtnState::Of, [](){ qDebug() << "🔧 工具F执行"; },{
                                                new MenuNode("投影设置", BtnState::Of, [](){ qDebug() << "🔧 工具A执行"; },{},0),
                                                new MenuNode("类型设置", BtnState::Of, [](){ qDebug() << "🔧 工具B执行"; },{},1),
                                                new MenuNode("网格设置", BtnState::Of, [](){ qDebug() << "🔧 工具B执行"; },{},2),
                                                new MenuNode("书签管理", BtnState::Of, [](){ qDebug() << "🔧 工具C执行"; },{},3),
                                                new MenuNode("图层选择", BtnState::Of, [](){ qDebug() << "🔧 工具C执行"; },{},4),
                                                new MenuNode("返回", BtnState::Of, [](){ qDebug() << "返回"; },{},8,true)
                                            },5),
                                            new MenuNode("导航设置", BtnState::Of, [](){ qDebug() << "🔧 工具F执行"; },{},7),
                                            new MenuNode("返回", BtnState::Of, [](){ qDebug() << "返回"; },{},8,true)
                                        },3);
        // ---------- 一级：武器控制 (order=4, 普通) ----------
        auto* WeaponControl = new MenuNode("武器控制",BtnState::Of,nullptr,
        {
                                               new MenuNode("激光单元", BtnState::Of, [](){ qDebug() << "🔧 工具A执行"; },{},2),
                                               new MenuNode("火控状态", BtnState::Of, [](){ qDebug() << "🔧 工具B执行"; },{},3),
                                               new MenuNode("烟幕发射", BtnState::Of, [](){ qDebug() << "🔧 工具C执行"; },{
                                                   new MenuNode("烟左1-2", BtnState::Of, [](){ qDebug() << "🔧 工具A执行"; },{},0),
                                                   new MenuNode("烟左3-4", BtnState::Of, [](){ qDebug() << "🔧 工具B执行"; },{},1),
                                                   new MenuNode("烟左齐发", BtnState::Of, [](){ qDebug() << "🔧 工具B执行"; },{},2),
                                                   new MenuNode("烟右1-2", BtnState::Of, [](){ qDebug() << "🔧 工具C执行"; },{},3),
                                                   new MenuNode("烟右3-4", BtnState::Of, [](){ qDebug() << "🔧 工具C执行"; },{},4),
                                                   new MenuNode("烟齐发", BtnState::Of, [](){ qDebug() << "🔧 工具C执行"; },{},5),
                                                   new MenuNode("榴齐发", BtnState::Of, [](){ qDebug() << "🔧 工具C执行"; },{},6),
                                                   new MenuNode("返回", BtnState::Of, [](){ qDebug() << "返回"; },{},8,true)
                                               },5),
                                               new MenuNode("返回", BtnState::Of, [](){ qDebug() << "返回"; },{},8,true)
                                           },4);
        // ---------- 一级：电气控制 (order=5, 普通) ----------
        auto* ElectricalControl = new MenuNode("电气控制",BtnState::Of,nullptr,
        {
                                                   new MenuNode("故障列表", BtnState::Of, [](){ qDebug() << "🔧 工具A执行"; },{},0),
                                                   new MenuNode("节点状态", BtnState::Of, [](){ qDebug() << "🔧 工具B执行"; },{},1),
                                                   new MenuNode("风扇状态", BtnState::Of, [](){ qDebug() << "🔧 工具C执行"; },{},2),
                                                   new MenuNode("三防状态", BtnState::Of, [](){ qDebug() << "🔧 工具C执行"; },{},3),
                                                   new MenuNode("灭火状态", BtnState::Of, [](){ qDebug() << "🔧 工具C执行"; },{},4),
                                                   new MenuNode("信息展示", BtnState::Of, [](){ qDebug() << "🔧 工具C执行"; },{},7),
                                                   new MenuNode("返回", BtnState::Of, [](){ qDebug() << "返回"; },{},8,true)
                                               },5);

        m_root->children = {InCar_Com, InterVehicle_Com, SystemMgnt,NavControl,WeaponControl,ElectricalControl};
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

                      /* ===== 菜单栏按钮样式 (纯视觉，不影响间距) ===== */
                      MenuButton {
                      border-radius: 0px;
                      font-size: 13px;
                      font-weight: 500;
                      }
                      MenuButton[state="Of"] {
                      background-color: black;
                      color: white;
                      font: 18px;
                      border: 1px solid white;
                      }
                      MenuButton[state="Of"]:hover:pressed {
                      background-color: rgb(115, 210, 22);
                      color: black;
                      font: 18px;
                      border: 1px solid white;
                      }
                      MenuButton[state="On"] {
                      background-color: rgb(115, 210, 22);
                      color: black;
                      font: 18px;
                      border: 1px solid white;
                      }
                      MenuButton[state="On"]:hover:pressed {
                      background-color: black;
                      color: white;
                      font: 18px;
                      border: 1px solid white;
                      }
                      MenuButton[state="Bk"] {
                      background-color: black;
                      color: white;
                      font: 18px;
                      border: 1px solid white;
                      }
                      MenuButton[state="Bk"]:hover:pressed {
                      background-color: rgb(115, 210, 22);
                      color: black;
                      font: 18px;
                      border: 1px solid #ffa726;
                      }
                      MenuButton[flashing="true"] {
                      background-color: #FFC107 !important;
                      color: black !important;
                      border: 1px solid #ffca28 !important;
                      font-weight: bold;
                      }
                      )");

        MenuButton *btn = m_bar->getBtn("车内指挥");
        loginfo << btn->styleSheet();
        loginfo << btn->property("state").toString();
        loginfo << btn->style()->metaObject()->className();
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
            const char* stateName[] = {"Of", "On", "Of"};
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
        MenuNode* target = findNodeRecursive(m_root, "系统管理");
        if (target) {
//            m_engine->triggerNode(target);
            m_bar->trigger(target);
            appendLog("🎯 已触发 '系统管理' 回调");
        } else {
            appendLog("❌ 未找到 '系统管理' 节点");
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


