#ifndef LOGFILE_H
#define LOGFILE_H

#include "FileUtility.h"
#include <memory>
#include <chrono>
#include <ctime>
#include <string>
#include <mutex>

/// @brief 日志文件管理类
/// 负责日志文件的创建、写入、滚动、刷新
/// 底层是调用FileUtility类的方法
class LogFile
{
    using system_clock_t = std::chrono::system_clock;
    using time_point_t = std::chrono::system_clock::time_point;
    using duration_t = std::chrono::system_clock::duration;

public:
    /// @brief 构造函数
    /// @param filename 日志路径
    /// @param filesize 用于滚动日子文件 日志文件限定大小 超过该值就需要要滚动日志 但是这个size需要小于FileUtility中缓冲区的容量
    /// @param interval 用于刷新写入日志文件 缓冲区写入文件的时间间隔 底层调用FileUtility的fflush方法将缓冲区中的数据写入到文件中
    /// @param write_num 用于滚动日子文件 日志文件写入次数限定值
    LogFile(const char *logdir, int filesize, int interval = 3, int write_num = 1024);

    ~LogFile();

    /// @brief 将数据写入到FileUtility的缓冲区中
    /// @param data 待写入数据
    /// @param len 待写入数据长度
    void append(const char *data, int len);

    /// @brief 刷新缓冲区 调用FileUtility中的刷新方法
    void flush();

    /// @brief 根据文件大小、写入次数进行滚动日志
    /// 这里有个bug 当日志信息过快导致新创建的文件名称和原来的日志文件一致时就会在原来的日志文件中继续写 而不是重新创建文件
    void RollFile();

private:
    /// @brief 获取文件名称
    /// @return 文件名字符串
    std::string GetFileName();

    /// @brief 获取当前时间点与上一时刻
    /// @param now 当前的时间点
    /// @param other 其他时间点
    /// @return 返回两个时间点之间的秒数
    int GetInterval(LogFile::time_point_t now, LogFile::time_point_t other);

private:
    std::string m_logdir;   // 日志路径
    std::string m_filename; // 日志名称 格式 y-m-d h:m:s.log

    off_t m_filesize;                  // 写入的文件大小 超过指定大小之后进行日志滚动
    int m_flushInterval;               // 刷新时间间隔
    int m_writeNumLimit;               // 文件写入次数限制
    int m_count;                       // 记录文件写入次数
    LogFile::time_point_t m_lastRoll;  // 上次滚动的时间点 也就是上次创建新日志文件的时间点
    LogFile::time_point_t m_lastFlush; // 上次刷新的时间

    std::mutex m_appendMtx;              // 写操作互斥锁
    std::unique_ptr<FileUtility> m_file; // 文件工具类智能指针
};

#endif // LOGFILE_H