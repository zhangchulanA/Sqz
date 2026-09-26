/**
 * @file    SqzModel.h
 * @brief   轻量级数据模型基类 - 单文件版本
 * @details 基于Qt5.12实现，支持JSON序列化/反序列化、本地文件持久化
 *          子类仅需通过宏声明字段，自动生成get/set方法与序列化逻辑
 *
 * 使用示例：
 * @code
 * BEGIN_MODEL(AddressModel)
 *     SQZ_FIELD_QSTRING(city)
 *     SQZ_FIELD_INT(number)
 * END_MODEL
 *
 * BEGIN_MODEL(UserModel)
 *     SQZ_FIELD_INT(id)
 *     SQZ_FIELD_QSTRING(name)
 *     SQZ_FIELD_SQZMODEL(address, AddressModel)
 * END_MODEL
 *
 * UserModel user;
 * user.setId(1001);
 * user.setName("ZhangSan");
 * user.address().setCity("Beijing");
 * user.saveToFile("./user.json");
 * user.loadFromFile("./user.json");
 * @endcode
 *
 * @note 拷贝/移动安全：字段 lambda 内部捕获的是成员偏移量（整数），
 *       而非绝对指针。因此拷贝、移动、跨对象调用都正确，
 *       且源对象销毁后副本仍可正常序列化。
 */

#pragma once

#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QFile>
#include <QList>
#include <QString>
#include <QByteArray>
#include <cstddef>
#include <functional>
#include <string>

namespace Sqz {

/**
 * @class SqzModel
 * @brief 所有数据模型的基类
 *
 * 提供统一的序列化、反序列化、文件读写能力。
 * 子类通过宏自动注册字段，无需手动实现序列化逻辑。
 *
 * 拷贝/移动语义：默认拷贝构造可用，字段 lambda 内部使用偏移量寻址，
 * 因此拷贝后的对象完全独立，操作互不影响。
 */
class SqzModel
{
public:
    SqzModel() = default;
    virtual ~SqzModel() = default;

    // ========== 拷贝 / 移动（默认即可，lambda 内已用偏移量寻址） ==========
    SqzModel(const SqzModel&) = default;
    SqzModel(SqzModel&&) noexcept = default;
    SqzModel& operator=(const SqzModel&) = default;
    SqzModel& operator=(SqzModel&&) noexcept = default;

    /**
     * @brief  将模型序列化为 QJsonObject
     * @return 序列化完成的JSON对象
     */
    QJsonObject toJson() const
    {
        QJsonObject obj;
        for (const auto& field : m_fields) {
            if (field.getter) {
                obj[field.name] = field.getter(this);
            }
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
            if (json.contains(field.name) && field.setter) {
                field.setter(this, json[field.name]);
            }
        }
        return true;
    }

    /**
     * @brief  将模型保存为本地JSON文件（格式化缩进）
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


    // ================================================================
    // QByteArray 序列化扩展（紧凑 JSON，二进制友好）
    // ================================================================

    /**
     * @brief  将模型序列化为紧凑JSON字节流
     * @return UTF-8 编码的紧凑 JSON 字节数组（无缩进，适合网络传输/存储）
     * @note   与 toJson() 数据等价，仅输出格式更紧凑；
     *         QByteArray 字段仍走 Base64，可安全嵌入字节流。
     */
    QByteArray toByteArray() const
    {
        return QJsonDocument(toJson()).toJson(QJsonDocument::Compact);
    }

    /**
     * @brief  从紧凑JSON字节流反序列化填充模型
     * @param  data UTF-8 编码的 JSON 字节数组
     * @return 反序列化是否成功
     * @note   缺失字段保持默认值，与 fromJson() 行为一致
     */
    bool fromByteArray(const QByteArray& data)
    {
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) {
            return false;
        }
        return fromJson(doc.object());
    }

    /**
     * @brief  将模型保存为本地二进制（紧凑JSON）文件
     * @param  filePath 目标文件路径
     * @return 是否保存成功
     * @note   与 saveToFile() 的区别：不写缩进、不按 Text 模式打开，
     *         文件体积更小，适合频繁读写。
     */
    bool saveToBinaryFile(const QString& filePath) const
    {
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        const qint64 written = file.write(toByteArray());
        file.close();
        return written >= 0;
    }

