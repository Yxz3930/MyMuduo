#include "AsyncLogger.h"
#include <iostream>
#include <unistd.h>
#include <assert.h>

AsyncLogger::AsyncLogger(int max_size)
    : active_buffer_ptr(&buffer_1), standby_buffer_ptr(&buffer_2), MAX_BUFFER_SIZE(max_size), file_appender("./logs")
{
    this->m_running = true;
    this->asynclogger_thread = std::thread(&AsyncLogger::AsyncLogger_func, this);
}

AsyncLogger::~AsyncLogger()
{
    this->m_running = false;
    this->log_cv.notify_one();
    if (this->asynclogger_thread.joinable())
        this->asynclogger_thread.join();
}

void AsyncLogger::asynclog_write(LogLevel level, const std::string &msg, const char *_file_, int _line_)
{
    {
        std::lock_guard<std::mutex> lock(this->log_mtx);
        std::string log_format = Formatter::format(level, msg, _file_, _line_);
        this->active_buffer_ptr->push_back(std::move(log_format));

        if (level != LogLevel::FATAL && active_buffer_ptr->size() >= MAX_BUFFER_SIZE)
        {
            this->SwapBuffer();
            this->log_cv.notify_one(); // 一个缓冲区已满可用唤醒线程写入文件了
        }
        else if (level == LogLevel::FATAL)
        {
            this->SwapBuffer();
            this->log_cv.notify_one();
        }
    }

    // 如果日志级别为FATAL则立即交换缓冲区进行日志写入
    if (level == LogLevel::FATAL)
    {
        std::cerr << "AsyncLogger is closed" << std::endl;
        this->stop();
        std::terminate();
    }
}

void AsyncLogger::AsyncLogger_func()
{
    while (this->m_running)
    {
        {
            std::unique_lock<std::mutex> lock(this->log_mtx);
            // this->log_cv.wait(lock, [this]()
            //                   { return !this->standby_buffer_ptr->empty() || !this->m_running; });
            this->log_cv.wait_for(lock, std::chrono::seconds(3));
        }
        this->SwapBuffer();
        this->file_appender.asyncWrite(standby_buffer_ptr);

        standby_buffer_ptr->clear();
        this->file_appender.flush();
    }
}

void AsyncLogger::stop()
{
    this->log_cv.notify_one();
    this->m_running = false;
    if (this->asynclogger_thread.joinable())
        this->asynclogger_thread.join();
}

void AsyncLogger::SwapBuffer()
{
    std::swap(this->active_buffer_ptr, this->standby_buffer_ptr);
}
