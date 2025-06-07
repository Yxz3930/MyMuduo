#ifndef SOCKET_H
#define SOCKET_H

#include <iostream>
#include <sys/socket.h>
#include <utility>
class InetAddr;

/**
 * Socket ��
 * ���԰��������ļ���������ͨ���ļ�������(accept��ȡ)
 * ��������socket, bind, listen, accept��
 */

/// @brief Socket�� ����socket fd������Ȩ, ����Ҫע��Socket�������Ҫ���ⲿ����һ���ļ������� Ȼ������ʱ���Զ��رն�Ӧ��fd
///        �����socket fd�Ĵ��������ϲ��Acceptr���߹������������ ���������� ������ֻ���ڹ��캯���д����ļ������������
///        ��Ȼ Ҳ�����ڹ��캯���д���fd ����Ҳû��ʲô����
class Socket
{
private:
    int m_fd{-1};

public:
    explicit Socket(int sockfd)
        : m_fd(sockfd) {}

    // Socket() { this->createNoBlocFd(); }

    ~Socket();

    int fd() {return this->m_fd;}

    // ͨ��socket��ȡ�����ļ�������
    int createSocketFd();

    /// @brief ����Acceptor�з��������ļ���������ȡ
    /// @return fd
    int createNoBlocFd();

    // bind
    int bind(InetAddr *addr);

    // listen
    int listen();

    /// @brief �������˽��յ��ͻ�������֮�� ����cfdͬʱ���ͻ��˵�ַд��clientAddr����
    /// @param clientAddr ����ͻ��˵ĵ�ַ
    /// @return 
    int accept(InetAddr* clientAddr);

    // accept
    InetAddr accept();

    // acceptͬʱ���صõ����ַ����Լ��ͻ��˵�ַ ����int������Ϊ������ Ĭ������Ϊ0
    std::pair<int, InetAddr> accept(int para = 0);

    /// @brief �ͻ����������ӷ���˵ķ���
    /// @param server_addr �������εĵ�ַ
    /// @return 
    int connect(InetAddr *server_addr);

    /// @brief socket�ļ��������رձ���д�������� ���˲��ܷ������� Զ�˻��յ�FIN���� �������Կ��Խ��н��ղ��� ���ڹر�tcp������4�λ����е�һ��
    void shutdownWrite();

    /// @brief �����Ƿ����Nagle �㷨 Nagle �㷨���ڼ��������ϴ����С���ݰ�����
    /// @param on 
    void setTcpNoDelay(bool on);

    /// @brief �����Ƿ�����ظ���ͬһ����ַ
    /// @param on 
    void setReuseAddr(bool on);

    /// @brief �����Ƿ�����ظ���ͬһ���˿�
    /// @param on 
    void setReusePort(bool on);

    /// @brief �������Ѿ����ӵ��׽����϶��ڴ�����Ϣ
    /// @param on 
    void setKeepAlive(bool on);
};

#endif // SOCKET_H
