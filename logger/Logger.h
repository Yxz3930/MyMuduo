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
 * Logger.h �ļ����а���LogLevel Logger Formater
 */

enum LogLevel
{
    DEBUG = 0,
    INFO,
    WARNING,
    ERROR,
    FATAL,
};

// // ����ʱ��־����Ĭ��ֵ��
// #ifndef LOG_LEVEL
// #define LOG_LEVEL LogLevel::DEBUG
// #endif


/// @brief ͨ�����еľ�̬������ʽ������
class Formatter
{
public:
    /// @brief �����ݸ�ʽ��
    /// @param level ��־����
    /// @param msg ��Ϣ
    /// @return string�ַ���
    static std::string format(LogLevel level, const std::string &msg, const char *_file_, int _line_);

    /// @brief ��ʱ����ʽ��Ϊ������ ʱ����ĸ�ʽ "%Y-%m-%d %H:%M:%S"
    /// @return
    static std::string formattime();

private:
    /// @brief ��LogLevel�����еĳ�Ա(int����)תΪ�ַ���
    /// @param level ��־����
    /// @return ��־�����ַ���
    static const std::string LogLeveltoString(LogLevel level);

    static const std::string GetColor(LogLevel level);
};

/// @brief ������ࣺ��־���
///        ����������ҪΪ�麯��
class BasicAppender
{
public:
    virtual ~BasicAppender() = default;

    /// @brief ���ն˻����ļ�д��
    /// @param level ��־����
    /// @param msg ��Ϣ
    virtual void write(LogLevel level, const std::string &msg, const char *_file_, int _line_) = 0;

    /// @brief ����ˢ�»����� ��������
    virtual void flush() = 0;
};

/// @brief ����̨�ն������
class ConsoleAppender : public BasicAppender
{
public:
    /// @brief ���ն����
    /// @param level ��־����
    /// @param msg ��Ϣ
    void write(LogLevel level, const std::string &msg, const char *_file_, int _line_) override;

    /// @brief ����ˢ�»����� ������������ն�
    void flush() override;

private:
    std::mutex m_cout_mutex;
};

class FileAppender : public BasicAppender
{
public:
    /// @brief ���� ��ʱû��log�ļ��򴴽�log�ļ�
    explicit FileAppender(std::string logdir = "./logs");

    /// @brief ���� �ж��ļ��Ƿ�ر� ���û����ر��ļ�
    ~FileAppender();

    /// @brief ���ļ�д��
    /// @param level ��־����
    /// @param msg ��Ϣ
    void write(LogLevel level, const std::string &msg, const char *_file_, int _line_) override;

    /// @brief �ļ�������첽д����

    /// @brief �ļ�������첽д���� ���������е���־��Ϣһ��д���ļ���
    /// @param buffer_ptr ������ָ��
    void asyncWrite(std::vector<std::string>* buffer_ptr);

    /// @brief ����ˢ�»����� ������������ļ�
    void flush() override;

    /// @brief ������־����ļ���·��
    /// @param logdir
    void ResetLogdir(const std::string &logdir);

    /// @brief �ر��ļ�
    void close();

private:
    /// @brief ���ݵ�ǰʱ�����ļ�·���Ƿ���ȷ
    void CheckFilePath();

    /// @brief ���ݵ�ǰʱ�䴴���µ���־�ļ�
    void OpenNewFile();

    /// @brief ���ڹ�����־ ���Ǽ����־·���Ƿ���ȷ, �Ƿ���Ҫ��һ���ļ������
    void RollLog();

    /// @brief ��buffer_deque�е�����д���ļ���
    void WritefromDeque();

private:
    std::ofstream m_ofs;                  // �ļ������
    std::string m_file_path;              // �ļ����·��
    std::string m_logdir;                 // ��־�ļ���
    std::mutex m_ofs_mutex;               // ofs���������
    std::string buffer;                   // ���ڴ洢��־��Ϣ
    std::deque<std::string> buffer_deque; // �洢������־��Ϣ

    int buffer_num; // ����ָ��buffer_deque�д洢����־����
};

class Logger
{
public:

    /// @brief ������־�������
    /// @param level 
    void setLogLevel(LogLevel level) { m_level = level; }

    /// @brief ����ģʽ ȷ��ֻ��һ����־����ʵ��
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

    /// @brief ��m_appenders��������������
    /// @param appender
    void AddAppender(std::shared_ptr<BasicAppender> appender);

    /// @brief ��m_appenders�еĶ���������
    /// @param level ��־����
    /// @param msg ��Ϣ
    void log(LogLevel level, const std::string &msg, const char *_file_, int _line_);

    /// @brief ����Log��������־�����ʽ
    /// @param cb 
    void SetWriteFunc(const std::function<void(LogLevel, const std::string &, const char *, int)>& cb);


private:
    /// @brief ˽�л�Ĭ�Ϲ��� ��ӿ����͸�ֵ֮�����Ҫ���Ĭ�Ϲ��� ��Ȼ�޷�ʵ��������
    Logger()
    {
        // ����FileAppender����ָ�벢��ӵ�m_appenders��
        init("./logs");
        this->m_appenders.push_back(std::shared_ptr<BasicAppender>(new FileAppender("../logs")));
        this->m_appenders.push_back(std::shared_ptr<BasicAppender>(new ConsoleAppender));

        // �����߳̽���ˢ��
        if (m_running == false)
        {
            this->m_running = true;
            this->flush_thread = std::thread(&Logger::FlushTask, this);
        }
    };

    /// @brief ��ֹ��������
    /// @param other
    Logger(const Logger &other) = delete;

    /// @brief ��ֹ��ֵ����
    /// @param other
    /// @return
    Logger &operator=(const Logger &other) = delete;

    /// @brief ��Logger���н������ˢ�� �ɵ������߳̽��й���
    /// @param appender ��Ҫˢ�µĶ�ʱ��
    void FlushTask();

private:
    int m_flush_interval{1}; // ˢ�¼�� ��λ��
    std::string m_logdir;
    std::vector<std::shared_ptr<BasicAppender>> m_appenders;
    std::mutex m_appender_mutex;

    std::atomic_bool m_running{false};
    std::thread flush_thread;

    std::function<void(LogLevel, const std::string &, const char *, int)> async_writeFunc; // ���������첽��־��������ص�

    LogLevel m_level;
};

#endif // LOGGER_H


// ��log��������#define��װ��ת��
#define LOG_DEBUG(msg) Logger::GetInstance().log(DEBUG, msg, __FILE__, __LINE__)
#define LOG_INFO(msg) Logger::GetInstance().log(INFO, msg, __FILE__, __LINE__)
#define LOG_WARNING(msg) Logger::GetInstance().log(WARNING, msg, __FILE__, __LINE__)
#define LOG_ERROR(msg) Logger::GetInstance().log(ERROR, msg, __FILE__, __LINE__)
#define LOG_FATAL(msg) Logger::GetInstance().log(FATAL, msg, __FILE__, __LINE__)


