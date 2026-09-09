/**
 * @file CommandLineParser.h
 * @brief 命令行参数解析器 - 优雅解析命令行参数
 *
 * 核心功能：
 * 1. 声明式参数定义（类型、默认值、必填等）
 * 2. 自动生成帮助信息（--help）
 * 3. 支持短选项（-p）和长选项（--port）
 * 4. 自动类型转换（字符串→整数、布尔、浮点数）
 * 5. 支持子命令（如 git commit -m "msg"）
 * 6. 参数校验（必填、范围、枚举等）
 *
 * 使用场景：
 * 1. 服务器程序：./server --port 8080 --workers 4
 * 2. 数据处理工具：./convert --input data.csv --output data.json
 * 3. 备份工具：./backup --source /home --target /backup
 * 4. 测试工具：./test --filter "TestLogin" --verbose
 * 5. 脚本工具：./deploy --env production --version 1.2.3
 */

#ifndef COMMANDLINEPARSER_H
#define COMMANDLINEPARSER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QList>
#include <QMap>
#include <QDebug>
#include <QVariant>
#include <functional>

/**
 * @brief 参数类型枚举
 */
enum class ArgType {
    String,     // 字符串
    Int,        // 整数
    Bool,       // 布尔值（不需要值，有则true）
    Float,      // 浮点数
    Double,     // 双精度浮点数
    List,       // 列表
    Enum        // 枚举（预定义值列表）
};

/**
 * @brief 参数选项类 - 定义单个命令行参数
 */
class CommandOption {
public:
    CommandOption(const QString& longName, const QString& shortName = "")
        : m_longName(longName)
        , m_shortName(shortName)
        , m_type(ArgType::String)
        , m_required(false)
        , m_hasDefault(false)
        , m_multiValue(false)
        , m_hasValue(true) {
    }
    CommandOption()
            : m_type(ArgType::String)
            , m_required(false)
            , m_hasDefault(false)
            , m_multiValue(false)
            , m_hasValue(true) {
        }

    // ========== 设置方法（链式调用） ==========

    /**
     * @brief 设置参数类型为字符串
     */
    CommandOption& isString() {
        m_type = ArgType::String;
        return *this;
    }

    /**
     * @brief 设置参数类型为整数
     */
    CommandOption& isInt() {
        m_type = ArgType::Int;
        return *this;
    }

    /**
     * @brief 设置参数类型为布尔值（不需要额外参数值）
     * 例如：--debug 表示 debug=true
     */
    CommandOption& isBool() {
        m_type = ArgType::Bool;
        m_hasValue = false;  // 布尔参数不需要值
        return *this;
    }

    /**
     * @brief 设置参数类型为浮点数
     */
    CommandOption& isFloat() {
        m_type = ArgType::Float;
        return *this;
    }

    /**
     * @brief 设置参数类型为双精度浮点数
     */
    CommandOption& isDouble() {
        m_type = ArgType::Double;
        return *this;
    }

    /**
     * @brief 设置参数类型为列表
     */
    CommandOption& isList() {
        m_type = ArgType::List;
        m_multiValue = true;
        return *this;
    }

    /**
     * @brief 设置参数为枚举类型
     * @param values 允许的枚举值列表
     */
    CommandOption& isEnum(const QStringList& values) {
        m_type = ArgType::Enum;
        m_enumValues = values;
        return *this;
    }

    /**
     * @brief 设置默认值
     */
    CommandOption& defaultValue(const QVariant& value) {
        m_defaultValue = value;
        m_hasDefault = true;
        return *this;
    }

    /**
     * @brief 标记为必填参数
     */
    CommandOption& required() {
        m_required = true;
        return *this;
    }

    /**
     * @brief 设置参数描述（用于帮助信息）
     */
    CommandOption& description(const QString& desc) {
        m_description = desc;
        return *this;
    }

    /**
     * @brief 设置是否允许多个值
     */
    CommandOption& allowMultiple() {
        m_multiValue = true;
        return *this;
    }

