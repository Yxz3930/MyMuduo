#ifndef ASYNCLOGGING_H
#define ASYNCLOGGING_H

#include <string>
#include "LogFile.h"
#include "FixedBuffer.h"
#include <mutex>
#include <condition_variable>
#include <memory>
#include <thread>
#include <atomic>
#include <vector>

/// @brief 异步日志类
/// 但是该类只负责异步日志方法方法的实现，包括双缓冲、开辟线程写入数据，最后方法需要注册到Logger类中
/// 为了简单方便 使用现有的容器作为缓冲进行保存
class AsyncLogging
{
public:
    /// @brief 构造函数 参数都是LogFile类的参数
    /// @param logdir 日志文件路径
    /// @param filesize 日志文件大小限定
    /// @param interval 日志刷新时间间隔
    /// @param write_num 日志文件写入次数限定
    AsyncLogging(const char *logdir, int filesize, int interval = 3, int write_num = 1024);

    /// @brief 析构函数
    ~AsyncLogging();

    /// @brief 异步日志中前端添加日志信息操作函数 这里的append是外部日志写入的函数
    /// append方法会被注册到Logger类中m_write_cb回调函数上 也即是说这里的data中是会存在日志级别信息的
    /// @param data 日志信息
    /// @param len 日志信息长度
    void append(const char *data, int len);

    /// @brief 刷新文件缓冲区
    void flush();

    /// @brief 异步线程工作函数 这个函数是异步日志系统中开辟的写入线程进行写入的逻辑
    void AsyncLoggingFunc();

    /// @brief 停止异步日志线程
    /// 可以是类内方法使用 将剩余数据写完
    /// 也可以是外部调用 停止日志系统
    void Stop();

private:
    /// @brief 交换两个缓冲区的指向
    void SwapBufferPtr();

    /// @brief 判断日志信息中是否存在FATAL
    /// @param data 格式化之后的日志信息
    /// @param len 日志信息长度
    bool isFatal(const char* data);

private:
    LogFile m_logfile; // 用于线程写入文件当中

    // 线程、锁相关变量
    std::thread m_asyncThread;    // 后端异步写入的线程
    std::mutex m_writeMtx;        // 写入操作互斥锁
    std::condition_variable m_cv; // 条件变量
    std::atomic_bool m_running{false}; // 运行标志

    std::unique_ptr<FixedBuffer> activeBuffer;
    std::unique_ptr<FixedBuffer> standbyBuffer;
    std::vector<std::unique_ptr<FixedBuffer>> bufferPool; // 存储多个缓冲区

};

#endif // ASYNCLOGGING_H