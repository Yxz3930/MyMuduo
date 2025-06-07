#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <iostream>
#include <thread>
#include <mutex>
#include <functional>
#include <condition_variable>
#include <deque>
#include <vector>
#include <atomic>
#include <future>
#include "Logger.h"

class EventLoop;

struct WorkerContext
{

    std::function<void()> task; // ʵ���Ͼ�ֻ��һ��loop û������������ ��������ֻ����һ���ص�����
    /**
     * ��Ȼֻ��һ�������� ��ôӦ�þͲ���Ҫ�������������ͻ�������
     */
    std::thread thread;
    EventLoop *loop = nullptr;
};

class ThreadPool
{
private:
    // �趨�߳�������������д�С
    std::atomic_int m_threadNums;
    std::atomic_int m_taskMaxNums;

    std::atomic_int m_max_tasks; // ������������

    using EventLoopPtrVector = std::vector<EventLoop *>;

    std::vector<WorkerContext> m_workerContext;

public:
    std::atomic_bool m_running{false};

public:
    ThreadPool(int threadNums, int taskMaxNums);

    ~ThreadPool();

    /// @brief �̳߳ؿ�ʼ�����̹߳���
    void start(EventLoopPtrVector &loopVec);

    // // ��װ���⺯�� ������Ҫ����ֵ
    // template <typename F, typename... Args>
    // auto addTask(F &&f, Args... args) -> std::future<typename std::result_of<F(Args...)>::type>;

    // �����߹������� ִ��m_workerContext����Ϊidx�Ĺ�������
    void worker(int idx, EventLoop *loop);

    /// @brief ֹͣ�̳߳ع���
    void stop();

};


// // �������з���ֵ�����
// template <typename F, typename... Args>
// inline auto ThreadPool::addTask(F &&f, Args... args)
//     -> std::future<typename std::result_of<F(Args...)>::type>
// {
//     // �������̵߳�����
//     {
//         std::unique_lock<std::mutex> lock(this->m_threadpool_mtx);

//         this->m_cv_producer.wait(lock, [this]()
//                                  { return ((this->m_running) && (this->m_task_deque.size() < this->m_max_tasks) || (!this->m_running)); });
//     }

//     using return_type = typename std::result_of<F(Args...)>::type;
//     if constexpr (std::is_void<return_type>::value)
//     {
//         // ����������Ϊ void ������
//         std::function<void()> task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
//         // ���뵽�������
//         {
//             std::lock_guard<std::mutex> lock(this->m_threadpool_mtx);
//             if (this->m_running)
//             {
//                 this->m_task_deque.emplace_back(task);
//                 this->m_cv_consumer.notify_one();
//             }
//             LOG_DEBUG(std::string("add task to threadpool, tasks nums is: " + std::to_string(this->m_task_deque.size())));
//         }

//         // ����Ҫ���� future
//         return std::future<void>(); // ����һ��Ĭ�Ϲ���� future
//     }
//     else
//     {
//         // ����� void �������͵�����
//         std::function<return_type()> task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
//         std::packaged_task<return_type()> p_task(task);
//         std::future<return_type> ret = p_task.get_future();

//         // ���뵽�������
//         {
//             std::lock_guard<std::mutex> lock(this->m_threadpool_mtx);
//             if (this->m_running)
//             {
//                 this->m_task_deque.emplace_back([p_task = std::move(p_task)]()
//                                                 { p_task(); });
//                 this->m_cv_consumer.notify_one();
//             }
//             LOG_INFO(std::string("add task to threadpool, tasks nums is: " + std::to_string(this->m_task_deque.size())));
//         }

//         return ret;
//     }
// }

#endif // THREADPOOL_H