    /**
     * @brief 设置值范围（仅对数值类型有效）
     */
    CommandOption& range(double min, double max) {
        m_minValue = min;
        m_maxValue = max;
        return *this;
    }

    /**
     * @brief 添加验证器
     */
    CommandOption& validator(std::function<bool(const QVariant&)> validator) {
        m_validator = validator;
        return *this;
    }

    // ========== 获取方法 ==========

    QString longName() const { return m_longName; }
    QString shortName() const { return m_shortName; }
    ArgType type() const { return m_type; }
    bool isRequired() const { return m_required; }
    bool hasDefault() const { return m_hasDefault; }
    QVariant defaultValue() const { return m_defaultValue; }
    QString description() const { return m_description; }
    QStringList enumValues() const { return m_enumValues; }
    bool hasValue() const { return m_hasValue; }

    /**
     * @brief 验证参数值是否合法
     */
    bool validate(const QVariant& value, QString& error) const {
        // 枚举校验
        if (m_type == ArgType::Enum) {
            if (!m_enumValues.contains(value.toString())) {
                error = QString("值 '%1' 不在允许范围内: %2")
                        .arg(value.toString())
                        .arg(m_enumValues.join(", "));
                return false;
            }
        }

        // 数值范围校验
        if (m_type == ArgType::Int || m_type == ArgType::Float || m_type == ArgType::Double) {
            double num = value.toDouble();
            if (m_minValue.has_value() && num < m_minValue.value()) {
                error = QString("值 %1 小于最小值 %2")
                        .arg(num)
                        .arg(m_minValue.value());
                return false;
            }
            if (m_maxValue.has_value() && num > m_maxValue.value()) {
                error = QString("值 %1 大于最大值 %2")
                        .arg(num)
                        .arg(m_maxValue.value());
                return false;
            }
        }

        // 自定义验证器
        if (m_validator) {
            if (!m_validator(value)) {
                error = "自定义验证失败";
                return false;
            }
        }

        return true;
    }

    /**
     * @brief 生成帮助文本
     */
    QString helpText() const {
        QString text;

        // 选项名称
        if (!m_shortName.isEmpty()) {
            text += QString("  -%1, --%2").arg(m_shortName).arg(m_longName);
        } else {
            text += QString("      --%1").arg(m_longName);
        }

        // 参数值占位符
        if (m_hasValue) {
            switch (m_type) {
                case ArgType::Int:    text += " <int>"; break;
                case ArgType::Float:  text += " <float>"; break;
                case ArgType::Double: text += " <double>"; break;
                case ArgType::String: text += " <string>"; break;
                case ArgType::List:   text += " <list>"; break;
                case ArgType::Enum:   text += " <" + m_enumValues.join("|") + ">"; break;
                default: break;
            }
        }

        // 必填标识
        if (m_required) {
            text += " (必填)";
        }

        // 默认值
        if (m_hasDefault) {
            text += " (默认: " + m_defaultValue.toString() + ")";
        }

        // 描述
        if (!m_description.isEmpty()) {
            text += "\n        " + m_description;
        }

        return text;
    }

private:
    QString m_longName;          // 长选项名（如 "port"）
    QString m_shortName;         // 短选项名（如 "p"）
    ArgType m_type;              // 参数类型
    bool m_required;             // 是否必填
    bool m_hasDefault;           // 是否有默认值
    QVariant m_defaultValue;     // 默认值
    bool m_multiValue;           // 是否允许多个值
    bool m_hasValue;             // 是否带值（布尔类型不带）
    QString m_description;       // 描述
    QStringList m_enumValues;    // 枚举值列表
    std::optional<double> m_minValue;  // 最小值
    std::optional<double> m_maxValue;  // 最大值
    std::function<bool(const QVariant&)> m_validator;  // 自定义验证器
};

/**
 * @brief 子命令类 - 支持git风格子命令
 */
