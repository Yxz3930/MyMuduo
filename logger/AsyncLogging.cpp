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
            // 将满的activeBuffer添加到缓冲区池当中
            this->bufferPool.push_back(std::move(this->activeBuffer));

            if (this->standbyBuffer)
            {
                // 如果备用缓冲区可用 则交换
                this->activeBuffer = std::move(this->standbyBuffer);
            }
            else
            {
                // 如果备用缓冲区不可用 则创建新的缓冲区
                this->activeBuffer.reset(new FixedBuffer(4096));
            }

            // 日志信息始终是往activeBuffer中写入
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
    // 下面这三个是为了与类中保存日志信息的缓冲区进行交换使用的
    std::unique_ptr<FixedBuffer> newActiveBuffer = std::make_unique<FixedBuffer>(4096);
    std::unique_ptr<FixedBuffer> newStandbyBuffer = std::make_unique<FixedBuffer>(4096);
    std::vector<std::unique_ptr<FixedBuffer>> newBufferPool;
    newBufferPool.reserve(16);

    while (this->m_running)
    {
        {
            /**
             * 在移动指针的过程中需要加锁 此时加锁是为了避免在移动过程中 前端还在写入 但是指针移动完成之后就不需要加锁了
             */
            // 加锁 设置条件变量阻塞 交换缓冲区
            std::unique_lock<std::mutex> lock(this->m_writeMtx);

            // 定时阻塞 进入阻塞前会先解开锁的
            this->m_cv.wait_for(lock, std::chrono::seconds(3));

            // 交换缓冲区数据
            if (!this->activeBuffer->empty())
            {
                this->bufferPool.push_back(std::move(this->activeBuffer));
                this->activeBuffer = std::move(newActiveBuffer);
            }

            // 如果备用缓冲区standbyBuffer为空 说明指针被activeBuffer拿走了 则将新的缓冲区指针拿过来
            if (!this->standbyBuffer)
                this->standbyBuffer = std::move(newStandbyBuffer);

            // 交换缓冲区池
            newBufferPool.swap(this->bufferPool);
        }

        // 在写入的过程中不需要加锁
        for (auto &buffer : newBufferPool)
        {
            this->m_logfile.append(buffer->data(), buffer->length());
        }

        // 上面两个缓冲区需要重新拿回指针
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
    // 不区分大小写的查找
    if (strcasestr(data, "fatal") != nullptr)
        return true;
    else
        return false;
}

void AsyncLogging::Stop()
{
    this->m_cv.notify_one(); // 先唤醒异步写操作子线程
    this->m_running = false; // 再设置运行标志位
    if (this->m_asyncThread.joinable())
        this->m_asyncThread.join();

}
