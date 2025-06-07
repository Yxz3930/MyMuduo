#ifndef TIME_STAMP_H
#define TIME_STAMP_H

#include <chrono>
#include <string>

/// @brief TimeStampʱ�����ֻ���е�ǰʱ���ı�������
class TimeStamp
{
private:
    using time_point_t = std::chrono::system_clock::time_point;
    time_point_t m_time_point;

public:
    TimeStamp() : m_time_point(std::chrono::system_clock::now()) {}
    TimeStamp(const time_point_t& ts) : m_time_point(ts) {}

    /// @brief תΪ��
    /// @return
    int64_t toSeconds();

    /// @brief תΪ���뼶
    /// @return 
    int64_t toMilliseconds();

    /// @brief תΪ΢��
    /// @return
    int64_t toMicroseconds();

    /// @brief תΪ��-��-��-ʱ-��-��ĸ�ʽ
    /// @return �ַ�����ʽ��ʱ���
    std::string toFormatString();

    /// @brief ��ȡʱ���
    /// @return
    std::chrono::system_clock::time_point GetTimePoint() const;

    /// @brief ����==�����
    /// @param ts
    /// @return
    bool operator==(const TimeStamp &ts) const;

    /// @brief ����<�����
    /// @param ts
    /// @return
    bool operator<(const TimeStamp &ts) const;

    /// @brief ���ظ�ֵ�����
    /// @param ts 
    /// @return 
    TimeStamp& operator=(const TimeStamp& ts);

    /// @brief ��ǰʱ����ȥ����һ��ʱ���õ�֮���duration
    /// @param ts ������
    /// @return ϵͳʱ��system_clock��rep
    std::chrono::system_clock::duration operator-(const TimeStamp& ts) const;

    /// @brief ��ǰʱ����ϼ���ʱ������ȡ��һ��ʱ���
    /// @param add_microseconds
    TimeStamp AddDuration(const std::chrono::microseconds &add_microseconds);
};

#endif // TIME_STAMP_H