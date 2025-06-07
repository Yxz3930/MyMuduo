#ifndef CHANNEL_H
#define CHANNEL_H

#include <memory>
#include <vector>
#include "Socket.h"
#include <functional>
#include <sys/epoll.h>

class EventLoop;
class Socket;
class Epoll;
class TimeStamp;

/**
 * @brief Channel �Ƕ��ļ���������fd���Լ������Ȥ�¼��ĳ���
 *        Ҳ����˵Channel���з�װ��Socket�Ͷ�Ӧ���¼��ص�
 *        Channel.enbleʵ��������������: ��epoll�Ͻ���ע���Ӧ�¼�; ��epoll��ChannelMap�в���<fd, Channel*> �������enable֮�����ݾͻ��¼��map��
 *        ��Ӧ�ģ�disenableʵ���Ͻ����Ǵ�epoll��ɾ��channel�����ǲ�û�д�Map��ɾ��channel*����remove��ɾ�������channel*
 *        Ҳ��˵��enbalexxһ�Σ��෴�Ĳ�����Ҫ����disenable + remove
 */
class Channel
{
public:
    using ReadEventCallback = std::function<void(TimeStamp)>;
    using EventCallback = std::function<void()>;

    Channel(EventLoop *_loop, int _fd);
    ~Channel();

    /// @brief ��loop���д����¼�
    /// @param receiveTime ���յ�ʱ��
    void handlEvent(TimeStamp receiveTime);

    void setReadCallback(ReadEventCallback cb)
    {
        this->m_readCallback = std::move(cb);
    }
    void setWriteCallback(EventCallback cb)
    {
        this->m_writeCallback = std::move(cb);
    }
    void setCloseCallback(EventCallback cb)
    {
        this->m_closeCallback = std::move(cb);
    }
    void setErrorCallback(EventCallback cb)
    {
        this->m_errorCallback = std::move(cb);
    }

    /// @brief ��epollע��fd�������¼�
    void enableRead();

    /// @brief ��epollɾ��fd�������¼�
    void disableRead();

    /// @brief ��epollע��fd����д�¼�
    void enableWrite();

    /// @brief ��epollɾ��fd����д�¼�
    void disableWrite();

    /// @brief ��epollɾ�����м������¼�����
    void disableAll();

    /// @brief �жϵ�ǰchannel�ǲ���ʲô�¼���û��ע��
    /// @return
    bool isNoneEvent();

    /// @brief �жϵ�ǰchannel�ǲ���ע���˶��¼�����
    /// @return
    bool isReadEvent();

    /// @brief �жϵ�ǰchannel�ǲ���ע����д�¼�����
    /// @return
    bool isWriteEvent();

    /// @brief ���ڽ�channel�󶨵�tcpconnection�� ��Ϊchannel���еĻص�����ʵ���϶���tcpconnection���еĳ�Ա
    /// ��ô����Ҫ��֤��channelִ�й�����tcpconnection���ܻ����� ����ͱ����Ұָ����
    /// @param ptr ����ָ���װ����һ��void����
    void tie(const std::shared_ptr<void> &ptr);

    /// @brief ��ȡ��channel�󶨵�fd�ļ�������
    /// @return
    int fd() const { return this->m_fd; }

    /// @brief ��ȡ��channel��Ӧ���¼�
    /// @return
    int events() const { return this->m_events; }

    /// @brief ���ݴ�epoll_wait�л�ȡ�����¼����� ��������
    /// @param _revent epoll_wait��⵽���¼�����
    /// @return
    void setRealEvents(uint32_t _revent) { this->m_realEvents = _revent; }

    /// @brief ��ȡ��EventLoop*��Ӧ��ָ��
    /// @return
    EventLoop *loop() { return this->m_loop; }

    /// @brief ����������channel��epoll�����Ƴ���
    void remove();

    /// @brief ��ȡ��ǰchannel��״̬ �жϵ�ǰ��channel���´����� �����Ѿ���epoll���˵� �����Ѿ���epoll��ɾ���˵�
    /// ��Ӧ������״̬������EpollPoller.h�ļ���
    /// @return
    int state();

    /// @brief epoll_ctl�޸�֮�� ��Ҫ������������ø���״̬
    void setState(int _state);

private:
    /// @brief ����chanel���¼� �ں�����������ʹ�õ���m_event ������m_realEvents
    /// m_realEvents��Ա����ֻ�ڽ��¼������ͺͽ����¼������е��¼��Ա�ʱ
    void updateEvents();

    /// @brief ��handleEvent�����е���
    /// @param receiveTime
    void handleEventWithGuard(TimeStamp receiveTime);

private:
    EventLoop *m_loop;
    const int m_fd;

    uint32_t m_events;     // ��¼��ǰfd����Ȥ����epoll��ע����¼����ͣ�
    uint32_t m_realEvents; // epoll�ϼ�⵽��������ʵ���¼�����
    int m_state{-1};       // ��ʼ��Ϊ-1 m_state���ڱ�ʾ���channel���µ� �����Ѿ���ӵ�epoll�е� �����Ѿ���epoll���Ƴ��˵�

    std::weak_ptr<void> m_tie;
    bool m_tied{false};

    ReadEventCallback m_readCallback;
    EventCallback m_writeCallback;
    EventCallback m_closeCallback;
    EventCallback m_errorCallback;

    // ��ָ���¼��������� ��Ӧ��m_events
    static constexpr int m_NoneEvent = 0;         // û���¼�����
    static constexpr int m_ReadEvent = EPOLLIN;   // ���¼�
    static constexpr int m_WriteEvent = EPOLLOUT; // д�¼�
    static constexpr int m_ErrorEvent = EPOLLERR; // �����¼�
    static constexpr int m_CloseEvent = EPOLLHUP; // �ر��¼�
};

#endif // CHANNEL_H