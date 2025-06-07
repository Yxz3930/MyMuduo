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

    // �̳߳�
    std::shared_ptr<ThreadPool> server_thread_pool;

    // std::shared_ptr<Channel> server_channel; // ������װserver_socket��server_epoll
    std::shared_ptr<Acceptor> server_acceptor; // ���������µĿͻ�������
    std::shared_ptr<EventLoop> server_eventloop; // �����¼�ѭ�� ��reactor (Reactor == EventLoop)
    std::vector<std::shared_ptr<EventLoop>> sub_reactor; // ��Reactor��������
    std::vector<std::thread> subReactorThread;

    // std::map<int, std::string> clientSet_map; // ��¼ <fd, user_name>
    // std::map<int, std::shared_ptr<Connection>> connection_map; // ��¼ <cfd, Connection����ָ��> �� channel_mapһ��
    // std::map<int, int> fd_subReactorIdx_map; // ��¼ÿ��fdע�ᵽ�ĸ�epoll�� <fd, subReactor_idx>
    // std::map<int, std::atomic<int>> epfd_fdCount_map; // ��¼ÿ��epoll�ļ��������е��ں˱��ж�Ӧ�Ŷ��ٸ��������ļ������� <epfd, fdCount>

    std::unordered_map<int, std::string> clientSet_map;
    std::unordered_map<int, std::shared_ptr<Connection>> connection_map;
    std::unordered_map<int, int> fd_subReactorIdx_map;
    std::unordered_map<int, std::atomic<int>> epfd_fdCount_map;

    // ���߳�map��
    std::mutex map_mtx;
    std::mutex server_mtx;
    
    // subEpoll�ַ�����
    std::atomic<int> reactorIndex{0};

    // ҵ����ص�����
    std::function<void(Connection*)> m_CallBack_HttpServer_func;

    // �Ƿ�ʹ�ö�Reactorģ��
    bool isMutiReactor{false};


    // ��ʱ����
    std::shared_ptr<TimeQueue> server_timer;
    // �����ʹ������Reactorģʽ �趨һ����ʱ���߳�
    std::thread m_timer_thread;

public:
    HttpServer(const std::string ip, int port, bool isMutiReactor);
    ~HttpServer();

    void Runing();

    // ��ҵ������߼�ע�ᵽ�����ҷ�������
    void SetHttpServerCallBack(std::function<void(Connection*)> _cb);

private:
    void HandleNewConnection(int connect_fd);

    void CloseClient(int fd);

    /// @brief �ͻ������ӳ�ʱ֮������������Ͽ�����
    /// @param conn_ptr Connection��ָ��
    void ServerCloseConn(Connection* conn_ptr);

};



#endif 
