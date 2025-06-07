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
 * @brief Channel 是对文件描述符（fd）以及其感兴趣事件的抽象
 *        也就是说Channel当中封装着Socket和对应的事件回调
 *        Channel.enble实际上做了两件事: 向epoll上进行注册对应事件; 向epoll的ChannelMap中插入<fd, Channel*> 无论如何enable之后数据就会记录在map中
 *        响应的，disenable实际上仅仅是从epoll上删除channel，但是并没有从Map中删除channel*，而remove则删除掉这个channel*
 *        也就说，enbalexx一次，相反的操作需要包含disenable + remove
 */
class Channel
{
public:
    using ReadEventCallback = std::function<void(TimeStamp)>;
    using EventCallback = std::function<void()>;

    Channel(EventLoop *_loop, int _fd);
    ~Channel();

    /// @brief 在loop当中处理事件
    /// @param receiveTime 接收的时间
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

    /// @brief 向epoll注册fd监听读事件
    void enableRead();

    /// @brief 向epoll删除fd监听读事件
    void disableRead();

    /// @brief 向epoll注册fd监听写事件
    void enableWrite();

    /// @brief 向epoll删除fd监听写事件
    void disableWrite();

    /// @brief 向epoll删除所有监听的事件类型
    void disableAll();

    /// @brief 判断当前channel是不是什么事件都没有注册
    /// @return
    bool isNoneEvent();

    /// @brief 判断当前channel是不是注册了读事件触发
    /// @return
    bool isReadEvent();

    /// @brief 判断当前channel是不是注册了写事件触发
    /// @return
    bool isWriteEvent();

    /// @brief 用于将channel绑定到tcpconnection上 因为channel当中的回调函数实际上都是tcpconnection当中的成员
    /// 那么就需要保证在channel执行过程中tcpconnection不能会销毁 否则就变成了野指针了
    /// @param ptr 智能指针包装的是一个void类型
    void tie(const std::shared_ptr<void> &ptr);

    /// @brief 获取该channel绑定的fd文件描述符
    /// @return
    int fd() const { return this->m_fd; }

    /// @brief 获取该channel对应的事件
    /// @return
    int events() const { return this->m_events; }

    /// @brief 根据从epoll_wait中获取到的事件类型 进行设置
    /// @param _revent epoll_wait检测到的事件类型
    /// @return
    void setRealEvents(uint32_t _revent) { this->m_realEvents = _revent; }

    /// @brief 获取该EventLoop*对应的指针
    /// @return
    EventLoop *loop() { return this->m_loop; }

    /// @brief 将本身的这个channel从epoll树上移除掉
    void remove();

    /// @brief 获取当前channel的状态 判断当前的channel是新创建的 还是已经在epoll上了的 还是已经从epoll上删除了的
    /// 对应的三种状态定义在EpollPoller.h文件中
    /// @return
    int state();

    /// @brief epoll_ctl修改之后 需要调用这个函数该更新状态
    void setState(int _state);

private:
    /// @brief 更新chanel的事件 在后续操作中中使用的是m_event 而不是m_realEvents
    /// m_realEvents成员变量只在将事件的类型和进行事件处理中的事件对比时
    void updateEvents();

    /// @brief 在handleEvent函数中调用
    /// @param receiveTime
    void handleEventWithGuard(TimeStamp receiveTime);

private:
    EventLoop *m_loop;
    const int m_fd;

    uint32_t m_events;     // 记录当前fd感兴趣（往epoll上注册的事件类型）
    uint32_t m_realEvents; // epoll上检测到触发的真实的事件类型
    int m_state{-1};       // 初始化为-1 m_state用于表示这个channel是新的 还是已经添加到epoll中的 还是已经从epoll上移除了的

    std::weak_ptr<void> m_tie;
    bool m_tied{false};

    ReadEventCallback m_readCallback;
    EventCallback m_writeCallback;
    EventCallback m_closeCallback;
    EventCallback m_errorCallback;

    // 对指定事件的重命名 对应着m_events
    static constexpr int m_NoneEvent = 0;         // 没有事件发生
    static constexpr int m_ReadEvent = EPOLLIN;   // 读事件
    static constexpr int m_WriteEvent = EPOLLOUT; // 写事件
    static constexpr int m_ErrorEvent = EPOLLERR; // 错误事件
    static constexpr int m_CloseEvent = EPOLLHUP; // 关闭事件
};

#endif // CHANNEL_H