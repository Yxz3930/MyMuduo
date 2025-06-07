#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

/**
 * TcpClient.cpp�Ƿ������汾�Ŀͻ��� ʵ��������Ϊ�鷳
 * TcpClient_Blocking.cpp �������汾�Ŀͻ��� ���������
 */

#include "InetAddress.h"
#include "EventLoop.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Socket.h"
#include <string>

class TcpClientNonLoop
{
public:
    TcpClientNonLoop(const InetAddr &serverAddr, const std::string &clientName);

    ~TcpClientNonLoop();

    /// @brief �ͻ��˿�ʼ���ӷ�����
    void connect();

    /// @brief �ͻ���������������Ͽ�����
    void disconnect();

    /// @brief �������������Ϣ ���� �ڲ�ʹ��write����
    /// @param msg 
    void send(const std::string& msg);

    /// @brief ���շ���������Ϣ ���� �ڲ�ʹ��read���� while+read
    /// @return 
    std::string receive();

private:
    /// @brief �����ͻ���socket
    /// @return �ͻ���fd
    int creadClientFd();

private:

    std::atomic_bool isConnected{false}; // �Ƿ����ӵı�־

    const InetAddr m_serverAddr;
    const std::string m_clientName;

    std::unique_ptr<Socket> m_clientSocket;
    TcpConnectionPtr m_connPtr;
};

#endif // TCP_CLIENT_H