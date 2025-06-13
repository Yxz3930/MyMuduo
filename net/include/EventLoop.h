#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <memory>
#include <atomic>
#include <mutex>
#include <queue>
#include <thread>
#include <functional>
#include "TimeStamp.h"
#include "SmartPointer.h"


class Channel;
class Poller;

/// @brief EventLoop 用于事件循环处理 其中包含Channel, Poller(EpollPoller的抽象父类)两个模块
///        1. Channel类中事件的循环 2. 不同线程之间eventfd的通知 3. mainloop中向subloop进行分发
class EventLoop
{
public:
    using Functor = std::function<void()>;

    EventLoop(std::thread::id thread_id = std::this_thread::get_id());
    ~EventLoop();

    /// @brief 事件循环
    void loop();

    /// @brief 退出loop
    void quit();

    /// @brief 向eventfd的写端写入数据 用于唤醒epoll_wait
    void wakeup();

    /// @brief 每次epoll_wait解除阻塞的时间 也就是有事件触发的时间
    /// @return 
    TimeStamp revcTime();

    /// @brief 在当前线程和非当前线程两种情况下执行函数 里面有eventfd唤醒操作
    void runInLoop(Functor cb);

    /// @brief 将回调函数插入到函数队列容器中(vector容器) 里面有eventfd唤醒操作
    /// @param cb 
    void queueInLoop(Functor cb);

    /// @brief 更新channel事件 Channel::updateEvents -> EventLoop::updateChannel -> Poller::updateChannel
    /// @param channel 
    void updateChannel(Channel* channel);

    /// @brief 移除channel Channel::remove -> EventLoop::removeChannel -> Poller::removeChannel
    /// @param channel 
    void removeChannel(Channel* channel);

    /// @brief 查看是否存在channel 
    /// @param channel 
    void hasChannle(Channel* channel);

    /// @brief 判断调用这个函数的是否是本身这个线程或者说是否是本身这个loop
    /// @return
    bool isThisThread();

    /// @brief 获取map的大小
    /// @return 
    size_t getMapSize();

private:

    /// @brief 创建eventfd
    /// @return
    int createEventFd();

    /// @brief 处理eventfd的读事件(和wakeup方法匹配) 因为eventfd只用于线程之间的事件通知
    void handleEventFd();

    /// @brief 执行添加到m_FunctorVector中的回调函数
    void doPendingFuntor();

private:
    // 接收epoll_wait返回过来的存储Channel*的vector容器 
    using ChannelVector = std::vector<Channel *>;

    // 控制loop循环
    std::atomic_bool m_looping; // 正在进行loop循环
    std::atomic_bool m_quit;    // 是否退出loop循环

    std::thread::id m_threadId; // 用于记录当前线程id

    // std::unique_ptr<Poller> m_pollerPtr; // 保存对应的Poller抽象类对象 实际上是EpollPoller
    std::unique_ptr<Poller, PoolDeleter<Poller>> m_pollerPtr; // 保存对应的Poller抽象类对象 实际上是EpollPoller

    /**
     * EventLoop在各自线程中运行 但是不同线程之间需要进行通知 这就需要eventfd
     * 需要fd的地方，往往还需要一个Channel来管理这个fd对应的事件
     * 当然这里是一个int与一个unique_ptr<Channel>
     * 也可以是一个unique_ptr<Socket>和一个unique_ptr<Channel>
     */
    int m_wakeupFd;                           // eventfd 用于新连接之后通过eventfd来唤醒一个subloop 也需要注册到epoll上
    // std::unique_ptr<Channel> m_wakeupChannel; // 用于eventfd对应的事件处理通道
    std::unique_ptr<Channel, PoolDeleter<Channel>> m_wakeupChannel; // 用于eventfd对应的事件处理通道

    ChannelVector m_activeChannel; // 存储触发事件的Channel* 用于执行回调

    TimeStamp m_recvTime; // 通过Poller接收事件的事件

    std::atomic_bool m_isDoingPendingFunctors; // 正在执行外部线程或者当前线程通过queueInLoop(cb)添加进来的函数
    std::vector<Functor> m_FunctorVector;      // 用于存储添加进来的函数 std::function<void()>
    std::mutex m_mtx;                          // 保护添加函数过程中的资源
    /**
     * EventLoop中加锁的原因
     * 因为主线程mainloop可能调用runInLoop方法往subloop上分配连接，里面调用queueInLoop方法，往容器里面的添加回调函数
     * 但是如果这个subloop正在执行doPendingFuntor就会通过swap把这个容器清空，出现矛盾
     * 一方面是主线程往容器中添加回调任务，一方面子线程也可能对容器操作，从而数显多个线程的情况，所以需要加锁
     */

};

#endif // EVENT_LOOP_H