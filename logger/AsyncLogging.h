#ifndef ASYNCLOGGING_H
#define ASYNCLOGGING_H

#include <string>
#include "LogFile.h"
#include "FixedBuffer.h"
#include <mutex>
#include <condition_variable>
#include <memory>
#include <thread>
#include <atomic>
#include <vector>

/// @brief �첽��־��
/// ���Ǹ���ֻ�����첽��־����������ʵ�֣�����˫���塢�����߳�д�����ݣ���󷽷���Ҫע�ᵽLogger����
/// Ϊ�˼򵥷��� ʹ�����е�������Ϊ������б���
class AsyncLogging
{
public:
    /// @brief ���캯�� ��������LogFile��Ĳ���
    /// @param logdir ��־�ļ�·��
    /// @param filesize ��־�ļ���С�޶�
    /// @param interval ��־ˢ��ʱ����
    /// @param write_num ��־�ļ�д������޶�
    AsyncLogging(const char *logdir, int filesize, int interval = 3, int write_num = 1024);

    /// @brief ��������
    ~AsyncLogging();

    /// @brief �첽��־��ǰ�������־��Ϣ�������� �����append���ⲿ��־д��ĺ���
    /// append�����ᱻע�ᵽLogger����m_write_cb�ص������� Ҳ����˵�����data���ǻ������־������Ϣ��
    /// @param data ��־��Ϣ
    /// @param len ��־��Ϣ����
    void append(const char *data, int len);

    /// @brief ˢ���ļ�������
    void flush();

    /// @brief �첽�̹߳������� ����������첽��־ϵͳ�п��ٵ�д���߳̽���д����߼�
    void AsyncLoggingFunc();

    /// @brief ֹͣ�첽��־�߳�
    /// ���������ڷ���ʹ�� ��ʣ������д��
    /// Ҳ�������ⲿ���� ֹͣ��־ϵͳ
    void Stop();

private:
    /// @brief ����������������ָ��
    void SwapBufferPtr();

    /// @brief �ж���־��Ϣ���Ƿ����FATAL
    /// @param data ��ʽ��֮�����־��Ϣ
    /// @param len ��־��Ϣ����
    bool isFatal(const char* data);

private:
    LogFile m_logfile; // �����߳�д���ļ�����

    // �̡߳�����ر���
    std::thread m_asyncThread;    // ����첽д����߳�
    std::mutex m_writeMtx;        // д�����������
    std::condition_variable m_cv; // ��������
    std::atomic_bool m_running{false}; // ���б�־

    std::unique_ptr<FixedBuffer> activeBuffer;
    std::unique_ptr<FixedBuffer> standbyBuffer;
    std::vector<std::unique_ptr<FixedBuffer>> bufferPool; // �洢���������

};

#endif // ASYNCLOGGING_H