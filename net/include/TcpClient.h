#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include "InetAddress.h"
#include "EventLoop.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Socket.h"
#include <string>

class TcpClient
{
public:
    TcpClient(EventLoop *loop, const InetAddr &serverAddr, const std::string &clientName);

    ~TcpClient();

    /// @brief 客户端开始连接服务器
    void connect();

    /// @brief 客户端主动与服务器断开连接
    void disconnect();

    /// @brief 向服务器发送消息
    /// @param msg
    void send(const std::string &msg);

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

private:
    /// @brief 创建客户端socket
    /// @return 客户端fd
    int creadClientFd();

    /// @brief 客户端连接关闭操作
    void handleClose();

private:
    EventLoop *m_loop;
    const InetAddr m_serverAddr;
    const std::string m_clientName;

    std::unique_ptr<Socket> m_clientSocket;   // 这个fd就是通信用的fd 在创建TcpConnection时需要传入这个fd
    // std::unique_ptr<Channel> m_clientChannel; // 这个客户端fd对应的channel不需要设置 因为客户端的fd本身就是用于通信的 在TcpConnection当中存在这么一个Channel
    TcpConnectionPtr m_connPtr;               // 用于通信 需要make_shared创建 这里用于增加引用计数使其不自动释放

    // 回调函数
    ConnectionCallback m_connectionCallback;
    MessageCallback m_messageCallback;
    WriteCompleteCallback m_writeCompleteCallback;
};

#endif // TCP_CLIENT_H