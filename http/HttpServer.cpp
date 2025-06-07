#include "HttpServer.h"
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
#include "TimeQueue.h"
#include "Logger.h"
#include <assert.h>
#include <climits>
#include <signal.h>

HttpServer::HttpServer(const std::string ip, int port, bool isMutiReactor)
    : isMutiReactor(isMutiReactor)
{
    signal(SIGPIPE, SIG_IGN); // 忽略 SIGPIPE

    // 创建线程池
    if (this->isMutiReactor)
    {
        this->server_thread_pool = std::make_shared<ThreadPool>(12, 20, 30);
        this->sub_reactor.reserve(16);
    }
    // 一个reactor本质上就是一个eventloop事件循环 同时也对应者一个单独的线程 主线程的loop在Runing函数中调用
    this->server_eventloop = std::make_shared<EventLoop>();
    LOG_DEBUG(std::string("server loop epoll epfd: ") + std::to_string(this->server_eventloop->GetEpfd()));

    // 建立acceptor类
    this->server_acceptor = std::make_shared<Acceptor>(this->server_eventloop, std::make_unique<InetAddr>(ip, port));
    std::function<void(int)> new_connection = std::bind(&HttpServer::HandleNewConnection, this, std::placeholders::_1);
    this->server_acceptor->SetHandleNewConnectionCallBack(new_connection);
    this->server_acceptor->Init();
    this->server_acceptor->EnableEpoll();
}

HttpServer::~HttpServer()
{
    // 针对多Reactor模式
    if (this->isMutiReactor)
    {
        // 对于智能指针不需要手动释放
        this->server_thread_pool->Stop();
        for (auto &event_loop : this->sub_reactor)
        {
            event_loop->Stop();
        }

        // 如果subEpoll上还有文件描述符未关闭 则移除并关闭该文件描述符
        for (auto &fd_name_pair : this->clientSet_map)
        {
            auto it = this->fd_subReactorIdx_map.find(fd_name_pair.first);
            if (it != this->fd_subReactorIdx_map.end())
            {
                this->sub_reactor[it->second]->GetEpoll()->Control(0, EPOLL_CTL_DEL, fd_name_pair.first);
                close(fd_name_pair.first);
                this->fd_subReactorIdx_map.erase(it);
            }
        }
        // 删除clientSet_map
        this->clientSet_map.clear();
    }
    // 针对单Reactor模式
    else
    {
        // 关闭sub_reactor中的循环
        for (auto &reactor : this->sub_reactor)
            reactor->Stop();

        for (auto &fd_name_pair : this->clientSet_map)
        {
            this->server_eventloop->GetEpoll()->Control(0, EPOLL_CTL_DEL, fd_name_pair.first);
            close(fd_name_pair.first);
        }
        this->clientSet_map.clear();

        // 等待定时器线程关闭
        if (this->m_timer_thread.joinable())
            this->m_timer_thread.join();
    }
}

void HttpServer::Runing()
{
    // 创建从Reactor 并将从Reactor的loop循环放到线程池当中 让某些线程单独运行从Reactor的事件循环
    int size = 0;
    if (this->isMutiReactor)
    {
        size = std::thread::hardware_concurrency() - 2;
        this->server_thread_pool->start();
    }
    std::cout << "Using " << size << " subReactor threads" << std::endl;
    LOG_DEBUG("Using " + std::to_string(size) + " subReactor threads");
    for (int i = 0; i < size; ++i)
    {
        // 创建EventLoop 也就是创建Reactor(这里是从Reactor) 并同步线程池
        std::shared_ptr<EventLoop> sub_loop = std::make_shared<EventLoop>();

        // 将从Reactor的事件循环任务放到线程池当中 让线程池中的线程单独执行
        std::function<void(void)> _loop_func = std::bind(&EventLoop::Loop, sub_loop);
        this->server_thread_pool->AddTask(_loop_func);

        // 记录epoll上每个fd的数量 初始化是0
        this->epfd_fdCount_map.emplace(sub_loop->GetEpfd(), 0);

        // 插入到sub_reactor的向量容器中
        this->sub_reactor.push_back(sub_loop);
    }

    // 创建定时器epoll 创建定时器队列 创建定时器loop 添加subloop
    std::shared_ptr<EventLoop> timer_loop = std::make_shared<EventLoop>(); // 这里面是定时器的epoll
    LOG_DEBUG(std::string("timer_loop epoll epfd: ") + std::to_string(timer_loop->GetEpfd()));
    this->server_timer = std::make_shared<TimeQueue>(timer_loop); // 这里面会将timer_fd注册到Epoll上
    if (this->isMutiReactor)
    {
        std::function<void(void)> timer_func = std::bind(&EventLoop::Loop, timer_loop);
        this->server_thread_pool->AddTask(timer_func);
        // this->m_timer_thread = std::move(std::thread(timer_func));
    }
    else
    {
        std::function<void(void)> timer_func = std::bind(&EventLoop::Loop, timer_loop);
        this->m_timer_thread = std::move(std::thread(timer_func));
    }
    this->sub_reactor.push_back(timer_loop);

    this->server_eventloop->Loop();
}

