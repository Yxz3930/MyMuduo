#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include "TimeStamp.h"
#include <functional>
#include <atomic>

/// @brief ��ʱ���࣬����һ��ʱ���(����һ��ʱ����)�Լ���Ӧ������
/// ���ܣ�����ĳ��ʱ���ִ��ĳ������ ���� ����ָ��ʱ����֮��ִ��ĳ������
class Timer
{
private:
    TimeStamp m_expiration;                // ����ʱ��
    std::function<void()> m_time_CallBack; // ��ʱ���� �ص�����
    int m_repeat_count{0};                 // ��Ҫ�ظ�ִ�е�����Ĵ��� Ĭ����1
    bool m_repeat;                         // �Ƿ��������Դ�����ʱ��
    int64_t m_interval_millisecond;        // �ظ�ִ�������ʱ���� ����

    std::atomic_bool m_runing{false}; // ��ʱ���Ƿ���������

public:
    Timer(TimeStamp ts, std::function<void()> func, int64_t m_interval_millisecond) 
    :   m_expiration(ts), m_time_CallBack(func), m_interval_millisecond(m_interval_millisecond), 
        m_repeat(m_interval_millisecond > 0), m_runing(true) {};

    /// @brief �ִ�ʱ��� ��ʼ��������
    void Start();

    /// @brief ��ȡ���ڵ�ʱ���
    /// @return
    TimeStamp GetExpiration() const;

    /// @brief ��ȡ�Ƿ��������Զ�ʱ��
    /// @return
    bool isRepeat() const;

    /// @brief �����Զ�ʱ������ʱ���
    /// @param ts ���뵱ǰ��ʱ��� ���� now �����µĴ���ʱ�� ����Ư�����
    TimeStamp ResetExpiration(const TimeStamp &now);

    /// @brief ��ȡtimer���еĻص�����
    /// @return
    std::function<void()> GetCallback();

    /// @brief ���¶�ʱ���ĵ���ʱ���
    /// @param ts
    void UpdateTimeStamp(const TimeStamp &ts);

    /// @brief �Զ�ʱ�����ûص�����
    /// @param _cb
    void ResetCallBack(const std::function<void(void)> &_cb);

    /// @brief ֹͣ��ʱ��
    void Stop();

    /// @brief �ж��Ƿ�������
    /// @return
    bool isRuning();

    /// @brief �����ظ�����
    /// @param repeat_count �ظ�����
    void SetRepeatCount(int repeat_count);

    /// @brief ��ȡʣ����ظ�����
    /// @return
    int GetRepeatCount();

    /// @brief ����ʱ����
    /// @param milli_seconds ���(����)
    void SetInterval(int64_t milli_seconds);

    /// @brief ��ȡ�趨��ʱ����
    /// @return
    int64_t GetInterval();
};

#endif // TIMER_H