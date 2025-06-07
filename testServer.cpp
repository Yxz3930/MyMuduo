#include "TcpServer.h"
#include "Logger.h"
#include "InetAddress.h"
#include "TimeStamp.h"
#include "TcpConnection.h"
#include "EventLoop.h"
#include "Logger.h"

#include <functional>
#include <string>
#include <thread>


class EchoServer
{
public:
    EchoServer(EventLoop *loop, const InetAddr &addr, const std::string &name)
        : server_(loop, addr, true, 3)
        , loop_(loop)
    {
        // 注册回调函数
        server_.setConnectionCallback(
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
        
        server_.setMessageCallback(
            std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // 设置合适的subloop线程数量
        server_.setThreadNums(3);
    }
    void start()
    {
        server_.start();
    }

private:
    // 连接建立或断开的回调函数
    void onConnection(const TcpConnectionPtr &conn)   
    {
        if (conn->isConnected())
        {
            LOG_INFO("connection up clietn ip:port >> " + conn->getClientIpPort());
        }
        else
        {
            LOG_INFO("connection down client ip:port >> " + conn->getClientIpPort());
        }
    }

    // 可读写事件回调
    void onMessage(const TcpConnectionPtr &conn, std::string buf, TimeStamp time)
    {
        std::cout << "recv from client: " << buf << std::endl;
        conn->send(buf);
        // conn->shutdown();   // 关闭写端 底层响应EPOLLHUP => 执行closeCallback_
    }
    TcpServer server_;
    EventLoop *loop_;

};

int main() {

    Logger::GetInstance().setLogLevel(LogLevel::ERROR);

    LOG_INFO("mainloop");
    EventLoop loop;
    InetAddr serverAddr("127.0.0.1", 9999);
    EchoServer server(&loop, serverAddr, "EchoServer");
    server.start();
    loop.loop();
    return 0;
}