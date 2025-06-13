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
    //   m_channelCfd(new Channel(m_loop, cfd)), // ��������ָ�벻��Ҫ������������
      m_channelCfd(make_unique_from_pool<Channel>(m_loop, cfd)), // ��������ָ�벻��Ҫ������������
      m_serverAddr(serverAddr), m_clientAddr(clientAddr)
{
    /**
     * ��TcpConnection�Ĺ��캯���� ֱ�Ӹ�����ͨ���ļ��������Ͷ�Ӧ�¼���Channelע��TcpConnection���е����лص��¼�
     * ��ΪTcpConnection�����������Щ�¼� �����¼������մ�������loop���е�Channel������
     */

    // �����Ӷ�Ӧ�������¼���ע�ᵽcfd��Ӧ��Channel��
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
     * û����Ҫ�رյ��ļ������� ����ָ��Ҳ�����Լ������������� �������������в���Ҫ����ʲô����
     * channel��صĲ������ڶ�Ӧλ�ý��е��� ������û�н���remove
     */
    LOG_INFO << "TcpConnection disconnecting, fd " << this->m_socketCfd->fd();
    // �����˳�ʱ��״̬Ӧ���Ѿ��� Disconnected
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
        // ���send�����Լ�loop���߳����ܵ� ��ô����ֱ�ӽ��з�����
        // ʵ�ʷ���һ��, ���Է���, else��䵱�е�runInLoop�������滹�������Ƿ����Լ��̵߳��ж� ����������Բ���if������
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
            // ע�⣺���̴߳���Bufferָ���ǲ���ȫ�ġ���ȷ�������Ǵ���string��
            // �������Ǽ����ϲ�ҵ��֤���������ڣ���ת��Ϊstring���ݡ�
            // Ϊ��ȫ���������Ҳת��Ϊstring
            std::string msg = buf->retrieveAllAsString();
            this->m_loop->runInLoop(
                std::bind(&TcpConnection::sendInLoop, shared_from_this(), msg.data(), msg.size()));
        }
    }
}

void TcpConnection::sendFile(int fileDescriptor, off_t offset, size_t count)
{
    // �������send˼·һ��

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
        // ����״̬
        this->setState(TcpConnection::Disconnecting);

        // ֱ��ִ�йرպ������߲��뵽���������еȴ�ִ��
        this->m_loop->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop, shared_from_this()));
    }
}

void TcpConnection::connectionEstablished()
{
    /**
     * ��������TcpConnection����֮�� ��Ҫ���ʵ���������ֶ�����connectionEstablished����
     */
    // ��������״̬
    this->setState(TcpConnection::Connected);
    // cfd��Ӧ��channel������ �Լ���epollע����¼�����
    this->m_channelCfd->tie(shared_from_this());
    this->m_channelCfd->enableRead();
    // ִ�����ӻص�
    this->m_connectionCallback(shared_from_this());
}

void TcpConnection::connectionDestroyed()
{
    if (this->m_state == TcpConnection::Connected || this->m_state == TcpConnection::Disconnecting)
    {
        // ��������״̬
        this->setState(TcpConnection::Disconnected);
        // cfd��Ӧ��channel��epoll��ɾ����
        this->m_channelCfd->disableAll();
        // ���ӻص�
        this->m_connectionCallback(shared_from_this());
    }
    // �����Ӷ�Ӧ��epoll��ChannelMapɾ����channel
    this->m_channelCfd->remove();
}

void TcpConnection::handleRead(TimeStamp recvTime)
{
    // ��ȡ�����ڵĶ�����������m_recvBuffer
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
        // ������
        errno = saveErrno;
        LOG_ERROR << "TcpConnection::handleRead readBytes < 0, error";
        // ���ô������ĺ���
        this->handleError();
    }
}

