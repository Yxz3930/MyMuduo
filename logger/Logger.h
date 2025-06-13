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

/// @brief ������־�������õ���
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

/// @brief Logger���в��ܴ��LogFile���� ��ΪLogger��RAII���� ��ÿ����־���ʱ���д��������� �������LogFile���� ��ôÿ��������ᴴ��һ����־�ļ�
/// ����Ȼ�ǲ����е�
class Logger
{

public:
    /// @brief ���캯�� ����ʼ�����еĳ�Ա����
    /// @param level ��־����
    /// @param __file__ �ļ���
    /// @param __line__ ������
    Logger(LogLevel level, const char *__file__, int __line__);

    /// @brief ��������
    /// �����������л�ȡ�ַ����������е����� Ȼ�����д��
    ~Logger();

    /// @brief ����LogStream��־������
    /// @return
    LogStream &stream() { return this->m_logstream; };

    /// @brief ע��д�����ص�����
    /// @param
    static void SetWritFunc(std::function<void(const char *msg, int len)>);

    /// @brief ע��ˢ�»������ص�����
    /// @param
    static void SetFlushFunc(std::function<void(void)>);

private:
    /// @brief ���û��ע��д�뷽�� ��ʹ�ø�Ĭ��д�뷽��
    /// @param data
    /// @param len
    void DefaultWriteFunc(const char *data, int len);

    /// @brief ���û��ע��ˢ�·��� ��ʹ�ø�Ĭ��ˢ�·���
    void DefaultFlushFunc();

private:
    static std::function<void(const char *msg, int len)> m_write_cb;
    static std::function<void(void)> m_flush_cb;

    LogStream m_logstream;
    // static LogFile m_logfile;

private:
    /// @brief ���ڸ�ʽ����־��Ϣ
    class Formatter
    {
    public:
        /// @brief Formatter����
        /// @param level
        /// @param file
        /// @param line
        Formatter(LogLevel level, const char *file, int line);

        /// @brief ����־��Ϣ��ʽ��
        std::string MsgtoFormat(const char *msg);

        /// @brief ��ȡ�����ļ��� ȥ�����е�·��
        /// @return �ļ���
        std::string GetCodeFileName();

        /// @brief ����־����ת��Ϊ�ַ���
        /// @param level ��־����
        /// @return �ַ���
        std::string LeveltoStr(LogLevel level);

        LogLevel m_level;
        std::string m_file;
        int m_line;
    };

private:
    Formatter m_formatter; // ������Ҫ����ʵ���� ����ᱨ��
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
