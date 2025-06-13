#include "TcpClient.h"
#include "Logger.h"
#include "InetAddress.h"
#include "TimeStamp.h"
#include "TcpConnection.h"
#include "EventLoop.h"
#include "AsyncLogging.h"
#include "Buffer.h"

#include <functional>
#include <string>
#include <thread>
#include <mutex>
#include <climits>

std::mutex coutMtx;

class EchoClinet
{
public:
    EchoClinet(EventLoop *loop, const InetAddr &addr, const std::string &name)
        : client_(loop, addr, name), loop_(loop)
    {
        // ע��ص�����
        client_.setConnectionCallback(
            std::bind(&EchoClinet::onConnection, this, std::placeholders::_1));

        client_.setMessageCallback(
            std::bind(&EchoClinet::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    }

    void connect()
    {
        client_.connect();
        std::cout << "[connect to server success]" << std::endl;
    }

    void send(const std::string &msg)
    {
        client_.send(msg);
    }

    void inputFromKeyboard()
    {
        std::cout << "client input (/quit exits): " << std::flush;
        while (true)
        {
            // std::lock_guard<std::mutex> lock(coutMtx);
            std::string line;

            if (!std::getline(std::cin, line))
            {
                break; // Ctrl+D or input error
            }
            if (line == "/quit")
            {
                this->client_.disconnect();
                break;
            }
            send(line + "\n"); // ��ӻ��б��ڷ�����ж���Ϣ����
        }
    }

private:
    // ���ӽ�����Ͽ��Ļص�����
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn->isConnected())
        {
            std::cout << "connection to server ip:port " << conn->getServerIpPort() << std::endl;
        }
        else
        {
            std::cout << "connection down server ip:port " << conn->getServerIpPort() << std::endl;
        }
    }

    // �ɶ�д�¼��ص�
    void onMessage(const TcpConnectionPtr &conn, Buffer* buf, TimeStamp time)
    {
        std::lock_guard<std::mutex> lock(coutMtx);
        std::cout << "[From Server] " << buf->retrieveAllAsString() << "\n";
        std::cout << "client input (/quit exits): " << std::flush;  // �ٴ���ʾ����
        // conn->shutdown();   // �ر�д�� �ײ���ӦEPOLLHUP => ִ��closeCallback_
    }

    TcpClient client_;
    EventLoop *loop_;
};

int main()
{
    // �첽��־����
    AsyncLogging async_log("../logs/client", INT_MAX, 3, 4096);
    Logger::SetWritFunc([&](const char *msg, int len)
                        { async_log.append(msg, len); });
    Logger::SetFlushFunc([&]()
                         { async_log.flush(); });
    LoggerControl::getInstance().setLoggerLevel(LogLevel::INFO);

    EventLoop loop;
    InetAddr serverAddr("127.0.0.1", 9999);
    EchoClinet echoClient(&loop, serverAddr, "EchoClinet");
    echoClient.connect();
    std::thread inputThread(&EchoClinet::inputFromKeyboard, &echoClient);

    loop.loop();

    // �ȴ������߳��˳�
    if (inputThread.joinable())
    {
        inputThread.join();
    }
    return 0;
}