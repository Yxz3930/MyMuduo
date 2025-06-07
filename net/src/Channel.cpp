
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
        // ��weak_ptr����Ϊshared_ptr 
        // �����Ǽ�����Channel��Ӧ��TcpConnection�Ƿ񻹴��� ������ڲŻ�ִ�лص�
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
     * TcpConnection��ʾһ������ ��Channel�������о���ִ�е�ģ�� ���TcpConnectionû����ôChannelִ�еĻص�Ҳ�ͻ��������
     * �����tie�����ǽ��ⲿ��TcpConnection��share_ptr����ָ�봫�ݽ�������weak_ptr������
     * ���Դ�TcpConnection֮�������ӻὫ����ָ�봫�ݽ������а󶨣�
     * Channel��ִ�е�ʱ���ȼ��һ���Ƿ�����Լ��Ƿ����shared_ptr������ ������ھ�ִ��
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
    // ʹ�õ���m_realEvent��epoll_wait�л�ȡ�õ����¼����� ���ݸ��¼������������¼�����

    // �ر��¼�
    if(this->m_realEvents & Channel::m_CloseEvent)
    {
        if(this->m_closeCallback)
            this->m_closeCallback();
    }

    // �����¼�
    if(this->m_realEvents & Channel::m_ErrorEvent)
    {
        if(this->m_errorCallback)
            this->m_errorCallback();
    }

    // ���¼�
    if(this->m_realEvents & Channel::m_ReadEvent)
    {
        if(this->m_readCallback)
            this->m_readCallback(receiveTime);
    }

    // д�¼�
    if(this->m_realEvents & Channel::m_WriteEvent)
    {
        if(this->m_writeCallback)
            this->m_writeCallback();
    }

}

