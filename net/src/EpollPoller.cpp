#include "EpollPoller.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <cstring>
#include "myutils.h"
#include "Logger.h"

EpollPoller::EpollPoller(EventLoop *_loop)
    : Poller(_loop),                            // ���๹�캯��
      m_eventsVec(EpollPoller::m_initEventSize) // vector<epoll_event>(16)
{
    this->m_epfd = ::epoll_create1(EPOLL_CLOEXEC);

    ErrorIf(this->m_epfd < 0, "epoll_create1 error");
}

EpollPoller::~EpollPoller()
{
    ::close(this->m_epfd);
}

TimeStamp EpollPoller::wait(ChannelVector *activeChannel)
{
    LOG_INFO("epoll wait...");

    // ʹ��vector�������ԭ����epoll_event[]���� �����ǶԵ�����ȡ*�õ�vector��Ԫ��,Ȼ����ȡ��ַ
    // ����ÿ��epoll_waitд��ʱ���Ǵ�begin���׵�ַԪ�ؿ�ʼд��
    int eventsNums = ::epoll_wait(this->m_epfd, &*this->m_eventsVec.begin(), this->m_eventsVec.size(), -1);
    TimeStamp now; // ��ȡepoll���������ʱ��

    if (eventsNums > 0)
    {
        LOG_INFO(std::to_string(eventsNums) + " events happend");
        this->pushActiveChannelVector(eventsNums, activeChannel);
        if (eventsNums == this->m_eventsVec.size()) // ����epoll_event��vevtor������������
        {
            // ���ݲ��ܷ���pushActiveChannelVector֮ǰ ����˵������û�н�m_eventsVec�е����ݱ�������֮ǰ�������� ��Ϊ�������ܵ���ԭ�������ݳ�����
            this->m_eventsVec.resize(this->m_eventsVec.size() * 2);
        }
    }
    else if (eventsNums == 0)
    {
        LOG_ERROR("eventNums = 0");
    }
    else
    {
    }

    return now;
}

void EpollPoller::updateChannel(Channel *channel)
{
    int curState = channel->state();
    LOG_INFO("EpollPoller::updateChannel channel ptr: " + ptrToString(channel));
    LOG_INFO("EpollPoller::updateChannel channel fd: " + std::to_string(channel->fd()) + ", curState " + std::to_string(curState));

    if (curState == EpollPoller::s_new || curState == EpollPoller::s_delete)
    {
        // ���channel(fd)���´����Ļ������Ѿ�����ʱ�Ƴ��˵�(ע������ʱ���Ƴ��˵� �䱾��û�д�channelMap�б�ɾ��) ��ô����Ҫ������ӵ�epoll��
        if (curState == EpollPoller::s_new)
        {
            int fd = channel->fd();
            this->m_channelMap[fd] = channel;
        }
        else
        {
            // ��ΪEpollPoller::s_delete״̬ʱ channel������Ȼ�Ǵ��ڵ� ��Ȼ��channelMap�����Բ���Ҫ�ٴ����
        }
        channel->setState(EpollPoller::s_add);
        this->control(EPOLL_CTL_ADD, channel);
    }
    else
    {
        // channel�Ѿ���epoll��ע����� ��ô����Ҫ�����޸�
        int fd = channel->fd();
        if (channel->isNoneEvent())
        {
            // ���channel����û���¼� ��ô����Ҫ�����epoll��ɾ�� ������״̬
            // ע�� ����ֻ�ǽ����epoll���Ƴ� ���ǲ�û��ɾ��channel������� ��������Ժ��Իᱻע��ʹ��
            this->control(EPOLL_CTL_DEL, channel);
            channel->setState(EpollPoller::s_delete);
        }
        else
            // ���������ֻ��Ҫ����Mod�޸ļ���
            this->control(EPOLL_CTL_MOD, channel);
    }
}

void EpollPoller::removeChannel(Channel *channel)
{
    // ������Ƴ�channel���Ǵ�channelMap�н�channelɾ������ ������Ȼû�н�channel��������
    int fd = channel->fd();
    this->m_channelMap.erase(fd);

    LOG_INFO("fd " + std::to_string(fd) + " is erased from channelMap");

    int curState = channel->state();
    if (curState == EpollPoller::s_add)
        this->control(EPOLL_CTL_DEL, channel);
    channel->setState(EpollPoller::s_new);
}

void EpollPoller::control(int operation, Channel *channel)
{
    epoll_event event;
    ::memset(&event, 0, sizeof(event));

    event.data.ptr = static_cast<void*>(channel);
    event.events = channel->events();

    int ret = ::epoll_ctl(this->m_epfd, operation, channel->fd(), &event);
    if (ret < 0)
    {
        if (operation == EPOLL_CTL_ADD)
            LOG_ERROR("epoll_ctl add error");
        if (operation == EPOLL_CTL_MOD)
            LOG_ERROR("epoll_ctl mod error");
        if (operation == EPOLL_CTL_DEL)
            LOG_ERROR("epoll_ctl del error");
    }
}

void EpollPoller::pushActiveChannelVector(int eventNums, ChannelVector *activeChannel)
{
    for (int i = 0; i < eventNums; ++i)
    {
        // ��epoll_event�����е�ptrתΪChannel*���б���
        Channel *channel = static_cast<Channel *>(this->m_eventsVec[i].data.ptr);

        LOG_INFO("EpollPoller::pushActiveChannelVector channel ptr: " + ptrToString(channel));
        LOG_INFO("EpollPoller::pushActiveChannelVector channel fd: " + std::to_string(channel->fd()));

        // ��epollʵ�ʼ�⵽���¼�����д�뵽channel��
        channel->setRealEvents(this->m_eventsVec[i].events);
        // ��channel���뵽m_activeChannelVec �������н��д洢
        activeChannel->push_back(channel);
    }
}
