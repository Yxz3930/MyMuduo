#include "Poller.h"
#include "Channel.h"
#include "EpollPoller.h"


bool Poller::hasChannel(Channel *channel) const
{
    auto it = this->m_channelMap.find(channel->fd());
    if(it != this->m_channelMap.end() && it->second == channel)
        return true;
    return false;
}

Poller *Poller::newPoller(EventLoop *loop)
{
    return new EpollPoller(loop);
}
