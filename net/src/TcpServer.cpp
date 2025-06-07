#include "TcpServer.h"
#include "Acceptor.h"
#include "ThreadPool.h"
#include "TcpConnection.h"
#include "EventLoop.h"


TcpServer::TcpServer(EventLoop *loop, InetAddr serverAddr, bool reusePort, int threadNums)
    : m_loop(loop), m_serverAddr(serverAddr), m_serverIp(serverAddr.getIp()), m_serverPort(serverAddr.getPort()), m_threadNums(threadNums),
      m_serverAcceptor(new Acceptor(loop, serverAddr, reusePort)),
      m_threadPool(new ThreadPool(threadNums, 10)), // ���̳߳صĹ��캯���а����̵߳Ĵ��� �� ÿ���߳��е�EventLoop�Ĵ���
      m_nextSubLoopIdx(0),
      m_isRuning(false)
{
    /**
     * ע��Acceptor�Ļص�
     * TcpConnection���ӵĻص���Ҫ�ȵ���������֮�������ûص�
     */
    this->m_serverAcceptor->setNewConnectionCallback(
        std::bind(&TcpServer::handleNewConnection, this, std::placeholders::_1, std::placeholders::_2));

    // �̳߳��Ѿ��ڳ�ʼ���б��д��������
}

TcpServer::~TcpServer()
{
    this->m_threadPool->stop();
    // �ͷ�����
    for (auto &connection : this->m_connMap)
    {
        // ת����Դ // ִ����ֲ�����֮����Զ��ͷŵ�
        TcpConnectionPtr conn = std::move(connection.second);
        // ɾ����map�е�����
        this->m_connMap.erase(connection.first);

        // ִ��tcp���ӵ����ٲ���
        // ͨ�������ҵ����Ե�loop Ȼ��ɾ���Լ��ϵ�tcp����
        conn->getLoop()->queueInLoop(
            std::bind(&TcpConnection::connectionDestroyed, conn));
    }
}

void TcpServer::setThreadNums(int threadNums)
{
    this->m_threadNums = threadNums;
}

void TcpServer::start()
{
    this->m_isRuning.store(true);
    LOG_INFO("threadPool(one subloop one thread) start\n");
    this->m_threadPool->start(this->m_subLoopVec);

    // ִ��Acceptor��Ӧ�ļ���ѭ�� ����ע����lfd(Acceptor)�Ķ��¼�
    this->m_loop->runInLoop(
        std::bind(&Acceptor::listen, this->m_serverAcceptor.get()));

    std::cout << "[TcpServer start success, enter loop]" << std::endl;
    // ���߳��¼�ѭ�� �����������ⲿ����ѭ��
    this->m_loop->loop();

}

void TcpServer::handleNewConnection(int sockfd, InetAddr &clientAddr)
{
    // ԭ�Ӳ���������ѯ�㷨
    std::atomic_size_t loopIndex = this->nextLoopIndex();

    EventLoop *subloop = this->m_subLoopVec[loopIndex];

    // ׼����������
    std::string connName = "TcpConnection_" + clientAddr.getIp() + "_" + clientAddr.getPort();
    LOG_INFO("client ip port: " + clientAddr.getIp() + ":" + clientAddr.getPort());

    TcpConnectionPtr connPtr = std::make_shared<TcpConnection>(subloop, connName,
                                                               sockfd,
                                                               this->m_serverAddr,
                                                               clientAddr);
    // ��¼���ӵ�TcpConnection
    this->m_connMap[connName] = connPtr;

    // ��TcpConnection���ûص�
    connPtr->setConnectionCallback(this->m_connectionCallback);
    connPtr->setMessageCallback(this->m_messageCallback);
    connPtr->setWriteCompleteCallback(this->m_writeCompleteCallback);
    connPtr->setCloseCallback(
        std::bind(&TcpServer::handleCloseConnection, this, std::placeholders::_1));

    // �ڶ�Ӧsubloop�н������� runInLoop�������л���subloop�Ĳ��� �������Ƕ�eventfd��д��д������ ʹ��epoll_wait�ٴν������
    // ������һ���ͻ������ӵĹ����� epoll_wait�������������
    // ��һ����Acceptorע��Ķ��¼�����(���¿ͻ���������), �ڶ��������ｫ��Ӧsubloop�е�eventfd����ʹ��epoll_wait�������
    // channel��ע������ӻص�����TcpConnection�Ĺ��캯���д���� �����Ҫ����Ӧsubloop����֮��ִ��ע������ӻص�
    subloop->runInLoop(
        std::bind(&TcpConnection::connectionEstablished, connPtr.get()));
}

size_t TcpServer::nextLoopIndex()
{
    std::atomic_size_t index = this->m_nextSubLoopIdx.load();
    this->m_nextSubLoopIdx = this->m_nextSubLoopIdx.fetch_add(1) % this->m_threadNums;

    return index;
}

void TcpServer::handleCloseConnection(const TcpConnectionPtr &conn)
{
    LOG_INFO("TcpServer::handleCloseConnection remove TcpConnection on m_connMap");

    const std::string connName = conn->name(); // ��ȡ���ӵ�����
    EventLoop* subloop = conn->getLoop(); // ��ȡ�������ڵ�subloop

    // ��TcpServer��ɾ���������
    auto it = this->m_connMap.find(connName);    
    TcpConnectionPtr _conn = std::move(it->second);
    this->m_connMap.erase(it);

    // ��subloop��ɾ���������
    subloop->runInLoop(
        std::bind(&TcpConnection::connectionDestroyed, _conn));
}
