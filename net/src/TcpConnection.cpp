#include "TcpConnection.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"

#include <cstring>
#include <unistd.h>
#include <sys/sendfile.h>

TcpConnection::TcpConnection(EventLoop *loop, const std::string &connName, int cfd, const InetAddr &serverAddr, const InetAddr &clientAddr)
    : m_loop(loop), m_connName(connName), m_state(TcpConnection::Connecting),
    //   m_socketCfd(new Socket(cfd)), 
      m_socketCfd(make_unique_from_pool<Socket>(cfd)), 
    //   m_channelCfd(new Channel(m_loop, cfd)), // 这俩智能指针不需要管理生命周期
      m_channelCfd(make_unique_from_pool<Channel>(m_loop, cfd)), // 这俩智能指针不需要管理生命周期
      m_serverAddr(serverAddr), m_clientAddr(clientAddr)
{
    /**
     * 在TcpConnection的构造函数中 直接给管理通信文件描述符和对应事件的Channel注册TcpConnection当中的所有回调事件
     * 因为TcpConnection本身承载着这些事件 但是事件的最终处理是在loop当中的Channel对象上
     */

    // 将连接对应的所有事件都注册到cfd对应的Channel上
    this->m_channelCfd->setReadCallback(
        std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    this->m_channelCfd->setWriteCallback(
        std::bind(&TcpConnection::handleWrite, this));
    this->m_channelCfd->setCloseCallback(
        std::bind(&TcpConnection::handleClose, this));
    this->m_channelCfd->setErrorCallback(
        std::bind(&TcpConnection::handleError, this));

    this->m_socketCfd->setKeepAlive(true);

    LOG_INFO << "TcpConnection start connecting, fd " << this->m_socketCfd->fd();
}

TcpConnection::~TcpConnection()
{
    /**
     * 没有需要关闭的文件描述符 智能指针也可以自己管理生命周期 所以析构函数中不需要进行什么操作
     * channel相关的操作会在对应位置进行调用 析构中没有进行remove
     */
    LOG_INFO << "TcpConnection disconnecting, fd " << this->m_socketCfd->fd();
    // 正常退出时，状态应该已经是 Disconnected
    assert(this->m_state == TcpConnection::Disconnected);
}

void TcpConnection::setCallbacks()
{
    auto self = shared_from_this();
    this->m_channelCfd->setReadCallback(
        std::bind(&TcpConnection::handleRead, self, std::placeholders::_1));
    this->m_channelCfd->setWriteCallback(
        std::bind(&TcpConnection::handleWrite, self));
    this->m_channelCfd->setCloseCallback(
        std::bind(&TcpConnection::handleClose, self));
    this->m_channelCfd->setErrorCallback(
        std::bind(&TcpConnection::handleError, self));
}

void TcpConnection::send(const std::string &data)
{
    if (this->isConnected())
    {
        // 如果send是在自己loop的线程中跑的 那么可以直接进行发送了
        // 实际分析一下, 可以发现, else语句当中的runInLoop方法里面还进行了是否在自己线程的判断 所以这里可以不加if的语句的
        if (this->m_loop->isThisThread())
        {
            this->sendInLoop(data.c_str(), data.size());
        }
        else
            this->m_loop->runInLoop(
                std::bind(&TcpConnection::sendInLoop, shared_from_this(), data.c_str(), data.size()));
    }
}

void TcpConnection::send(Buffer *buf)
{
    if (this->isConnected())
    {
        if (this->m_loop->isThisThread())
        {
            this->sendInLoop(buf->peek(), buf->readableBytes());
        }
        else
        {
            // 注意：跨线程传递Buffer指针是不安全的。正确的做法是传递string。
            // 这里我们假设上层业务保证了生命周期，或转换为string传递。
            // 为安全起见，这里也转换为string
            std::string msg = buf->retrieveAllAsString();
            this->m_loop->runInLoop(
                std::bind(&TcpConnection::sendInLoop, shared_from_this(), msg.data(), msg.size()));
        }
    }
}

void TcpConnection::sendFile(int fileDescriptor, off_t offset, size_t count)
{
    // 和上面的send思路一样

    if (this->isConnected())
    {
        if (this->m_loop->isThisThread())
        {
            sendFileInLoop(fileDescriptor, offset, count);
        }
        else
            this->m_loop->runInLoop(
                std::bind(&TcpConnection::sendFileInLoop, shared_from_this(), fileDescriptor, offset, count));
    }
}

void TcpConnection::shutdown()
{
    if (this->m_state == TcpConnection::Connected)
    {
        // 设置状态
        this->setState(TcpConnection::Disconnecting);

        // 直接执行关闭函数或者插入到函数队列中等待执行
        this->m_loop->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop, shared_from_this()));
    }
}

