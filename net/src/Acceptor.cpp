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

    // 先设置回调 在下一步中再将fd注册到epoll上
    // 注意这里注册的是Accetor本类中的方法 而不是直接传入的回调函数 因为此时回调函数仍然为空 
    this->m_acceptChannel->setReadCallback(
        std::bind(&Acceptor::handleNewConnection, this));
    /**
     * 回调的注册顺序: 先设置回调，再注册到 epoll
     * 如果反过来就会出现问题, 比如 "注册后立即可能触发事件", "回调函数还没有设置完 但是已经触发了 导致之前出现的空回调问题"
     */
}

Acceptor::~Acceptor()
{
    /**
     * Socket可以将对应的监听文件描述符lfd直接关闭掉 不用管
     * Channel则需要从epoll上删除掉 此外还需要从对应EpollPoller中的ChannelMap中删除掉该channel
     * disableAll用于将channel从epoll上删除掉 remove用于将channel从Map中删除掉
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
    // 将设置回调的channel注册到epoll上
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
            // 将获取的通信文件描述符设置为非阻塞
            this->setNonBlocking(cfd);
            
            /**
             * mainloop也就是TcpServer中注册的 主loop向子loop分配的方法
             */
            this->m_connectionFunc(cfd, clientAddr);
        }
        else
        {
            // 外部没有注册mainloop轮询分配方法
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
     * 服务器监听文件描述符设置为非阻塞的
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
