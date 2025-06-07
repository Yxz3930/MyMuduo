#include "EpollPoller.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <cstring>
#include "myutils.h"
#include "Logger.h"

EpollPoller::EpollPoller(EventLoop *_loop)
    : Poller(_loop),                            // 父类构造函数
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

    // 使用vector容器替代原本的epoll_event[]数组 方法是对迭代器取*得到vector首元素,然后再取地址
    // 这里每次epoll_wait写入时都是从begin的首地址元素开始写入
    int eventsNums = ::epoll_wait(this->m_epfd, &*this->m_eventsVec.begin(), this->m_eventsVec.size(), -1);
    TimeStamp now; // 获取epoll解除阻塞的时间

    if (eventsNums > 0)
    {
        LOG_INFO(std::to_string(eventsNums) + " events happend");
        this->pushActiveChannelVector(eventsNums, activeChannel);
        if (eventsNums == this->m_eventsVec.size()) // 保存epoll_event的vevtor容器进行扩容
        {
            // 扩容不能放在pushActiveChannelVector之前 或者说不能在没有将m_eventsVec中的数据保存下来之前进行扩容 因为这样可能导致原本的数据出问题
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
        // 如果channel(fd)是新创建的或者是已经被暂时移除了的(注意是暂时被移除了的 其本身并没有从channelMap中被删除) 那么就需要重新添加到epoll上
        if (curState == EpollPoller::s_new)
        {
            int fd = channel->fd();
            this->m_channelMap[fd] = channel;
        }
        else
        {
            // 因为EpollPoller::s_delete状态时 channel本身仍然是存在的 仍然在channelMap中所以不需要再次添加
        }
        channel->setState(EpollPoller::s_add);
        this->control(EPOLL_CTL_ADD, channel);
    }
    else
    {
        // channel已经在epoll上注册过了 那么就需要进行修改
        int fd = channel->fd();
        if (channel->isNoneEvent())
        {
            // 如果channel本身没有事件 那么就需要将其从epoll上删除 并设置状态
            // 注意 这里只是将其从epoll上移除 但是并没有删除channel这个对象 这个对象以后仍会被注册使用
            this->control(EPOLL_CTL_DEL, channel);
            channel->setState(EpollPoller::s_delete);
        }
        else
            // 其他的情况只需要进行Mod修改即可
            this->control(EPOLL_CTL_MOD, channel);
    }
}

void EpollPoller::removeChannel(Channel *channel)
{
    // 这里的移除channel就是从channelMap中将channel删除掉了 但是仍然没有将channel对象析构
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
        // 将epoll_event数据中的ptr转为Channel*进行保存
        Channel *channel = static_cast<Channel *>(this->m_eventsVec[i].data.ptr);

        LOG_INFO("EpollPoller::pushActiveChannelVector channel ptr: " + ptrToString(channel));
        LOG_INFO("EpollPoller::pushActiveChannelVector channel fd: " + std::to_string(channel->fd()));

        // 将epoll实际检测到的事件类型写入到channel中
        channel->setRealEvents(this->m_eventsVec[i].events);
        // 将channel填入到m_activeChannelVec 容器当中进行存储
        activeChannel->push_back(channel);
    }
}
