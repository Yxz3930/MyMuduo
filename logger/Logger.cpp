#include "Logger.h"
#include "TimeStamp.h"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <string.h>

std::string Formatter::format(LogLevel level, const std::string &msg, const char *_file_, int _line_)
{
    const std::string s_level = LogLeveltoString(level);
    std::ostringstream oss;

    std::time_t time = std::chrono::system_clock::to_time_t(TimeStamp().GetTimePoint());
    std::tm tm;

#ifdef _WIN32
    localtime_s(&tm, &time); // Windows�̰߳�ȫ�汾
#else
    localtime_r(&time, &tm); // POSIX�̰߳�ȫ�汾
#endif

    // ��ȡ�ļ��� ������ȫ���ĵ�ַ·��
    std::string file(_file_);
    ssize_t pos = file.find_last_of("/");
    file = file.substr(pos + 1);
    std::string file_info = std::string(" [" + std::string(file) + ":" + std::to_string(_line_) + "]");

    oss << "[" << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "]"
        << " [" << s_level << "]" << "\t"
        << std::setw(25) << std::left << file_info
        // << " threadId: " << std::this_thread::get_id() << "\t"
        << " msg: " << msg;

    return oss.str();
}

std::string Formatter::formattime()
{
    std::ostringstream oss;

    std::time_t time = std::chrono::system_clock::to_time_t(TimeStamp().GetTimePoint());
    std::tm tm;

#ifdef _WIN32
    localtime_s(&tm, &time); // Windows�̰߳�ȫ�汾
#else
    localtime_r(&time, &tm); // POSIX�̰߳�ȫ�汾
#endif

    oss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");

    return oss.str();
}

const std::string Formatter::LogLeveltoString(LogLevel level)
{
    switch (level)
    {
    case DEBUG:
        return "DEBUG";
    case INFO:
        return "INFO";
    case WARNING:
        return "WARNING";
    case ERROR:
        return "ERROR";
    case FATAL:
        return "FATAL";
    default:
        return "UNKNOWN";
    }
}

const std::string Formatter::GetColor(LogLevel level)
{
    switch (level)
    {
    case DEBUG:
        return "\033[37m"; // ��ɫ
    case INFO:
        return "\033[32m"; // ��ɫ
    case WARNING:
        return "\033[33m"; // ��ɫ
    case ERROR:
        return "\033[31m"; // ��ɫ
    default:
        return "\033[0m"; // Ĭ��
    }
}

void ConsoleAppender::write(LogLevel level, const std::string &msg, const char *_file_, int _line_)
{
    std::lock_guard<std::mutex> lock(this->m_cout_mutex);
    std::cout << Formatter::format(level, msg, _file_, _line_) << std::endl;
}

void ConsoleAppender::flush()
{
    std::cout << std::flush;
}

FileAppender::FileAppender(std::string logdir)
    : m_logdir(logdir), buffer_num(1000)
{
    this->OpenNewFile();
}

FileAppender::~FileAppender()
{
    // this->WritefromDeque();
    if (this->m_ofs.is_open())
        this->m_ofs.close();
}

void FileAppender::write(LogLevel level, const std::string &msg, const char *_file_, int _line_)
{
    // д��֮ǰ��鵱ǰ�ļ�·���Ƿ���ȷ
    this->CheckFilePath();
    {
        std::lock_guard<std::mutex> lock(this->m_ofs_mutex);
        if (this->m_ofs.is_open())
            this->buffer_deque.push_back(std::move(Formatter::format(level, msg, _file_, _line_)));
        else
            std::cout << "FileAppender::write file is not open" << std::endl;

        if (this->buffer_deque.size() > this->buffer_num)
        {
            this->WritefromDeque();
        }

        if (level == LogLevel::FATAL)
        {
            this->WritefromDeque();
            abort();
        }
    }
}

