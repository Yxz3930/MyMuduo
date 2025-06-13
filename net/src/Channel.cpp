
#include "Channel.h"
#include "Socket.h"
#include <assert.h>
#include "myutils.h"
#include "TimeStamp.h"
#include "Logger.h"
#include <string>
#include <assert.h>
#include <TimeStamp.h>
#include "EventLoop.h"

Channel::Channel(EventLoop *_loop, int _fd)
    : m_loop(_loop), m_fd(_fd),
      m_events(0), m_realEvents(0),
      m_tied(false)
{
}

Channel::~Channel()
{
}

void Channel::handlEvent(TimeStamp receiveTime)
{
    std::shared_ptr<void> guard;
    if (this->m_tied)
    {
        // 处理绑定的通信连接的描述符
        guard = this->m_tie.lock();
        if (guard)
        {
            this->handleEventWithGuard(receiveTime);
        }
        else
        {
            // debug输出：对象已经销毁，跳过事件
            LOG_WARNING << "[Channel] tied but TcpConnection expired";
        }
    }
    else
    {
        // 处理没有绑定的文件描述符 也就是eventfd等这些不需要绑定的文件描述符
        handleEventWithGuard(receiveTime);
    }

    // std::shared_ptr<void> guard;
    // // 尝试从 weak_ptr 提升为 shared_ptr
    // guard = this->m_tie.lock(); // 即使 m_tied 为 false，也可以尝试 lock()，结果就是 nullptr
    // if (guard || !m_tied)       // 如果有 guard 保护，或者没有绑定（即非 TcpConnection 类的 Channel，如 timerfd）
    // {
    //     this->handleEventWithGuard(receiveTime);
    // }
    // else
    // {
    //     // 如果 m_tied 为 true 但 guard 为空，说明 TcpConnection 已经销毁
    //     LOG_WARNING << "[Channel] tied but TcpConnection expired, skipping event for fd " << m_fd;
    // }
}

void Channel::enableRead()
{
    this->m_events |= Channel::m_ReadEvent;
    this->updateEvents();
}

void Channel::disableRead()
{
    this->m_events &= ~Channel::m_ReadEvent;
    this->updateEvents();
}

void Channel::enableWrite()
{
    this->m_events |= Channel::m_WriteEvent;
    this->updateEvents();
}

void Channel::disableWrite()
{
    this->m_events &= ~Channel::m_WriteEvent;
    this->updateEvents();
}

void Channel::disableAll()
{
    this->m_events &= Channel::m_NoneEvent;
    this->updateEvents();
}

bool Channel::isNoneEvent()
{
    return this->m_events == Channel::m_NoneEvent;
}

bool Channel::isReadEvent()
{
    return this->m_events & Channel::m_ReadEvent;
}

bool Channel::isWriteEvent()
{
    return this->m_events & Channel::m_WriteEvent;
}

void Channel::tie(const std::shared_ptr<void> &ptr)
{
    /**
     * TcpConnection表示一个连接 而Channel是连接中具体执行的模块 如果TcpConnection没了那么Channel执行的回调也就会出现问题
     * 这里的tie作用是将外部的TcpConnection的share_ptr智能指针传递进来，用weak_ptr来接收
     * 所以创TcpConnection之后建立连接会将智能指针传递进来进行绑定，
     * Channel在执行的时候先检测一下是否绑定了以及是否这个shared_ptr还存在 如果存在就执行
     */
    this->m_tie = ptr;
    this->m_tied = true;
    LOG_INFO << "Channel::tie complete, TcpConnection ptr: " << ptr.get();
}

void Channel::remove()
{
    this->m_loop->removeChannel(this);
}

int Channel::state()
{
    return this->m_state;
}

void Channel::setState(int _state)
{
    this->m_state = _state;
}

void Channel::updateEvents()
{
    this->m_loop->updateChannel(this);
}


void Channel::handleEventWithGuard(TimeStamp receiveTime)
{
    LOG_INFO << "Channel::handleEventWithGuard start handle event, realEvents: " << m_realEvents;

    // 优先处理挂断或错误事件
    if (m_realEvents & (EPOLLHUP | EPOLLERR))
    { // 合并 EPOLLHUP 和 EPOLLERR 检查
        if (m_realEvents & EPOLLHUP)
        { // 对方关闭连接，可能同时有读事件
            if (m_closeCallback)
            {
                m_closeCallback();
            }
        }
        if (m_realEvents & EPOLLERR)
        { // 错误事件
            if (m_errorCallback)
            {
                m_errorCallback();
            }
        }
        // 如果只有 EPOLLHUP/EPOLLERR，则在此返回，不继续处理读写
        if (!(m_realEvents & (EPOLLIN | EPOLLOUT | EPOLLPRI | EPOLLRDHUP)))
        {
            return;
        }
    }

    // 读事件：包括普通数据、紧急数据和对端关闭读端
    if (m_realEvents & (EPOLLIN | EPOLLPRI | EPOLLRDHUP))
    {
        if (m_readCallback)
        {
            m_readCallback(receiveTime);
        }
    }

    // 写事件
    if (m_realEvents & EPOLLOUT)
    {
        if (m_writeCallback)
        {
            m_writeCallback();
        }
    }
}