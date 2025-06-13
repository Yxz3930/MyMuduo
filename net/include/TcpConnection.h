#ifndef TCP_CONNECTION_H
#define TCP_CONNECTION_H

#include <memory>
#include <string>
#include <atomic>

#include "InetAddress.h"
#include "Callbacks.h"
#include "TimeStamp.h"
#include "Buffer.h"
#include "SmartPointer.h"


class Channel;
class EventLoop;
class Socket;

/// @brief TcpConnection 是对Tcp连接的封装 管理tcp连接的读写, 事件处理, tcp连接的状态变化等
///        一个连接对应着一个通信文件描述符cfd, 那么自然也就会有一个channel来管理这个通信文件描述符和对应的事件(fd和channel是配对出现的)
class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
private:
    enum ConnectionState
    {
        Disconnected = 0, // 断开连接状态
        Connecting,       // 正在连接
        Connected,        // 建立连接
        Disconnecting     // 正在断开连接
    };

public:
    /// @brief 构造函数
    /// @param loop EventLoop事件循环 是在主线程中通过轮询的方式获取到的在线程池当中的subloop
    /// @param connName 连接的名称 该名称也会用于记录TcpConnection对象
    /// @param cfd 通信文件描述符
    /// @param serverAddr 服务器地址
    /// @param clientAddr 客户端地址
    TcpConnection(EventLoop *loop, const std::string &connName, int cfd, const InetAddr &serverAddr, const InetAddr &clientAddr);

    ~TcpConnection();

    /// @brief 获取loop事件循环
    /// @return
    EventLoop *getLoop() { return this->m_loop; }

    const std::string &name() { return this->m_connName; }
    const InetAddr &getServerAddr() { return this->m_serverAddr; }
    const InetAddr &getClientAddr() { return this->m_clientAddr; }
    std::string getServerIpPort() { return this->m_serverAddr.getIpPort(); }
    std::string getClientIpPort() { return this->m_clientAddr.getIpPort(); }

    /// @brief 设置读时间、写事件、关闭回调
    void setCallbacks();

    /// @brief 判断是否已经建立了连接
    /// @return
    bool isConnected() { return this->m_state == ConnectionState::Connected; }

    /// @brief 发送数据
    /// @param data 数据
    void send(const std::string &data);

    void send(Buffer* buf);

    /// @brief 发送大型文件
    /// @param fileDescriptor 文件的描述符
    /// @param offset 偏移量 指定从文件的哪个位置开始读取
    /// @param count 要传输的最大字节数
    void sendFile(int fileDescriptor, off_t offset, size_t count);

    /// @brief 这个shutdown是用于如果检测到对端关闭了 那么这一方也需要进行关闭
    void shutdown();

    /// @brief 建立tcp连接 在mainloop中调用 channel在这里面注册了写事件触发
    void connectionEstablished();

    /// @brief 销毁tcp连接 在mainloop中调用 里面调用了mainloop中注册的关闭函数(进行连接的销毁)
    void connectionDestroyed();

    /// @brief 外部向TcpConnection注册消息回调
    /// @param cb
    void setConnectionCallback(ConnectionCallback cb)
    {
        this->m_connectionCallback = std::move(cb);
    }

    void setMessageCallback(MessageCallback cb)
    {
        this->m_messageCallback = std::move(cb);
    }

    void setWriteCompleteCallback(WriteCompleteCallback cb)
    {
        this->m_writeCompleteCallback = std::move(cb);
    }

    void setCloseCallback(CloseCallback cb)
    {
        this->m_closeCallback = std::move(cb);
    }

private:
    void setState(ConnectionState state) { this->m_state = state; }

    /// @brief 服务器端接收缓冲区收到数据(读事件触发) 进行消息回调处理
    /// @param recvTime
    void handleRead(TimeStamp recvTime);

    /// @brief 处理写事件
    void handleWrite();

    /// @brief 处理关闭连接事件
    void handleClose();

    /// @brief 处理错误事件
    void handleError();

    /// @brief 发送数据 通过write或者send发送数据
    /// @param data 发送的数据
    /// @param data_len 数据长度
    void sendInLoop(const void *data, size_t data_len);

    /// @brief 发送数据 通过sendfile发送数据
    /// @param fileDescriptor 文件的文件描述符
    /// @param offset 偏移量 指定从文件的哪个位置开始读取
    /// @param count 要传输的最大字节数
    void sendFileInLoop(int fileDescriptor, off_t &offset, size_t count);

    /// @brief 将cfd的写端关闭 本质上是tcp断开连接中四次挥手中的第一步 优雅地断开连接
    void shutdownInLoop();

    /// @brief 通过while+read的方式读取所有数据
    /// @param cfd 通信文件描述符
    /// @param message 传出参数 读取的数据
    /// @return 返回读取到的总共的字符长度
    ssize_t recvAllMessage(int cfd, std::string &message);

private:
    const std::string m_connName;
    const InetAddr m_serverAddr;
    const InetAddr m_clientAddr;

    EventLoop *m_loop;
    // std::unique_ptr<Socket> m_socketCfd;   // cfd对应的socket
    // std::unique_ptr<Channel> m_channelCfd; // 对cfd和对应事件的封装channel
    std::unique_ptr<Socket, PoolDeleter<Socket>> m_socketCfd;   // cfd对应的socket
    std::unique_ptr<Channel, PoolDeleter<Channel>> m_channelCfd; // 对cfd和对应事件的封装channel
    ConnectionState m_state;               // 连接的状态

    // 回调函数
    ConnectionCallback m_connectionCallback;       // 连接回调 新客户端建立连接/断开连接之后需要做什么
    MessageCallback m_messageCallback;             // 消息回调 接收到消息之后需要做什么
    WriteCompleteCallback m_writeCompleteCallback; // 写完成回调 服务器端将数据发送完成之后需要做什么
    CloseCallback m_closeCallback;                 // 关闭连接回调

    // 接收和发送缓冲区 使用string的话性能会非常慢 不推荐
    Buffer m_recvBuffer;
    Buffer m_sendBuffer;
    // std::string m_recvBuffer;
    // std::string m_sendBuffer;
    // size_t m_sendOffset = 0; // 用于记录m_sendBuffer继续发送的索引地址 表示的是下一步需要从offset索引位置继续发送
    //                          // 比如0时表示需要从索引0开始发送 10时表示需要从索引10开始发送
};

#endif // TCP_CONNECTION_H



