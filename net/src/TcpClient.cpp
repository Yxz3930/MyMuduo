#include "TcpClient.h"
#include "myutils.h"
#include <unistd.h>
#include "TcpConnection.h"
#include "Channel.h"
#include "Logger.h"

TcpClient::TcpClient(EventLoop *loop, const InetAddr &serverAddr, const std::string &clientName)
    : m_loop(loop), m_serverAddr(serverAddr), m_clientName(clientName),
      m_clientSocket(new Socket(creadClientFd()))
    //   m_clientChannel(new Channel(loop, m_clientSocket->fd()))
{
    /**
     * TcpClient里面的m_clientSocket就是TcpConnection需要传入的fd，
     * 在TcpConnection类中也存在着和这个fd对应的Channel，所以这里不需要设置m_clientChannel这个成员变量
     */
}

TcpClient::~TcpClient()
{
    if(this->m_connPtr->isConnected())
        this->m_loop->queueInLoop(
            std::bind(&TcpConnection::connectionDestroyed, this->m_connPtr.get()));
}

void TcpClient::connect()
{
    InetAddr *serverAddr = const_cast<InetAddr *>(&this->m_serverAddr);
    int ret = this->m_clientSocket->connect(serverAddr);
    ErrorIf(ret < 0 && (errno != EINPROGRESS), "TcpClient::connect error");

    if (ret == 0 || (ret < 0 && errno == EINPROGRESS))
    {
        // 连接成功
        InetAddr localAddr;
        sockaddr_in local;
        socklen_t addrlen = sizeof(local);
        if (::getsockname(this->m_clientSocket->fd(), (sockaddr *)&local, &addrlen) == 0)
        {
            localAddr.setSockAddr(local);
        }

        const std::string connName = "clientConnection_" + localAddr.getIpPort();
        this->m_connPtr = std::make_shared<TcpConnection>(this->m_loop, connName,
                                                          this->m_clientSocket->fd(), this->m_serverAddr, localAddr);

        // 设置tcp连接回调
        this->m_connPtr->setConnectionCallback(this->m_connectionCallback);
        this->m_connPtr->setMessageCallback(this->m_messageCallback);
        this->m_connPtr->setWriteCompleteCallback(this->m_writeCompleteCallback);
        this->m_connPtr->setCloseCallback(
            std::bind(&TcpClient::handleClose, this));

        // 这里调用建立连接方法 那么就需要要在关闭的时候调用对应销毁的方法
        this->m_connPtr->connectionEstablished();

        LOG_INFO("client m_connPtr: " + ptrToString(this->m_connPtr.get()));
    }
    LOG_INFO("TcpClient::connect complete");
}

void TcpClient::disconnect()
{
    if (this->m_connPtr->isConnected())
    {
        this->m_loop->queueInLoop([conn = this->m_connPtr, this]() {
            conn->connectionDestroyed();
            this->m_loop->quit();
        });
    }
}

void TcpClient::send(const std::string &msg)
{
    if (this->m_connPtr->isConnected())
    {
        this->m_connPtr->send(msg);
    }
    else
    {
        LOG_ERROR("the tcp connection is not connected");
        // 服务器已经关闭 直接强制退出
        ::exit(0);
    }
}

int TcpClient::creadClientFd()
{
    /**
     * 因为这个版本的TcpClient使用到了loop，本质上仍是事件驱动，那么客户端的文件描述符则也需要设置为非阻塞的
     * 当文件描述符为非阻塞时，直接进行连接会出现（返回值-1&&errno == EINPROGRESS）这是正常的情况
     * 也可以表示正常连接
     */
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    ErrorIf(fd < 0, "Socket::createNoBlocFd create socket fd error, fd < 0");
    return fd;
}

void TcpClient::handleClose()
{
    if (this->m_connPtr->isConnected())
    {
        // 使用 queueInLoop 保证线程安全 + 唤醒 epoll_wait
        this->m_loop->queueInLoop([conn = this->m_connPtr]() {
            conn->connectionDestroyed();
        });
    }
}
