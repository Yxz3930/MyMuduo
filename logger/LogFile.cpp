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
    // �����ǰ��Ӧ�ļ����еĻ��������пռ䲻���� ����ˢ�������
    if(this->m_file->freeSize() < len)
        this->flush(); // ������õ���this->m_file->fluash()
    // д���ļ�����
    this->m_file->append(data, len);
    ++this->m_count;


    // ��д���ļ���С����
    if (this->m_file->WritenBytes() >= this->m_filesize)
    {
        // ��ǰд�����־�ļ�д��Ĵ�С����ָ����С ��Ҫ���Ĺ�����־�ļ���
        this->RollFile();
        return;
    }

    // ��д��������й���
    if (this->m_count >= this->m_writeNumLimit)
    {
        this->RollFile();
        return;
    }

    // ÿ��һ��ʱ���ˢ��һ�� �����õĻ������е�����д�뵽���̵���
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
