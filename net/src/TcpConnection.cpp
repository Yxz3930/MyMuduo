#include "TcpConnection.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include <cstring>
#include <unistd.h>
#include <sys/sendfile.h>

TcpConnection::TcpConnection(EventLoop *loop, const std::string &connName, int cfd, const InetAddr &serverAddr, const InetAddr &clientAddr)
    : m_loop(loop), m_connName(connName), m_cfd(cfd), m_state(TcpConnection::Connecting),
      m_socketCfd(new Socket(m_cfd)), m_channelCfd(new Channel(m_loop, m_cfd)), // ��������ָ�벻��Ҫ������������
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
    // this->m_channelCfd->setErrorCallback(
    //     std::bind(&TcpConnection::handleError, this));

    this->m_socketCfd->setKeepAlive(true);

    LOG_INFO("TcpConnection start connecting, fd " + std::to_string(this->m_cfd));
}

TcpConnection::~TcpConnection()
{
    /**
     * û����Ҫ�رյ��ļ������� ����ָ��Ҳ�����Լ������������� �������������в���Ҫ����ʲô����
     * channel��صĲ������ڶ�Ӧλ�ý��е��� ������û�н���remove
     */
    LOG_INFO("TcpConnection disconnecting, fd " + std::to_string(this->m_cfd));
    this->setState(TcpConnection::Disconnected);
}

void TcpConnection::send(const std::string &data)
{
    if (this->isConnected())
    {
        // ���send�����Լ�loop���߳����ܵ� ��ô����ֱ�ӽ��з�����
        // ʵ�ʷ���һ��, ���Է���, else��䵱�е�runInLoop�������滹�������Ƿ����Լ��̵߳��ж� ����������Բ���if������
        if (this->m_loop->isThisThread())
            this->sendInLoop(data.c_str(), data.size());

        else
            this->m_loop->runInLoop(
                std::bind(&TcpConnection::sendInLoop, this, data.c_str(), data.size()));
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
                std::bind(&TcpConnection::sendFileInLoop, this, fileDescriptor, offset, count));
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
            std::bind(&TcpConnection::shutdownInLoop, this));
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
    if(this->m_state == TcpConnection::Connected)
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
    /**
     * Channel��fd�Ľ��ջ������������ǣ����¼�����������Ҫ���Ķ���
     * ����û�в���readv���շ�ʽ ����ԭʼ��read���� ͨ��whileѭ��һ���Զ�ȡ����������
     */

    this->m_recvBuffer.clear(); // ������ڽ��ջ����������� �������
    ssize_t readBytes = this->recvAllMessage(this->m_cfd, this->m_recvBuffer);

    // �����ж��Ƿ�ɹ���ȡ���� Ȼ��ֲ�ͬ������д��� �ɹ���ȡ�����������Ϣ�ص�ִ�ж�Ӧ�ķ���
    if (readBytes > 0)
    {
        // ˵����ȡ�������� Ӧ�ý�����Ϣ�ص�
        this->m_messageCallback(shared_from_this(), this->m_recvBuffer, recvTime);
    }
    else if (readBytes == 0)
    {
        // �Զ˹ر� Ӧ�õ��ùرյĺ���
        this->handleClose(); // ��������������ô���رյĻص�
        return;
    }
    else
    {
        // ������
        LOG_ERROR("TcpConnection::handleRead readBytes < 0, error");

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
    if (this->m_channelCfd->isWriteEvent() && !this->m_sendBuffer.empty())
    {
        size_t writeLen = ::write(this->m_cfd, this->m_sendBuffer.data() + this->m_sendOffset, this->m_sendBuffer.size() - this->m_sendOffset);
        if (writeLen > 0)
        {
            this->m_sendOffset += writeLen;
            if (this->m_sendOffset == this->m_sendBuffer.size()) // ���ͻ�����������д����
            {
                // ���������е����ݷ����� ��д�¼������ر�
                this->m_channelCfd->disableWrite();

                // ���д��ɻص���Ϊ��
                if (this->m_writeCompleteCallback)
                {
                    this->m_writeCompleteCallback(shared_from_this());
                }

                // ����Ͽ������� ��ر�tcp����
                if (this->m_state == TcpConnection::Disconnected || this->m_state == TcpConnection::Disconnecting)
                {
                    this->shutdownInLoop();
                }
            }
        }
    }
    else
    {
        LOG_ERROR("TcpConnection::handleWrite cannot write");
    }
}

