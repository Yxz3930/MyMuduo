// #include "../include/ThreadPool.h"
#include "ThreadPool.h"
#include <iostream>
#include <chrono>
#include <algorithm>
#include "Logger.h"
#include <signal.h>
#include "EventLoop.h"
#include "myutils.h"

ThreadPool::ThreadPool(int threadNums, int taskMaxNums)
    : m_threadNums(threadNums), m_max_tasks(taskMaxNums),
      m_running(false)

{
}

ThreadPool::~ThreadPool()
{
    this->m_running.store(false);

    // 析构中释放new出来的EventLoop
    for (int i = 0; i < this->m_threadNums; ++i)
    {
        delete this->m_workerContext[i].loop;
        this->m_workerContext[i].loop = nullptr;
    }
    // 通知每个线程退出
    for (auto &ctx : this->m_workerContext)
    {
        if (ctx.loop)
            ctx.loop->quit(); // 告诉每个 loop 退出
    }
    // 等待线程回收
    for (auto &ctx : this->m_workerContext)
    {
        if (ctx.thread.joinable())
            ctx.thread.join();
    }

    // 清理数组
    this->m_workerContext.clear();
}

void ThreadPool::start(EventLoopPtrVector &loopVec)
{
    // m_running 为true
    this->m_running.store(true);
    this->m_workerContext.resize(this->m_threadNums);

    LOG_INFO << "subloop nums: " << this->m_threadNums;

    for (int i = 0; i < this->m_threadNums; ++i)
    {
        auto &workerContext = this->m_workerContext[i];
        workerContext.thread = std::thread([this, i, &loopVec, &workerContext]()
                                           {

            // m_workerContext设置
            EventLoop* subloop = new EventLoop();
            LOG_INFO << "worker thread id: " << std::hash<std::thread::id>{}(std::this_thread::get_id());
            LOG_INFO << "subloop ptr: " << ptrToString(subloop) << "\n";
            this->m_workerContext[i].loop = subloop;
            this->m_workerContext[i].task = std::bind(&EventLoop::loop, subloop);

            // 将subloop指针传回 里面的subloop生命周期掌管在线程池中
            loopVec.push_back(subloop);

            this->worker(i, subloop); });
    }
}

void ThreadPool::worker(int idx, EventLoop *loop)
{
    auto &workerContext = this->m_workerContext[idx];
    EventLoop *subloop = workerContext.loop;

    if (workerContext.task)
        workerContext.task();
}

void ThreadPool::stop()
{
    // !!! 这里不能加锁 一是本身是atomic不需要加锁 二是Stop在线程执行任务时被调用会造成死锁
    // std::lock_guard<std::mutex> lock(this->m_threadpool_mtx);

    this->m_running = false;
}
