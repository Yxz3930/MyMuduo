#include "LogFile.h"
#include <iomanip>
#include <sstream>
#include <iostream>

LogFile::LogFile(const char *logdir, int filesize, int interval, int write_num)
    : m_logdir(logdir), m_filesize(filesize), m_flushInterval(interval), m_writeNumLimit(write_num), m_count(0),
      m_lastFlush(system_clock_t::now()), m_lastRoll(system_clock_t::now()), m_filename("xxx.log")
{
    this->RollFile();
}

LogFile::~LogFile()
{
}

void LogFile::append(const char *data, int len)
{
    std::lock_guard<std::mutex> lock(this->m_appendMtx);
    // 如果当前对应文件当中的缓冲区空闲空间不够了 则先刷新再添加
    if(this->m_file->freeSize() < len)
        this->flush(); // 里面调用的是this->m_file->fluash()
    // 写入文件当中
    this->m_file->append(data, len);
    ++this->m_count;


    // 按写入文件大小滚动
    if (this->m_file->WritenBytes() >= this->m_filesize)
    {
        // 当前写入的日志文件写入的大小超过指定大小 需要更改滚动日志文件了
        this->RollFile();
        return;
    }

    // 按写入次数进行滚动
    if (this->m_count >= this->m_writeNumLimit)
    {
        this->RollFile();
        return;
    }

    // 每个一定时间就刷新一下 将设置的缓冲区中的数据写入到磁盘当中
    if (this->GetInterval(system_clock_t::now(), this->m_lastFlush) > this->m_flushInterval)
    {
        this->m_lastFlush = system_clock_t::now();
        this->flush();
    }
}

void LogFile::flush()
{
    this->m_file->flush();
}

void LogFile::RollFile()
{
    std::string filename = this->GetFileName();
    time_point_t now = system_clock_t::now();
    if (now > this->m_lastRoll)
    {
        this->m_count = 0;
        this->m_lastFlush = now;
        this->m_lastRoll = now;
        std::cout << "new file name: " << filename << std::endl;
        this->m_file.reset(new FileUtility(filename.c_str()));
    }
}

std::string LogFile::GetFileName()
{
    LogFile::time_point_t now = system_clock_t::now();
    struct tm tm;
    time_t time = std::chrono::system_clock::to_time_t(now);
    localtime_r(&time, &tm);

    std::ostringstream oss;
    oss << this->m_logdir << "/";
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << ".log";
    return oss.str();
}

int LogFile::GetInterval(LogFile::time_point_t now, LogFile::time_point_t other)
{
    duration_t duration = now - other;
    int seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    return seconds;
}
