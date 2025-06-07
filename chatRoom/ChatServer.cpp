#include "ChatServer.h"
#include "Socket.h"
#include "InetAddress.h"
#include "Epoll.h"
#include "myutils.h"
#include <strings.h>
#include <unistd.h>
#include "Channel.h"
#include "Acceptor.h"
#include "EventLoop.h"
#include "Connection.h"
#include "ThreadPool.h"
#include <fcntl.h>
#include <functional>


ChatServer::ChatServer(const std::string ip, int port)
{
    this->server_socket = std::make_shared<Socket>();
    this->server_addr = std::make_shared<InetAddr>(ip, port);
    this->server_epoll = std::make_shared<Epoll>();

    // 创建线程池
    this->server_thread_pool = std::make_shared<ThreadPool>(2, 20, 100);

    // 建立acceptor类
    this->server_acceptor = std::make_shared<Acceptor>(this->server_socket, this->server_epoll, this->server_addr);
    
    // 一个reactor本质上就是一个eventloop事件循环 同时也对应者一个线程池
    // 主Reactor 对应 server_eventloop 对应 server_thread_pool 主Reactor进行监听和连接
    this->server_eventloop = std::make_shared<EventLoop>(this->server_socket, this->server_epoll);

    // 创建从Reactor 并将从Reactor的loop循环放到线程池当中 让某些线程单独运行从Reactor的事件循环
    int size = 3;
    // int size = std::thread::hardware_concurrency();
    // std::cout << "thread hardware concurrency: " << size << std::endl;
    for(int i=0; i<size; ++i)
    {
        // 每个EventLoop都有自己的epoll文件描述符 所以需要创建Epoll
        std::shared_ptr<Epoll> sub_epoll = std::make_shared<Epoll>();
        // 创建EventLoop 也就是创建Reactor(这里是从Reactor) 并同步线程池
        std::shared_ptr<EventLoop> sub_loop = std::make_shared<EventLoop>(std::make_shared<Socket>(-1, -1), sub_epoll);
        
        // 将从Reactor的事件循环任务放到线程池当中 让线程池中的线程单独执行 
        std::function<void(void)> _loop_func = std::bind(&EventLoop::Loop, sub_loop);
        this->server_thread_pool->AddTask(_loop_func);

        // 记录epoll上每个fd的数量 初始化是0
        this->epfd_fdCount_map.insert({sub_epoll->GetEpfd(), 0});

        // 插入到sub_reactor的向量容器中
        this->sub_reactor.emplace_back(sub_loop);
    }

}

ChatServer::~ChatServer()
{
    // 对于智能指针不需要手动释放

    // 如果subEpoll上还有文件描述符未关闭 则移除并关闭该文件描述符
    for(auto& fd_name_pair : this->clientSet_map)
    {
        auto it = this->fd_subReactorIdx_map.find(fd_name_pair.first);
        if (it != this->fd_subReactorIdx_map.end())
        {
            this->sub_reactor[it->second]->GetEpoll()->Control(0, EPOLL_CTL_DEL, fd_name_pair.first);
            close(fd_name_pair.first);
            this->fd_subReactorIdx_map.erase(it);
        }
    }

    // 移除并关闭主Reactor上的服务器监听文件描述符
    this->server_epoll->Control(0, EPOLL_CTL_DEL, this->server_socket->GetLfd());
    close(this->server_socket->GetLfd());

    // 循环关闭从Reactor的epoll
    for(auto& event_loop : this->sub_reactor)
    {
        close(event_loop->GetEpoll()->GetEpfd());
    }
    // 关闭主Reactor的epoll
    close(this->server_socket->GetLfd());
}


void ChatServer::Init()
{
    std::function<void(const std::shared_ptr<Socket>&)> new_connection = std::bind(&ChatServer::HandleNewConnection, this, std::placeholders::_1);
    this->server_acceptor->SetHandleNewConnectionCallBack(new_connection);
    this->server_acceptor->Init();
}



void ChatServer::Runing()
{
    this->server_eventloop->Loop();
}



