#ifndef ASYNCLOGGER_H
#define ASYNCLOGGER_H

#include "Logger.h"
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>

class AsyncLogger
{
public:
    AsyncLogger(int max_size = 100);
    ~AsyncLogger();

    /// @brief �첽��־д����
    /// @param level ��־����
    /// @param msg ��־��Ϣ
    void asynclog_write(LogLevel level, const std::string &msg, const char *_file_, int _line_);

private:
private:
    using buffer_t = std::vector<std::string>;
    using bufferPtr_t = std::vector<std::string> *;
    buffer_t buffer_1; // ������
    buffer_t buffer_2; // ������

    bufferPtr_t active_buffer_ptr;  // ��Ծ������ ������־д��Ļ�����
    bufferPtr_t standby_buffer_ptr; // ���û����� ʵ�������첽��־�߳���Ҫ����Ļ�����

    size_t MAX_BUFFER_SIZE = 100; // ���û������б������־��Ϣ��С

    std::ofstream m_ofs;
    FileAppender file_appender;

    // ������������
    std::mutex log_mtx;
    std::condition_variable log_cv;
    std::atomic_bool m_running;
    std::thread asynclogger_thread;

private:
    /// @brief ����������ָ��ָ��
    /// @param active_buffer
    /// @param standby_buffer
    void SwapBuffer();

    /// @brief �첽��־�̹߳�������
    void AsyncLogger_func();

    /// @brief ֹͣ�첽��־
    void stop();
};

#endif // ASYNCLOGGER_H