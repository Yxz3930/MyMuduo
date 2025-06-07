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
     * TcpClient�����m_clientSocket����TcpConnection��Ҫ�����fd��
     * ��TcpConnection����Ҳ�����ź����fd��Ӧ��Channel���������ﲻ��Ҫ����m_clientChannel�����Ա����
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
        // ���ӳɹ�
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

        // ����tcp���ӻص�
        this->m_connPtr->setConnectionCallback(this->m_connectionCallback);
        this->m_connPtr->setMessageCallback(this->m_messageCallback);
        this->m_connPtr->setWriteCompleteCallback(this->m_writeCompleteCallback);
        this->m_connPtr->setCloseCallback(
            std::bind(&TcpClient::handleClose, this));

        // ������ý������ӷ��� ��ô����ҪҪ�ڹرյ�ʱ����ö�Ӧ���ٵķ���
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
        // �������Ѿ��ر� ֱ��ǿ���˳�
        ::exit(0);
    }
}

int TcpClient::creadClientFd()
{
    /**
     * ��Ϊ����汾��TcpClientʹ�õ���loop�������������¼���������ô�ͻ��˵��ļ���������Ҳ��Ҫ����Ϊ��������
     * ���ļ�������Ϊ������ʱ��ֱ�ӽ������ӻ���֣�����ֵ-1&&errno == EINPROGRESS���������������
     * Ҳ���Ա�ʾ��������
     */
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    ErrorIf(fd < 0, "Socket::createNoBlocFd create socket fd error, fd < 0");
    return fd;
}

void TcpClient::handleClose()
{
    if (this->m_connPtr->isConnected())
    {
        // ʹ�� queueInLoop ��֤�̰߳�ȫ + ���� epoll_wait
        this->m_loop->queueInLoop([conn = this->m_connPtr]() {
            conn->connectionDestroyed();
        });
    }
}