// 注册到Acceptor类中
void ChatServer::HandleNewConnection(const std::shared_ptr<Socket>& connect_socket_ptr)
{
    // mainReactor向subReactor进行随机分发
    this->reactorIndex = (this->reactorIndex + 1) % this->sub_reactor.size();
    std::cout << "reactor idx: " << this->reactorIndex << std::endl;
    std::shared_ptr<EventLoop> subReactor = this->sub_reactor[this->reactorIndex];
    std::shared_ptr<Epoll> subEpoll = subReactor->GetEpoll();
    this->fd_subReactorIdx_map.insert({connect_socket_ptr->GetCfd(), this->reactorIndex});

    // 记录epoll上文件描述符的数量
    int old_value = this->epfd_fdCount_map[subEpoll->GetEpfd()];
    this->epfd_fdCount_map[subEpoll->GetEpfd()] = old_value + 1;
    // 打印所有的epoll上的文件描述符数量
    for(const auto& pair : this->epfd_fdCount_map)
    {
        std::cout << "epoll fd: " << pair.first << ", fd count: " << pair.second << std::endl;
    }

    // 使用subReactor的epoll创建connection连接
    std::shared_ptr<Connection> new_connection = std::make_shared<Connection>(connect_socket_ptr, subEpoll);
    new_connection->SetHandleTaskCallBack(this->m_CallBack_ChatRoomLogic);

    // Connection注册CloseClient函数
    std::function<void(int)> closeFd_func = std::bind(&ChatServer::CloseClient, this, std::placeholders::_1);
    new_connection->SetCloseFdCallBack(closeFd_func);

    // Connection注册Broadcast函数
    std::function<void(std::string, Socket&)> broadcast_func = std::bind(&ChatServer::Broadcast, this, std::placeholders::_1, std::placeholders::_2);
    new_connection->SetBroadcastCallBack(broadcast_func);

    // 添加到connection_map中
    this->connection_map.insert({connect_socket_ptr->GetCfd(), new_connection});
    // 创建用户信息 并添加到clientSet_map中
    std::string name = "User" + std::to_string(this->clientSet_map.size()+1);
    this->clientSet_map.insert({connect_socket_ptr->GetCfd(), name});

    new_connection->InsertClient(connect_socket_ptr->GetCfd(), name); // 因为多人聊天室需要客户端信息
}



// 注册到Connection类中
void ChatServer::CloseClient(int fd)
{
    // 对应epoll上的fd数量减一
    int subReactor_idx = this->fd_subReactorIdx_map.find(fd)->second;
    std::shared_ptr<Epoll> subEpoll = this->sub_reactor[subReactor_idx]->GetEpoll();
    this->epfd_fdCount_map[subEpoll->GetEpfd()]--;

    // 清理其他map
    this->clientSet_map.erase(fd);
    this->connection_map.erase(fd); // 删除对应的connection之后,该connection智能指针的最后一个引用没了,执行析构,在自己的析构中从epoll上删除
    this->fd_subReactorIdx_map.erase(fd);

    close(fd); // connection中的文件描述符需要先在析构中删除 然后再关闭

    int still_num = this->clientSet_map.size();
    std::cout << "Client id " << fd << " disconncectd; " << "Client num is: " << still_num << std::endl;
}


// 注册到Connection类中
void ChatServer::Broadcast(const std::string& msg, Socket& event_socket)
{
    for(const auto& fd_name_pair : this->clientSet_map)
    {
        int cfd = fd_name_pair.first;
        std::string name = fd_name_pair.second;
        // 加入发送消息的客户端信息
        std::string client_name = clientSet_map.find(event_socket.GetCfd())->second; // 查找发送信息的客户端名称
        std::string new_msg = client_name + ": " + msg;
        
        if(cfd != event_socket.GetCfd())
        {
            ssize_t ret_len = write(cfd, new_msg.c_str(), new_msg.size());
            ErrorIf(ret_len == -1, "EventHandle::Broadcast Error");
        }
    }
}

void ChatServer::SetChatRoomCallBack(const std::function<void(Connection*)> &_cb)
{
    this->m_CallBack_ChatRoomLogic = _cb;
}
