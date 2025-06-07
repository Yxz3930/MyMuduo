#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include "TimeStamp.h"
#include <functional>
#include <atomic>

/// @brief 定时器类，包含一个时间点(或者一个时间间隔)以及对应的任务
/// 功能：到达某个时间点执行某个任务 或者 超过指定时间间隔之后执行某个任务
class Timer
{
private:
    TimeStamp m_expiration;                // 到期时间
    std::function<void()> m_time_CallBack; // 定时任务 回调函数
    int m_repeat_count{0};                 // 需要重复执行的任务的次数 默认是1
    bool m_repeat;                         // 是否是周期性触发定时器
    int64_t m_interval_millisecond;        // 重复执行任务的时间间隔 毫秒

    std::atomic_bool m_runing{false}; // 定时器是否正在运行

public:
    Timer(TimeStamp ts, std::function<void()> func, int64_t m_interval_millisecond) 
    :   m_expiration(ts), m_time_CallBack(func), m_interval_millisecond(m_interval_millisecond), 
        m_repeat(m_interval_millisecond > 0), m_runing(true) {};

    /// @brief 抵达时间点 开始运行任务
    void Start();

    /// @brief 获取到期的时间戳
    /// @return
    TimeStamp GetExpiration() const;

    /// @brief 获取是否是周期性定时器
    /// @return
    bool isRepeat() const;

    /// @brief 周期性定时器重置时间戳
    /// @param ts 传入当前的时间戳 基于 now 计算新的触发时间 纠正漂移误差
    TimeStamp ResetExpiration(const TimeStamp &now);

    /// @brief 获取timer当中的回调函数
    /// @return
    std::function<void()> GetCallback();

    /// @brief 更新定时器的到期时间戳
    /// @param ts
    void UpdateTimeStamp(const TimeStamp &ts);

    /// @brief 对定时器重置回调函数
    /// @param _cb
    void ResetCallBack(const std::function<void(void)> &_cb);

    /// @brief 停止定时器
    void Stop();

    /// @brief 判断是否在运行
    /// @return
    bool isRuning();

    /// @brief 设置重复次数
    /// @param repeat_count 重复次数
    void SetRepeatCount(int repeat_count);

    /// @brief 获取剩余的重复次数
    /// @return
    int GetRepeatCount();

    /// @brief 设置时间间隔
    /// @param milli_seconds 间隔(毫秒)
    void SetInterval(int64_t milli_seconds);

    /// @brief 获取设定的时间间隔
    /// @return
    int64_t GetInterval();
};

#endif // TIMER_H