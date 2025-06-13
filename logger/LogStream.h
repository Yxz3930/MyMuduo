#ifndef LOGSTREAM_H
#define LOGSTREAM_H

#include <string>
#include <sstream>
#include <vector>
#include <cstring>

/// @brief ��־�� ����<<�����
/// ��װ��ostringstream ���ڽ���ͬ������д���ַ�����������
/// ԭ��������õķ�ʽ��ֱ��ʹ��ostringstream����<<�������������ݣ��ٶ������˵�Ƚ��� 
/// ���ڸ�Ϊvector<char>��Ϊ���������б��� д���������Ҫ����ת��
class LogStream
{

public:
    LogStream() 
    {
        this->buffer.reserve(1024);
    }

    ~LogStream() = default;

    /// @brief �Ը�������<<�����
    /// @tparam T ģ�����
    /// @param value ��ͬ���͵�����
    /// @return *this ֧����ʽ����
    template <typename T>
    LogStream &operator<<(const T &value)
    {
        this->append(this->to_string(value));
        return *this;
    }

    /// @brief ��ȡ��ȡ���ַ���
    /// @return �ַ���
    std::string str();

    /// @brief ����ַ�������������
    void clear();

private:
    std::vector<char> buffer;

    /// @brief ׷���ַ�����������
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

    /// @brief ��������������ͬ����ת��Ϊ�ַ���
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
            // ��Ч������ת�ַ�������ʵ�֣����滻Ϊ������㷨��
            return std::to_string(value);
        }
        else if constexpr (std::is_floating_point_v<T>)
        {
            // ��Ч�ĸ�����ת�ַ������ɶ��Ƹ�ʽ��
            return std::to_string(value);
        }
        else
        {
            // ��������ʹ�� std::ostringstream�����滻Ϊ fmt �� std::format��
            std::ostringstream oss;
            oss << value;
            return oss.str();
        }
    }
};

#endif // LOGSTREAM_H