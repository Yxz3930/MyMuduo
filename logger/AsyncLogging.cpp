#include "AsyncLogging.h"
#include <string.h>
#include <iostream>

AsyncLogging::AsyncLogging(const char *logdir, int filesize, int interval, int write_num)
    : m_logfile(logdir, filesize, interval, write_num),
      m_running(true),
      activeBuffer(new FixedBuffer(4096)),
      standbyBuffer(new FixedBuffer(4096))
{
    this->bufferPool.reserve(16);
    this->m_asyncThread = std::move(std::thread(&AsyncLogging::AsyncLoggingFunc, this));
}

AsyncLogging::~AsyncLogging()
{
    this->Stop();
    // std::cout << "active_buffer size: " << this->activeBuffer->length() << std::endl;
    // std::cout << "standby_buffer size: " << this->standbyBuffer->length() << std::endl;
}

void AsyncLogging::append(const char *data, int len)
{
    {
        std::lock_guard<std::mutex> lock(this->m_writeMtx);

        if (this->activeBuffer->available() > len)
        {
            this->activeBuffer->append(data, len);
        }
        else
        {
            // ������activeBuffer��ӵ��������ص���
            this->bufferPool.push_back(std::move(this->activeBuffer));

            if (this->standbyBuffer)
            {
                // ������û��������� �򽻻�
                this->activeBuffer = std::move(this->standbyBuffer);
            }
            else
            {
                // ������û����������� �򴴽��µĻ�����
                this->activeBuffer.reset(new FixedBuffer(4096));
            }

            // ��־��Ϣʼ������activeBuffer��д��
            this->activeBuffer->append(data, len);
            this->m_cv.notify_one();
        }
    }

    if (isFatal(data))
    {
        this->Stop();
        std::cout << "AsyncLogging::append encounter fatal level msg" << std::endl;
        std::abort();
    }
}

void AsyncLogging::flush()
{
    this->m_logfile.flush();
}

void AsyncLogging::AsyncLoggingFunc()
{
    // ������������Ϊ�������б�����־��Ϣ�Ļ��������н���ʹ�õ�
    std::unique_ptr<FixedBuffer> newActiveBuffer = std::make_unique<FixedBuffer>(4096);
    std::unique_ptr<FixedBuffer> newStandbyBuffer = std::make_unique<FixedBuffer>(4096);
    std::vector<std::unique_ptr<FixedBuffer>> newBufferPool;
    newBufferPool.reserve(16);

    while (this->m_running)
    {
        {
            /**
             * ���ƶ�ָ��Ĺ�������Ҫ���� ��ʱ������Ϊ�˱������ƶ������� ǰ�˻���д�� ����ָ���ƶ����֮��Ͳ���Ҫ������
             */
            // ���� ���������������� ����������
            std::unique_lock<std::mutex> lock(this->m_writeMtx);

            // ��ʱ���� ��������ǰ���Ƚ⿪����
            this->m_cv.wait_for(lock, std::chrono::seconds(3));

            // ��������������
            if (!this->activeBuffer->empty())
            {
                this->bufferPool.push_back(std::move(this->activeBuffer));
                this->activeBuffer = std::move(newActiveBuffer);
            }

            // ������û�����standbyBufferΪ�� ˵��ָ�뱻activeBuffer������ ���µĻ�����ָ���ù���
            if (!this->standbyBuffer)
                this->standbyBuffer = std::move(newStandbyBuffer);

            // ������������
            newBufferPool.swap(this->bufferPool);
        }

        // ��д��Ĺ����в���Ҫ����
        for (auto &buffer : newBufferPool)
        {
            this->m_logfile.append(buffer->data(), buffer->length());
        }

        // ����������������Ҫ�����û�ָ��
        if (!newActiveBuffer)
        {
            newActiveBuffer = std::move(newBufferPool.back());
            newBufferPool.pop_back();
            newActiveBuffer->reset();
        }
        if (!newStandbyBuffer)
        {
            newStandbyBuffer = std::move(newBufferPool.back());
            newBufferPool.pop_back();
            newStandbyBuffer->reset();
        }
        newBufferPool.clear();
        this->flush();
    }
}

void AsyncLogging::SwapBufferPtr()
{
    std::swap(this->activeBuffer, this->standbyBuffer);
}

bool AsyncLogging::isFatal(const char *data)
{
    // �����ִ�Сд�Ĳ���
    if (strcasestr(data, "fatal") != nullptr)
        return true;
    else
        return false;
}

void AsyncLogging::Stop()
{
    this->m_cv.notify_one(); // �Ȼ����첽д�������߳�
    this->m_running = false; // ���������б�־λ
    if (this->m_asyncThread.joinable())
        this->m_asyncThread.join();

}