void FileAppender::asyncWrite(std::vector<std::string> *buffer_ptr)
{
    if (buffer_ptr->empty())
        return;
    // д��֮ǰ��鵱ǰ�ļ�·���Ƿ���ȷ
    this->CheckFilePath();
    {
        std::lock_guard<std::mutex> lock(this->m_ofs_mutex);
        if (this->m_ofs.is_open())
        {
            for (const auto &log_str : *buffer_ptr)
            {
                this->m_ofs << log_str << std::endl;
            }
        }
        this->flush();
    }
}

void FileAppender::flush()
{
    this->m_ofs.flush();
}

void FileAppender::ResetLogdir(const std::string &logdir)
{
    this->m_logdir = logdir;
}

void FileAppender::close()
{
    if (this->m_ofs.is_open())
        this->m_ofs.close();
}

void FileAppender::CheckFilePath()
{
    // ͨ����־�ļ����Ƶ�ʱ��͵�ǰ
    // ��ȡ��ǰʱ��ĸ�����
    std::string time = Formatter::formattime();
    ssize_t pos = time.find(" ");
    std::string time_ymd_now = time.substr(0, pos);  // ��ȡ�����ղ���
    std::string time_hms_now = time.substr(pos + 1); // ��ȡʱ���벿��

    // ��ȡ��ǰ�ļ�·����ʱ��ĸ�����
    ssize_t pos_start = this->m_file_path.find_last_of("/");
    ssize_t pos_mid = this->m_file_path.find_last_of(" ");
    ssize_t pos_end = this->m_file_path.find_last_of(".");
    std::string time_ymd_file = this->m_file_path.substr(pos_start + 1, time_ymd_now.size());
    std::string time_hms_file = this->m_file_path.substr(pos_mid + 1, time_hms_now.size());

    // �������(�����ղ���)�����仯 �����´���һ����־�ļ�
    if (time_ymd_now != time_ymd_file)
    {
        // std::cout << time_ymd_file << std::endl;
        // std::cout << time_ymd_now << std::endl;
        // std::cout << "not equal" << std::endl;
        this->OpenNewFile();
    }
}

void FileAppender::OpenNewFile()
{
    // ���ԭ�����ļ����ڴ���ر�
    if (this->m_ofs.is_open())
        this->m_ofs.close();

    std::string time = Formatter::formattime();
    this->m_file_path = this->m_logdir + "/" + time + ".log";

    this->m_ofs.open(this->m_file_path, std::ios::app);
    if (!this->m_ofs.is_open())
    {
        std::cerr << "open file error: " << strerror(errno) << std::endl;
    }
}

void FileAppender::WritefromDeque()
{
    while (!this->buffer_deque.empty())
    {
        std::string str_log = this->buffer_deque.front();
        this->m_ofs << str_log << std::endl;
        this->buffer_deque.pop_front();
    }
    this->flush();
}

void Logger::AddAppender(std::shared_ptr<BasicAppender> appender)
{
    this->m_appenders.push_back(appender);
}

void Logger::log(LogLevel level, const std::string &msg, const char *_file_, int _line_)
{
    // ���˴�С�ڸü������־��Ϣ
    if (level < this->m_level)
        return;

    if (this->async_writeFunc)
    {
        this->async_writeFunc(level, msg, _file_, _line_);
    }
    else
    {
        for (const auto &appender : this->m_appenders)
        {
            std::lock_guard<std::mutex> lock(this->m_appender_mutex);
            appender->write(level, msg, _file_, _line_);
            // appender->flush();
        }
    }
}

void Logger::FlushTask()
{
    while (this->m_running)
    {
        for (const auto &appender : this->m_appenders)
        {
            std::lock_guard<std::mutex> lock(this->m_appender_mutex);
            appender->flush();
        }
        std::this_thread::sleep_for(std::chrono::seconds(this->m_flush_interval));
    }
}

void Logger::SetWriteFunc(const std::function<void(LogLevel, const std::string &, const char *, int)> &cb)
{
    this->async_writeFunc = cb;
}
