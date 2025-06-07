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
        // ע��ص�����
        server_.setConnectionCallback(
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
        
        server_.setMessageCallback(
            std::bind(&EchoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        // ���ú��ʵ�subloop�߳�����
        server_.setThreadNums(3);
    }
    void start()
    {
        server_.start();
    }

private:
    // ���ӽ�����Ͽ��Ļص�����
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

    // �ɶ�д�¼��ص�
    void onMessage(const TcpConnectionPtr &conn, std::string buf, TimeStamp time)
    {
        std::cout << "recv from client: " << buf << std::endl;
        conn->send(buf);
        // conn->shutdown();   // �ر�д�� �ײ���ӦEPOLLHUP => ִ��closeCallback_
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