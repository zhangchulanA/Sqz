#ifndef DATABIND_H
#define DATABIND_H
#include <QObject>
#include <QMetaObject>
#include <QMetaProperty>
#include <QWidget>
#include <functional>
#include <QPointer>
/**
 * @brief DataBind Widget MVVM 绑定工具类
 * Qt5.12，纯头文件实现
 * 无 SIGNAL/SLOT 字符串宏，信号全部使用成员函数指针编译期检查
 * ViewModel：标准 Q_PROPERTY + NOTIFY信号，禁止包含任何Widget头
 * View层：所有UI操作全部放在View，ViewModel完全不感知控件
 * 对外接口：bind(自带属性快捷绑定) / bindLambda(自定义逻辑) / bindCommand(事件命令)
 */
class DataBind : public QObject
{
    Q_OBJECT
public:
    static DataBind* instance()
    {
        static DataBind obj;
        return &obj;
    }
    // vm -> ui：ViewModel变更，设置UI
    using VmToUiFunc = std::function<void(const QVariant&)>;
    // ui -> vm：读取UI返回QVariant回写ViewModel；nullptr代表单向绑定
    using UiToVmFunc = std::function<QVariant()>;

    //--------------------------------------------------------------------------
    //【新增重载：单向绑定，ViewModel属性 → UI属性，不需要传入UI变更信号】
    // 独立实现逻辑，不复用老模板函数，解决成员函数指针类型不匹配编译报错
    //--------------------------------------------------------------------------
    template<typename SenderT>
    void bind(QObject* vm,
              const QString& vmPropName,
              SenderT* sender,
              const QString& widgetPropName)
    {
        if (!vm || !sender)
            return;
        const QMetaObject* vmMo = vm->metaObject();
        int vmPropIdx = vmMo->indexOfProperty(vmPropName.toUtf8().constData());
        if (vmPropIdx < 0)
            return;
        QMetaProperty vmProp = vmMo->property(vmPropIdx);

        const QMetaObject* wMo = sender->metaObject();
        int wPropIdx = wMo->indexOfProperty(widgetPropName.toUtf8().constData());
        if (wPropIdx < 0)
            return;
        QMetaProperty widgetProp = wMo->property(wPropIdx);

        // ViewModel notify信号触发，刷新UI控件
        if (vmProp.hasNotifySignal())
        {
            connect(vm, vmProp.notifySignal(), this, [=]()
            {
                QVariant val = vmProp.read(vm);
                widgetProp.write(sender, val);
            });
        }
        // 初始化同步一次数据
        widgetProp.write(sender, vmProp.read(vm));
    }

    //--------------------------------------------------------------------------
    //【原有bind接口完整保留，代码、注释完全不变】
    /**
     * @brief bind 快捷绑定，控件拥有原生Q_PROPERTY时优先使用
     * @tparam SenderT UI控件类型
     * @tparam SignalArgs 信号参数包
     * @param vm ViewModel对象
     * @param vmPropName ViewModel Q_PROPERTY属性名字符串
     * @param sender UI控件指针(支持PTR()包装的QPointer)
     * @param widgetPropName 控件Q_PROPERTY属性名，如 "text" "value"
     * @param signal 控件变更信号函数指针；单向绑定此处不能传nullptr！请调用上面少参数重载
     * @param twoWay true双向；false仅ViewModel→UI单向
     */
    template<typename SenderT, typename ...SignalArgs>
    void bind(QObject* vm,
              const QString& vmPropName,
              SenderT* sender,
              const QString& widgetPropName,
              void(SenderT::*signal)(SignalArgs...),
              bool twoWay = true)
    {
        if (!vm || !sender)
            return;
        const QMetaObject* vmMo = vm->metaObject();
        int vmPropIdx = vmMo->indexOfProperty(vmPropName.toUtf8().constData());
        if (vmPropIdx < 0)
            return;
        QMetaProperty vmProp = vmMo->property(vmPropIdx);
        const QMetaObject* wMo = sender->metaObject();
        int wPropIdx = wMo->indexOfProperty(widgetPropName.toUtf8().constData());
        if (wPropIdx < 0)
            return;
        QMetaProperty widgetProp = wMo->property(wPropIdx);
        // ViewModel变更同步到UI
        if (vmProp.hasNotifySignal())
        {
            connect(vm, vmProp.notifySignal(), this, [=]()
            {
                QVariant val = vmProp.read(vm);
                widgetProp.write(sender, val);
            });
        }
        // 初始数据同步
        widgetProp.write(sender, vmProp.read(vm));
        // UI变更回写ViewModel，双向绑定
        if (twoWay && signal != nullptr)
        {
            connect(sender, signal, this, [=]()
            {
                QVariant val = widgetProp.read(sender);
                vmProp.write(vm, val);
            });
        }
    }

    //--------------------------------------------------------------------------
    //【新增重载：单向bindLambda，不需要传入signal】
    //--------------------------------------------------------------------------
    template<typename SenderT>
    void bindLambda(QObject* vm,
                    const QString& vmPropName,
                    SenderT* sender,
                    VmToUiFunc vmToUi)
    {
        if (!vm || !vmToUi)
            return;
        const QMetaObject* vmMo = vm->metaObject();
        int vmPropIdx = vmMo->indexOfProperty(vmPropName.toUtf8().constData());
        if (vmPropIdx < 0)
            return;
        QMetaProperty vmProp = vmMo->property(vmPropIdx);
        // ViewModel notify信号刷新UI
        if (vmProp.hasNotifySignal())
        {
            connect(vm, vmProp.notifySignal(), this, [=]()
            {
                QVariant val = vmProp.read(vm);
                vmToUi(val);
            });
        }
        // 初始化同步
        vmToUi(vmProp.read(vm));
    }

