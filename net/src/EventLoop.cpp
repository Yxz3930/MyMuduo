#include "EventLoop.h"
#include "Channel.h"
#include "myutils.h"
#include "Poller.h"
#include "Logger.h"
#include "EpollPoller.h"

#include <vector>
#include <thread>
#include <signal.h>
#include <sys/eventfd.h>
#include <string>
#include "myutils.h"

__thread EventLoop* thisThread = nullptr;


EventLoop::EventLoop(std::thread::id thread_id)
    : m_looping(false), m_quit(false),
      m_threadId(thread_id),
      m_pollerPtr(Poller::newPoller(this)), // 这是使用自己的智能指针替代太麻烦了
      m_wakeupFd(createEventFd()),
    //   m_wakeupChannel(new Channel(this, m_wakeupFd)),
      m_wakeupChannel(make_unique_from_pool<Channel>(this, m_wakeupFd))
{
    // 确保一个线程只有一个EventLoop 
    LOG_INFO << "EventLoop::EventLoop EventLoop threadId: " << std::to_string(std::hash<std::thread::id>{}(this->m_threadId));
    LOG_INFO << "EventLoop::EventLoop m_pollerPtr ptr: " << ptrToString(this->m_pollerPtr.get());

    // 设置eventfd的回调和注册 先注册回调 然后再往epoll上注册
    this->m_wakeupChannel->setReadCallback(
        std::bind(&EventLoop::handleEventFd, this)
    );
    LOG_INFO << "loop eventfd register read event";
    this->m_wakeupChannel->enableRead(); // 注册到epoll上 因为只是用于通知 所以只需要关注read即可
}

EventLoop::~EventLoop()
{
    /**
     * 因为eventfd是在本类中创建 那么就需要响应的措施析构掉
     */
    this->m_wakeupChannel->disableAll();
    this->m_wakeupChannel->remove();
    // 关闭 eventfd
    if (this->m_wakeupFd >= 0) { // 良好的习惯，确保fd有效
        ::close(this->m_wakeupFd);
        this->m_wakeupFd = -1; // 避免双重关闭
    }
    thisThread = nullptr;
}

void EventLoop::loop()
{
    this->m_looping.store(true);
    this->m_quit.store(false);
    size_t tid = std::hash<std::thread::id>{}(this->m_threadId);
    LOG_INFO << "thread " << tid << " start loop..." ;
    
    /**
     * EventLoop进行事件循环 但是事件触发之后是通过Channel来执行各自的任务的
     */
    while(!this->m_quit)
    {
        // 用于存储实际触发了的事件channel
        this->m_activeChannel.clear();
        this->m_recvTime = this->m_pollerPtr->wait(&this->m_activeChannel);
        // 挨个执行回调
        for(Channel* channel : this->m_activeChannel)
        {
            channel->handlEvent(this->m_recvTime);
        }

        // 执行添加的函数 如果没有则函数vector容器为空 空跑一遍
        this->doPendingFuntor();

    }
    LOG_INFO << "thread " << tid << " stop loop..." ;
    this->m_looping.store(false);
}

void EventLoop::quit()
{
    // quit实际上只需要通过eventfd解除epoll_wait阻塞 然后quit置为true 自然就会退出loop循环了
    this->m_quit.store(true);

    if(! this->isThisThread())
        this->wakeup();
}

void EventLoop::wakeup()
{
    uint64_t buf = 1;
    size_t n = write(this->m_wakeupFd, &buf, sizeof(buf));
    ErrorIf(n != sizeof(buf), 
                    std::string("EventLoop::handleEventFd read " + std::to_string(n) + "bytes instead of 8 bytes").c_str());
}

TimeStamp EventLoop::revcTime()
{
    return this->m_recvTime;
}

void EventLoop::runInLoop(Functor cb)
{
    // 如果是本线程的 那么可以直接执行回调
    if(this->isThisThread())
        cb();

    // 如果不是当前线程的loop 则需要插入到loop当中的函数队列中
    else
        this->queueInLoop(cb);
    
}

void EventLoop::queueInLoop(Functor cb)
{
    /**
     * EventLoop中加锁的原因
     * 因为主线程mainloop可能调用runInLoop方法往subloop上分配连接，里面调用queueInLoop方法，往容器里面的添加回调函数
     * 但是如果这个subloop正在执行doPendingFuntor就会通过swap把这个容器清空，出现矛盾
     * 一方面是主线程往容器中添加回调任务，一方面子线程也可能对容器操作，从而数显多个线程的情况，所以需要加锁
     */
    {
        std::lock_guard<std::mutex> lock(this->m_mtx);
        this->m_FunctorVector.push_back(cb);
    }

    // 如果不是本线程的函数插入 或者 本线程的loop正在执行之前添加到容器当中的函数
    // 只有! this->isThisThread()也可以, 只不过这里考虑到了本线程中的loop正在执行添加的函数 此时就在本线程当中 不添加的话就不会进行唤醒了
    if(! this->isThisThread() || this->m_isDoingPendingFunctors)
        wakeup();
}

/**
 * 下面这三个函数都是Channel类中进行调用 然后调用EventLoop类中的方法 最终调用EpollPoller当中的方法
 * 因为EpollPoller(Poller是抽象父类)是对epfd的封装, 记录着所有的channel
 */

void EventLoop::updateChannel(Channel *channel)
{
    this->m_pollerPtr->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    this->m_pollerPtr->removeChannel(channel);
}

void EventLoop::hasChannle(Channel *channel)
{
    this->m_pollerPtr->hasChannel(channel);
}

bool EventLoop::isThisThread()
{
    return this->m_threadId == std::this_thread::get_id();
}

size_t EventLoop::getMapSize()
{
    return this->m_pollerPtr->getMapSize();
}

int EventLoop::createEventFd()
{
    int eventfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    ErrorIf(eventfd < 0, "eventfd create error");
    return eventfd;
}

void EventLoop::handleEventFd()
{
    // 使用的是 eventfd 机制，read() 时需要读取 8 字节（uint64_t），写入时也必须是 8 字节
    // 只是用于读出唤醒信号而已，内容并不重要，重要的是这里的读取实际上是用于清空 eventfd 内核缓冲区，使其下次能继续触发事件
    uint64_t buf = 1; // 8字节
    size_t n = read(this->m_wakeupFd, &buf, sizeof(buf));
    ErrorIf(n != sizeof(buf), 
                    std::string("EventLoop::handleEventFd read " + std::to_string(n) + "bytes instead of 8 bytes").c_str());
}

void EventLoop::doPendingFuntor()
{
    std::vector<Functor> funtionVec;
    this->m_isDoingPendingFunctors.store(true);

    {
        // 将m_FunctorVector中的任务交换出来
        std::lock_guard<std::mutex> lock(this->m_mtx);
        funtionVec.swap(this->m_FunctorVector);
    }

    // 挨个执行回调函数 一般来说是主线程mainloop往subloop中添加的函数 需要subloop来执行
    for(Functor func : funtionVec)
        func();

    this->m_isDoingPendingFunctors.store(false);
}


