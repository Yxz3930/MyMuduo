
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
    if(this->m_tied)
    {
        // 将weak_ptr提升为shared_ptr 
        // 这里是检测这个Channel对应的TcpConnection是否还存在 如果存在才会执行回调
        std::shared_ptr<void> guard = this->m_tie.lock();
        if(guard)
        {
            this->handleEventWithGuard(receiveTime);
        }
    }
    else
    {
        handleEventWithGuard(receiveTime);
    }
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
    return this->m_events == Channel::m_ReadEvent;
}

bool Channel::isWriteEvent()
{
    return this->m_events == Channel::m_WriteEvent;
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
    LOG_INFO("Channel::handleEventWithGuard start handle event");
    // 使用的是m_realEvent从epoll_wait中获取得到的事件类型 根据该事件类型来进行事件处理

    // 关闭事件
    if(this->m_realEvents & Channel::m_CloseEvent)
    {
        if(this->m_closeCallback)
            this->m_closeCallback();
    }

    // 错误事件
    if(this->m_realEvents & Channel::m_ErrorEvent)
    {
        if(this->m_errorCallback)
            this->m_errorCallback();
    }

    // 读事件
    if(this->m_realEvents & Channel::m_ReadEvent)
    {
        if(this->m_readCallback)
            this->m_readCallback(receiveTime);
    }

    // 写事件
    if(this->m_realEvents & Channel::m_WriteEvent)
    {
        if(this->m_writeCallback)
            this->m_writeCallback();
    }

}

