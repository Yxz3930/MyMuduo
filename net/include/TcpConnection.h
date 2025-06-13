#ifndef TCP_CONNECTION_H
#define TCP_CONNECTION_H

#include <memory>
#include <string>
#include <atomic>

#include "InetAddress.h"
#include "Callbacks.h"
#include "TimeStamp.h"
#include "Buffer.h"
#include "SmartPointer.h"


class Channel;
class EventLoop;
class Socket;

/// @brief TcpConnection �Ƕ�Tcp���ӵķ�װ ����tcp���ӵĶ�д, �¼�����, tcp���ӵ�״̬�仯��
///        һ�����Ӷ�Ӧ��һ��ͨ���ļ�������cfd, ��ô��ȻҲ�ͻ���һ��channel���������ͨ���ļ��������Ͷ�Ӧ���¼�(fd��channel����Գ��ֵ�)
class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
private:
    enum ConnectionState
    {
        Disconnected = 0, // �Ͽ�����״̬
        Connecting,       // ��������
        Connected,        // ��������
        Disconnecting     // ���ڶϿ�����
    };

public:
    /// @brief ���캯��
    /// @param loop EventLoop�¼�ѭ�� �������߳���ͨ����ѯ�ķ�ʽ��ȡ�������̳߳ص��е�subloop
    /// @param connName ���ӵ����� ������Ҳ�����ڼ�¼TcpConnection����
    /// @param cfd ͨ���ļ�������
    /// @param serverAddr ��������ַ
    /// @param clientAddr �ͻ��˵�ַ
    TcpConnection(EventLoop *loop, const std::string &connName, int cfd, const InetAddr &serverAddr, const InetAddr &clientAddr);

    ~TcpConnection();

    /// @brief ��ȡloop�¼�ѭ��
    /// @return
    EventLoop *getLoop() { return this->m_loop; }

    const std::string &name() { return this->m_connName; }
    const InetAddr &getServerAddr() { return this->m_serverAddr; }
    const InetAddr &getClientAddr() { return this->m_clientAddr; }
    std::string getServerIpPort() { return this->m_serverAddr.getIpPort(); }
    std::string getClientIpPort() { return this->m_clientAddr.getIpPort(); }

    /// @brief ���ö�ʱ�䡢д�¼����رջص�
    void setCallbacks();

    /// @brief �ж��Ƿ��Ѿ�����������
    /// @return
    bool isConnected() { return this->m_state == ConnectionState::Connected; }

    /// @brief ��������
    /// @param data ����
    void send(const std::string &data);

    void send(Buffer* buf);

    /// @brief ���ʹ����ļ�
    /// @param fileDescriptor �ļ���������
    /// @param offset ƫ���� ָ�����ļ����ĸ�λ�ÿ�ʼ��ȡ
    /// @param count Ҫ���������ֽ���
    void sendFile(int fileDescriptor, off_t offset, size_t count);

    /// @brief ���shutdown�����������⵽�Զ˹ر��� ��ô��һ��Ҳ��Ҫ���йر�
    void shutdown();

    /// @brief ����tcp���� ��mainloop�е��� channel��������ע����д�¼�����
    void connectionEstablished();

    /// @brief ����tcp���� ��mainloop�е��� ���������mainloop��ע��Ĺرպ���(�������ӵ�����)
    void connectionDestroyed();

    /// @brief �ⲿ��TcpConnectionע����Ϣ�ص�
    /// @param cb
    void setConnectionCallback(ConnectionCallback cb)
    {
        this->m_connectionCallback = std::move(cb);
    }

    void setMessageCallback(MessageCallback cb)
    {
        this->m_messageCallback = std::move(cb);
    }

    void setWriteCompleteCallback(WriteCompleteCallback cb)
    {
        this->m_writeCompleteCallback = std::move(cb);
    }

    void setCloseCallback(CloseCallback cb)
    {
        this->m_closeCallback = std::move(cb);
    }

private:
    void setState(ConnectionState state) { this->m_state = state; }

    /// @brief �������˽��ջ������յ�����(���¼�����) ������Ϣ�ص�����
    /// @param recvTime
    void handleRead(TimeStamp recvTime);

    /// @brief ����д�¼�
    void handleWrite();

    /// @brief ����ر������¼�
    void handleClose();

    /// @brief ��������¼�
    void handleError();

    /// @brief �������� ͨ��write����send��������
    /// @param data ���͵�����
    /// @param data_len ���ݳ���
    void sendInLoop(const void *data, size_t data_len);

    /// @brief �������� ͨ��sendfile��������
    /// @param fileDescriptor �ļ����ļ�������
    /// @param offset ƫ���� ָ�����ļ����ĸ�λ�ÿ�ʼ��ȡ
    /// @param count Ҫ���������ֽ���
    void sendFileInLoop(int fileDescriptor, off_t &offset, size_t count);

    /// @brief ��cfd��д�˹ر� ��������tcp�Ͽ��������Ĵλ����еĵ�һ�� ���ŵضϿ�����
    void shutdownInLoop();

    /// @brief ͨ��while+read�ķ�ʽ��ȡ��������
    /// @param cfd ͨ���ļ�������
    /// @param message �������� ��ȡ������
    /// @return ���ض�ȡ�����ܹ����ַ�����
    ssize_t recvAllMessage(int cfd, std::string &message);

private:
    const std::string m_connName;
    const InetAddr m_serverAddr;
    const InetAddr m_clientAddr;

    EventLoop *m_loop;
    // std::unique_ptr<Socket> m_socketCfd;   // cfd��Ӧ��socket
    // std::unique_ptr<Channel> m_channelCfd; // ��cfd�Ͷ�Ӧ�¼��ķ�װchannel
    std::unique_ptr<Socket, PoolDeleter<Socket>> m_socketCfd;   // cfd��Ӧ��socket
    std::unique_ptr<Channel, PoolDeleter<Channel>> m_channelCfd; // ��cfd�Ͷ�Ӧ�¼��ķ�װchannel
    ConnectionState m_state;               // ���ӵ�״̬

    // �ص�����
    ConnectionCallback m_connectionCallback;       // ���ӻص� �¿ͻ��˽�������/�Ͽ�����֮����Ҫ��ʲô
    MessageCallback m_messageCallback;             // ��Ϣ�ص� ���յ���Ϣ֮����Ҫ��ʲô
    WriteCompleteCallback m_writeCompleteCallback; // д��ɻص� �������˽����ݷ������֮����Ҫ��ʲô
    CloseCallback m_closeCallback;                 // �ر����ӻص�

    // ���պͷ��ͻ����� ʹ��string�Ļ����ܻ�ǳ��� ���Ƽ�
    Buffer m_recvBuffer;
    Buffer m_sendBuffer;
    // std::string m_recvBuffer;
    // std::string m_sendBuffer;
    // size_t m_sendOffset = 0; // ���ڼ�¼m_sendBuffer�������͵�������ַ ��ʾ������һ����Ҫ��offset����λ�ü�������
    //                          // ����0ʱ��ʾ��Ҫ������0��ʼ���� 10ʱ��ʾ��Ҫ������10��ʼ����
};

#endif // TCP_CONNECTION_H



