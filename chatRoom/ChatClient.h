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
    std::mutex cout_mutex; // ��׼���������
    std::atomic_bool running{false}; // �߳�ͬ���ź�

    std::unique_ptr<Socket> client_socket;
    std::unique_ptr<InetAddr> server_addr;


public:
    ChatClient(std::string ip, int port);
    ~ChatClient();

    void Init(void);

    void Start(void);


private:

    void HandleInput();

    void HandleBroadcastData(); // �������̴߳��������ͻ��˵Ĺ㲥����



};



#endif // CHAT_CLIENT_H