class Command {
public:
    Command(const QString& name, const QString& description = "")
        : m_name(name), m_description(description) {}
    Command() : m_name(""), m_description("") {}
    /**
     * @brief 添加子命令选项
     */
    Command& addOption(CommandOption option) {
        m_options[option.longName()] = option;
        if (!option.shortName().isEmpty()) {
            m_shortOptions[option.shortName()] = option.longName();
        }
        return *this;
    }

    /**
     * @brief 添加子命令选项（便利方法）
     */
    Command& addOption(const QString& longName, const QString& shortName = "",
                       const QString& description = "") {
        CommandOption option(longName, shortName);
        option.description(description);
        m_options[longName] = option;
        if (!shortName.isEmpty()) {
            m_shortOptions[shortName] = longName;
        }
        return *this;
    }

    QString name() const { return m_name; }
    QString description() const { return m_description; }
    QHash<QString, CommandOption> options() const { return m_options; }

    /**
     * @brief 获取选项
     */
    CommandOption* getOption(const QString& name) {
        if (m_options.contains(name)) {
            return &m_options[name];
        }
        if (m_shortOptions.contains(name)) {
            QString longName = m_shortOptions[name];
            return &m_options[longName];
        }
        return nullptr;
    }

    /**
     * @brief 生成帮助文本
     */
    QString helpText() const {
        QString text = QString("\n命令: %1").arg(m_name);
        if (!m_description.isEmpty()) {
            text += " - " + m_description;
        }
        text += "\n\n选项:\n";

        for (const auto& option : m_options) {
            text += option.helpText() + "\n";
        }

        return text;
    }

private:
    QString m_name;
    QString m_description;
    QHash<QString, CommandOption> m_options;
    QHash<QString, QString> m_shortOptions;  // 短选项 → 长选项
};

/**
 * @brief 命令行参数解析器主类
 */
class CommandLineParser : public QObject {
    Q_OBJECT
public:
    CommandLineParser(QObject* parent = nullptr) : QObject(parent) {}

    // ========== 选项定义接口 ==========

    /**
     * @brief 添加一个选项
     * @param longName 长选项名（如 "port"）
     * @param shortName 短选项名（如 "p"）
     * @param description 描述（用于帮助信息）
     * @return CommandOption引用，可以链式调用设置属性
     */
    CommandOption& addOption(const QString& longName,
                            const QString& shortName = "",
                            const QString& description = "") {
        if (m_options.contains(longName)) {
            qWarning() << "[CommandLineParser] 选项已存在:" << longName;
            return m_options[longName];
        }

        CommandOption option(longName, shortName);
        option.description(description);
        m_options[longName] = option;

        if (!shortName.isEmpty()) {
            m_shortOptions[shortName] = longName;
        }

        return m_options[longName];
    }

    /**
     * @brief 添加子命令
     */
    Command& addCommand(const QString& name, const QString& description = "") {
        if (m_commands.contains(name)) {
            qWarning() << "[CommandLineParser] 命令已存在:" << name;
            return m_commands[name];
        }

        Command cmd(name, description);
        m_commands[name] = cmd;
        return m_commands[name];
    }

    /**
     * @brief 设置程序名称（用于帮助信息）
     */
    void setProgramName(const QString& name) {
        m_programName = name;
    }

    /**
     * @brief 设置程序描述（用于帮助信息）
     */
    void setProgramDescription(const QString& desc) {
        m_programDescription = desc;
    }

    /**
     * @brief 设置版本号
     */
    void setVersion(const QString& version) {
        m_version = version;
    }

    // ========== 解析接口 ==========

    /**
     * @brief 解析命令行参数
     * @param argc 参数个数（main函数传入）
     * @param argv 参数数组（main函数传入）
     * @return true表示解析成功，false表示失败（已自动输出错误信息）
     */
    bool parse(int argc, char* argv[]) {
        QStringList args;
        for (int i = 0; i < argc; i++) {
            args << QString::fromLocal8Bit(argv[i]);
        }
        return parse(args);
    }