void TcpConnection::handleClose()
{
    // ��������״̬ ɾ�����д����¼�
    this->setState(TcpConnection::Disconnected);

    /**
     * ������Ҫ�Ƚ��ж�channel disableAll����ʹ�����m_closeCallback�ص���������һ�ν�����disableAll
     * ����һ��������һ���Ͽ����� ��ô����Ҫ�������Ӷ�Ӧ��Channel�����д����¼�����epoll��ɾ���� ������ܻ��ظ������Ѿ���Ч��Channel
     */
    this->m_channelCfd->disableAll(); // ��Ϊ����Ĺرջص��Ὣ���channel��epoll��ɾ����

    // ִ�����ӻص� ��Ϊ���ӻص�����Ҳ���ڹرյ�ʱ��������
    if(this->m_connectionCallback)  
        this->m_connectionCallback(shared_from_this());

    // ִ�йرջص� ��mainloop��ע��Ĺرպ��� �������Ӵ�
    if(this->m_closeCallback)
        this->m_closeCallback(shared_from_this());
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if (::getsockopt(this->m_cfd, SOL_SOCKET, SO_ERROR,
                     &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:" + this->m_connName + " - SO_ERROR: " + std::to_string(err));
}

void TcpConnection::sendInLoop(const void *data, size_t data_len)
{
    /**
     * �����ݷ��ͳ�ȥ���������̫�� ��ô��ͨ���������ע��д�¼� ʹ�ÿ��Զ�ν��з��� ֱ�������ݷ������
     */

    // ��һ�η��͵�ʱ��Ӧ����û��ע��д�¼���
    // ����ǰһ���Ѿ�������� ��ô�����disenbleWrite��д�¼�����ɾ����
    // ���ڵ�m_sendBufferֻ�����ڴ洢û�з�����ɵ����� ������һ��ʼ���ݾ���������
    if (! this->m_channelCfd->isWriteEvent() && this->m_sendBuffer.empty())
    {
        size_t writeLen = ::write(this->m_cfd, data, data_len);

        if (writeLen >= 0)
        {
            // ? ���1��write �ɹ����� >=0 ���ͳɹ�

            if (writeLen == data_len)
            {
                // д��ɻص�
                if (this->m_writeCompleteCallback)
                    // this->m_writeCompleteCallback(shared_from_this()); // ֱ��ִ��
                    this->m_loop->queueInLoop(
                        std::bind(m_writeCompleteCallback, shared_from_this())); // ���뵽������ �ȵ����¼�����ʱִ��
            }
            else if (writeLen < data_len)
            {
                // û�з����� �����ݱ���������(�û��ռ���) ע��д�¼����� �ȵ����ͻ������п��пռ�ʱ��ȥ����
                this->m_sendBuffer.append(static_cast<const char *>(data) + writeLen, data_len - writeLen);
                this->m_sendOffset = 0;

                // channelע��д�¼����� �����ͻ������д��ڿ��пռ�ʱ�ʹ�����ʼ��һ�η���
                // ���ﲻ��Ҫ���queueInLoopp����ΪhandleWrite�����Ѿ�ע�ᵽ��Channel��
                if (!this->m_channelCfd->isWriteEvent())
                    this->m_channelCfd->enableWrite();
            }
        }
        else
        {
            // // ? ���2��write ʧ�ܣ����� -1  ����Ҫ�����治ͬ�����
            //
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                // ?? ���2.1���ں˻������� -> д���� -> ע��д�¼� �Ȼ�����������д
                m_sendBuffer.append(static_cast<const char *>(data), data_len);
                m_channelCfd->enableWrite();
            }
            else if (errno == EINTR)
            {
                // ?? ���2.2�����ź��ж� -> ����ѡ�����³��� write ������ֱ��ѡ�������ƶ����������н��д��������ע��д�¼�
                m_sendBuffer.append(static_cast<const char *>(data), data_len);
                m_channelCfd->enableWrite();
            }
            else
            {
                // ? ���2.3���������󣬱��� ECONNRESET��EPIPE -> ˵�����ӿ����쳣
                LOG_ERROR("TcpConnection::sendInLoop write error: " + std::string(strerror(errno)));
            }
        }
    }
    else
    {
        // ���3���Ѿ�ע��д�¼����߻�������Ϊ�� -> ��������Ȼ������ݣ�Ȼ��ȴ� write �¼�������������
        m_sendBuffer.append(static_cast<const char *>(data), data_len);
        if (!m_channelCfd->isWriteEvent())
            m_channelCfd->enableWrite(); // ȷ��һ��ע��д�¼��Լ�������
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
        LOG_ERROR("tcp disconnect, TcpConnection::sendFileInLoop error");
        return;
    }

    // ���û��ע��д�¼��һ����������� ˵���ǵ�һ�η��ͻ�����ǰ�����Ϣ�Ѿ��������
    if (!this->m_channelCfd->isWriteEvent() && this->m_sendBuffer.empty())
    {
        size_t sendBytes = ::sendfile(this->m_cfd, fileDescriptor, &offset, remainBytes);
        if (sendBytes >= 0)
        {
            // sendBytes >= 0 ���ͳɹ�
            remainBytes -= sendBytes;
            if (remainBytes == 0)
            {
                // ɾ��д�¼�����
                if(this->m_channelCfd->isWriteEvent())
                    this->m_channelCfd->disableWrite();

                // ���д��ɻص�����
                if(this->m_writeCompleteCallback)
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
                LOG_ERROR("TcpConnection::sendFileInLoop error: peer closed connection");
                this->handleClose();
                return;
            }
            else
            {
                LOG_ERROR("TcpConnection::sendFileInLoop errno");
                this->handleError();
            }
        }
    }
    // �����Ȼû�з����� ����뵽���������
    if (remainBytes > 0)
    {
        this->m_loop->queueInLoop(
            std::bind(&TcpConnection::sendFileInLoop, this, fileDescriptor, offset, remainBytes));
    }
}

void TcpConnection::shutdownInLoop()
{
    // �ж��Ƿ����ݷ����� ����������˾ͻὫд�¼��رյ� �Ӷ����Է����Ʋ�����Ƿ����ݷ��������
    if (!this->m_channelCfd->isWriteEvent())
    {
        this->m_socketCfd->shutdownWrite();
    }
    else // �����Ȼ������ ��ô���Ƚ�����������뵽�������е��� ֱ���ر����
        this->m_loop->queueInLoop(
            std::bind(&TcpConnection::shutdownInLoop, this));
}

ssize_t TcpConnection::recvAllMessage(int cfd, std::string &message)
{
    char buf[64];
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