void TcpConnection::connectionEstablished()
{
    /**
     * 创建出来TcpConnection对象之后 需要这个实例化对象手动调用connectionEstablished方法
     */
    // 连接设置状态
    this->setState(TcpConnection::Connected);
    // cfd对应的channel绑定连接 以及向epoll注册读事件触发
    this->m_channelCfd->tie(shared_from_this());
    this->m_channelCfd->enableRead();
    // 执行连接回调
    this->m_connectionCallback(shared_from_this());
}

void TcpConnection::connectionDestroyed()
{
    if (this->m_state == TcpConnection::Connected || this->m_state == TcpConnection::Disconnecting)
    {
        // 设置连接状态
        this->setState(TcpConnection::Disconnected);
        // cfd对应的channel从epoll上删除掉
        this->m_channelCfd->disableAll();
        // 连接回调
        this->m_connectionCallback(shared_from_this());
    }
    // 从连接对应的epoll中ChannelMap删除该channel
    this->m_channelCfd->remove();
}

void TcpConnection::handleRead(TimeStamp recvTime)
{
    // 读取到类内的都缓冲区当中m_recvBuffer
    int saveErrno = 0;
    this->m_recvBuffer.readFd(this->m_socketCfd->fd(), &saveErrno);

    if (this->m_recvBuffer.readableBytes() > 0)
    {
        if (this->m_messageCallback)
            this->m_messageCallback(shared_from_this(), &this->m_recvBuffer, recvTime);
    }
    else if (this->m_recvBuffer.readableBytes() == 0)
    {
        this->handleClose();
    }
    else
    {
        // 出错了
        errno = saveErrno;
        LOG_ERROR << "TcpConnection::handleRead readBytes < 0, error";
        // 调用处理错误的函数
        this->handleError();
    }
}

void TcpConnection::handleWrite()
{
    /**
     * handleWrite处理大的数据都已经缓存到类内的m_sendBuffer上了
     * 在构造函数中 handleWrite 被注册到 Channel 上
     * 这个函数也就是Channel在可写时（写事件触发）所需要做的东西
     * 通过while+write的方式将数据发送给对端
     * 暂时先不处理
     */

    if (this->m_channelCfd->isWriteEvent())
    {
        size_t writeLen = ::write(this->m_socketCfd->fd(), this->m_sendBuffer.peek(), this->m_sendBuffer.readableBytes());
        if (writeLen > 0)
        {
            this->m_sendBuffer.retrieve(writeLen);
            if (this->m_sendBuffer.readableBytes() == 0)
            {
                this->m_channelCfd->disableWrite();
                if (this->m_writeCompleteCallback)
                    this->m_writeCompleteCallback(shared_from_this());
            }

            if (this->m_state == TcpConnection::Disconnected || this->m_state == TcpConnection::Disconnecting)
            {
                this->shutdown();
            }
        }
        else
        {
            LOG_ERROR << "::write error";
        }
    }
    else
    {
        LOG_ERROR << "Connection fd = " << m_socketCfd->fd()
                  << " is down, no more writing";
    }
}

