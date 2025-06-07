#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

/**
 * TcpClient.cpp是非阻塞版本的客户端 实现起来较为麻烦
 * TcpClient_Blocking.cpp 是阻塞版本的客户端 更容易理解
 */

#include "InetAddress.h"
#include "EventLoop.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Socket.h"
#include <string>

class TcpClientNonLoop
{
public:
    TcpClientNonLoop(const InetAddr &serverAddr, const std::string &clientName);

    ~TcpClientNonLoop();

    /// @brief 客户端开始连接服务器
    void connect();

    /// @brief 客户端主动与服务器断开连接
    void disconnect();

    /// @brief 向服务器发送消息 阻塞 内部使用write方法
    /// @param msg 
    void send(const std::string& msg);

    /// @brief 接收服务器的消息 阻塞 内部使用read方法 while+read
    /// @return 
    std::string receive();

private:
    /// @brief 创建客户端socket
    /// @return 客户端fd
    int creadClientFd();

private:

    std::atomic_bool isConnected{false}; // 是否连接的标志

    const InetAddr m_serverAddr;
    const std::string m_clientName;

    std::unique_ptr<Socket> m_clientSocket;
    TcpConnectionPtr m_connPtr;
};

#endif // TCP_CLIENT_H