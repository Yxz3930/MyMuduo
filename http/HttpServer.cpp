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
    signal(SIGPIPE, SIG_IGN); // ���� SIGPIPE

    // �����̳߳�
    if (this->isMutiReactor)
    {
        this->server_thread_pool = std::make_shared<ThreadPool>(12, 20, 30);
        this->sub_reactor.reserve(16);
    }
    // һ��reactor�����Ͼ���һ��eventloop�¼�ѭ�� ͬʱҲ��Ӧ��һ���������߳� ���̵߳�loop��Runing�����е���
    this->server_eventloop = std::make_shared<EventLoop>();
    LOG_DEBUG(std::string("server loop epoll epfd: ") + std::to_string(this->server_eventloop->GetEpfd()));

    // ����acceptor��
    this->server_acceptor = std::make_shared<Acceptor>(this->server_eventloop, std::make_unique<InetAddr>(ip, port));
    std::function<void(int)> new_connection = std::bind(&HttpServer::HandleNewConnection, this, std::placeholders::_1);
    this->server_acceptor->SetHandleNewConnectionCallBack(new_connection);
    this->server_acceptor->Init();
    this->server_acceptor->EnableEpoll();
}

HttpServer::~HttpServer()
{
    // ��Զ�Reactorģʽ
    if (this->isMutiReactor)
    {
        // ��������ָ�벻��Ҫ�ֶ��ͷ�
        this->server_thread_pool->Stop();
        for (auto &event_loop : this->sub_reactor)
        {
            event_loop->Stop();
        }

        // ���subEpoll�ϻ����ļ�������δ�ر� ���Ƴ����رո��ļ�������
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
        // ɾ��clientSet_map
        this->clientSet_map.clear();
    }
    // ��Ե�Reactorģʽ
    else
    {
        // �ر�sub_reactor�е�ѭ��
        for (auto &reactor : this->sub_reactor)
            reactor->Stop();

        for (auto &fd_name_pair : this->clientSet_map)
        {
            this->server_eventloop->GetEpoll()->Control(0, EPOLL_CTL_DEL, fd_name_pair.first);
            close(fd_name_pair.first);
        }
        this->clientSet_map.clear();

        // �ȴ���ʱ���̹߳ر�
        if (this->m_timer_thread.joinable())
            this->m_timer_thread.join();
    }
}

void HttpServer::Runing()
{
    // ������Reactor ������Reactor��loopѭ���ŵ��̳߳ص��� ��ĳЩ�̵߳������д�Reactor���¼�ѭ��
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
        // ����EventLoop Ҳ���Ǵ���Reactor(�����Ǵ�Reactor) ��ͬ���̳߳�
        std::shared_ptr<EventLoop> sub_loop = std::make_shared<EventLoop>();

        // ����Reactor���¼�ѭ������ŵ��̳߳ص��� ���̳߳��е��̵߳���ִ��
        std::function<void(void)> _loop_func = std::bind(&EventLoop::Loop, sub_loop);
        this->server_thread_pool->AddTask(_loop_func);

        // ��¼epoll��ÿ��fd������ ��ʼ����0
        this->epfd_fdCount_map.emplace(sub_loop->GetEpfd(), 0);

        // ���뵽sub_reactor������������
        this->sub_reactor.push_back(sub_loop);
    }

    // ������ʱ��epoll ������ʱ������ ������ʱ��loop ���subloop
    std::shared_ptr<EventLoop> timer_loop = std::make_shared<EventLoop>(); // �������Ƕ�ʱ����epoll
    LOG_DEBUG(std::string("timer_loop epoll epfd: ") + std::to_string(timer_loop->GetEpfd()));
    this->server_timer = std::make_shared<TimeQueue>(timer_loop); // ������Ὣtimer_fdע�ᵽEpoll��
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

