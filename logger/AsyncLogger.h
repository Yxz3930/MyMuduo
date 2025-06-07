#ifndef ASYNCLOGGER_H
#define ASYNCLOGGER_H

#include "Logger.h"
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>

class AsyncLogger
{
public:
    AsyncLogger(int max_size = 100);
    ~AsyncLogger();

    /// @brief 异步日志写操作
    /// @param level 日志级别
    /// @param msg 日志信息
    void asynclog_write(LogLevel level, const std::string &msg, const char *_file_, int _line_);

private:
private:
    using buffer_t = std::vector<std::string>;
    using bufferPtr_t = std::vector<std::string> *;
    buffer_t buffer_1; // 缓冲区
    buffer_t buffer_2; // 缓冲区

    bufferPtr_t active_buffer_ptr;  // 活跃缓冲区 程序将日志写入的缓冲区
    bufferPtr_t standby_buffer_ptr; // 备用缓冲区 实际上是异步日志线程需要输出的缓冲区

    size_t MAX_BUFFER_SIZE = 100; // 设置缓冲区中保存的日志信息大小

    std::ofstream m_ofs;
    FileAppender file_appender;

    // 锁、条件变量
    std::mutex log_mtx;
    std::condition_variable log_cv;
    std::atomic_bool m_running;
    std::thread asynclogger_thread;

private:
    /// @brief 交换缓冲区指针指向
    /// @param active_buffer
    /// @param standby_buffer
    void SwapBuffer();

    /// @brief 异步日志线程工作函数
    void AsyncLogger_func();

    /// @brief 停止异步日志
    void stop();
};

#endif // ASYNCLOGGER_H