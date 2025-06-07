#include "ChatClient.h"

#include "Socket.h"
#include "InetAddress.h"
#include <thread>
#include <strings.h>
#include <unistd.h>
#include "myutils.h"


ChatClient::ChatClient(std::string ip, int port)
{
    this->client_socket = std::make_unique<Socket>();
    this->server_addr = std::make_unique<InetAddr>(ip, port);
}

ChatClient::~ChatClient()
{
    // 使用智能指针不需要自己释放内存
}

void ChatClient::Init(void)
{
    this->client_socket->Connect(this->server_addr.get());
    this->client_socket->SetNoBlocking(Socket::SET_LFD);
}

void ChatClient::Start(void)
{
    this->running = true;
    std::thread Broadcast_handler(&ChatClient::HandleBroadcastData, this);

    this->HandleInput();

    // 清理工作
    this->running = false;
    Broadcast_handler.join();    
}

void ChatClient::HandleInput()
{
    std::cout << ">" ;
    while(true)
    {
        std::string msg;
        getline(std::cin, msg);

        if(msg == "/exit")
        {
            this->running = false;
            break;
        }

        int ret_len = write(this->client_socket->GetLfd(), msg.c_str(), msg.size());
        ErrorIf(ret_len == -1, "ChatClient::HandleInput Error");
    }

}

void ChatClient::HandleBroadcastData()
{
    while(running)
    {
        char buffer[BUFFER_SIZE];
        bzero(buffer, sizeof(buffer));
        std::string msg;
        int ret_len;
        while((ret_len = read(this->client_socket->GetLfd(), buffer, sizeof(buffer)-1)) > 0)
        {
            msg.append(buffer);
            bzero(buffer, sizeof(buffer));
        }

        // 数据读取完成
        if(ret_len == -1 && ((errno != EAGAIN) || (errno != EWOULDBLOCK)))
        {
            std::lock_guard<std::mutex> lock(this->cout_mutex);
            std::cout << "read error" << std::endl;
            close(client_socket->GetLfd());
            break;
        }
        else if(ret_len == 0) // 连接断开
        {
            std::lock_guard<std::mutex> lock(this->cout_mutex);
            std::cout << "socket disconneted..." << std::endl;
            close(client_socket->GetLfd());
            return; 
        }

        if(!msg.empty()) {
            std::lock_guard<std::mutex> lock(this->cout_mutex); // 自动加锁和解锁 要在大括号内
            std::cout << "[Received]: " << msg << std::endl << std::endl;
            std::cout << "> " << std::flush;  // 重新显示输入提示符
        }

    }
}