// 注册到Acceptor类中
void HttpServer::HandleNewConnection(int connect_fd)
{
    int subloop_num = this->sub_reactor.size() - 1; // 去掉最后的一个time_epoll定时器loop
    if (this->isMutiReactor)                        // 说明存在子Reactor 主Reactor往下面的子Reactor进行通信文件描述符的分配
    {
        // // mainReactor向subReactor进行随机分发
        this->reactorIndex = this->reactorIndex % subloop_num;
        assert(this->reactorIndex < this->sub_reactor.size());
        std::shared_ptr<EventLoop> subReactor = this->sub_reactor[this->reactorIndex];
        std::shared_ptr<Epoll> subEpoll = subReactor->GetEpoll();

        // 记录epoll上文件描述符的数量
        this->epfd_fdCount_map[subEpoll->GetEpfd()]++;

        // this->epfd_fdCount_map[subEpoll->GetEpfd()].fetch_add(1, std::memory_order_relaxed);
        LOG_DEBUG(std::string("reactor idx: " + std::to_string(this->reactorIndex)));
        // for (const auto &pair : this->epfd_fdCount_map)
        // {
        //  LOG_DEBUG(std::string("epoll fd: " + std::to_string(pair.first) + ", fd count: " + std::to_string(pair.second)));
        // }

        // 向SubEpoll上注册通信文件描述符
        std::shared_ptr<Connection> new_connection = std::make_shared<Connection>(connect_fd, subReactor);
        new_connection->SetHandleTaskCallBack(this->m_CallBack_HttpServer_func);

        // Connection注册CloseClient函数
        std::function<void(int)> closeFd_func = std::bind(&HttpServer::CloseClient, this, std::placeholders::_1);
        new_connection->SetCloseFdCallBack(closeFd_func);

        // Connection注册TimerTask函数
        std::function<void(void)> timer_task = std::bind(&HttpServer::ServerCloseConn, this, new_connection.get());
        new_connection->SetTimerTaskCallBack(timer_task);

        {
            std::lock_guard<std::mutex> lock(this->server_mtx);

            // 添加定时器 当连接超过指定时长服务器主动关闭连接
            this->server_timer->InsertTimer(new_connection->GetTimerPtr());

            // 添加<fd, subReactorIdx>元素
            this->fd_subReactorIdx_map.insert({connect_fd, this->reactorIndex});

            // 添加到connection_map中
            this->connection_map.insert({connect_fd, new_connection});

            // 创建用户信息 并添加到clientSet_map中
            std::string name = "User_" + std::to_string(connect_fd);
            this->clientSet_map.insert({connect_fd, name});
            LOG_DEBUG(name + std::string(" is created, cfd: " + std::to_string(connect_fd)));

            new_connection->InsertClient(connect_fd, name);
        }

        // 将所有的回调都注册完 再将channel对象和对应的工作函数注册到epoll上
        new_connection->EnableEpoll();
        this->reactorIndex++;
    }
    else // 不存在子Reactor 则直接在服务器epoll上进行注册
    {
        // 向服务器epoll上注册通信文件描述符
        // 创建Connection对象 注册http业务处理逻辑
        std::shared_ptr<Connection> new_connection = std::make_shared<Connection>(connect_fd, this->server_eventloop);
        new_connection->SetHandleTaskCallBack(this->m_CallBack_HttpServer_func);

        // Connection对象注册问价描述符关闭回调函数
        std::function<void(int)> closeFd_func = std::bind(&HttpServer::CloseClient, this, std::placeholders::_1);
        new_connection->SetCloseFdCallBack(closeFd_func);

        // Connection对象注册定时器回调函数
        std::function<void(void)> timer_task = std::bind(&HttpServer::ServerCloseConn, this, new_connection.get());
        new_connection->SetTimerTaskCallBack(timer_task);

        this->connection_map[connect_fd] = new_connection;

        {
            std::lock_guard<std::mutex> lock(this->server_mtx);
            // 向服务器中的定时器队列中添加定时器
            this->server_timer->InsertTimer(new_connection->GetTimerPtr());
            // Connection对象添加到connection_map中
            this->connection_map.insert({connect_fd, new_connection});
            // 创建用户信息 并添加到clientSet_map中
            std::string name = "User" + std::to_string(connect_fd);
            this->clientSet_map.insert({connect_fd, name});
            LOG_DEBUG(name + std::string(" is created, cfd: " + std::to_string(connect_fd)));

            new_connection->InsertClient(connect_fd, name);
        }

        // 等connection对象中的回调函数都注册完 再注册整个业务逻辑函数 因为里面的函数会直接注册到channel对象上
        new_connection->EnableEpoll();
    }
}

