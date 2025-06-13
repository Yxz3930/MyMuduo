#include "Socket.h"
#include "myutils.h"
#include "InetAddress.h"
#include <assert.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include "Logger.h"

Socket::~Socket()
{
    ::close(this->m_fd);
    this->m_fd = -1;
}

int Socket::createSocketFd()
{
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0)
        LOG_ERROR << "lfd < 0, get socket error";
    this->m_fd = fd;
    return 0;
}

int Socket::createNoBlocFd()
{
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    ErrorIf(fd < 0, "Socket::createNoBlocFd create socket fd error, fd < 0");
    this->m_fd = fd;
    return fd;
}

int Socket::bind(InetAddr *addr)
{
    sockaddr_in sock_addr = addr->toSocketAddr();
    int ret = ::bind(this->m_fd, (sockaddr*)&sock_addr, sizeof(sock_addr));
    if(ret < 0)
        LOG_ERROR << "ret < 0, bind error";
    return ret;
}

int Socket::listen()
{
    int ret = ::listen(this->m_fd, SOMAXCONN);
    if(ret < 0)
        LOG_ERROR << "ret < 0, listen error";
    return ret;
}

int Socket::accept(InetAddr *clientAddr)
{
    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int cfd = ::accept(this->m_fd, (sockaddr*)&client_addr, &client_addr_len);
    if(cfd < 0)
        LOG_ERROR << "cfd < 0, Accept error";
    else
        clientAddr->setSockAddr(client_addr);
    return cfd;
}

InetAddr Socket::accept()
{
    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int cfd = ::accept(this->m_fd, (sockaddr*)&client_addr, &client_addr_len);
    if(cfd < 0)
        LOG_ERROR << "cfd < 0, Accept error";
    this->m_fd = cfd;

    // 转换为可以直接获取地址和端口的形式
    InetAddr inetAddr_client(client_addr);
    return inetAddr_client;
}



std::pair<int, InetAddr> Socket::accept(int para)
{
    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int cfd = ::accept(this->m_fd, (sockaddr*)&client_addr, &client_addr_len);
    if(cfd < 0)
        LOG_ERROR << "cfd < 0, Accept error";

    // 转换为可以直接获取地址和端口的形式
    InetAddr inetAddr_client(client_addr);
    return std::pair<int, InetAddr>(cfd, inetAddr_client);
}



int Socket::connect(InetAddr *server_addr)
{
    sockaddr_in server_addr_socket = server_addr->toSocketAddr();
    int ret = ::connect(this->m_fd, (sockaddr*)&server_addr_socket, sizeof(server_addr_socket));
    ErrorIf(ret < 0 && (errno != EINPROGRESS), "TcpClient::connect error");

    return ret;
}

void Socket::shutdownWrite()
{
    int ret = ::shutdown(this->m_fd, SHUT_WR);
    if(ret < 0)
        LOG_ERROR << "ret < 0, ShutdownWrite error";
}


void Socket::setTcpNoDelay(bool on)
{
    // TCP_NODELAY 用于禁用 Nagle 算法。
    // Nagle 算法用于减少网络上传输的小数据包数量。
    // 将 TCP_NODELAY 设置为 1 可以禁用该算法，允许小数据包立即发送。
    int optval = on ? 1 : 0;
    ::setsockopt(this->m_fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
}

void Socket::setReuseAddr(bool on)
{
    // SO_REUSEADDR 允许一个套接字强制绑定到一个已被其他套接字使用的端口。
    // 这对于需要重启并绑定到相同端口的服务器应用程序非常有用。
    int optval = on ? 1 : 0;
    ::setsockopt(this->m_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

void Socket::setReusePort(bool on)
{
    // SO_REUSEPORT 允许同一主机上的多个套接字绑定到相同的端口号。
    // 这对于在多个线程或进程之间负载均衡传入连接非常有用。
    int optval = on ? 1 : 0;
    ::setsockopt(this->m_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
}

void Socket::setKeepAlive(bool on)
{
    // SO_KEEPALIVE 启用在已连接的套接字上定期传输消息。
    // 如果另一端没有响应，则认为连接已断开并关闭。
    // 这对于检测网络中失效的对等方非常有用。
    int optval = on ? 1 : 0;
    ::setsockopt(this->m_fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval));
}

