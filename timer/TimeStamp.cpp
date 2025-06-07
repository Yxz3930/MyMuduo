#include "TimeStamp.h"
#include <sstream>
#include <iomanip>

int64_t TimeStamp::toSeconds()
{
    return std::chrono::duration_cast<std::chrono::seconds>(this->m_time_point.time_since_epoch()).count();
}

int64_t TimeStamp::toMilliseconds()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(this->m_time_point.time_since_epoch()).count();
}

int64_t TimeStamp::toMicroseconds()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(this->m_time_point.time_since_epoch()).count();
}

std::string TimeStamp::toFormatString()
{
    // to_time_t 只会保留
    std::time_t time = std::chrono::system_clock::to_time_t(this->m_time_point);
    std::tm tm;
#ifdef _WIN32
    localtime_s(&tm, &time); // Windows线程安全版本
#else
    localtime_r(&time, &tm); // POSIX线程安全版本
#endif
    std::ostringstream oss;
    oss.clear();
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");

    return oss.str();
}

std::chrono::system_clock::time_point TimeStamp::GetTimePoint() const
{
    return this->m_time_point;
}

bool TimeStamp::operator==(const TimeStamp &ts) const
{
    return (this->m_time_point == ts.m_time_point);
}

bool TimeStamp::operator<(const TimeStamp &ts) const
{
    return (this->m_time_point < ts.m_time_point);
}

TimeStamp &TimeStamp::operator=(const TimeStamp &ts) 
{
    if(this == &ts)
        return *this;
    
    this->m_time_point = ts.m_time_point;
    return *this;
}

std::chrono::system_clock::duration TimeStamp::operator-(const TimeStamp &ts) const
{
    return this->m_time_point - ts.m_time_point;
}

TimeStamp TimeStamp::AddDuration(const std::chrono::microseconds &add_microseconds)
{
    // // std::chrono::system_clock::time_point中的时间间隔是按照nanosecond进行的所以add_microseconds转为纳秒再相加
    // std::chrono::nanoseconds add_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(add_microseconds);
    // this->m_time_point += add_duration;

    // chrono库支持时间点与时间间隔之间的运算操作 即使二者单位不同也可以进行操作 不需要手动转换
    this->m_time_point += add_microseconds;
    
    return TimeStamp(this->m_time_point);
}
