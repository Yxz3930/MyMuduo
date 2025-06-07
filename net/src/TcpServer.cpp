#include "TcpServer.h"
#include "Acceptor.h"
#include "ThreadPool.h"
#include "TcpConnection.h"
#include "EventLoop.h"


TcpServer::TcpServer(EventLoop *loop, InetAddr serverAddr, bool reusePort, int threadNums)
    : m_loop(loop), m_serverAddr(serverAddr), m_serverIp(serverAddr.getIp()), m_serverPort(serverAddr.getPort()), m_threadNums(threadNums),
      m_serverAcceptor(new Acceptor(loop, serverAddr, reusePort)),
      m_threadPool(new ThreadPool(threadNums, 10)), // 在线程池的构造函数中包括线程的创建 和 每个线程中的EventLoop的创建
      m_nextSubLoopIdx(0),
      m_isRuning(false)
{
    /**
     * 注册Acceptor的回调
     * TcpConnection连接的回调需要等到创建连接之后再设置回调
     */
    this->m_serverAcceptor->setNewConnectionCallback(
        std::bind(&TcpServer::handleNewConnection, this, std::placeholders::_1, std::placeholders::_2));

    // 线程池已经在初始化列表中创建完成了
}

TcpServer::~TcpServer()
{
    this->m_threadPool->stop();
    // 释放连接
    for (auto &connection : this->m_connMap)
    {
        // 转移资源 // 执行完局部函数之后会自动释放掉
        TcpConnectionPtr conn = std::move(connection.second);
        // 删除掉map中的连接
        this->m_connMap.erase(connection.first);

        // 执行tcp连接的销毁操作
        // 通过连接找到各自的loop 然后删除自己上的tcp连接
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

    // 执行Acceptor对应的监听循环 里面注册了lfd(Acceptor)的读事件
    this->m_loop->runInLoop(
        std::bind(&Acceptor::listen, this->m_serverAcceptor.get()));

    std::cout << "[TcpServer start success, enter loop]" << std::endl;
    // 主线程事件循环 这个还是最好外部进行循环
    this->m_loop->loop();

}

void TcpServer::handleNewConnection(int sockfd, InetAddr &clientAddr)
{
    // 原子操作进行轮询算法
    std::atomic_size_t loopIndex = this->nextLoopIndex();

    EventLoop *subloop = this->m_subLoopVec[loopIndex];

    // 准备创建链接
    std::string connName = "TcpConnection_" + clientAddr.getIp() + "_" + clientAddr.getPort();
    LOG_INFO("client ip port: " + clientAddr.getIp() + ":" + clientAddr.getPort());

    TcpConnectionPtr connPtr = std::make_shared<TcpConnection>(subloop, connName,
                                                               sockfd,
                                                               this->m_serverAddr,
                                                               clientAddr);
    // 记录连接的TcpConnection
    this->m_connMap[connName] = connPtr;

    // 给TcpConnection设置回调
    connPtr->setConnectionCallback(this->m_connectionCallback);
    connPtr->setMessageCallback(this->m_messageCallback);
    connPtr->setWriteCompleteCallback(this->m_writeCompleteCallback);
    connPtr->setCloseCallback(
        std::bind(&TcpServer::handleCloseConnection, this, std::placeholders::_1));

    // 在对应subloop中建立连接 runInLoop方法中有唤醒subloop的操作 本质上是对eventfd的写端写入数据 使得epoll_wait再次解除阻塞
    // 所以在一个客户端连接的过程中 epoll_wait解除两次阻塞，
    // 第一次是Acceptor注册的读事件触发(有新客户端连接了), 第二次是这里将对应subloop中的eventfd唤醒使得epoll_wait解除阻塞
    // channel中注册的连接回调是在TcpConnection的构造函数中传入的 因此需要将对应subloop唤醒之后执行注册的连接回调
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

    const std::string connName = conn->name(); // 获取连接的名字
    EventLoop* subloop = conn->getLoop(); // 获取连接所在的subloop

    // 在TcpServer中删除这个连接
    auto it = this->m_connMap.find(connName);    
    TcpConnectionPtr _conn = std::move(it->second);
    this->m_connMap.erase(it);

    // 在subloop中删除这个连接
    subloop->runInLoop(
        std::bind(&TcpConnection::connectionDestroyed, _conn));
}
