#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <memory>
#include <atomic>
#include <string>
#include <vector>
#include <thread>
#include "Callbacks.h"
#include "InetAddress.h"

class EventLoop;
class Acceptor;
class ThreadPool;

class TcpServer
{
private:
    using ConnectionPtrMap = std::unordered_map<std::string, TcpConnectionPtr>;
    using EventLoopPtrVector = std::vector<EventLoop *>;

public:
    /// @brief ���캯��
    /// @param loop EventLoop ������mainloop�¼�ѭ�� �ⲿ����
    /// @param serverAddr (InetAddr) ��������ַ �ⲿ����
    /// @param reusePort �Ƿ����ñ��ض˿�
    /// @param threadNums �߳�����
    TcpServer(EventLoop *loop, InetAddr serverAddr, bool reusePort = true, int threadNums = 1);
    ~TcpServer();

    /// @brief �����̳߳��е��߳�
    /// @param threadNums
    void setThreadNums(int threadNums);

    /// @brief ע�����ӻص�ҵ��
    /// @param cb
    void setConnectionCallback(ConnectionCallback cb)
    {
        this->m_connectionCallback = std::move(cb);
    }

    /// @brief ע����Ϣ�ص�ҵ��
    /// @param cb
    void setMessageCallback(MessageCallback cb)
    {
        this->m_messageCallback = std::move(cb);
    }

    /// @brief ע��д�������ҵ��
    /// @param cb
    void setWriteCompleteCallback(WriteCompleteCallback cb)
    {
        this->m_writeCompleteCallback = std::move(cb);
    }

    /// @brief ִ��mainloop�¼�ѭ��
    void start();

private:
    /// @brief mainloop�д���������֮�����tcp���ӻص�����, ��subloop���з���
    /// @param sockfd
    /// @param clientAddr
    void handleNewConnection(int sockfd, InetAddr &clientAddr);

    /// @brief ��ȡ��һ��loop������
    /// @return ������һ��loop������
    size_t nextLoopIndex();

    /// @brief �ر�tcp���ӵĲ���
    void handleCloseConnection(const TcpConnectionPtr &conn);

private:
    EventLoop *m_loop;
    std::string m_serverIp;
    std::string m_serverPort;
    InetAddr m_serverAddr;

    std::unique_ptr<Acceptor> m_serverAcceptor; // ������mainloop�� �����¿ͻ��˵����Ӽ���
    EventLoopPtrVector m_subLoopVec;            // �洢subLoop
    std::unique_ptr<ThreadPool> m_threadPool;   // �ڴ�� �����ÿ���߳�������subloop��loop�¼�ѭ��
    int m_threadNums;                           // �߳�����

    std::atomic_size_t m_nextSubLoopIdx{0}; // ��һ��subloop������ �� m_subLoopVec ��ȡ����
    std::atomic_bool m_isRuning{false};     // �Ƿ����˷���

    ConnectionPtrMap m_connMap; // ��¼�������ӵ�Connection

    // ���ע���ҵ����
    ConnectionCallback m_connectionCallback;       // ���ӻص�
    MessageCallback m_messageCallback;             // ��Ϣ�ص�
    WriteCompleteCallback m_writeCompleteCallback; // д��ɻص�
};

#endif // TCP_SERVER_H