// ע�ᵽAcceptor����
void HttpServer::HandleNewConnection(int connect_fd)
{
    int subloop_num = this->sub_reactor.size() - 1; // ȥ������һ��time_epoll��ʱ��loop
    if (this->isMutiReactor)                        // ˵��������Reactor ��Reactor���������Reactor����ͨ���ļ��������ķ���
    {
        // // mainReactor��subReactor��������ַ�
        this->reactorIndex = this->reactorIndex % subloop_num;
        assert(this->reactorIndex < this->sub_reactor.size());
        std::shared_ptr<EventLoop> subReactor = this->sub_reactor[this->reactorIndex];
        std::shared_ptr<Epoll> subEpoll = subReactor->GetEpoll();

        // ��¼epoll���ļ�������������
        this->epfd_fdCount_map[subEpoll->GetEpfd()]++;

        // this->epfd_fdCount_map[subEpoll->GetEpfd()].fetch_add(1, std::memory_order_relaxed);
        LOG_DEBUG(std::string("reactor idx: " + std::to_string(this->reactorIndex)));
        // for (const auto &pair : this->epfd_fdCount_map)
        // {
        //  LOG_DEBUG(std::string("epoll fd: " + std::to_string(pair.first) + ", fd count: " + std::to_string(pair.second)));
        // }

        // ��SubEpoll��ע��ͨ���ļ�������
        std::shared_ptr<Connection> new_connection = std::make_shared<Connection>(connect_fd, subReactor);
        new_connection->SetHandleTaskCallBack(this->m_CallBack_HttpServer_func);

        // Connectionע��CloseClient����
        std::function<void(int)> closeFd_func = std::bind(&HttpServer::CloseClient, this, std::placeholders::_1);
        new_connection->SetCloseFdCallBack(closeFd_func);

        // Connectionע��TimerTask����
        std::function<void(void)> timer_task = std::bind(&HttpServer::ServerCloseConn, this, new_connection.get());
        new_connection->SetTimerTaskCallBack(timer_task);

        {
            std::lock_guard<std::mutex> lock(this->server_mtx);

            // ��Ӷ�ʱ�� �����ӳ���ָ��ʱ�������������ر�����
            this->server_timer->InsertTimer(new_connection->GetTimerPtr());

            // ���<fd, subReactorIdx>Ԫ��
            this->fd_subReactorIdx_map.insert({connect_fd, this->reactorIndex});

            // ��ӵ�connection_map��
            this->connection_map.insert({connect_fd, new_connection});

            // �����û���Ϣ ����ӵ�clientSet_map��
            std::string name = "User_" + std::to_string(connect_fd);
            this->clientSet_map.insert({connect_fd, name});
            LOG_DEBUG(name + std::string(" is created, cfd: " + std::to_string(connect_fd)));

            new_connection->InsertClient(connect_fd, name);
        }

        // �����еĻص���ע���� �ٽ�channel����Ͷ�Ӧ�Ĺ�������ע�ᵽepoll��
        new_connection->EnableEpoll();
        this->reactorIndex++;
    }
    else // ��������Reactor ��ֱ���ڷ�����epoll�Ͻ���ע��
    {
        // �������epoll��ע��ͨ���ļ�������
        // ����Connection���� ע��httpҵ�����߼�
        std::shared_ptr<Connection> new_connection = std::make_shared<Connection>(connect_fd, this->server_eventloop);
        new_connection->SetHandleTaskCallBack(this->m_CallBack_HttpServer_func);

        // Connection����ע���ʼ��������رջص�����
        std::function<void(int)> closeFd_func = std::bind(&HttpServer::CloseClient, this, std::placeholders::_1);
        new_connection->SetCloseFdCallBack(closeFd_func);

        // Connection����ע�ᶨʱ���ص�����
        std::function<void(void)> timer_task = std::bind(&HttpServer::ServerCloseConn, this, new_connection.get());
        new_connection->SetTimerTaskCallBack(timer_task);

        this->connection_map[connect_fd] = new_connection;

        {
            std::lock_guard<std::mutex> lock(this->server_mtx);
            // ��������еĶ�ʱ����������Ӷ�ʱ��
            this->server_timer->InsertTimer(new_connection->GetTimerPtr());
            // Connection������ӵ�connection_map��
            this->connection_map.insert({connect_fd, new_connection});
            // �����û���Ϣ ����ӵ�clientSet_map��
            std::string name = "User" + std::to_string(connect_fd);
            this->clientSet_map.insert({connect_fd, name});
            LOG_DEBUG(name + std::string(" is created, cfd: " + std::to_string(connect_fd)));

            new_connection->InsertClient(connect_fd, name);
        }

        // ��connection�����еĻص�������ע���� ��ע������ҵ���߼����� ��Ϊ����ĺ�����ֱ��ע�ᵽchannel������
        new_connection->EnableEpoll();
    }
}

// ע�ᵽConnection����
void HttpServer::CloseClient(int fd)
{
    assert(fd > 0);
    std::lock_guard<std::mutex> lock(this->server_mtx);

    // ���������clientSet_map���Ҳ��� ��ֱ�ӷ���
    if (this->clientSet_map.find(fd) == this->clientSet_map.end())
        return;

    if (this->connection_map.find(fd) != this->connection_map.end())
        this->connection_map.erase(fd); // ɾ����Ӧ��connection֮��,��connection����ָ������һ������û��,ִ������,���Լ��������д�epoll��ɾ��

    int still_num = this->clientSet_map.size();
    if (this->isMutiReactor)
    {
        // ��Ӧepoll�ϵ�fd������һ
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
