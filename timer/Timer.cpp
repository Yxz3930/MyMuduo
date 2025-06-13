#include "Timer.h"
#include <iostream>
#include "Logger.h"

void Timer::Start()
{
    this->m_time_CallBack();
}

TimeStamp Timer::GetExpiration() const
{
    return this->m_expiration;
}

bool Timer::isRepeat() const
{
    return this->m_repeat;
}

TimeStamp Timer::ResetExpiration(const TimeStamp &now)
{
    if (this->m_repeat)
    {
        this->m_expiration = TimeStamp(now.GetTimePoint() + std::chrono::milliseconds(this->m_interval_millisecond));
    }
    else
        LOG_ERROR << "the timer is not periodical timer";

    return this->m_expiration;
}

std::function<void()> Timer::GetCallback()
{
    return this->m_time_CallBack;
}

void Timer::UpdateTimeStamp(const TimeStamp &ts)
{
    this->m_expiration = ts;
    LOG_DEBUG << "next expiration timestamp is: " << this->m_expiration.toFormatString() << ", duration<miroseconds>: " << this->m_expiration.toMilliseconds();
}

void Timer::ResetCallBack(const std::function<void(void)> &_cb)
{
    this->m_time_CallBack = std::move(_cb);
}

void Timer::Stop()
{
    this->m_runing = false;
}

bool Timer::isRuning()
{
    return this->m_runing;
}

void Timer::SetRepeatCount(int repeat_count)
{
    this->m_repeat_count = repeat_count;
}

int Timer::GetRepeatCount()
{
    return this->m_repeat_count;
}

void Timer::SetInterval(int64_t milli_seconds)
{
    this->m_interval_millisecond = milli_seconds;
}

int64_t Timer::GetInterval()
{
    return this->m_interval_millisecond;
}
