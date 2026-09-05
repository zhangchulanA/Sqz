/**
 * @file    SqzModel.h
 * @brief   轻量级数据模型基类 - 单文件版本
 * @details 基于Qt5.12实现，支持JSON序列化/反序列化、本地文件持久化
 *          子类仅需通过宏声明字段，自动生成get/set方法与序列化逻辑
 *
 * 使用示例：
 * @code
 * // 定义地址模型
 * BEGIN_MODEL(AddressModel)
 *     SQZ_FIELD_QSTRING(city)
 *     SQZ_FIELD_INT(number)
 * END_MODEL
 *
 * // 定义用户模型（嵌套地址模型）
 * BEGIN_MODEL(UserModel)
 *     SQZ_FIELD_INT(id)
 *     SQZ_FIELD_QSTRING(name)
 *     SQZ_FIELD_SQZMODEL(address, AddressModel)
 * END_MODEL
 *
 * // 业务使用
 * UserModel user;
 * user.setId(1001);
 * user.setName("ZhangSan");
 * user.address().setCity("Beijing");
 * user.saveToFile("./user.json"); // 保存到文件
 * user.loadFromFile("./user.json"); // 从文件加载
 * @endcode
 */

#pragma once

#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QFile>
#include <QList>
#include <QString>
#include <QByteArray>
#include <functional>
#include <string>

/**
 * @class SqzModel
 * @brief 所有数据模型的基类
 *
 * 提供统一的序列化、反序列化、文件读写能力
 * 子类通过宏自动注册字段，无需手动实现序列化逻辑
 */
class SqzModel
{
public:
    /**
     * @brief 默认构造函数，字段自动零初始化
     */
    SqzModel() = default;

    /**
     * @brief 虚析构函数，保证子类对象正确析构
     */
    virtual ~SqzModel() = default;

    /**
     * @brief  将模型序列化为 QJsonObject
     * @return 序列化完成的JSON对象
     */
    QJsonObject toJson() const
    {
        QJsonObject obj;
        for (const auto& field : m_fields) {
            obj[field.name] = field.getter();
        }
        return obj;
    }

    /**
     * @brief  从 QJsonObject 反序列化填充模型
     * @param  json 输入JSON对象
     * @return 反序列化是否成功
     * @note   缺失字段保持默认值，不返回错误
     */
    bool fromJson(const QJsonObject& json)
    {
        for (const auto& field : m_fields) {
            if (json.contains(field.name)) {
                field.setter(json[field.name]);
            }
        }
        return true;
    }

    /**
     * @brief  将模型保存为本地JSON文件（格式化缩进）
     * @param  filePath 文件路径
     * @return 保存是否成功
     */
    bool saveToFile(const QString& filePath) const
    {
        QJsonDocument doc(toJson());
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return false;
        }
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        return true;
    }

    /**
     * @brief  从本地JSON文件加载模型
     * @param  filePath 文件路径
     * @return 加载是否成功
     */
    bool loadFromFile(const QString& filePath)
    {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return false;
        }
        QByteArray data = file.readAll();
        file.close();

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            return false;
        }

        return fromJson(doc.object());
    }

    /**
     * @brief 注册字段（内部接口，由宏自动调用，请勿手动调用）
     * @param name   字段名称
     * @param getter 字段读回调
     * @param setter 字段写回调
     */
    void registerField(const QString& name,
                       std::function<QJsonValue()> getter,
                       std::function<void(const QJsonValue&)> setter)
    {
        m_fields.append({name, std::move(getter), std::move(setter)});
    }

protected:
    /**
     * @struct SqzFieldInfo
     * @brief  字段元信息结构体
     *
     * 存储每个字段的名称与读写回调，基类通过该列表统一遍历序列化
     */
    struct SqzFieldInfo
    {
        QString name;
        std::function<QJsonValue()> getter;
        std::function<void(const QJsonValue&)> setter;
    };

private:
    QList<SqzFieldInfo> m_fields; ///< 所有已注册字段的元数据列表
};

// ==================================================
// 模型定义起止宏
// ==================================================

/**
 * @def   BEGIN_MODEL(ModelClass)
 * @brief 开始定义模型类，自动继承 SqzModel 并继承基类构造
 * @param ModelClass 模型类的类名
 */
#define BEGIN_MODEL(ModelClass) \
class ModelClass : public SqzModel { \
public: \
    using SqzModel::SqzModel;

/**
 * @def   END_MODEL
 * @brief 结束模型类定义
 */
#define END_MODEL \
};

// ==================================================
// 基础数值类型字段宏
// ==================================================

/**
 * @def   SQZ_FIELD_INT(Name)
 * @brief 声明 int 类型字段
 * @param Name 字段名，自动生成 Name() / setName() 方法
 */