    /**
     * @brief 解析命令行参数（QStringList版本）
     */
    bool parse(const QStringList& args) {
        if (args.isEmpty()) {
            return false;
        }

        // 设置程序名
        if (m_programName.isEmpty()) {
            m_programName = args[0];
        }

        // 检查是否显示帮助
        if (args.contains("--help") || args.contains("-h")) {
            printHelp();
            return false;
        }

        // 检查是否显示版本
        if (args.contains("--version") || args.contains("-v")) {
            printVersion();
            return false;
        }

        // 检测子命令
        if (args.size() > 1 && !args[1].startsWith('-')) {
            m_currentCommand = args[1];
            if (!m_commands.contains(m_currentCommand)) {
                qCritical() << "[CommandLineParser] 未知命令:" << m_currentCommand;
                printHelp();
                return false;
            }
        }

        // 解析参数
        m_parsedValues.clear();
        QHash<QString, QStringList> multiValues;

        int i = 1;
        if (!m_currentCommand.isEmpty()) {
            i = 2;  // 跳过命令名
        }

        while (i < args.size()) {
            QString arg = args[i];

            // 检查是否是选项
            if (!arg.startsWith('-')) {
                // 位置参数（未实现，可以扩展）
                i++;
                continue;
            }

            // 去除前导横杠
            QString name = arg;
            bool isShort = false;
            if (arg.startsWith("--")) {
                name = arg.mid(2);
            } else if (arg.startsWith('-')) {
                name = arg.mid(1);
                isShort = true;
            }

            // 处理 "key=value" 格式
            QString value;
            if (name.contains('=')) {
                QStringList parts = name.split('=');
                name = parts[0];
                value = parts[1];
            }

            // 查找选项定义
            CommandOption* option = findOption(name);
            if (!option) {
                qCritical() << "[CommandLineParser] 未知选项:" << arg;
                printHelp();
                return false;
            }

            // 获取值
            if (option->hasValue()) {
                if (value.isEmpty()) {
                    // 下一个参数是值
                    if (i + 1 >= args.size() || args[i + 1].startsWith('-')) {
                        qCritical() << "[CommandLineParser] 缺少参数值:" << arg;
                        return false;
                    }
                    value = args[i + 1];
                    i++;
                }

                // 存储值
                if (option->type() == ArgType::List) {
                    multiValues[name].append(value);
                } else {
                    m_parsedValues[name] = value;
                }
            } else {
                // 布尔类型，值为 true
                m_parsedValues[name] = true;
            }

            i++;
        }

        // 处理多值选项
        for (auto it = multiValues.begin(); it != multiValues.end(); ++it) {
            m_parsedValues[it.key()] = it.value();
        }

        // 验证必填参数
        QHash<QString, CommandOption> options = getCurrentOptions();
        for (const auto& option : options) {
            if (option.isRequired() && !m_parsedValues.contains(option.longName())) {
                qCritical() << "[CommandLineParser] 缺少必填参数: --" << option.longName();
                printHelp();
                return false;
            }
        }

        // 应用默认值
        for (const auto& option : options) {
            if (!m_parsedValues.contains(option.longName()) && option.hasDefault()) {
                m_parsedValues[option.longName()] = option.defaultValue();
            }
        }

        // 验证参数值
        for (auto it = m_parsedValues.begin(); it != m_parsedValues.end(); ++it) {
            CommandOption* option = findOption(it.key());
            if (option) {
                QString error;
                if (!option->validate(it.value(), error)) {
                    qCritical() << "[CommandLineParser] 参数验证失败 --"
                                << it.key() << ":" << error;
                    return false;
                }
            }
        }

        return true;
    }

    // ========== 获取值接口 ==========

    /**
     * @brief 检查参数是否存在
     */
    bool has(const QString& name) const {
        return m_parsedValues.contains(name);
    }