void TcpConnection::handleWrite()
{
    /**
     * handleWrite���������ݶ��Ѿ����浽���ڵ�m_sendBuffer����
     * �ڹ��캯���� handleWrite ��ע�ᵽ Channel ��
     * �������Ҳ����Channel�ڿ�дʱ��д�¼�����������Ҫ���Ķ���
     * ͨ��while+write�ķ�ʽ�����ݷ��͸��Զ�
     * ��ʱ�Ȳ�����
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
        // ��������״̬ ɾ�����д����¼�
        this->setState(TcpConnection::Disconnected);

        /**
         * ������Ҫ�Ƚ��ж�channel disableAll����ʹ�����m_closeCallback�ص���������һ�ν�����disableAll
         * ����һ��������һ���Ͽ����� ��ô����Ҫ�������Ӷ�Ӧ��Channel�����д����¼�����epoll��ɾ���� ������ܻ��ظ������Ѿ���Ч��Channel
         */
        this->m_channelCfd->disableAll(); // ��Ϊ����Ĺرջص��Ὣ���channel��epoll��ɾ����

        // ����һ��TcpConnection���� ȷ�����Ӳ��ᱻ����
        TcpConnectionPtr guardObj = shared_from_this();
        // ִ�����ӻص� ��Ϊ���ӻص�����Ҳ���ڹرյ�ʱ��������
        if (this->m_connectionCallback)
            this->m_connectionCallback(guardObj);

        // ִ�йرջص� ��mainloop��ע��Ĺرպ��� �������Ӵ�
        if (this->m_closeCallback)
            this->m_closeCallback(guardObj); // �����Ӵ�mainloop��ɾ�� ��channel�Ӷ�Ӧ��epoll�ϵ�map��ɾ��
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
     * �����ݷ��ͳ�ȥ���������̫�� ��ô��ͨ���������ע��д�¼� ʹ�ÿ��Զ�ν��з��� ֱ�������ݷ������
     */
    // ��һ�η��͵�ʱ��Ӧ����û��ע��д�¼���
    // ����ǰһ���Ѿ�������� ��ô�����disenbleWrite��д�¼�����ɾ����
    // ���ڵ�m_sendBufferֻ�����ڴ洢û�з�����ɵ����� ������һ��ʼ���ݾ���������

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
     * �ڲ�ͨ��sendfile�㿽���������������ʹ���ļ�
     * ����Ҫע�� sendfile��������Ҳ����һ���Խ��ǳ�����ļ����ݷ������ һ����˵Ҳ��Ҫwhileѭ������
     * ��������ͨ�������sendFileInLoop�����ٴ���ӵ�����������, �Ӷ������ٴη��� ֱ���������
     */

    size_t remainBytes = count;

    if (this->m_state == TcpConnection::Disconnecting || this->m_state == TcpConnection::Disconnected)
    {
        LOG_ERROR << "tcp disconnect, TcpConnection::sendFileInLoop error";
        return;
    }

    // ���û��ע��д�¼��һ����������� ˵���ǵ�һ�η��ͻ�����ǰ�����Ϣ�Ѿ��������
    if (!this->m_channelCfd->isWriteEvent() && this->m_sendBuffer.writableBytes() == 0)
    {
        size_t sendBytes = ::sendfile(this->m_socketCfd->fd(), fileDescriptor, &offset, remainBytes);
        if (sendBytes >= 0)
        {
            // sendBytes >= 0 ���ͳɹ�
            remainBytes -= sendBytes;
            if (remainBytes == 0)
            {
                // ɾ��д�¼�����
                if (this->m_channelCfd->isWriteEvent())
                    this->m_channelCfd->disableWrite();

                // ���д��ɻص�����
                if (this->m_writeCompleteCallback)
                    this->m_loop->queueInLoop(
                        std::bind(this->m_writeCompleteCallback, shared_from_this()));
            }
        }
        else
        {
            // sendBytes < 0 ����ʧ�ܳ�������
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // ������������ ��Ҫ���п�����ȥ��������
                this->m_channelCfd->enableWrite(); // �ȴ�����������д
                /**
                 * ע��, ����ע��д�¼�����֮�� �����ͻ������п��пռ�ʱ�ͻᴥ�� Ȼ��channelִ�ж�Ӧ��handleWrite����
                 * ��handleWrite������, ����˻�������Ϊ�յ���������������
                 * ��Ϊsendfile����������û���漰�����еĻ�������, ��������ע��֮���ڴ�����channel��(д�¼���������ִ��), Ŀ����ִ��doPendingFucntor����ķ���
                 */
            }
            else if (errno == EPIPE || errno == ECONNRESET)
            {
                // �Զ˹ر��� ����ر����ӵĲ���
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

    // �����Ȼû�з����� ����뵽��������� ��Ϊ�����ļ���û�н��ļ����е����ݲ��뵽���ڵķ��ͻ������� ���ǲ��ϵؼ�Ъ�ط����ļ�����
    if (remainBytes > 0)
    {
        this->m_loop->queueInLoop(
            std::bind(&TcpConnection::sendFileInLoop, shared_from_this(), fileDescriptor, offset, remainBytes));
    }
}

void TcpConnection::shutdownInLoop()
{
    // �ж��Ƿ����ݷ����� ����������˾ͻὫд�¼��رյ� �Ӷ����Է����Ʋ�����Ƿ����ݷ��������
    if (!this->m_channelCfd->isWriteEvent())
    {
        this->m_socketCfd->shutdownWrite();
    }
}

ssize_t TcpConnection::recvAllMessage(int cfd, std::string &message)
{
    char buf[1024];
    ::memset(buf, 0, sizeof(buf));
    ssize_t totalBytes = 0; // ��¼�ܳ��� Ҳ�����ܵ��ֽ���

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
            // �Զ˹ر�����
            break;
        }
        else
        {
            // ���ź��жϣ�������
            if (errno == EINTR)
                continue;
            // ������IO��û�������� �����Ķ�ȡ���
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            // ��������
            else
                return -1;
        }
        // ��������
        ::memset(buf, 0, sizeof(buf));
    }

    return totalBytes;
}