#define SQZ_FIELD_INT(Name) \
private: \
    int m_##Name{}; \
    struct _Reg_##Name { \
        _Reg_##Name(SqzModel* base, int* ptr, const char* name) { \
            base->registerField(name, \
                [ptr]() -> QJsonValue { return QJsonValue(*ptr); }, \
                [ptr](const QJsonValue& v) { *ptr = v.toInt(); } \
            ); \
        } \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    int Name() const { return m_##Name; } \
    void set##Name(int val) { m_##Name = val; }

/**
 * @def   SQZ_FIELD_UINT(Name)
 * @brief 声明 unsigned int 类型字段
 * @param Name 字段名
 */
#define SQZ_FIELD_UINT(Name) \
private: \
    unsigned int m_##Name{}; \
    struct _Reg_##Name { \
        _Reg_##Name(SqzModel* base, unsigned int* ptr, const char* name) { \
            base->registerField(name, \
                [ptr]() -> QJsonValue { return QJsonValue(static_cast<int>(*ptr)); }, \
                [ptr](const QJsonValue& v) { *ptr = v.toVariant().toUInt(); } \
            ); \
        } \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    unsigned int Name() const { return m_##Name; } \
    void set##Name(unsigned int val) { m_##Name = val; }

/**
 * @def   SQZ_FIELD_LLONG(Name)
 * @brief 声明 long long 类型字段
 * @param Name 字段名
 * @note  JSON数值精度上限为2^53，超出范围建议用字符串存储
 */
#define SQZ_FIELD_LLONG(Name) \
private: \
    long long m_##Name{}; \
    struct _Reg_##Name { \
        _Reg_##Name(SqzModel* base, long long* ptr, const char* name) { \
            base->registerField(name, \
                [ptr]() -> QJsonValue { return QJsonValue(static_cast<qint64>(*ptr)); }, \
                [ptr](const QJsonValue& v) { *ptr = v.toVariant().toLongLong(); } \
            ); \
        } \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    long long Name() const { return m_##Name; } \
    void set##Name(long long val) { m_##Name = val; }

/**
 * @def   SQZ_FIELD_ULLONG(Name)
 * @brief 声明 unsigned long long 类型字段
 * @param Name 字段名
 */
#define SQZ_FIELD_ULLONG(Name) \
private: \
    unsigned long long m_##Name{}; \
    struct _Reg_##Name { \
        _Reg_##Name(SqzModel* base, unsigned long long* ptr, const char* name) { \
            base->registerField(name, \
                [ptr]() -> QJsonValue { return QJsonValue(static_cast<quint64>(*ptr)); }, \
                [ptr](const QJsonValue& v) { *ptr = v.toVariant().toULongLong(); } \
            ); \
        } \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    unsigned long long Name() const { return m_##Name; } \
    void set##Name(unsigned long long val) { m_##Name = val; }

// ==================================================
// 浮点类型字段宏
// ==================================================

/**
 * @def   SQZ_FIELD_FLOAT(Name)
 * @brief 声明 float 类型字段
 * @param Name 字段名
 */
#define SQZ_FIELD_FLOAT(Name) \
private: \
    float m_##Name{}; \
    struct _Reg_##Name { \
        _Reg_##Name(SqzModel* base, float* ptr, const char* name) { \
            base->registerField(name, \
                [ptr]() -> QJsonValue { return QJsonValue(static_cast<double>(*ptr)); }, \
                [ptr](const QJsonValue& v) { *ptr = static_cast<float>(v.toDouble()); } \
            ); \
        } \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    float Name() const { return m_##Name; } \
    void set##Name(float val) { m_##Name = val; }

/**
 * @def   SQZ_FIELD_DOUBLE(Name)
 * @brief 声明 double 类型字段
 * @param Name 字段名
 */
#define SQZ_FIELD_DOUBLE(Name) \
private: \
    double m_##Name{}; \
    struct _Reg_##Name { \
        _Reg_##Name(SqzModel* base, double* ptr, const char* name) { \
            base->registerField(name, \
                [ptr]() -> QJsonValue { return QJsonValue(*ptr); }, \
                [ptr](const QJsonValue& v) { *ptr = v.toDouble(); } \
            ); \
        } \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    double Name() const { return m_##Name; } \
    void set##Name(double val) { m_##Name = val; }

// ==================================================
// 布尔与字符类型字段宏
// ==================================================

/**
 * @def   SQZ_FIELD_BOOL(Name)
 * @brief 声明 bool 类型字段
 * @param Name 字段名
 */
#define SQZ_FIELD_BOOL(Name) \
private: \
    bool m_##Name{}; \
    struct _Reg_##Name { \
        _Reg_##Name(SqzModel* base, bool* ptr, const char* name) { \
            base->registerField(name, \
                [ptr]() -> QJsonValue { return QJsonValue(*ptr); }, \
                [ptr](const QJsonValue& v) { *ptr = v.toBool(); } \
            ); \
        } \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    bool Name() const { return m_##Name; } \
    void set##Name(bool val) { m_##Name = val; }

/**
 * @def   SQZ_FIELD_CHAR(Name)
 * @brief 声明 char 类型字段
 * @param Name 字段名
 */
#define SQZ_FIELD_CHAR(Name) \
private: \
    char m_##Name{}; \
    struct _Reg_##Name { \
        _Reg_##Name(SqzModel* base, char* ptr, const char* name) { \
            base->registerField(name, \
                [ptr]() -> QJsonValue { return QJsonValue(QString(*ptr)); }, \
                [ptr](const QJsonValue& v) { *ptr = v.toVariant().toChar().toLatin1(); } \
            ); \
        } \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    char Name() const { return m_##Name; } \
    void set##Name(char val) { m_##Name = val; }

/**
 * @def   SQZ_FIELD_UCHAR(Name)
 * @brief 声明 unsigned char 类型字段
 * @param Name 字段名
 */
#define SQZ_FIELD_UCHAR(Name) \
private: \
    unsigned char m_##Name{}; \
    struct _Reg_##Name { \
        _Reg_##Name(SqzModel* base, unsigned char* ptr, const char* name) { \
            base->registerField(name, \
                [ptr]() -> QJsonValue { return QJsonValue(static_cast<int>(*ptr)); }, \
                [ptr](const QJsonValue& v) { *ptr = static_cast<unsigned char>(v.toVariant().toUInt()); } \
            ); \
        } \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    unsigned char Name() const { return m_##Name; } \
    void set##Name(unsigned char val) { m_##Name = val; }

// ==================================================
// 字符串类型字段宏
// ==================================================

/**
 * @def   SQZ_FIELD_STRING(Name)
 * @brief 声明 std::string 类型字段
 * @param Name 字段名
 */
#define SQZ_FIELD_STRING(Name) \
private: \
    std::string m_##Name{}; \
    struct _Reg_##Name { \
        _Reg_##Name(SqzModel* base, std::string* ptr, const char* name) { \
            base->registerField(name, \
                [ptr]() -> QJsonValue { return QString::fromStdString(*ptr); }, \
                [ptr](const QJsonValue& v) { *ptr = v.toString().toStdString(); } \
            ); \
        } \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    const std::string& Name() const { return m_##Name; } \
    void set##Name(const std::string& val) { m_##Name = val; }

/**
 * @def   SQZ_FIELD_QSTRING(Name)
 * @brief 声明 QString 类型字段
 * @param Name 字段名
 */
#define SQZ_FIELD_QSTRING(Name) \
private: \
    QString m_##Name{}; \
    struct _Reg_##Name { \
        _Reg_##Name(SqzModel* base, QString* ptr, const char* name) { \
            base->registerField(name, \
                [ptr]() -> QJsonValue { return QJsonValue(*ptr); }, \
                [ptr](const QJsonValue& v) { *ptr = v.toString(); } \
            ); \
        } \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    const QString& Name() const { return m_##Name; } \
    void set##Name(const QString& val) { m_##Name = val; }

// ==================================================
// Qt扩展类型字段宏
// ==================================================

/**
 * @def   SQZ_FIELD_QBYTEARRAY(Name)
 * @brief 声明 QByteArray 类型字段（序列化时自动Base64编码）
 * @param Name 字段名
 */
#define SQZ_FIELD_QBYTEARRAY(Name) \
private: \
    QByteArray m_##Name{}; \
    struct _Reg_##Name { \
        _Reg_##Name(SqzModel* base, QByteArray* ptr, const char* name) { \
            base->registerField(name, \
                [ptr]() -> QJsonValue { return QLatin1String(ptr->toBase64()); }, \
                [ptr](const QJsonValue& v) { *ptr = QByteArray::fromBase64(v.toString().toLatin1()); } \
            ); \
        } \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    const QByteArray& Name() const { return m_##Name; } \
    void set##Name(const QByteArray& val) { m_##Name = val; }

/**
 * @def   SQZ_FIELD_QJSONOBJECT(Name)
 * @brief 声明 QJsonObject 类型字段
 * @param Name 字段名
 */
#define SQZ_FIELD_QJSONOBJECT(Name) \
private: \
    QJsonObject m_##Name{}; \
    struct _Reg_##Name { \
        _Reg_##Name(SqzModel* base, QJsonObject* ptr, const char* name) { \
            base->registerField(name, \
                [ptr]() -> QJsonValue { return QJsonValue(*ptr); }, \
                [ptr](const QJsonValue& v) { *ptr = v.toObject(); } \
            ); \
        } \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    const QJsonObject& Name() const { return m_##Name; } \
    void set##Name(const QJsonObject& val) { m_##Name = val; }

// ==================================================
// 嵌套模型字段宏
// ==================================================

/**
 * @def   SQZ_FIELD_SQZMODEL(Name, ModelType)
 * @brief 声明嵌套的 SqzModel 子类字段，自动递归序列化
 * @param Name      字段名
 * @param ModelType 嵌套模型的类名（必须也是 SqzModel 子类）
 */
#define SQZ_FIELD_SQZMODEL(Name, ModelType) \
private: \
    ModelType m_##Name{}; \
    struct _Reg_##Name { \
        _Reg_##Name(SqzModel* base, ModelType* ptr, const char* name) { \
            base->registerField(name, \
                [ptr]() -> QJsonValue { return ptr->toJson(); }, \
                [ptr](const QJsonValue& v) { ptr->fromJson(v.toObject()); } \
            ); \
        } \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    const ModelType& Name() const { return m_##Name; } \
    ModelType& Name() { return m_##Name; } \
    void set##Name(const ModelType& val) { m_##Name = val; }
