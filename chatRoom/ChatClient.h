#ifndef CHAT_CLIENT_H
#define CHAT_CLIENT_H

#include <mutex>
#include <thread>
#include <atomic>   
#include <memory>
#include <string>

class Socket;
class InetAddr;

#define BUFFER_SIZE 5

class ChatClient
{
private:
    std::mutex cout_mutex; // 标准输出互斥锁
    std::atomic_bool running{false}; // 线程同步信号

    std::unique_ptr<Socket> client_socket;
    std::unique_ptr<InetAddr> server_addr;


public:
    ChatClient(std::string ip, int port);
    ~ChatClient();

    void Init(void);

    void Start(void);


private:

    void HandleInput();

    void HandleBroadcastData(); // 单独的线程处理其他客户端的广播数据



};



#endif // CHAT_CLIENT_H