void TcpConnection::handleClose()
{
    if (this->m_state == TcpConnection::Connected || this->m_state == TcpConnection::Disconnecting)
    {
        // 设置连接状态 删除所有触发事件
        this->setState(TcpConnection::Disconnected);

        /**
         * 这里需要先进行对channel disableAll，即使下面的m_closeCallback回调函数中再一次进行了disableAll
         * 但是一旦检测出来一方断开连接 那么就需要将该连接对应的Channel的所有触发事件都从epoll上删除掉 否则可能会重复触发已经无效的Channel
         */
        this->m_channelCfd->disableAll(); // 因为后面的关闭回调会将这个channel从epoll上删除掉

        // 创建一个TcpConnection对象 确保连接不会被销毁
        TcpConnectionPtr guardObj = shared_from_this();
        // 执行连接回调 因为连接回调本身也会在关闭的时候起作用
        if (this->m_connectionCallback)
            this->m_connectionCallback(guardObj);

        // 执行关闭回调 在mainloop中注册的关闭函数 将该连接从
        if (this->m_closeCallback)
            this->m_closeCallback(guardObj); // 将连接从mainloop中删除 将channel从对应的epoll上的map中删除
    }
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(this->m_socketCfd->fd(), SOL_SOCKET, SO_ERROR,
                     &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR << "TcpConnection::handleError name:" << this->m_connName << " - SO_ERROR: " << err;
}

void TcpConnection::sendInLoop(const void *data, size_t data_len)
{
    /**
     * 将数据发送出去，如果数据太大 那么就通过多次设置注册写事件 使得可以多次进行发送 直到将数据发送完成
     */
    // 第一次发送的时候应该是没有注册写事件的
    // 或者前一次已经发送完成 那么会调用disenbleWrite将写事件触发删除掉
    // 类内的m_sendBuffer只是用于存储没有发送完成的数据 并不是一开始数据就在这里了

    if (m_state == Disconnected)
    {
        LOG_WARNING << "disconnected, give up writing";
        return;
    }

    bool faultError = 0;
    ssize_t writeLen = 0;
    size_t remainLen = data_len;

    if (!this->m_channelCfd->isWriteEvent() && this->m_sendBuffer.readableBytes() == 0)
    {
        writeLen = ::write(this->m_socketCfd->fd(), data, data_len);
        if(writeLen > 0)
        {
            remainLen -= writeLen;
            if(remainLen == 0 && this->m_writeCompleteCallback)
                this->m_loop->queueInLoop(std::bind(this->m_writeCompleteCallback, shared_from_this()));
        }
        else
        {
            if (errno != EWOULDBLOCK) {
                LOG_ERROR << "TcpConnection::sendInLoop";
                if (errno == EPIPE || errno == ECONNRESET) {
                    faultError = true;
                }
            }
        }
    }
    if(! faultError && remainLen > 0)
    {
        this->m_sendBuffer.append(static_cast<const char*>(data) + writeLen, remainLen);
        this->m_channelCfd->enableWrite();
    }
}

void TcpConnection::sendFileInLoop(int fileDescriptor, off_t &offset, size_t count)
{
    /**
     * 内部通过sendfile零拷贝技术方法来发送大的文件
     * 但是要注意 sendfile方法本身也不会一次性将非常大的文件数据发送完成 一般来说也需要while循环处理
     * 不过这里通过将这个sendFileInLoop函数再次添加到函数队列中, 从而可以再次发送 直到发送完成
     */

    size_t remainBytes = count;

    if (this->m_state == TcpConnection::Disconnecting || this->m_state == TcpConnection::Disconnected)
    {
        LOG_ERROR << "tcp disconnect, TcpConnection::sendFileInLoop error";
        return;
    }

    // 如果没有注册写事件且缓冲区无数据 说明是第一次发送或者是前面的消息已经发送完成
    if (!this->m_channelCfd->isWriteEvent() && this->m_sendBuffer.writableBytes() == 0)
    {
        size_t sendBytes = ::sendfile(this->m_socketCfd->fd(), fileDescriptor, &offset, remainBytes);
        if (sendBytes >= 0)
        {
            // sendBytes >= 0 发送成功
            remainBytes -= sendBytes;
            if (remainBytes == 0)
            {
                // 删除写事件触发
                if (this->m_channelCfd->isWriteEvent())
                    this->m_channelCfd->disableWrite();

                // 添加写完成回调函数
                if (this->m_writeCompleteCallback)
                    this->m_loop->queueInLoop(
                        std::bind(this->m_writeCompleteCallback, shared_from_this()));
            }
        }
        else
        {
            // sendBytes < 0 发送失败出现问题
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 正常缓冲区满 需要等有空闲再去发送数据
                this->m_channelCfd->enableWrite(); // 等待缓冲区空再写
                /**
                 * 注意, 这里注册写事件触发之后 当发送缓冲区有空闲空间时就会触发 然后channel执行对应的handleWrite方法
                 * 在handleWrite方法中, 添加了缓冲区不为空的条件来进行限制
                 * 因为sendfile方法本身是没有涉及到类中的缓冲区的, 所以这里注册之后在触发的channel中(写事件触发并不执行), 目的是执行doPendingFucntor里面的方法
                 */
            }
            else if (errno == EPIPE || errno == ECONNRESET)
            {
                // 对端关闭了 处理关闭连接的操作
                LOG_ERROR << "TcpConnection::sendFileInLoop error: peer closed connection";
                this->handleClose();
                return;
            }
            else
            {
                LOG_ERROR << "TcpConnection::sendFileInLoop errno";
                this->handleError();
            }
        }
    }

    // 如果仍然没有发送完 则插入到任务队列中 因为发送文件并没有将文件当中的数据插入到类内的发送缓冲区中 而是不断地间歇地发送文件内容
    if (remainBytes > 0)
    {
        this->m_loop->queueInLoop(
            std::bind(&TcpConnection::sendFileInLoop, shared_from_this(), fileDescriptor, offset, remainBytes));
    }
}

void TcpConnection::shutdownInLoop()
{
    // 判断是否将数据发送完 如果发送完了就会将写事件关闭掉 从而可以反向推测出来是否将数据发送完成了
    if (!this->m_channelCfd->isWriteEvent())
    {
        this->m_socketCfd->shutdownWrite();
    }
}

ssize_t TcpConnection::recvAllMessage(int cfd, std::string &message)
{
    char buf[1024];
    ::memset(buf, 0, sizeof(buf));
    ssize_t totalBytes = 0; // 记录总长度 也就是总的字节数

    while (true)
    {
        ssize_t retlen = ::read(cfd, buf, sizeof(buf));
        if (retlen > 0)
        {
            message.append(buf, retlen);
            totalBytes += retlen;
        }
        else if (retlen == 0)
        {
            // 对端关闭连接
            break;
        }
        else
        {
            // 被信号中断，继续读
            if (errno == EINTR)
                continue;
            // 非阻塞IO，没有数据了 真正的读取完毕
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            // 真正出错
            else
                return -1;
        }
        // 重新置零
        ::memset(buf, 0, sizeof(buf));
    }

    return totalBytes;
}