    /**
     * @brief  从本地二进制（紧凑JSON）文件加载模型
     * @param  filePath 源文件路径
     * @return 是否加载成功
     */
    bool loadFromBinaryFile(const QString& filePath)
    {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        const QByteArray data = file.readAll();
        file.close();
        return fromByteArray(data);
    }

    /**
     * @brief  序列化为 QVariant（QByteArray 载体），便于放入 Qt 信号槽/模型视图
     * @return 含紧凑 JSON 的 QByteArray 封装的 QVariant
     */
    QVariant toVariant() const
    {
        return QVariant(toByteArray());
    }

    /**
     * @brief  从 QVariant（QByteArray 载体）反序列化
     * @param  var 含紧凑 JSON 的 QVariant
     * @return 反序列化是否成功
     */
    bool fromVariant(const QVariant& var)
    {
        if (!var.canConvert<QByteArray>()) {
            return false;
        }
        return fromByteArray(var.toByteArray());
    }


    /**
     * @brief 注册字段（内部接口，由宏自动调用，请勿手动调用）
     * @param name   字段名称
     * @param getter 字段读回调，第一参数为对象自身指针
     * @param setter 字段写回调，第一参数为对象自身指针
     *
     * @note getter/setter 不再捕获对象指针，而是捕获成员偏移量，
     *       运行时通过 (const char*)self + offset 计算真实地址。
     */
    void registerField(const QString& name,
                       std::function<QJsonValue(const SqzModel*)> getter,
                       std::function<void(SqzModel*, const QJsonValue&)> setter)
    {
        m_fields.append({name, std::move(getter), std::move(setter)});
    }

protected:
    /**
     * @struct SqzFieldInfo
     * @brief  字段元信息结构体
     */
    struct SqzFieldInfo
    {
        QString name;
        std::function<QJsonValue(const SqzModel*)> getter;
        std::function<void(SqzModel*, const QJsonValue&)> setter;
    };

private:
    QList<SqzFieldInfo> m_fields; ///< 所有已注册字段的元数据列表
};

// ==================================================
// 模型定义起止宏
// ==================================================

/**
 * @def   BEGIN_MODEL(ModelClass)
 * @brief 开始定义模型类，自动继承 SqzModel
 * @param ModelClass 模型类的类名
 */
#define BEGIN_MODEL(ModelClass) \
class ModelClass : public SqzModel { \
public: \
    using SqzModel::SqzModel; \
    ModelClass() = default; \
    ModelClass(const ModelClass&) = default; \
    ModelClass(ModelClass&&) noexcept = default; \
    ModelClass& operator=(const ModelClass&) = default; \
    ModelClass& operator=(ModelClass&&) noexcept = default;

/**
 * @def   END_MODEL
 * @brief 结束模型类定义
 */
#define END_MODEL };

// ==================================================
// 内部工具宏：生成字段注册器的公共骨架
// ==================================================

// 说明：
// - offset 在构造时计算一次：&m_##Name 相对 this 的字节偏移
// - getter/setter 只捕获 offset（整数），不捕获任何指针
// - 拷贝构造为空实现：避免默认拷贝构造时重新注册造成 m_fields 重复
#define SQZ_FIELD_REG_CTOR_BEGIN(Name, Type) \
    _Reg_##Name(SqzModel* base, Type* ptr, const char* name) { \
        const ptrdiff_t offset = \
            reinterpret_cast<char*>(ptr) - reinterpret_cast<char*>(base);

#define SQZ_FIELD_REG_CTOR_END(Name) \
    } \
    _Reg_##Name(const _Reg_##Name&) noexcept {} \
    _Reg_##Name& operator=(const _Reg_##Name&) noexcept { return *this; }

// ==================================================
// 基础数值类型字段宏
// ==================================================

/**
 * @def   SQZ_FIELD_INT(Name)
 * @brief 声明 int 类型字段
 */
