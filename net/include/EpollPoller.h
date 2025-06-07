#ifndef EPOLL_H
#define EPOLL_H
#include <sys/epoll.h>
#include <vector>
#include "Channel.h"
#include "Poller.h"
#include "TimeStamp.h"

#define MAX_EVENTS 1024

class Channel;
class EventLoop;

/// @brief EpollPoller �̳��� Poller , �Ƕ�epoll�ķ�װ ����Poller������
///        1. ��channel��epoll�ϵ�״̬���¼������޸� 2. IO��·��������, Ҳ����epoll_wait
class EpollPoller : public Poller
{
    /**
     * EpollPoller�����ƹ��ŷ�������һ�˵�epfd������Ȩ
     * ������Ҫ�ڹ��캯���е���epoll_create1 �����������йر�epfd
     */
public:
    EpollPoller(EventLoop *_loop);

    ~EpollPoller() override;

    // ��д�麯��

    /// @brief ʵ�ʾ��Ƕ�epoll_wait�İ�װ �������epoll_wait�¼����ͽ����Լ����м�¼
    /// @param activeChannel ���ڱ��津����Channel
    /// @return ���ص���epoll_wait���������ʱ���
    TimeStamp wait(ChannelVector* activeChannel) override;

    /// @brief ����channel���� ʵ���Ͼ��Ǹ���channel��Ӧ����epoll�ϵ�״̬��add��del�����¼����޸��¼�ֻ��Ҫmod��
    /// ����ʵ���ϰ�����������: һ���Ǹ���channel������epoll�ϵ�״̬(���,ɾ��,�޸�); һ���Ǹ���channel�м������¼�
    /// @param channel
    void updateChannel(Channel *channel) override;

    /// @brief ��channel��epoll���Ƴ� ͬʱ��channel�Ӽ�¼��������ɾ��
    /// @param channel
    void removeChannel(Channel *channel) override;

private:
    /// @brief ��װ�ײ��epoll_ctl
    /// @param operation
    /// @param channel
    void control(int operation, Channel *channel);

    /// @brief ��epoll_wait�õ���m_eventsVec�е�epoll_event�ṹ�� תΪChannel*֮����뵽vector������
    /// @param eventNums epoll�ϴ������¼�����
    /// @param
    void pushActiveChannelVector(int eventNums, ChannelVector* activeChannel);

private:
    int m_epfd;                                // epoll��Ӧ���ļ�������epfd
    static constexpr int m_initEventSize = 16; // epoll_wait������� ��ʼʱ�����¼����� ������ʼ��vector�����Ĵ�С

    using EventVector = std::vector<epoll_event>;
    EventVector m_eventsVec; // ���ڴ�epoll_wait�϶�ȡ�¼� ���������Ҫ��Ϊepoll_wait�Ĳ���

    // channel(ʵ������fd)��epoll�ϵ�״̬ Channel�ʼ��״̬��-1,Ҳ����new��״̬
    static constexpr int s_new = -1;   // �µ�fd(�µ�channel)
    static constexpr int s_add = 1;    // fd����˵channel�Ѿ���ӵ�epoll����
    static constexpr int s_delete = 2; // fd����˵channel�Ѿ���epoll���Ƴ���

    // // ��¼ÿ��channel�Ͷ�Ӧ��fd <socket_fd, channel*>
    // // using ChannelMap = std::unordered_map<int, Channel*>;
    // // ChannelMap m_channelMap; ����Ǹ���ı�����Ա���� ���ڼ�¼ÿ��channel�Ͷ�Ӧ��fd
};

#endif // EPOLL_H