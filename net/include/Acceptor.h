#ifndef ACCEPTOR_H
#define ACCEPTOR_H

#include <memory>
#include <functional>
#include <atomic>
#include <map>
#include <fcntl.h>
#include <utility>
#include "Socket.h"
#include "Channel.h"
#include "SmartPointer.h"

class EventLoop;
class InetAddr;

/// @brief Acceptor类 用来建立新的客户端连接 服务器端的mainloop用于监听
class Acceptor
{
public:
    // mainloop(服务器端)注册的处理新连接轮询分配的方法
    using NewConnectionCallback = std::function<void(int sockfd, InetAddr &)>;

    /// @brief 构造函数
    /// @param loop 传入mainloop
    /// @param listenAddr 服务器端的监听地址
    /// @param reusePort 默认重复使用端口
    Acceptor(EventLoop *loop, InetAddr &listenAddr, bool reusePort = true);

    ~Acceptor();

    /// @brief 监听事件 内部调用Socket类中的listen方法以及将Channel注册到epoll上
    void listen();

    /// @brief 判断Acceptor是否在监听
    /// @return 
    bool isListening() { return this->m_isListening; }
    

    void setNewConnectionCallback(NewConnectionCallback cb)
    {
        this->m_connectionFunc = std::move(cb);
    }

private:
    /// @brief 接收新连接的客户端fd 然后调用外部mainloop注册的新连接回调函数方法m_connectionFunc来进行分配
    ///        内部实际上是执行的NewConnectionCallback回调函数 这个回调函数是mainloop注册进来的 用于轮询分配
    void handleNewConnection();

    /// @brief 创建非阻塞的socket fd
    /// @return 
    int createNonBlockingFd();

    /// @brief 给得到的通信文件描述符设置非阻塞
    void setNonBlocking(int cfd);

    EventLoop *m_loop;                      // 服务器对应的mainloop
    // std::unique_ptr<Socket> m_acceptSocket; // 服务器的socket
    // std::unique_ptr<Channel> m_acceptChannel; // 对应于上面socket fd的channel
    std::unique_ptr<Socket, PoolDeleter<Socket>> m_acceptSocket; // 服务器的socket
    std::unique_ptr<Channel, PoolDeleter<Channel>> m_acceptChannel; // 对应于上面socket fd的channel
    NewConnectionCallback m_connectionFunc; // 这个回调函数是mainloop中用于轮询分配subloop的函数 注意不是服务器对象的连接回调
    std::atomic_bool m_isListening;         // 是否仍在监听
};

#endif // ACCEPTOR_H