#ifndef ACCEPTOR_H
#define ACCEPTOR_H

#include <memory>
#include <functional>
#include <atomic>
#include <map>
#include <fcntl.h>
#include <utility>
#include "Socket.h"
#include "Channel.h"
#include "SmartPointer.h"

class EventLoop;
class InetAddr;

/// @brief Acceptor�� ���������µĿͻ������� �������˵�mainloop���ڼ���
class Acceptor
{
public:
    // mainloop(��������)ע��Ĵ�����������ѯ����ķ���
    using NewConnectionCallback = std::function<void(int sockfd, InetAddr &)>;

    /// @brief ���캯��
    /// @param loop ����mainloop
    /// @param listenAddr �������˵ļ�����ַ
    /// @param reusePort Ĭ���ظ�ʹ�ö˿�
    Acceptor(EventLoop *loop, InetAddr &listenAddr, bool reusePort = true);

    ~Acceptor();

    /// @brief �����¼� �ڲ�����Socket���е�listen�����Լ���Channelע�ᵽepoll��
    void listen();

    /// @brief �ж�Acceptor�Ƿ��ڼ���
    /// @return 
    bool isListening() { return this->m_isListening; }
    

    void setNewConnectionCallback(NewConnectionCallback cb)
    {
        this->m_connectionFunc = std::move(cb);
    }

private:
    /// @brief ���������ӵĿͻ���fd Ȼ������ⲿmainloopע��������ӻص���������m_connectionFunc�����з���
    ///        �ڲ�ʵ������ִ�е�NewConnectionCallback�ص����� ����ص�������mainloopע������� ������ѯ����
    void handleNewConnection();

    /// @brief ������������socket fd
    /// @return 
    int createNonBlockingFd();

    /// @brief ���õ���ͨ���ļ����������÷�����
    void setNonBlocking(int cfd);

    EventLoop *m_loop;                      // ��������Ӧ��mainloop
    // std::unique_ptr<Socket> m_acceptSocket; // ��������socket
    // std::unique_ptr<Channel> m_acceptChannel; // ��Ӧ������socket fd��channel
    std::unique_ptr<Socket, PoolDeleter<Socket>> m_acceptSocket; // ��������socket
    std::unique_ptr<Channel, PoolDeleter<Channel>> m_acceptChannel; // ��Ӧ������socket fd��channel
    NewConnectionCallback m_connectionFunc; // ����ص�������mainloop��������ѯ����subloop�ĺ��� ע�ⲻ�Ƿ�������������ӻص�
    std::atomic_bool m_isListening;         // �Ƿ����ڼ���
};

#endif // ACCEPTOR_H