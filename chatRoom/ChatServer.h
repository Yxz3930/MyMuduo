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

    // �̳߳�
    std::shared_ptr<ThreadPool> server_thread_pool;

    // std::shared_ptr<Channel> server_channel; // ������װserver_socket��server_epoll
    std::shared_ptr<Acceptor> server_acceptor; // ���������µĿͻ�������
    std::shared_ptr<EventLoop> server_eventloop; // �����¼�ѭ�� ��reactor (Reactor == EventLoop)
    std::vector<std::shared_ptr<EventLoop>> sub_reactor; // ��Reactor��������

    std::map<int, std::string> clientSet_map; // ��¼ <fd, user_name>
    std::map<int, std::shared_ptr<Connection>> connection_map; // ��¼ <cfd, Connection����ָ��> �� channel_mapһ��
    std::map<int, int> fd_subReactorIdx_map; // ��¼ÿ��fdע�ᵽ�ĸ�epoll�� <fd, subReactor_idx>
    std::map<int, int> epfd_fdCount_map; // ��¼ÿ��epoll�ļ��������е��ں˱��ж�Ӧ�Ŷ��ٸ��������ļ������� <epfd, fdCount>

    // ���ӷַ�����
    ssize_t reactorIndex = 0;

    // ҵ����ص�����
    std::function<void(Connection*)> m_CallBack_ChatRoomLogic;

public:
    ChatServer(const std::string ip, int port);
    ~ChatServer();

    void Init();

    void Runing();

    // ��ҵ������߼�ע�ᵽ�����ҷ�������
    void SetChatRoomCallBack(const std::function<void(Connection*)>& _cb);

private:
    void HandleNewConnection(const std::shared_ptr<Socket>& connect_socket_ptr);

    void CloseClient(int fd);

    void Broadcast(const std::string& msg, Socket& event_socket);
};



#endif