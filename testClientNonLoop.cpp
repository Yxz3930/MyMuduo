#include "TcpClientNonLoop.h"
#include "Logger.h"
#include "InetAddress.h"

#include <functional>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>

std::mutex coutMtx;

class EchoClinet
{
public:
    EchoClinet(const InetAddr &addr, const std::string &name)
        : client_(addr, name)
    {

    }

    void connect()
    {
        client_.connect();
    }

    void send(const std::string &msg)
    {
        client_.send(msg);
    }

    std::string recv()
    {
        return client_.receive();
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
            this->send(line + "\n"); // 添加换行便于服务端判断消息结束

            // 需要等待一段时间 等待服务器将数据发送回来 要不然数据不会及时返回过来
            // 发送之后直接等待接收
            std::string recvMsg = this->recv();
            std::cout << "[From Server] " << recvMsg <<std::endl << std::endl;
            std::cout << "client input (/quit exits): " << std::flush;

        }
    }

private:

    TcpClientNonLoop client_;
};

int main()
{
    LoggerControl::getInstance().setLoggerLevel(LogLevel::INFO);

    InetAddr serverAddr("127.0.0.1", 9999);
    EchoClinet echoClient(serverAddr, "EchoClinetBlocking");
    echoClient.connect();
    std::thread inputThread(&EchoClinet::inputFromKeyboard, &echoClient);


    // 等待输入线程退出
    if (inputThread.joinable())
    {
        inputThread.join();
    }
    return 0;
}