    /**
     * @brief 获取参数值（模板版本，自动转换类型）
     */
    template<typename T>
    T get(const QString& name, const T& defaultValue = T()) const {
        if (!m_parsedValues.contains(name)) {
            return defaultValue;
        }

        QVariant value = m_parsedValues[name];

        if (std::is_same<T, QString>::value) {
            return value.toString();
        } else if (std::is_same<T, int>::value) {
            return value.toInt();
        } else if (std::is_same<T, bool>::value) {
            return value.toBool();
        } else if (std::is_same<T, double>::value) {
            return value.toDouble();
        } else if (std::is_same<T, float>::value) {
            return value.toFloat();
        } else if (std::is_same<T, QStringList>::value) {
            return value.toStringList();
        }

        return defaultValue;
    }

    /**
     * @brief 获取参数值（QVariant版本）
     */
    QVariant getValue(const QString& name, const QVariant& defaultValue = QVariant()) const {
        if (m_parsedValues.contains(name)) {
            return m_parsedValues[name];
        }
        return defaultValue;
    }

    /**
     * @brief 获取当前子命令
     */
    QString currentCommand() const {
        return m_currentCommand;
    }

    /**
     * @brief 检查是否在子命令中
     */
    bool hasCommand() const {
        return !m_currentCommand.isEmpty();
    }

    // ========== 打印信息接口 ==========

    /**
     * @brief 打印帮助信息
     */
    void printHelp() const {
        qDebug().noquote() << generateHelpText();
    }

    /**
     * @brief 生成帮助文本
     */
    QString generateHelpText() const {
        QString text;

        // 程序名
        text += QString("用法: %1").arg(m_programName);

        // 子命令
        if (!m_commands.isEmpty()) {
            text += " <命令>";
        }

        text += " [选项]\n\n";

        // 描述
        if (!m_programDescription.isEmpty()) {
            text += m_programDescription + "\n\n";
        }

        // 子命令列表
        if (!m_commands.isEmpty()) {
            text += "命令:\n";
            for (const auto& cmd : m_commands) {
                text += QString("  %1    %2\n")
                        .arg(cmd.name(), -15)
                        .arg(cmd.description());
            }
            text += "\n";
        }

        // 选项列表
        QHash<QString, CommandOption> options = getCurrentOptions();
        if (!options.isEmpty()) {
            text += "选项:\n";
            for (const auto& option : options) {
                text += option.helpText() + "\n";
            }
        }

        return text;
    }

    /**
     * @brief 打印版本信息
     */
    void printVersion() const {
        qDebug() << m_programName << "版本:" << m_version;
    }

private:
    /**
     * @brief 查找选项（先查当前命令，再查全局）
     */
    CommandOption* findOption(const QString& name) {
        // 先在当前命令中查找
        if (!m_currentCommand.isEmpty() && m_commands.contains(m_currentCommand)) {
            auto* option = m_commands[m_currentCommand].getOption(name);
            if (option) {
                return option;
            }
        }

        // 再在全局选项中查找
        if (m_options.contains(name)) {
            return &m_options[name];
        }

        // 通过短选项查找
        if (m_shortOptions.contains(name)) {
            QString longName = m_shortOptions[name];
            if (m_options.contains(longName)) {
                return &m_options[longName];
            }
        }

        return nullptr;
    }

    /**
     * @brief 获取当前有效的选项列表（全局+当前命令）
     */
    QHash<QString, CommandOption> getCurrentOptions() const {
        QHash<QString, CommandOption> result = m_options;

        if (!m_currentCommand.isEmpty() && m_commands.contains(m_currentCommand)) {
            for (const auto& option : m_commands[m_currentCommand].options()) {
                result[option.longName()] = option;
            }
        }

        return result;
    }

private:
    QString m_programName;                       // 程序名称
    QString m_programDescription;               // 程序描述
    QString m_version;                          // 版本号

    QHash<QString, CommandOption> m_options;    // 所有选项
    QHash<QString, QString> m_shortOptions;     // 短选项 → 长选项映射

    QHash<QString, Command> m_commands;         // 子命令

    QHash<QString, QVariant> m_parsedValues;    // 解析后的值
    QString m_currentCommand;                   // 当前子命令
};

#endif // COMMANDLINEPARSER_H
