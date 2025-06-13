
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
        // ����󶨵�ͨ�����ӵ�������
        guard = this->m_tie.lock();
        if (guard)
        {
            this->handleEventWithGuard(receiveTime);
        }
        else
        {
            // debug����������Ѿ����٣������¼�
            LOG_WARNING << "[Channel] tied but TcpConnection expired";
        }
    }
    else
    {
        // ����û�а󶨵��ļ������� Ҳ����eventfd����Щ����Ҫ�󶨵��ļ�������
        handleEventWithGuard(receiveTime);
    }

    // std::shared_ptr<void> guard;
    // // ���Դ� weak_ptr ����Ϊ shared_ptr
    // guard = this->m_tie.lock(); // ��ʹ m_tied Ϊ false��Ҳ���Գ��� lock()��������� nullptr
    // if (guard || !m_tied)       // ����� guard ����������û�а󶨣����� TcpConnection ��� Channel���� timerfd��
    // {
    //     this->handleEventWithGuard(receiveTime);
    // }
    // else
    // {
    //     // ��� m_tied Ϊ true �� guard Ϊ�գ�˵�� TcpConnection �Ѿ�����
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
     * TcpConnection��ʾһ������ ��Channel�������о���ִ�е�ģ�� ���TcpConnectionû����ôChannelִ�еĻص�Ҳ�ͻ��������
     * �����tie�����ǽ��ⲿ��TcpConnection��share_ptr����ָ�봫�ݽ�������weak_ptr������
     * ���Դ�TcpConnection֮�������ӻὫ����ָ�봫�ݽ������а󶨣�
     * Channel��ִ�е�ʱ���ȼ��һ���Ƿ�����Լ��Ƿ����shared_ptr������ ������ھ�ִ��
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

    // ���ȴ���Ҷϻ�����¼�
    if (m_realEvents & (EPOLLHUP | EPOLLERR))
    { // �ϲ� EPOLLHUP �� EPOLLERR ���
        if (m_realEvents & EPOLLHUP)
        { // �Է��ر����ӣ�����ͬʱ�ж��¼�
            if (m_closeCallback)
            {
                m_closeCallback();
            }
        }
        if (m_realEvents & EPOLLERR)
        { // �����¼�
            if (m_errorCallback)
            {
                m_errorCallback();
            }
        }
        // ���ֻ�� EPOLLHUP/EPOLLERR�����ڴ˷��أ������������д
        if (!(m_realEvents & (EPOLLIN | EPOLLOUT | EPOLLPRI | EPOLLRDHUP)))
        {
            return;
        }
    }

    // ���¼���������ͨ���ݡ��������ݺͶԶ˹رն���
    if (m_realEvents & (EPOLLIN | EPOLLPRI | EPOLLRDHUP))
    {
        if (m_readCallback)
        {
            m_readCallback(receiveTime);
        }
    }

    // д�¼�
    if (m_realEvents & EPOLLOUT)
    {
        if (m_writeCallback)
        {
            m_writeCallback();
        }
    }
}