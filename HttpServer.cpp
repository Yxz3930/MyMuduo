#include "TcpServer.h"
#include "Logger.h"
#include "InetAddress.h"
#include "TimeStamp.h"
#include "TcpConnection.h"
#include "EventLoop.h"
#include "Logger.h"
#include "HttpContext.h"
#include "AsyncLogging.h"
#include "Buffer.h"
#include "MemoryPool.h"


#include <functional>
#include <string>
#include <thread>
#include <climits>

class EchoServer
{
public:
    EchoServer(EventLoop *loop, const InetAddr &addr, const std::string &name)
        : server_(loop, addr, true, 6), loop_(loop)
    {
        // 注册回调函数
        server_.setConnectionCallback(
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1));

        server_.setMessageCallback(
            std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

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
            TimeStamp now;
            // std::cout << "connection up clietn ip:port >> " << conn->getClientIpPort() << ", now: " << now.toFormatString() << std::endl;
            LOG_INFO << "connection up clietn ip:port >> " << conn->getClientAddr().getIpPort() << ", now: " << now.toFormatString();
        }
        else
        {
            TimeStamp now;
            // std::cout << "connection down client ip:port >> " << conn->getClientIpPort() << ", now: " << now.toFormatString() << std::endl;
            LOG_INFO << "connection down client ip:port >> " << conn->getClientAddr().getIpPort() << ", now: " << now.toFormatString();
        }
    }

    // 可读写事件回调
    void onMessage(const TcpConnectionPtr &conn, Buffer* buf, TimeStamp time)
    {
        HttpContext ctx;
        if (ctx.parseRequest(buf))
        {
            std::string response = ctx.makeResponse();
            conn->send(response);
            // conn->shutdown(); // HTTP 1.0/1.1 短连接方式
        }
        else
        {
            conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
            conn->shutdown();
        }
    }
    TcpServer server_;
    EventLoop *loop_;
};

int main()
{
    // 内存池预热
    memory_pool::memoryPoolWarmUp();
    std::cout << "memoryPool warm-up complete" << std::endl;

    // 异步日志设置
    AsyncLogging async_log("../logs/server", INT_MAX, 3, 2048);
    Logger::SetWritFunc([&](const char *msg, int len)
                        { async_log.append(msg, len); });
    Logger::SetFlushFunc([&]()
                         { async_log.flush(); });
    LoggerControl::getInstance().setLoggerLevel(LogLevel::WARNING);
    std::cout << "asynclogger config complete" << std::endl;


    LOG_INFO << "mainloop";
    EventLoop loop;
    // InetAddr serverAddr("127.0.0.1", 8888);
    InetAddr serverAddr("0.0.0.0", 8888);
    EchoServer server(&loop, serverAddr, "EchoServer");
    server.start();
    loop.loop();
    return 0;
}