// 注册到Connection类中
void HttpServer::CloseClient(int fd)
{
    assert(fd > 0);
    std::lock_guard<std::mutex> lock(this->server_mtx);

    // 如果可以在clientSet_map中找不到 则直接返回
    if (this->clientSet_map.find(fd) == this->clientSet_map.end())
        return;

    if (this->connection_map.find(fd) != this->connection_map.end())
        this->connection_map.erase(fd); // 删除对应的connection之后,该connection智能指针的最后一个引用没了,执行析构,在自己的析构中从epoll上删除

    int still_num = this->clientSet_map.size();
    if (this->isMutiReactor)
    {
        // 对应epoll上的fd数量减一
        int subReactor_idx = this->fd_subReactorIdx_map.find(fd)->second;
        std::shared_ptr<Epoll> subEpoll = this->sub_reactor[subReactor_idx]->GetEpoll();
        int remain_nums = this->epfd_fdCount_map[subEpoll->GetEpfd()];
        if (remain_nums > 0)
        {
            this->epfd_fdCount_map[subEpoll->GetEpfd()].fetch_sub(1, std::memory_order_relaxed);
        }

        if (this->clientSet_map.find(fd) != this->clientSet_map.end())
            this->clientSet_map.erase(fd);
        if (this->fd_subReactorIdx_map.find(fd) != this->fd_subReactorIdx_map.end())
            this->fd_subReactorIdx_map.erase(fd);
        LOG_DEBUG(std::string("Client id " + std::to_string(fd) + " disconncectd; " +
                              "epfd is: " + std::to_string(subEpoll->GetEpfd()) + ", Client num is: " + std::to_string(still_num)));
    }
    else
    {
        LOG_DEBUG(std::string("Client id " + std::to_string(fd) + " disconncectd; " +
                              "epfd is: " + std::to_string(this->server_eventloop->GetEpfd()) + ", Client num is: " + std::to_string(still_num)));
    }

    // std::cout << "Client id " << fd << " disconncectd; " << "Client num is: " << this->clientSet_map.size() << std::endl;
}

void HttpServer::SetHttpServerCallBack(std::function<void(Connection *)> _cb)
{
    this->m_CallBack_HttpServer_func = std::move(_cb);
}

void HttpServer::ServerCloseConn(Connection *conn_ptr)
{
    std::shared_ptr<Timer> timer = conn_ptr->GetTimerPtr();
    if (timer->GetExpiration() < TimeStamp())
    {
        LOG_DEBUG(std::string("connection time more than " + std::to_string(CLIENT_TIME_OUT / 60) +
                              "mins" + ", server closes the connection, fd: " + std::to_string(conn_ptr->GetCfd())));

        std::cout << "HttpServer::ServerCloseConn connection time is too long, server closes the connection, fd: "
                  << conn_ptr->GetCfd() << std::endl;
        this->CloseClient(conn_ptr->GetCfd());
    }
    else
    {
        LOG_DEBUG("update timer set");
        this->server_timer->UpdateSet(timer);
    }
}
