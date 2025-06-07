#include "Acceptor.h"
#include "InetAddress.h"
#include "Logger.h"
#include "EventLoop.h"
#include "myutils.h"

#include <sys/epoll.h>
#include <unistd.h>
#include <thread>

Acceptor::Acceptor(EventLoop *loop, InetAddr &listenAddr, bool reusePort)
    : m_loop(loop),
      m_acceptSocket(new Socket(createNonBlockingFd())),        //  Socket(int sockfd) : m_fd(sockfd) {}
      m_acceptChannel(new Channel(loop, m_acceptSocket->fd())), // Channel(EventLoop*, int fd);
      m_isListening(false)
{
    this->m_acceptSocket->setReuseAddr(reusePort);
    this->m_acceptSocket->setReusePort(reusePort);
    this->m_acceptSocket->bind(&listenAddr);

    // �����ûص� ����һ�����ٽ�fdע�ᵽepoll��
    // ע������ע�����Accetor�����еķ��� ������ֱ�Ӵ���Ļص����� ��Ϊ��ʱ�ص�������ȻΪ�� 
    this->m_acceptChannel->setReadCallback(
        std::bind(&Acceptor::handleNewConnection, this));
    /**
     * �ص���ע��˳��: �����ûص�����ע�ᵽ epoll
     * ����������ͻ��������, ���� "ע����������ܴ����¼�", "�ص�������û�������� �����Ѿ������� ����֮ǰ���ֵĿջص�����"
     */
}

Acceptor::~Acceptor()
{
    /**
     * Socket���Խ���Ӧ�ļ����ļ�������lfdֱ�ӹرյ� ���ù�
     * Channel����Ҫ��epoll��ɾ���� ���⻹��Ҫ�Ӷ�ӦEpollPoller�е�ChannelMap��ɾ������channel
     * disableAll���ڽ�channel��epoll��ɾ���� remove���ڽ�channel��Map��ɾ����
     */
    this->m_acceptChannel->disableAll();
    this->m_acceptChannel->remove();
}

void Acceptor::listen()
{
    this->m_isListening.store(true);
    this->m_acceptSocket->listen();
    LOG_INFO("Acceptor listen fd register read event");
    LOG_INFO("Acceptor channel ptr: " + ptrToString(this->m_acceptChannel.get()));
    // �����ûص���channelע�ᵽepoll��
    this->m_acceptChannel->enableRead();
    LOG_INFO("Acceptor::listen complete");
}

void Acceptor::handleNewConnection()
{
    InetAddr clientAddr;
    int cfd = this->m_acceptSocket->accept(&clientAddr);
    if (cfd > 0)
    {
        if (this->m_connectionFunc)
        {
            // ����ȡ��ͨ���ļ�����������Ϊ������
            this->setNonBlocking(cfd);
            
            /**
             * mainloopҲ����TcpServer��ע��� ��loop����loop����ķ���
             */
            this->m_connectionFunc(cfd, clientAddr);
        }
        else
        {
            // �ⲿû��ע��mainloop��ѯ���䷽��
            LOG_ERROR("the handle new connection assgin function is empty");
            ::close(cfd);
        }
    }
    else
    {
        LOG_ERROR("cfd < 0, Acceptor::handleNewConnection error");
    }
}

int Acceptor::createNonBlockingFd()
{
    /**
     * �����������ļ�����������Ϊ��������
     */
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    ErrorIf(fd < 0, "Socket::createNoBlocFd create socket fd error, fd < 0");
    return fd;
}

void Acceptor::setNonBlocking(int cfd)
{
    int flags = ::fcntl(cfd, F_GETFL, 0);
    if (flags == -1) return;

    ::fcntl(cfd, F_SETFL, flags | O_NONBLOCK);
}
