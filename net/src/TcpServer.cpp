#include "TcpServer.h"
#include "Acceptor.h"
#include "ThreadPool.h"
#include "TcpConnection.h"
#include "EventLoop.h"

TcpServer::TcpServer(EventLoop *loop, InetAddr serverAddr, bool reusePort, int threadNums)
    : m_loop(loop), m_serverAddr(serverAddr), m_serverIp(serverAddr.getIp()), m_serverPort(serverAddr.getPort()), m_threadNums(threadNums),
      //   m_serverAcceptor(new Acceptor(loop, serverAddr, reusePort)),
      m_serverAcceptor(make_unique_from_pool<Acceptor>(loop, serverAddr, reusePort)),
      //   m_threadPool(new ThreadPool(threadNums, 20)), // ���̳߳صĹ��캯���а����̵߳Ĵ��� �� ÿ���߳��е�EventLoop�Ĵ���
      m_threadPool(make_unique_from_pool<ThreadPool>(threadNums, 20)), // ���̳߳صĹ��캯���а����̵߳Ĵ��� �� ÿ���߳��е�EventLoop�Ĵ���
      m_nextSubLoopIdx(0),
      m_isRuning(false)
{
    /**
     * ע��Acceptor�Ļص�
     * TcpConnection���ӵĻص���Ҫ�ȵ���������֮�������ûص�
     */
    this->m_serverAcceptor->setNewConnectionCallback(
        std::bind(&TcpServer::handleNewConnection, this, std::placeholders::_1, std::placeholders::_2));

    // Ĭ�ϻص��������ָ��
    m_connectionCallback = [](const TcpConnectionPtr &)
    { LOG_INFO << "Default ConnectionCallback: New connection."; };
    m_messageCallback = [](const TcpConnectionPtr &, Buffer *, TimeStamp)
    { LOG_INFO << "Default MessageCallback: Message received."; };
    m_writeCompleteCallback = [](const TcpConnectionPtr &)
    { LOG_INFO << "Default WriteCompleteCallback: Write completed."; };

    // �̳߳��Ѿ��ڳ�ʼ���б��д��������
}

TcpServer::~TcpServer()
{
    LOG_INFO << "TcpServer::~TcpServer() destroying...";
    this->m_threadPool->stop();
    // �ͷ�����
    for (auto &connection : this->m_connMap)
    {
        // ת����Դ // ִ����ֲ�����֮����Զ��ͷŵ�
        TcpConnectionPtr conn = std::move(connection.second);
        // ��ָ��ĵ�ַreset
        connection.second.reset();

        // ִ��tcp���ӵ����ٲ���
        // ͨ�������ҵ����Ե�loop Ȼ��ɾ���Լ��ϵ�tcp����
        conn->getLoop()->queueInLoop(
            std::bind(&TcpConnection::connectionDestroyed, conn));
    }
    LOG_INFO << "All connections queued for destruction.";
}

void TcpServer::setThreadNums(int threadNums)
{
    this->m_threadNums = threadNums;
}

void TcpServer::start()
{
    // ȷ��ֻ����һ��
    if (!this->m_isRuning.exchange(true))
    {
        LOG_INFO << "threadPool(one subloop one thread) start\n";
        this->m_threadPool->start(this->m_subLoopVec);

        // ִ��Acceptor��Ӧ�ļ���ѭ�� ����ע����lfd(Acceptor)�Ķ��¼�
        this->m_loop->runInLoop(
            std::bind(&Acceptor::listen, this->m_serverAcceptor.get()));

        std::cout << "[TcpServer start success, enter loop]" << std::endl;
    }
}

