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
            this->send(line + "\n"); // ��ӻ��б��ڷ�����ж���Ϣ����

            // ��Ҫ�ȴ�һ��ʱ�� �ȴ������������ݷ��ͻ��� Ҫ��Ȼ���ݲ��ἰʱ���ع���
            // ����֮��ֱ�ӵȴ�����
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


    // �ȴ������߳��˳�
    if (inputThread.joinable())
    {
        inputThread.join();
    }
    return 0;
}