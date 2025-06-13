#ifndef LOGGER_H
#define LOGGER_H

#include "LogFile.h"
#include "LogStream.h"
#include <functional>

enum LogLevel
{
    DEBUG = 0,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

/// @brief 控制日志级别设置的类
class LoggerControl
{
public:
    static LoggerControl &getInstance()
    {
        static LoggerControl instance;
        return instance;
    }

    void setLoggerLevel(LogLevel level)
    {
        this->m_level = level;
    }

    LogLevel getLevel()
    {
        return this->m_level;
    }

private:
    LogLevel m_level{LogLevel::INFO};
};

/// @brief Logger类中不能存放LogFile对象 因为Logger是RAII风格的 在每次日志输出时进行创建和销毁 如果包含LogFile对象 那么每次输出都会创建一个日志文件
/// 这显然是不可行的
class Logger
{

public:
    /// @brief 构造函数 并初始化类中的成员变量
    /// @param level 日志级别
    /// @param __file__ 文件名
    /// @param __line__ 代码行
    Logger(LogLevel level, const char *__file__, int __line__);

    /// @brief 析构函数
    /// 在析构函数中获取字符串缓冲区中的数据 然后进行写入
    ~Logger();

    /// @brief 返回LogStream日志流对象
    /// @return
    LogStream &stream() { return this->m_logstream; };

    /// @brief 注册写操作回调函数
    /// @param
    static void SetWritFunc(std::function<void(const char *msg, int len)>);

    /// @brief 注册刷新缓冲区回调函数
    /// @param
    static void SetFlushFunc(std::function<void(void)>);

private:
    /// @brief 如果没有注册写入方法 则使用该默认写入方法
    /// @param data
    /// @param len
    void DefaultWriteFunc(const char *data, int len);

    /// @brief 如果没有注册刷新方法 则使用该默认刷新方法
    void DefaultFlushFunc();

private:
    static std::function<void(const char *msg, int len)> m_write_cb;
    static std::function<void(void)> m_flush_cb;

    LogStream m_logstream;
    // static LogFile m_logfile;

private:
    /// @brief 用于格式化日志信息
    class Formatter
    {
    public:
        /// @brief Formatter构造
        /// @param level
        /// @param file
        /// @param line
        Formatter(LogLevel level, const char *file, int line);

        /// @brief 将日志信息格式化
        std::string MsgtoFormat(const char *msg);

        /// @brief 获取代码文件名 去掉其中的路径
        /// @return 文件名
        std::string GetCodeFileName();

        /// @brief 将日志级别转换为字符串
        /// @param level 日志级别
        /// @return 字符串
        std::string LeveltoStr(LogLevel level);

        LogLevel m_level;
        std::string m_file;
        int m_line;
    };

private:
    Formatter m_formatter; // 这里需要进行实例化 否则会报错
};

#endif // LOGGER_H

#define LOG(level)                                       \
    if (level < LoggerControl::getInstance().getLevel()) \
        ;                                                \
    else                                                 \
        Logger(level, __FILE__, __LINE__).stream()

#define LOG_DEBUG LOG(LogLevel::DEBUG)
#define LOG_INFO LOG(LogLevel::INFO)
#define LOG_WARNING LOG(LogLevel::WARNING)
#define LOG_ERROR LOG(LogLevel::ERROR)
#define LOG_FATAL LOG(LogLevel::FATAL)