void TcpServer::handleNewConnection(int sockfd, InetAddr &clientAddr)
{

    // ԭ�Ӳ���������ѯ�㷨
    std::atomic_size_t loopIndex = this->nextLoopIndex();

    EventLoop *subloop = this->m_subLoopVec[loopIndex];

    // ׼����������
    std::string connName = "TcpConnection_" + clientAddr.getIp() + "_" + clientAddr.getPort();
    LOG_INFO << "client ip port: " + clientAddr.getIp() + ":" + clientAddr.getPort();

    // TcpConnectionPtr connPtr = std::make_shared<TcpConnection>(subloop, connName,
    //                                                            sockfd,
    //                                                            this->m_serverAddr,
    //                                                            clientAddr);
    TcpConnectionPtr connPtr = make_shared_from_pool<TcpConnection>(subloop, connName,
                                                               sockfd,
                                                               this->m_serverAddr,
                                                               clientAddr);
    // // ������Ӵ���ʱ��
    // TimeStamp now;
    // std::cout << "TcpConnectionPtr is created, now: " << now.toFormatString() << std::endl;

    // ��¼���ӵ�TcpConnection
    this->m_connMap[connName] = connPtr;

    // ��TcpConnection���ûص�
    connPtr->setConnectionCallback(this->m_connectionCallback);
    connPtr->setMessageCallback(this->m_messageCallback);
    connPtr->setWriteCompleteCallback(this->m_writeCompleteCallback);

    /**
     * ������ӵ�������handleClose������handleCloseConnection
     * ������ΪhandleClose�������ǽ�handleCloseConnection���������ӵ�mainloop�����������е���
     * ���ֱ�ӽ�handleCloseConnection��Ϊע�ắ�� ��ô���ǽ�handleCloseConnectionֱ�ӱ�subloopִ���� ���ǲ������
     */
    connPtr->setCloseCallback(
        std::bind(&TcpServer::handleClose, this, std::placeholders::_1));

    // �ڶ�Ӧsubloop�н������� runInLoop�������л���subloop�Ĳ��� �������Ƕ�eventfd��д��д������ ʹ��epoll_wait�ٴν������
    // ������һ���ͻ������ӵĹ����� epoll_wait�������������
    // ��һ����Acceptorע��Ķ��¼�����(���¿ͻ���������), �ڶ��������ｫ��Ӧsubloop�е�eventfd����ʹ��epoll_wait�������
    // channel��ע������ӻص�����TcpConnection�Ĺ��캯���д���� �����Ҫ����Ӧsubloop����֮��ִ��ע������ӻص�
    subloop->runInLoop(
        std::bind(&TcpConnection::connectionEstablished, connPtr));

    // // ���ÿ��subloop�����ӵ�����
    // LOG_WARNING << "main loop map size: " << this->m_connMap.size();
    // for(auto& subloop : this->m_subLoopVec)
    // {
    //     LOG_WARNING << "sub loop map size: " << subloop->getMapSize();
    // }
    // LOG_ERROR << "";
}

size_t TcpServer::nextLoopIndex()
{
    size_t index = this->m_nextSubLoopIdx.fetch_add(1, std::memory_order_relaxed);
    return index % this->m_threadNums;
}

void TcpServer::handleClose(const TcpConnectionPtr &conn)
{
    this->m_loop->runInLoop(std::bind(&TcpServer::handleCloseConnection, this, conn));
}

void TcpServer::handleCloseConnection(const TcpConnectionPtr &conn)
{
    LOG_INFO << "TcpServer::handleCloseConnection remove TcpConnection on m_connMap";

    const std::string connName = conn->name(); // ��ȡ���ӵ�����
    EventLoop *subloop = conn->getLoop();      // ��ȡ�������ڵ�subloop

    // ��TcpServer��ɾ��������� ͨ�����ķ�ʽɾ�� ��ʹû����ôһ����ֵ��Ҳ�����������
    this->m_connMap.erase(connName);

    // ��subloop��ɾ���������
    subloop->queueInLoop(
        std::bind(&TcpConnection::connectionDestroyed, conn));

    // // �����������ʱ��
    // TimeStamp now;
    // std::cout << "TcpConnectionPtr is delete, now: " << now.toFormatString() << std::endl;
}
