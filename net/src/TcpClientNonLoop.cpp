#include "TcpClientNonLoop.h"
#include "myutils.h"
#include <unistd.h>
#include <cstring>

TcpClientNonLoop::TcpClientNonLoop(const InetAddr &serverAddr, const std::string &clientName)
    : m_serverAddr(serverAddr), m_clientName(clientName),
    //   m_clientSocket(new Socket(creadClientFd())),
      m_clientSocket(make_unique_from_pool<Socket>(creadClientFd()))
{
}

TcpClientNonLoop::~TcpClientNonLoop()
{
}

void TcpClientNonLoop::connect()
{
    InetAddr *serverAddr = const_cast<InetAddr *>(&this->m_serverAddr);
    int ret = this->m_clientSocket->connect(serverAddr);
    ErrorIf(ret < 0 && (errno != EINPROGRESS), "TcpClient::connect error");
    if (ret == 0 || (ret < 0 && (errno == EINPROGRESS)))
    {
        std::cout << "client connect to server, ip:port " << this->m_serverAddr.getIpPort() << std::endl;
        this->isConnected.store(true);
    }
}

void TcpClientNonLoop::disconnect()
{
    if (this->m_clientSocket && this->isConnected.load())
    {
        // 使用reset将m_clientSocket对应的地址销毁掉 使其调用析构函数关闭掉客户端的文件描述符
        this->m_clientSocket.reset();
        std::cout << "client disconnected from server." << std::endl;
    }
}

void TcpClientNonLoop::send(const std::string &msg)
{
    size_t totalBytes = msg.size();
    size_t offset = 0;

    while (offset < totalBytes)
    {
        ssize_t n = ::write(this->m_clientSocket->fd(), msg.data() + offset, totalBytes - offset);
        if (n > 0)
        {
            offset += n;
        }
        else if (n == -1 && errno == EINTR)
        {
            continue; // 被信号中断，继续发送
        }
        else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            // 如果是非阻塞fd，可以选择 epoll 写就绪后再写，这里我们是阻塞的，通常不会走到这
            continue;
        }
        else
        {
            perror("write");
            break; // 发生错误
        }
    }
}

std::string TcpClientNonLoop::receive()
{
    std::string recvBuffer;
    char buf[1024];

    while (this->isConnected)
    {
        ::memset(buf, 0, sizeof(buf));
        ssize_t n = ::read(this->m_clientSocket->fd(), buf, sizeof(buf));
        if (n > 0)
        {
            recvBuffer.append(buf, n);

            // 找到消息结束符\n，说明读完整了
            if (recvBuffer.find("\n") != std::string::npos)
                break;
        }
        else if (n == 0)
        {
            // 对端关闭
            this->disconnect();
            ::exit(0);
        }
        else if (n == -1)
        {
            if (errno == EINTR)
            {
                continue; // 被信号打断，继续读
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 非阻塞 fd 会出现这个情况，暂时没有数据 但是这里需要继续保持在while循环中
                // 直到服务器将消息发送过来 然后成功读取
                continue;
            }
            else
            {
                perror("read");
                break; // 其他错误
            }
        }
    }

    // 去除 \n 或 \r\n
    if (!recvBuffer.empty() && recvBuffer.back() == '\n') {
        recvBuffer.pop_back();
        if (!recvBuffer.empty() && recvBuffer.back() == '\r') {
            recvBuffer.pop_back();
        }
    }

    return recvBuffer;
}

int TcpClientNonLoop::creadClientFd()
{
    /**
     * 这个版本是阻塞版本的客户端 不需要loop
     * 无论哪个版本都需要将客户端的文件描述符设置为非阻塞的 要不然无法进行循环读取
     */
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    ErrorIf(fd < 0, "Socket::createNoBlocFd create socket fd error, fd < 0");
    return fd;
}
