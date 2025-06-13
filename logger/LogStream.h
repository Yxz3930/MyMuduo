#ifndef LOGSTREAM_H
#define LOGSTREAM_H

#include <string>
#include <sstream>
#include <vector>
#include <cstring>

/// @brief 日志流 重载<<运算符
/// 封装了ostringstream 用于将不同的数据写入字符串缓冲流中
/// 原来这里采用的方式是直接使用ostringstream接收<<运算符输出的数据，速度相对来说比较慢 
/// 现在改为vector<char>作为缓冲区进行保存 写入的数据需要进行转换
class LogStream
{

public:
    LogStream() 
    {
        this->buffer.reserve(1024);
    }

    ~LogStream() = default;

    /// @brief 对该类重载<<运算符
    /// @tparam T 模板参数
    /// @param value 不同类型的数据
    /// @return *this 支持链式运算
    template <typename T>
    LogStream &operator<<(const T &value)
    {
        this->append(this->to_string(value));
        return *this;
    }

    /// @brief 获取读取的字符串
    /// @return 字符串
    std::string str();

    /// @brief 清空字符串缓冲区数据
    void clear();

private:
    std::vector<char> buffer;

    /// @brief 追加字符串到缓冲区
    void append(const std::string &str)
    {
        buffer.insert(buffer.end(), str.begin(), str.end());
    }

    void append(const char *str)
    {
        if (str)
        {
            buffer.insert(buffer.end(), str, str + strlen(str));
        }
    }

    /// @brief 辅助函数：将不同类型转换为字符串
    template <typename T>
    std::string to_string(const T &value) const
    {
        if constexpr (std::is_same_v<T, std::string>)
        {
            return value;
        }
        else if constexpr (std::is_same_v<T, const char *>)
        {
            return value ? value : "";
        }
        else if constexpr (std::is_integral_v<T>)
        {
            // 高效的整数转字符串（简单实现，可替换为更快的算法）
            return std::to_string(value);
        }
        else if constexpr (std::is_floating_point_v<T>)
        {
            // 高效的浮点数转字符串（可定制格式）
            return std::to_string(value);
        }
        else
        {
            // 其他类型使用 std::ostringstream（可替换为 fmt 或 std::format）
            std::ostringstream oss;
            oss << value;
            return oss.str();
        }
    }
};

#endif // LOGSTREAM_H