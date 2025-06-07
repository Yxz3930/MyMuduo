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

/// @brief EpollPoller 继承自 Poller , 是对epoll的封装 包含Poller的特性
///        1. 对channel在epoll上的状态和事件进行修改 2. IO多路复用阻塞, 也就是epoll_wait
class EpollPoller : public Poller
{
    /**
     * EpollPoller本身掌管着服务器这一端的epfd的所有权
     * 所有需要在构造函数中调用epoll_create1 在析构函数中关闭epfd
     */
public:
    EpollPoller(EventLoop *_loop);

    ~EpollPoller() override;

    // 重写虚函数

    /// @brief 实际就是对epoll_wait的包装 里面完成epoll_wait事件类型接收以及进行记录
    /// @param activeChannel 用于保存触发的Channel
    /// @return 返回的是epoll_wait解除阻塞的时间点
    TimeStamp wait(ChannelVector* activeChannel) override;

    /// @brief 更新channel本身 实际上就是更新channel对应的在epoll上的状态（add和del）和事件（修改事件只需要mod）
    /// 更新实际上包含两个部分: 一个是更新channel本身在epoll上的状态(添加,删除,修改); 一个是更新channel中监听的事件
    /// @param channel
    void updateChannel(Channel *channel) override;

    /// @brief 将channel从epoll上移除 同时将channel从记录的容器中删除
    /// @param channel
    void removeChannel(Channel *channel) override;

private:
    /// @brief 封装底层的epoll_ctl
    /// @param operation
    /// @param channel
    void control(int operation, Channel *channel);

    /// @brief 将epoll_wait得到的m_eventsVec中的epoll_event结构体 转为Channel*之后存入到vector容器中
    /// @param eventNums epoll上触发的事件数量
    /// @param
    void pushActiveChannelVector(int eventNums, ChannelVector* activeChannel);

private:
    int m_epfd;                                // epoll对应的文件描述符epfd
    static constexpr int m_initEventSize = 16; // epoll_wait解除阻塞 初始时最大的事件数量 用来初始化vector容器的大小

    using EventVector = std::vector<epoll_event>;
    EventVector m_eventsVec; // 用于从epoll_wait上读取事件 这个变量需要作为epoll_wait的参数

    // channel(实际上是fd)在epoll上的状态 Channel最开始的状态是-1,也就是new的状态
    static constexpr int s_new = -1;   // 新的fd(新的channel)
    static constexpr int s_add = 1;    // fd或者说channel已经添加到epoll上了
    static constexpr int s_delete = 2; // fd或者说channel已经从epoll上移除了

    // // 记录每个channel和对应的fd <socket_fd, channel*>
    // // using ChannelMap = std::unordered_map<int, Channel*>;
    // // ChannelMap m_channelMap; 这个是父类的保护成员变量 用于记录每个channel和对应的fd
};

#endif // EPOLL_H