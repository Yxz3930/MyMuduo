#ifndef CHAT_SERVER_H
#define CHAT_SERVER_H

#include <memory>
#include <atomic>
#include <map>
#include <string>
#include <vector>
#include <functional>
#include <unistd.h>
#include "Socket.h"


class Socket;
class InetAddr;
class Epoll;
class Channel;
class Acceptor;
class EventLoop;
class Connection;
class ThreadPool;

#define BUFFER_SIZE 1024

class ChatServer
{
private:
    std::shared_ptr<Socket> server_socket;
    std::shared_ptr<InetAddr> server_addr;
    std::shared_ptr<Epoll> server_epoll;

    // 线程池
    std::shared_ptr<ThreadPool> server_thread_pool;

    // std::shared_ptr<Channel> server_channel; // 用来包装server_socket和server_epoll
    std::shared_ptr<Acceptor> server_acceptor; // 用来建立新的客户端连接
    std::shared_ptr<EventLoop> server_eventloop; // 用来事件循环 主reactor (Reactor == EventLoop)
    std::vector<std::shared_ptr<EventLoop>> sub_reactor; // 从Reactor向量容器

    std::map<int, std::string> clientSet_map; // 记录 <fd, user_name>
    std::map<int, std::shared_ptr<Connection>> connection_map; // 记录 <cfd, Connection智能指针> 与 channel_map一致
    std::map<int, int> fd_subReactorIdx_map; // 记录每个fd注册到哪个epoll上 <fd, subReactor_idx>
    std::map<int, int> epfd_fdCount_map; // 记录每个epoll文件描述符中的内核表中对应着多少个监听的文件描述符 <epfd, fdCount>

    // 连接分发索引
    ssize_t reactorIndex = 0;

    // 业务处理回调函数
    std::function<void(Connection*)> m_CallBack_ChatRoomLogic;

public:
    ChatServer(const std::string ip, int port);
    ~ChatServer();

    void Init();

    void Runing();

    // 将业务处理的逻辑注册到聊天室服务器中
    void SetChatRoomCallBack(const std::function<void(Connection*)>& _cb);

private:
    void HandleNewConnection(const std::shared_ptr<Socket>& connect_socket_ptr);

    void CloseClient(int fd);

    void Broadcast(const std::string& msg, Socket& event_socket);
};



#endif