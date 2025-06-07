#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <memory>
#include <atomic>
#include <map>
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>
#include <unistd.h>
#include "Socket.h"
#include "TimeQueue.h"
#include <mutex>
#include <atomic>
#include <thread>


class Socket;
class InetAddr;
class Epoll;
class Channel;
class Acceptor;
class EventLoop;
class Connection;
class ThreadPool;

#define BUFFER_SIZE 1024

class HttpServer
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
    std::vector<std::thread> subReactorThread;

    // std::map<int, std::string> clientSet_map; // 记录 <fd, user_name>
    // std::map<int, std::shared_ptr<Connection>> connection_map; // 记录 <cfd, Connection智能指针> 与 channel_map一致
    // std::map<int, int> fd_subReactorIdx_map; // 记录每个fd注册到哪个epoll上 <fd, subReactor_idx>
    // std::map<int, std::atomic<int>> epfd_fdCount_map; // 记录每个epoll文件描述符中的内核表中对应着多少个监听的文件描述符 <epfd, fdCount>

    std::unordered_map<int, std::string> clientSet_map;
    std::unordered_map<int, std::shared_ptr<Connection>> connection_map;
    std::unordered_map<int, int> fd_subReactorIdx_map;
    std::unordered_map<int, std::atomic<int>> epfd_fdCount_map;

    // 多线程map锁
    std::mutex map_mtx;
    std::mutex server_mtx;
    
    // subEpoll分发索引
    std::atomic<int> reactorIndex{0};

    // 业务处理回调函数
    std::function<void(Connection*)> m_CallBack_HttpServer_func;

    // 是否使用多Reactor模型
    bool isMutiReactor{false};


    // 定时器类
    std::shared_ptr<TimeQueue> server_timer;
    // 如果不使用主从Reactor模式 设定一个定时器线程
    std::thread m_timer_thread;

public:
    HttpServer(const std::string ip, int port, bool isMutiReactor);
    ~HttpServer();

    void Runing();

    // 将业务处理的逻辑注册到聊天室服务器中
    void SetHttpServerCallBack(std::function<void(Connection*)> _cb);

private:
    void HandleNewConnection(int connect_fd);

    void CloseClient(int fd);

    /// @brief 客户端连接超时之后服务器主动断开连接
    /// @param conn_ptr Connection裸指针
    void ServerCloseConn(Connection* conn_ptr);

};



#endif 
