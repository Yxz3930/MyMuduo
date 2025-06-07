#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include "InetAddress.h"
#include "EventLoop.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Socket.h"
#include <string>

class TcpClient
{
public:
    TcpClient(EventLoop *loop, const InetAddr &serverAddr, const std::string &clientName);

    ~TcpClient();

    /// @brief �ͻ��˿�ʼ���ӷ�����
    void connect();

    /// @brief �ͻ���������������Ͽ�����
    void disconnect();

    /// @brief �������������Ϣ
    /// @param msg
    void send(const std::string &msg);

    void setConnectionCallback(ConnectionCallback cb)
    {
        this->m_connectionCallback = std::move(cb);
    }

    void setMessageCallback(MessageCallback cb)
    {
        this->m_messageCallback = std::move(cb);
    }

    void setWriteCompleteCallback(WriteCompleteCallback cb)
    {
        this->m_writeCompleteCallback = std::move(cb);
    }

private:
    /// @brief �����ͻ���socket
    /// @return �ͻ���fd
    int creadClientFd();

    /// @brief �ͻ������ӹرղ���
    void handleClose();

private:
    EventLoop *m_loop;
    const InetAddr m_serverAddr;
    const std::string m_clientName;

    std::unique_ptr<Socket> m_clientSocket;   // ���fd����ͨ���õ�fd �ڴ���TcpConnectionʱ��Ҫ�������fd
    // std::unique_ptr<Channel> m_clientChannel; // ����ͻ���fd��Ӧ��channel����Ҫ���� ��Ϊ�ͻ��˵�fd�����������ͨ�ŵ� ��TcpConnection���д�����ôһ��Channel
    TcpConnectionPtr m_connPtr;               // ����ͨ�� ��Ҫmake_shared���� ���������������ü���ʹ�䲻�Զ��ͷ�

    // �ص�����
    ConnectionCallback m_connectionCallback;
    MessageCallback m_messageCallback;
    WriteCompleteCallback m_writeCompleteCallback;
};

#endif // TCP_CLIENT_H