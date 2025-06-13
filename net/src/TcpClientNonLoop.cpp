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
        // ʹ��reset��m_clientSocket��Ӧ�ĵ�ַ���ٵ� ʹ��������������رյ��ͻ��˵��ļ�������
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
            continue; // ���ź��жϣ���������
        }
        else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            // ����Ƿ�����fd������ѡ�� epoll д��������д�����������������ģ�ͨ�������ߵ���
            continue;
        }
        else
        {
            perror("write");
            break; // ��������
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

            // �ҵ���Ϣ������\n��˵����������
            if (recvBuffer.find("\n") != std::string::npos)
                break;
        }
        else if (n == 0)
        {
            // �Զ˹ر�
            this->disconnect();
            ::exit(0);
        }
        else if (n == -1)
        {
            if (errno == EINTR)
            {
                continue; // ���źŴ�ϣ�������
            }
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // ������ fd ���������������ʱû������ ����������Ҫ����������whileѭ����
                // ֱ������������Ϣ���͹��� Ȼ��ɹ���ȡ
                continue;
            }
            else
            {
                perror("read");
                break; // ��������
            }
        }
    }

    // ȥ�� \n �� \r\n
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
     * ����汾�������汾�Ŀͻ��� ����Ҫloop
     * �����ĸ��汾����Ҫ���ͻ��˵��ļ�����������Ϊ�������� Ҫ��Ȼ�޷�����ѭ����ȡ
     */
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    ErrorIf(fd < 0, "Socket::createNoBlocFd create socket fd error, fd < 0");
    return fd;
}