#define SQZ_FIELD_INT(Name) \
private: \
    int m_##Name{}; \
    struct _Reg_##Name { \
        SQZ_FIELD_REG_CTOR_BEGIN(Name, int) \
            base->registerField(name, \
                [offset](const SqzModel* self) -> QJsonValue { \
                    const int* p = reinterpret_cast<const int*>( \
                        reinterpret_cast<const char*>(self) + offset); \
                    return QJsonValue(*p); \
                }, \
                [offset](SqzModel* self, const QJsonValue& v) { \
                    int* p = reinterpret_cast<int*>( \
                        reinterpret_cast<char*>(self) + offset); \
                    *p = v.toInt(); \
                } \
            ); \
        SQZ_FIELD_REG_CTOR_END(Name) \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    int Name() const { return m_##Name; } \
    void set##Name(int val) { m_##Name = val; }

/**
 * @def   SQZ_FIELD_UINT(Name)
 * @brief 声明 unsigned int 类型字段
 */
#define SQZ_FIELD_UINT(Name) \
private: \
    unsigned int m_##Name{}; \
    struct _Reg_##Name { \
        SQZ_FIELD_REG_CTOR_BEGIN(Name, unsigned int) \
            base->registerField(name, \
                [offset](const SqzModel* self) -> QJsonValue { \
                    const unsigned int* p = reinterpret_cast<const unsigned int*>( \
                        reinterpret_cast<const char*>(self) + offset); \
                    return QJsonValue(static_cast<int>(*p)); \
                }, \
                [offset](SqzModel* self, const QJsonValue& v) { \
                    unsigned int* p = reinterpret_cast<unsigned int*>( \
                        reinterpret_cast<char*>(self) + offset); \
                    *p = v.toVariant().toUInt(); \
                } \
            ); \
        SQZ_FIELD_REG_CTOR_END(Name) \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    unsigned int Name() const { return m_##Name; } \
    void set##Name(unsigned int val) { m_##Name = val; }

/**
 * @def   SQZ_FIELD_LLONG(Name)
 * @brief 声明 long long 类型字段
 * @note  JSON数值精度上限为2^53，超出范围建议用字符串存储
 */
#define SQZ_FIELD_LLONG(Name) \
private: \
    long long m_##Name{}; \
    struct _Reg_##Name { \
        SQZ_FIELD_REG_CTOR_BEGIN(Name, long long) \
            base->registerField(name, \
                [offset](const SqzModel* self) -> QJsonValue { \
                    const long long* p = reinterpret_cast<const long long*>( \
                        reinterpret_cast<const char*>(self) + offset); \
                    return QJsonValue(static_cast<qint64>(*p)); \
                }, \
                [offset](SqzModel* self, const QJsonValue& v) { \
                    long long* p = reinterpret_cast<long long*>( \
                        reinterpret_cast<char*>(self) + offset); \
                    *p = v.toVariant().toLongLong(); \
                } \
            ); \
        SQZ_FIELD_REG_CTOR_END(Name) \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    long long Name() const { return m_##Name; } \
    void set##Name(long long val) { m_##Name = val; }

/**
 * @def   SQZ_FIELD_ULLONG(Name)
 * @brief 声明 unsigned long long 类型字段
 */
#define SQZ_FIELD_ULLONG(Name) \
private: \
    unsigned long long m_##Name{}; \
    struct _Reg_##Name { \
        SQZ_FIELD_REG_CTOR_BEGIN(Name, unsigned long long) \
            base->registerField(name, \
                [offset](const SqzModel* self) -> QJsonValue { \
                    const unsigned long long* p = reinterpret_cast<const unsigned long long*>( \
                        reinterpret_cast<const char*>(self) + offset); \
                    return QJsonValue(static_cast<quint64>(*p)); \
                }, \
                [offset](SqzModel* self, const QJsonValue& v) { \
                    unsigned long long* p = reinterpret_cast<unsigned long long*>( \
                        reinterpret_cast<char*>(self) + offset); \
                    *p = v.toVariant().toULongLong(); \
                } \
            ); \
        SQZ_FIELD_REG_CTOR_END(Name) \
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
 */
#define SQZ_FIELD_FLOAT(Name) \
private: \
    float m_##Name{}; \
    struct _Reg_##Name { \
        SQZ_FIELD_REG_CTOR_BEGIN(Name, float) \
            base->registerField(name, \
                [offset](const SqzModel* self) -> QJsonValue { \
                    const float* p = reinterpret_cast<const float*>( \
                        reinterpret_cast<const char*>(self) + offset); \
                    return QJsonValue(static_cast<double>(*p)); \
                }, \
                [offset](SqzModel* self, const QJsonValue& v) { \
                    float* p = reinterpret_cast<float*>( \
                        reinterpret_cast<char*>(self) + offset); \
                    *p = static_cast<float>(v.toDouble()); \
                } \
            ); \
        SQZ_FIELD_REG_CTOR_END(Name) \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    float Name() const { return m_##Name; } \
    void set##Name(float val) { m_##Name = val; }

/**
 * @def   SQZ_FIELD_DOUBLE(Name)
 * @brief 声明 double 类型字段
 */
#define SQZ_FIELD_DOUBLE(Name) \
private: \
    double m_##Name{}; \
    struct _Reg_##Name { \
        SQZ_FIELD_REG_CTOR_BEGIN(Name, double) \
            base->registerField(name, \
                [offset](const SqzModel* self) -> QJsonValue { \
                    const double* p = reinterpret_cast<const double*>( \
                        reinterpret_cast<const char*>(self) + offset); \
                    return QJsonValue(*p); \
                }, \
                [offset](SqzModel* self, const QJsonValue& v) { \
                    double* p = reinterpret_cast<double*>( \
                        reinterpret_cast<char*>(self) + offset); \
                    *p = v.toDouble(); \
                } \
            ); \
        SQZ_FIELD_REG_CTOR_END(Name) \
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
 */
#define SQZ_FIELD_BOOL(Name) \
private: \
    bool m_##Name{}; \
    struct _Reg_##Name { \
        SQZ_FIELD_REG_CTOR_BEGIN(Name, bool) \
            base->registerField(name, \
                [offset](const SqzModel* self) -> QJsonValue { \
                    const bool* p = reinterpret_cast<const bool*>( \
                        reinterpret_cast<const char*>(self) + offset); \
                    return QJsonValue(*p); \
                }, \
                [offset](SqzModel* self, const QJsonValue& v) { \
                    bool* p = reinterpret_cast<bool*>( \
                        reinterpret_cast<char*>(self) + offset); \
                    *p = v.toBool(); \
                } \
            ); \
        SQZ_FIELD_REG_CTOR_END(Name) \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    bool Name() const { return m_##Name; } \
    void set##Name(bool val) { m_##Name = val; }

/**
 * @def   SQZ_FIELD_CHAR(Name)
 * @brief 声明 char 类型字段
 */
#define SQZ_FIELD_CHAR(Name) \
private: \
    char m_##Name{}; \
    struct _Reg_##Name { \
        SQZ_FIELD_REG_CTOR_BEGIN(Name, char) \
            base->registerField(name, \
                [offset](const SqzModel* self) -> QJsonValue { \
                    const char* p = reinterpret_cast<const char*>( \
                        reinterpret_cast<const char*>(self) + offset); \
                    return QJsonValue(QString(*p)); \
                }, \
                [offset](SqzModel* self, const QJsonValue& v) { \
                    char* p = reinterpret_cast<char*>( \
                        reinterpret_cast<char*>(self) + offset); \
                    *p = v.toVariant().toChar().toLatin1(); \
                } \
            ); \
        SQZ_FIELD_REG_CTOR_END(Name) \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    char Name() const { return m_##Name; } \
    void set##Name(char val) { m_##Name = val; }

/**
 * @def   SQZ_FIELD_UCHAR(Name)
 * @brief 声明 unsigned char 类型字段
 */
#define SQZ_FIELD_UCHAR(Name) \
private: \
    unsigned char m_##Name{}; \
    struct _Reg_##Name { \
        SQZ_FIELD_REG_CTOR_BEGIN(Name, unsigned char) \
            base->registerField(name, \
                [offset](const SqzModel* self) -> QJsonValue { \
                    const unsigned char* p = reinterpret_cast<const unsigned char*>( \
                        reinterpret_cast<const char*>(self) + offset); \
                    return QJsonValue(static_cast<int>(*p)); \
                }, \
                [offset](SqzModel* self, const QJsonValue& v) { \
                    unsigned char* p = reinterpret_cast<unsigned char*>( \
                        reinterpret_cast<char*>(self) + offset); \
                    *p = static_cast<unsigned char>(v.toVariant().toUInt()); \
                } \
            ); \
        SQZ_FIELD_REG_CTOR_END(Name) \
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
 */
#define SQZ_FIELD_STRING(Name) \
private: \
    std::string m_##Name{}; \
    struct _Reg_##Name { \
        SQZ_FIELD_REG_CTOR_BEGIN(Name, std::string) \
            base->registerField(name, \
                [offset](const SqzModel* self) -> QJsonValue { \
                    const std::string* p = reinterpret_cast<const std::string*>( \
                        reinterpret_cast<const char*>(self) + offset); \
                    return QJsonValue(QString::fromStdString(*p)); \
                }, \
                [offset](SqzModel* self, const QJsonValue& v) { \
                    std::string* p = reinterpret_cast<std::string*>( \
                        reinterpret_cast<char*>(self) + offset); \
                    *p = v.toString().toStdString(); \
                } \
            ); \
        SQZ_FIELD_REG_CTOR_END(Name) \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    const std::string& Name() const { return m_##Name; } \
    void set##Name(const std::string& val) { m_##Name = val; }

/**
 * @def   SQZ_FIELD_QSTRING(Name)
 * @brief 声明 QString 类型字段
 */
#define SQZ_FIELD_QSTRING(Name) \
private: \
    QString m_##Name{}; \
    struct _Reg_##Name { \
        SQZ_FIELD_REG_CTOR_BEGIN(Name, QString) \
            base->registerField(name, \
                [offset](const SqzModel* self) -> QJsonValue { \
                    const QString* p = reinterpret_cast<const QString*>( \
                        reinterpret_cast<const char*>(self) + offset); \
                    return QJsonValue(*p); \
                }, \
                [offset](SqzModel* self, const QJsonValue& v) { \
                    QString* p = reinterpret_cast<QString*>( \
                        reinterpret_cast<char*>(self) + offset); \
                    *p = v.toString(); \
                } \
            ); \
        SQZ_FIELD_REG_CTOR_END(Name) \
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
 */
#define SQZ_FIELD_QBYTEARRAY(Name) \
private: \
    QByteArray m_##Name{}; \
    struct _Reg_##Name { \
        SQZ_FIELD_REG_CTOR_BEGIN(Name, QByteArray) \
            base->registerField(name, \
                [offset](const SqzModel* self) -> QJsonValue { \
                    const QByteArray* p = reinterpret_cast<const QByteArray*>( \
                        reinterpret_cast<const char*>(self) + offset); \
                    return QJsonValue(QLatin1String(p->toBase64())); \
                }, \
                [offset](SqzModel* self, const QJsonValue& v) { \
                    QByteArray* p = reinterpret_cast<QByteArray*>( \
                        reinterpret_cast<char*>(self) + offset); \
                    *p = QByteArray::fromBase64(v.toString().toLatin1()); \
                } \
            ); \
        SQZ_FIELD_REG_CTOR_END(Name) \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    const QByteArray& Name() const { return m_##Name; } \
    void set##Name(const QByteArray& val) { m_##Name = val; }

/**
 * @def   SQZ_FIELD_QJSONOBJECT(Name)
 * @brief 声明 QJsonObject 类型字段
 */
#define SQZ_FIELD_QJSONOBJECT(Name) \
private: \
    QJsonObject m_##Name{}; \
    struct _Reg_##Name { \
        SQZ_FIELD_REG_CTOR_BEGIN(Name, QJsonObject) \
            base->registerField(name, \
                [offset](const SqzModel* self) -> QJsonValue { \
                    const QJsonObject* p = reinterpret_cast<const QJsonObject*>( \
                        reinterpret_cast<const char*>(self) + offset); \
                    return QJsonValue(*p); \
                }, \
                [offset](SqzModel* self, const QJsonValue& v) { \
                    QJsonObject* p = reinterpret_cast<QJsonObject*>( \
                        reinterpret_cast<char*>(self) + offset); \
                    *p = v.toObject(); \
                } \
            ); \
        SQZ_FIELD_REG_CTOR_END(Name) \
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
 */
#define SQZ_FIELD_SQZMODEL(Name, ModelType) \
private: \
    ModelType m_##Name{}; \
    struct _Reg_##Name { \
        SQZ_FIELD_REG_CTOR_BEGIN(Name, ModelType) \
            base->registerField(name, \
                [offset](const SqzModel* self) -> QJsonValue { \
                    const ModelType* p = reinterpret_cast<const ModelType*>( \
                        reinterpret_cast<const char*>(self) + offset); \
                    return p->toJson(); \
                }, \
                [offset](SqzModel* self, const QJsonValue& v) { \
                    ModelType* p = reinterpret_cast<ModelType*>( \
                        reinterpret_cast<char*>(self) + offset); \
                    p->fromJson(v.toObject()); \
                } \
            ); \
        SQZ_FIELD_REG_CTOR_END(Name) \
    }; \
    _Reg_##Name m_reg_##Name{this, &m_##Name, #Name}; \
public: \
    const ModelType& Name() const { return m_##Name; } \
    ModelType& Name() { return m_##Name; } \
    void set##Name(const ModelType& val) { m_##Name = val; }

} // namespace Sqz