    //--------------------------------------------------------------------------
    //【原有bindLambda接口完整保留，代码、注释完全不变】
    /**
     * @brief bindLambda 自定义绑定，控件无Q_PROPERTY / 需要数据转换/自定义样式逻辑
     * @tparam SenderT UI控件类型
     * @tparam SignalArgs 信号参数包
     * @param vm ViewModel对象
     * @param vmPropName ViewModel Q_PROPERTY属性名字符串
     * @param sender UI控件指针(支持PTR()包装)
     * @param signal 控件变更信号函数指针；单向绑定不要传nullptr，请调用少参数重载版本
     * @param vmToUi vm变化更新UI逻辑
     * @param uiToVm UI变化回写vm；nullptr 为单向绑定
     */
    template<typename SenderT, typename ...SignalArgs>
    void bindLambda(QObject* vm,
                    const QString& vmPropName,
                    SenderT* sender,
                    void(SenderT::*signal)(SignalArgs...),
                    VmToUiFunc vmToUi,
                    UiToVmFunc uiToVm = nullptr)
    {
        if (!vm || !vmToUi)
            return;
        const QMetaObject* vmMo = vm->metaObject();
        int vmPropIdx = vmMo->indexOfProperty(vmPropName.toUtf8().constData());
        if (vmPropIdx < 0)
            return;
        QMetaProperty vmProp = vmMo->property(vmPropIdx);
        // ViewModel notify信号刷新UI
        if (vmProp.hasNotifySignal())
        {
            connect(vm, vmProp.notifySignal(), this, [=]()
            {
                QVariant val = vmProp.read(vm);
                vmToUi(val);
            });
        }
        // 初始化同步
        vmToUi(vmProp.read(vm));
        // UI回写ViewModel
        if (uiToVm && sender && signal)
        {
            connect(sender, signal, this, [=]()
            {
                QVariant val = uiToVm();
                vmProp.write(vm, val);
            });
        }
    }

    //--------------------------------------------------------------------------
    //【原有bindCommand接口完整保留，代码、注释完全不变】
    /**
     * @brief bindCommand 命令绑定，UI信号触发回调（按钮点击等事件）
     * @tparam SenderT UI控件类型
     * @tparam SignalArgs 信号参数包
     * @param sender UI控件
     * @param signal 信号函数指针 &QPushButton::clicked
     * @param func 回调lambda，捕获PTR包装指针访问ViewModel/控件
     */
    template<typename SenderT, typename ...SignalArgs>
    void bindCommand(SenderT* sender,
                     void(SenderT::*signal)(SignalArgs...),
                     std::function<void(SignalArgs...)> func)
    {
        if (!sender || !func)
            return;
        connect(sender, signal, this, func);
    }

private:
    explicit DataBind(QObject *parent = nullptr):QObject(parent){}
};

//==================== 便捷宏 ====================
/**
 * @brief PROP_NAME 将属性名转为编译期字符串
 * 要求宏参数名称必须和ViewModel Q_PROPERTY属性名完全一致
 * @example PROP_NAME(userName) → "userName"
 */
#define PROP_NAME(prop) #prop
/**
 * @brief PTR 快速生成QPointer智能指针，防止lambda捕获野指针
 * @example auto edit = PTR(ui->lineEdit);
 */
#define PTR(ptr) QPointer<decltype(ptr)>(ptr)
/**
 * @brief BIND 快捷调用DataBind::instance()->bind
 * @example BIND(vm, PROP_NAME(userName), edit, "text", &QLineEdit::textChanged, true);
 */
#define BIND(vm, vmProp, sender, widgetProp, signal, twoWay) \
    DataBind::instance()->bind(vm, vmProp, sender, widgetProp, signal, twoWay)
/**
 * @brief BIND_LAMBDA 快捷调用bindLambda
 */
#define BIND_LAMBDA(vm, vmProp, sender, signal, vmToUi, uiToVm) \
    DataBind::instance()->bindLambda(vm, vmProp, sender, signal, vmToUi, uiToVm)
/**
 * @brief BIND_COMMAND 快捷调用bindCommand
 */
#define BIND_COMMAND(sender, signal, func) \
    DataBind::instance()->bindCommand(sender, signal, func)

/**
 * @brief BIND_ONE_WAY 单向绑定宏，ViewModel属性单向同步UI，无需UI信号
 */
#define BIND_ONE_WAY(vm, vmProp, sender, widgetProp) \
    DataBind::instance()->bind(vm, vmProp, sender, widgetProp)

/**
 * @brief BIND_LAMBDA_ONE_WAY 单向lambda绑定宏，不需要UI信号
 */
#define BIND_LAMBDA_ONE_WAY(vm, vmProp, sender, vmToUi) \
    DataBind::instance()->bindLambda(vm, vmProp, sender, vmToUi)

#endif // DATABIND_H
