#ifndef TIME_STAMP_H
#define TIME_STAMP_H

#include <chrono>
#include <string>

/// @brief TimeStamp时间戳类只进行当前时间点的保存和输出
class TimeStamp
{
private:
    using time_point_t = std::chrono::system_clock::time_point;
    time_point_t m_time_point;

public:
    TimeStamp() : m_time_point(std::chrono::system_clock::now()) {}
    TimeStamp(const time_point_t& ts) : m_time_point(ts) {}

    /// @brief 转为秒
    /// @return
    int64_t toSeconds();

    /// @brief 转为毫秒级
    /// @return 
    int64_t toMilliseconds();

    /// @brief 转为微秒
    /// @return
    int64_t toMicroseconds();

    /// @brief 转为年-月-日-时-分-秒的格式
    /// @return 字符串格式的时间戳
    std::string toFormatString();

    /// @brief 获取时间戳
    /// @return
    std::chrono::system_clock::time_point GetTimePoint() const;

    /// @brief 重载==运算符
    /// @param ts
    /// @return
    bool operator==(const TimeStamp &ts) const;

    /// @brief 重载<运算符
    /// @param ts
    /// @return
    bool operator<(const TimeStamp &ts) const;

    /// @brief 重载赋值运算符
    /// @param ts 
    /// @return 
    TimeStamp& operator=(const TimeStamp& ts);

    /// @brief 当前时间点减去另外一个时间点得到之间的duration
    /// @param ts 被减数
    /// @return 系统时钟system_clock的rep
    std::chrono::system_clock::duration operator-(const TimeStamp& ts) const;

    /// @brief 向当前时间点上加入时间间隔获取另一个时间点
    /// @param add_microseconds
    TimeStamp AddDuration(const std::chrono::microseconds &add_microseconds);
};

#endif // TIME_STAMP_H