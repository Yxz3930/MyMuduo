#include <iostream>
#include "Logger.h"
#include <iomanip>
#include <climits>
#include <fmt/core.h>

// 静态成员 类内声明 类外定义
std::function<void(const char *, int)> Logger::m_write_cb = nullptr;
std::function<void()> Logger::m_flush_cb = nullptr;
// LogFile Logger::m_logfile("../logs", INT_MAX, 1, 1024); // 只要生成文件 那么这里就会在main函数开始前就初始化一个日志文件


Logger::Formatter::Formatter(LogLevel level, const char *file, int line)
    : m_level(level), m_file(file), m_line(line)
{
}

std::string Logger::Formatter::MsgtoFormat(const char *msg)
{
    std::string filename = this->GetCodeFileName();
    auto now = std::chrono::system_clock::now();
    time_t time = std::chrono::system_clock::to_time_t(now);
    struct tm tm;
    localtime_r(&time, &tm);

    // 这里使用ostringstream很慢 而且是每次写入日志都会进行格式化 这里会带来非常大的时间损耗
    // std::ostringstream oss;
    // std::string level_str = this->LeveltoStr(this->m_level);
    // std::string filename_line = std::string(" [") + filename + ":" + std::to_string(this->m_line) + "]";
    // oss << "[" << std::put_time(::localtime(&time), "%Y-%m-%d %H:%M:%S") << "] "
    //     << "[" << level_str << "]" << "\t"
    //     << std::setw(20) << std::left << filename_line << "\t"
    //     << "msg: " << msg << "\n";
    // return oss.str();

    return fmt::format("[{:04}-{:02}-{:02} {:02}:{:02}:{:02}] [{}]\t[{}:{}]\tmsg: {}\n",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec,
        this->LeveltoStr(this->m_level),
        this->GetCodeFileName(), this->m_line,
        msg);
}

std::string Logger::Formatter::GetCodeFileName()
{
    ssize_t pos = this->m_file.find_last_of("/");
    return this->m_file.substr(pos + 1);
}

std::string Logger::Formatter::LeveltoStr(LogLevel level)
{
    switch (level)
    {
    case LogLevel::DEBUG:
        return "DEBUG";
    case LogLevel::INFO:
        return "INFO";
    case LogLevel::WARNING:
        return "WARNING";
    case LogLevel::ERROR:
        return "ERROR";
    case LogLevel::FATAL:
        return "FATAL";
    }
    return "UNKNOWN";
}

Logger::Logger(LogLevel level, const char *__file__, int __line__)
    : m_formatter(level, __file__, __line__)
{
}

Logger::~Logger()
{
    // 日志级别过滤
    if (this->m_formatter.m_level < LoggerControl::getInstance().getLevel())
    {
        // 当前这个日志级别的日志不需要输出 将缓冲区中的消息删除掉
        this->m_logstream.clear();
        return;
    }
    std::string msg = this->m_logstream.str();
    std::string msg_format = this->m_formatter.MsgtoFormat(msg.c_str());
    if(this->m_write_cb)
        this->m_write_cb(msg_format.c_str(), msg_format.size());
    else
        this->DefaultWriteFunc(msg_format.c_str(), msg_format.size());

    if (this->m_formatter.m_level == LogLevel::FATAL)
    {
        if(this->m_flush_cb)
            this->m_flush_cb();
        else
            this->DefaultFlushFunc();
        std::cout << "Logger::~Logger encounter FATAL LEVEL msg" << std::endl;
        std::abort();
    }
}

void Logger::SetWritFunc(std::function<void(const char *msg, int len)> cb)
{
    Logger::m_write_cb = std::move(cb);
}

void Logger::SetFlushFunc(std::function<void(void)> cb)
{
    Logger::m_flush_cb = std::move(cb);
}

void Logger::DefaultWriteFunc(const char *data, int len)
{
    // Logger::m_logfile.append(data, len); // 输出到文件
    fwrite(data, 1, len, stdout);      // 输出到终端
}

void Logger::DefaultFlushFunc()
{
    // Logger::m_logfile.flush();              // 文件刷新缓冲区
    std::cout << std::flush << std::endl; // 终端刷新缓冲区
}
