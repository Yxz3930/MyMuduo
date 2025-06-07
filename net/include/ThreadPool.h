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

    std::function<void()> task; // 实际上就只有一个loop 没有其他函数了 所以这里只设置一个回调函数
    /**
     * 既然只有一个方法了 那么应该就不需要设置条件变量和互斥锁了
     */
    std::thread thread;
    EventLoop *loop = nullptr;
};

class ThreadPool
{
private:
    // 设定线程数量和任务队列大小
    std::atomic_int m_threadNums;
    std::atomic_int m_taskMaxNums;

    std::atomic_int m_max_tasks; // 最大的任务数量

    using EventLoopPtrVector = std::vector<EventLoop *>;

    std::vector<WorkerContext> m_workerContext;

public:
    std::atomic_bool m_running{false};

public:
    ThreadPool(int threadNums, int taskMaxNums);

    ~ThreadPool();

    /// @brief 线程池开始创建线程工作
    void start(EventLoopPtrVector &loopVec);

    // // 包装任意函数 并且需要返回值
    // template <typename F, typename... Args>
    // auto addTask(F &&f, Args... args) -> std::future<typename std::result_of<F(Args...)>::type>;

    // 消费者工作函数 执行m_workerContext索引为idx的工作函数
    void worker(int idx, EventLoop *loop);

    /// @brief 停止线程池工作
    void stop();

};


// // 函数带有返回值的情况
// template <typename F, typename... Args>
// inline auto ThreadPool::addTask(F &&f, Args... args)
//     -> std::future<typename std::result_of<F(Args...)>::type>
// {
//     // 生产者线程的阻塞
//     {
//         std::unique_lock<std::mutex> lock(this->m_threadpool_mtx);

//         this->m_cv_producer.wait(lock, [this]()
//                                  { return ((this->m_running) && (this->m_task_deque.size() < this->m_max_tasks) || (!this->m_running)); });
//     }

//     using return_type = typename std::result_of<F(Args...)>::type;
//     if constexpr (std::is_void<return_type>::value)
//     {
//         // 处理返回类型为 void 的任务
//         std::function<void()> task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
//         // 插入到任务队列
//         {
//             std::lock_guard<std::mutex> lock(this->m_threadpool_mtx);
//             if (this->m_running)
//             {
//                 this->m_task_deque.emplace_back(task);
//                 this->m_cv_consumer.notify_one();
//             }
//             LOG_DEBUG(std::string("add task to threadpool, tasks nums is: " + std::to_string(this->m_task_deque.size())));
//         }

//         // 不需要返回 future
//         return std::future<void>(); // 返回一个默认构造的 future
//     }
//     else
//     {
//         // 处理非 void 返回类型的任务
//         std::function<return_type()> task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
//         std::packaged_task<return_type()> p_task(task);
//         std::future<return_type> ret = p_task.get_future();

//         // 插入到任务队列
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