#ifndef POLLER_H
#define POLLER_H

#include <vector>
#include <unordered_map>
#include <TimeStamp.h>

class Channel;
class EventLoop;

// muduo库中多路事件分发器的核心IO复用模块
// 可以认为这个类就是服务器中进行多个客户端信息保存的类
// 本身是抽象类
// 对epoll封装的类则需要继承自该类


/// @brief Poller类 是对epoll类或者其他用于IO多路复用的抽象父类
///        类中可以记录注册到epoll上的chanenl和对应fd 所有channel在epoll上的更改最终都会落在这个类和该类的派生类上
class Poller
{
public:
    // 存储Channel*
    using ChannelVector = std::vector<Channel*>;
    // 记录每个channel和对应的fd <socket_fd, channel*>
    using ChannelMap = std::unordered_map<int, Channel*>;


    /// @brief Poller构造函数
    /// @param loop EventLoop指针 所有权不属于该类
    Poller(EventLoop* loop)
        : m_loop(loop) { }


    /// @brief 析构函数 因为没有什么需要处理的指针 所以即使这里采用了多态的特性 也可以使用默认析构
    virtual ~Poller() = default;

    // 设置虚函数 

    /// @brief 将epoll_wait中触发的事件写入到activeChannel当中
    /// @param activeChannel 用于保存触发的Channel
    /// @return 
    virtual TimeStamp wait(ChannelVector* activeChannel) = 0;
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;
    virtual size_t getMapSize() = 0;


    /// @brief 判断传入的参数中是否存在这么一个通道
    /// @param channel 
    /// @return bool
    bool hasChannel(Channel* channel) const;

    /// @brief 静态方法 获取Poller实例(指针)
    /// @param loop 传入的EventLoop*指针
    /// @return Poller*
    static Poller* newPoller(EventLoop* loop);

protected:
    // 子类不可访问父类的私有成员变量 所以这里需要设置为 protected
    ChannelMap m_channelMap; // 用于记录每个客户端的fd和对应的channel*

private:
    EventLoop* m_loop;

};













#endif // POLLER_H