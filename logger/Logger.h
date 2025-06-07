#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <deque>
#include <functional>
/**
 * Logger.h 文件夹中包含LogLevel Logger Formater
 */

enum LogLevel
{
    DEBUG = 0,
    INFO,
    WARNING,
    ERROR,
    FATAL,
};

// // 运行时日志级别（默认值）
// #ifndef LOG_LEVEL
// #define LOG_LEVEL LogLevel::DEBUG
// #endif


/// @brief 通过类中的静态方法格式化数据
class Formatter
{
public:
    /// @brief 将数据格式化
    /// @param level 日志级别
    /// @param msg 信息
    /// @return string字符串
    static std::string format(LogLevel level, const std::string &msg, const char *_file_, int _line_);

    /// @brief 将时间点格式化为年月日 时分秒的格式 "%Y-%m-%d %H:%M:%S"
    /// @return
    static std::string formattime();

private:
    /// @brief 将LogLevel对象中的成员(int类型)转为字符串
    /// @param level 日志级别
    /// @return 日志级别字符串
    static const std::string LogLeveltoString(LogLevel level);

    static const std::string GetColor(LogLevel level);
};

/// @brief 抽象基类：日志输出
///        析构函数需要为虚函数
class BasicAppender
{
public:
    virtual ~BasicAppender() = default;

    /// @brief 向终端或者文件写入
    /// @param level 日志级别
    /// @param msg 信息
    virtual void write(LogLevel level, const std::string &msg, const char *_file_, int _line_) = 0;

    /// @brief 立即刷新缓冲区 将输出输出
    virtual void flush() = 0;
};

/// @brief 控制台终端输出类
class ConsoleAppender : public BasicAppender
{
public:
    /// @brief 向终端输出
    /// @param level 日志级别
    /// @param msg 信息
    void write(LogLevel level, const std::string &msg, const char *_file_, int _line_) override;

    /// @brief 立即刷新缓冲区 将数据输出到终端
    void flush() override;

private:
    std::mutex m_cout_mutex;
};

class FileAppender : public BasicAppender
{
public:
    /// @brief 构造 此时没有log文件则创建log文件
    explicit FileAppender(std::string logdir = "./logs");

    /// @brief 析构 判断文件是否关闭 如果没有则关闭文件
    ~FileAppender();

    /// @brief 向文件写入
    /// @param level 日志级别
    /// @param msg 信息
    void write(LogLevel level, const std::string &msg, const char *_file_, int _line_) override;

    /// @brief 文件输出的异步写操作

    /// @brief 文件输出的异步写操作 将缓冲区中的日志消息一并写入文件中
    /// @param buffer_ptr 缓冲区指针
    void asyncWrite(std::vector<std::string>* buffer_ptr);

    /// @brief 立即刷新缓冲区 将数据输出到文件
    void flush() override;

    /// @brief 重置日志输出文件夹路径
    /// @param logdir
    void ResetLogdir(const std::string &logdir);

    /// @brief 关闭文件
    void close();

private:
    /// @brief 根据当前时间检查文件路径是否正确
    void CheckFilePath();

    /// @brief 根据当前时间创建新的日志文件
    void OpenNewFile();

    /// @brief 用于滚动日志 就是检查日志路径是否正确, 是否需要换一个文件输出等
    void RollLog();

    /// @brief 将buffer_deque中的数据写入文件中
    void WritefromDeque();

private:
    std::ofstream m_ofs;                  // 文件输出流
    std::string m_file_path;              // 文件输出路径
    std::string m_logdir;                 // 日志文件夹
    std::mutex m_ofs_mutex;               // ofs输出互斥锁
    std::string buffer;                   // 用于存储日志信息
    std::deque<std::string> buffer_deque; // 存储多条日志信息

    int buffer_num; // 用于指定buffer_deque中存储的日志数量
};

class Logger
{
public:

    /// @brief 设置日志输出级别
    /// @param level 
    void setLogLevel(LogLevel level) { m_level = level; }

    /// @brief 单例模式 确保只有一个日志对象实例
    /// @return
    static Logger &GetInstance()
    {
        static Logger instance;
        return instance;
    }

    ~Logger()
    {
        this->m_running = false;
        if (this->flush_thread.joinable())
            this->flush_thread.join();
    }

    void init(const std::string &logdir = "./logs") { this->m_logdir = logdir; };

    /// @brief 向m_appenders中添加日期输出器
    /// @param appender
    void AddAppender(std::shared_ptr<BasicAppender> appender);

    /// @brief 对m_appenders中的对象进行输出
    /// @param level 日志级别
    /// @param msg 信息
    void log(LogLevel level, const std::string &msg, const char *_file_, int _line_);

    /// @brief 设置Log函数中日志输出方式
    /// @param cb 
    void SetWriteFunc(const std::function<void(LogLevel, const std::string &, const char *, int)>& cb);


private:
    /// @brief 私有化默认构造 添加拷贝和赋值之后必须要添加默认构造 不然无法实例化对象
    Logger()
    {
        // 创建FileAppender智能指针并添加到m_appenders中
        init("./logs");
        this->m_appenders.push_back(std::shared_ptr<BasicAppender>(new FileAppender("../logs")));
        this->m_appenders.push_back(std::shared_ptr<BasicAppender>(new ConsoleAppender));

        // 创建线程进行刷新
        if (m_running == false)
        {
            this->m_running = true;
            this->flush_thread = std::thread(&Logger::FlushTask, this);
        }
    };

    /// @brief 禁止拷贝构造
    /// @param other
    Logger(const Logger &other) = delete;

    /// @brief 禁止赋值构造
    /// @param other
    /// @return
    Logger &operator=(const Logger &other) = delete;

    /// @brief 在Logger类中进行输出刷新 由单独的线程进行工作
    /// @param appender 需要刷新的定时器
    void FlushTask();

private:
    int m_flush_interval{1}; // 刷新间隔 单位秒
    std::string m_logdir;
    std::vector<std::shared_ptr<BasicAppender>> m_appenders;
    std::mutex m_appender_mutex;

    std::atomic_bool m_running{false};
    std::thread flush_thread;

    std::function<void(LogLevel, const std::string &, const char *, int)> async_writeFunc; // 用于设置异步日志输出函数回调

    LogLevel m_level;
};

#endif // LOGGER_H


// 对log方法进行#define封装和转换
#define LOG_DEBUG(msg) Logger::GetInstance().log(DEBUG, msg, __FILE__, __LINE__)
#define LOG_INFO(msg) Logger::GetInstance().log(INFO, msg, __FILE__, __LINE__)
#define LOG_WARNING(msg) Logger::GetInstance().log(WARNING, msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) Logger::GetInstance().log(ERROR, msg, __FILE__, __LINE__)
#define LOG_FATAL(msg) Logger::GetInstance().log(FATAL, msg, __FILE__, __LINE__)


