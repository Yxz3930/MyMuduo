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

    // �����̳߳�
    this->server_thread_pool = std::make_shared<ThreadPool>(2, 20, 100);

    // ����acceptor��
    this->server_acceptor = std::make_shared<Acceptor>(this->server_socket, this->server_epoll, this->server_addr);
    
    // һ��reactor�����Ͼ���һ��eventloop�¼�ѭ�� ͬʱҲ��Ӧ��һ���̳߳�
    // ��Reactor ��Ӧ server_eventloop ��Ӧ server_thread_pool ��Reactor���м���������
    this->server_eventloop = std::make_shared<EventLoop>(this->server_socket, this->server_epoll);

    // ������Reactor ������Reactor��loopѭ���ŵ��̳߳ص��� ��ĳЩ�̵߳������д�Reactor���¼�ѭ��
    int size = 3;
    // int size = std::thread::hardware_concurrency();
    // std::cout << "thread hardware concurrency: " << size << std::endl;
    for(int i=0; i<size; ++i)
    {
        // ÿ��EventLoop�����Լ���epoll�ļ������� ������Ҫ����Epoll
        std::shared_ptr<Epoll> sub_epoll = std::make_shared<Epoll>();
        // ����EventLoop Ҳ���Ǵ���Reactor(�����Ǵ�Reactor) ��ͬ���̳߳�
        std::shared_ptr<EventLoop> sub_loop = std::make_shared<EventLoop>(std::make_shared<Socket>(-1, -1), sub_epoll);
        
        // ����Reactor���¼�ѭ������ŵ��̳߳ص��� ���̳߳��е��̵߳���ִ�� 
        std::function<void(void)> _loop_func = std::bind(&EventLoop::Loop, sub_loop);
        this->server_thread_pool->AddTask(_loop_func);

        // ��¼epoll��ÿ��fd������ ��ʼ����0
        this->epfd_fdCount_map.insert({sub_epoll->GetEpfd(), 0});

        // ���뵽sub_reactor������������
        this->sub_reactor.emplace_back(sub_loop);
    }

}

ChatServer::~ChatServer()
{
    // ��������ָ�벻��Ҫ�ֶ��ͷ�

    // ���subEpoll�ϻ����ļ�������δ�ر� ���Ƴ����رո��ļ�������
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

    // �Ƴ����ر���Reactor�ϵķ����������ļ�������
    this->server_epoll->Control(0, EPOLL_CTL_DEL, this->server_socket->GetLfd());
    close(this->server_socket->GetLfd());

    // ѭ���رմ�Reactor��epoll
    for(auto& event_loop : this->sub_reactor)
    {
        close(event_loop->GetEpoll()->GetEpfd());
    }
    // �ر���Reactor��epoll
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



// ע�ᵽAcceptor����
void ChatServer::HandleNewConnection(const std::shared_ptr<Socket>& connect_socket_ptr)
{
    // mainReactor��subReactor��������ַ�
    this->reactorIndex = (this->reactorIndex + 1) % this->sub_reactor.size();
    std::cout << "reactor idx: " << this->reactorIndex << std::endl;
    std::shared_ptr<EventLoop> subReactor = this->sub_reactor[this->reactorIndex];
    std::shared_ptr<Epoll> subEpoll = subReactor->GetEpoll();
    this->fd_subReactorIdx_map.insert({connect_socket_ptr->GetCfd(), this->reactorIndex});

    // ��¼epoll���ļ�������������
    int old_value = this->epfd_fdCount_map[subEpoll->GetEpfd()];
    this->epfd_fdCount_map[subEpoll->GetEpfd()] = old_value + 1;
    // ��ӡ���е�epoll�ϵ��ļ�����������
    for(const auto& pair : this->epfd_fdCount_map)
    {
        std::cout << "epoll fd: " << pair.first << ", fd count: " << pair.second << std::endl;
    }

    // ʹ��subReactor��epoll����connection����
    std::shared_ptr<Connection> new_connection = std::make_shared<Connection>(connect_socket_ptr, subEpoll);
    new_connection->SetHandleTaskCallBack(this->m_CallBack_ChatRoomLogic);

    // Connectionע��CloseClient����
    std::function<void(int)> closeFd_func = std::bind(&ChatServer::CloseClient, this, std::placeholders::_1);
    new_connection->SetCloseFdCallBack(closeFd_func);

    // Connectionע��Broadcast����
    std::function<void(std::string, Socket&)> broadcast_func = std::bind(&ChatServer::Broadcast, this, std::placeholders::_1, std::placeholders::_2);
    new_connection->SetBroadcastCallBack(broadcast_func);

    // ��ӵ�connection_map��
    this->connection_map.insert({connect_socket_ptr->GetCfd(), new_connection});
    // �����û���Ϣ ����ӵ�clientSet_map��
    std::string name = "User" + std::to_string(this->clientSet_map.size()+1);
    this->clientSet_map.insert({connect_socket_ptr->GetCfd(), name});

    new_connection->InsertClient(connect_socket_ptr->GetCfd(), name); // ��Ϊ������������Ҫ�ͻ�����Ϣ
}



// ע�ᵽConnection����
void ChatServer::CloseClient(int fd)
{
    // ��Ӧepoll�ϵ�fd������һ
    int subReactor_idx = this->fd_subReactorIdx_map.find(fd)->second;
    std::shared_ptr<Epoll> subEpoll = this->sub_reactor[subReactor_idx]->GetEpoll();
    this->epfd_fdCount_map[subEpoll->GetEpfd()]--;

    // ��������map
    this->clientSet_map.erase(fd);
    this->connection_map.erase(fd); // ɾ����Ӧ��connection֮��,��connection����ָ������һ������û��,ִ������,���Լ��������д�epoll��ɾ��
    this->fd_subReactorIdx_map.erase(fd);

    close(fd); // connection�е��ļ���������Ҫ����������ɾ�� Ȼ���ٹر�

    int still_num = this->clientSet_map.size();
    std::cout << "Client id " << fd << " disconncectd; " << "Client num is: " << still_num << std::endl;
}


// ע�ᵽConnection����
void ChatServer::Broadcast(const std::string& msg, Socket& event_socket)
{
    for(const auto& fd_name_pair : this->clientSet_map)
    {
        int cfd = fd_name_pair.first;
        std::string name = fd_name_pair.second;
        // ���뷢����Ϣ�Ŀͻ�����Ϣ
        std::string client_name = clientSet_map.find(event_socket.GetCfd())->second; // ���ҷ�����Ϣ�Ŀͻ�������
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
