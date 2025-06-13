#include "TcpServer.h"
#include "Acceptor.h"
#include "ThreadPool.h"
#include "TcpConnection.h"
#include "EventLoop.h"

TcpServer::TcpServer(EventLoop *loop, InetAddr serverAddr, bool reusePort, int threadNums)
    : m_loop(loop), m_serverAddr(serverAddr), m_serverIp(serverAddr.getIp()), m_serverPort(serverAddr.getPort()), m_threadNums(threadNums),
      //   m_serverAcceptor(new Acceptor(loop, serverAddr, reusePort)),
      m_serverAcceptor(make_unique_from_pool<Acceptor>(loop, serverAddr, reusePort)),
      //   m_threadPool(new ThreadPool(threadNums, 20)), // 在线程池的构造函数中包括线程的创建 和 每个线程中的EventLoop的创建
      m_threadPool(make_unique_from_pool<ThreadPool>(threadNums, 20)), // 在线程池的构造函数中包括线程的创建 和 每个线程中的EventLoop的创建
      m_nextSubLoopIdx(0),
      m_isRuning(false)
{
    /**
     * 注册Acceptor的回调
     * TcpConnection连接的回调需要等到创建连接之后再设置回调
     */
    this->m_serverAcceptor->setNewConnectionCallback(
        std::bind(&TcpServer::handleNewConnection, this, std::placeholders::_1, std::placeholders::_2));

    // 默认回调，避免空指针
    m_connectionCallback = [](const TcpConnectionPtr &)
    { LOG_INFO << "Default ConnectionCallback: New connection."; };
    m_messageCallback = [](const TcpConnectionPtr &, Buffer *, TimeStamp)
    { LOG_INFO << "Default MessageCallback: Message received."; };
    m_writeCompleteCallback = [](const TcpConnectionPtr &)
    { LOG_INFO << "Default WriteCompleteCallback: Write completed."; };

    // 线程池已经在初始化列表中创建完成了
}

TcpServer::~TcpServer()
{
    LOG_INFO << "TcpServer::~TcpServer() destroying...";
    this->m_threadPool->stop();
    // 释放连接
    for (auto &connection : this->m_connMap)
    {
        // 转移资源 // 执行完局部函数之后会自动释放掉
        TcpConnectionPtr conn = std::move(connection.second);
        // 将指向的地址reset
        connection.second.reset();

        // 执行tcp连接的销毁操作
        // 通过连接找到各自的loop 然后删除自己上的tcp连接
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
    // 确保只启动一次
    if (!this->m_isRuning.exchange(true))
    {
        LOG_INFO << "threadPool(one subloop one thread) start\n";
        this->m_threadPool->start(this->m_subLoopVec);

        // 执行Acceptor对应的监听循环 里面注册了lfd(Acceptor)的读事件
        this->m_loop->runInLoop(
            std::bind(&Acceptor::listen, this->m_serverAcceptor.get()));

        std::cout << "[TcpServer start success, enter loop]" << std::endl;
    }
}

void TcpServer::handleNewConnection(int sockfd, InetAddr &clientAddr)
{

    // 原子操作进行轮询算法
    std::atomic_size_t loopIndex = this->nextLoopIndex();

    EventLoop *subloop = this->m_subLoopVec[loopIndex];

    // 准备创建链接
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
    // // 输出连接创建时间
    // TimeStamp now;
    // std::cout << "TcpConnectionPtr is created, now: " << now.toFormatString() << std::endl;

    // 记录连接的TcpConnection
    this->m_connMap[connName] = connPtr;

    // 给TcpConnection设置回调
    connPtr->setConnectionCallback(this->m_connectionCallback);
    connPtr->setMessageCallback(this->m_messageCallback);
    connPtr->setWriteCompleteCallback(this->m_writeCompleteCallback);

    /**
     * 这里添加的任务是handleClose而不是handleCloseConnection
     * 这是因为handleClose的作用是将handleCloseConnection这个方法添加到mainloop自身的任务队列当中
     * 如果直接将handleCloseConnection作为注册函数 那么就是将handleCloseConnection直接被subloop执行了 这是不允许的
     */
    connPtr->setCloseCallback(
        std::bind(&TcpServer::handleClose, this, std::placeholders::_1));

    // 在对应subloop中建立连接 runInLoop方法中有唤醒subloop的操作 本质上是对eventfd的写端写入数据 使得epoll_wait再次解除阻塞
    // 所以在一个客户端连接的过程中 epoll_wait解除两次阻塞，
    // 第一次是Acceptor注册的读事件触发(有新客户端连接了), 第二次是这里将对应subloop中的eventfd唤醒使得epoll_wait解除阻塞
    // channel中注册的连接回调是在TcpConnection的构造函数中传入的 因此需要将对应subloop唤醒之后执行注册的连接回调
    subloop->runInLoop(
        std::bind(&TcpConnection::connectionEstablished, connPtr));

    // // 输出每个subloop中连接的数量
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

    const std::string connName = conn->name(); // 获取连接的名字
    EventLoop *subloop = conn->getLoop();      // 获取连接所在的subloop

    // 在TcpServer中删除这个连接 通过键的方式删除 即使没有这么一个键值对也不会出现问题
    this->m_connMap.erase(connName);

    // 在subloop中删除这个连接
    subloop->queueInLoop(
        std::bind(&TcpConnection::connectionDestroyed, conn));

    // // 输出连接销毁时间
    // TimeStamp now;
    // std::cout << "TcpConnectionPtr is delete, now: " << now.toFormatString() << std::endl;
}
