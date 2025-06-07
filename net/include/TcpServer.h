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
    /// @brief 构造函数
    /// @param loop EventLoop 创建的mainloop事件循环 外部传入
    /// @param serverAddr (InetAddr) 服务器地址 外部传入
    /// @param reusePort 是否重用本地端口
    /// @param threadNums 线程数量
    TcpServer(EventLoop *loop, InetAddr serverAddr, bool reusePort = true, int threadNums = 1);
    ~TcpServer();

    /// @brief 设置线程池中的线程
    /// @param threadNums
    void setThreadNums(int threadNums);

    /// @brief 注册连接回调业务
    /// @param cb
    void setConnectionCallback(ConnectionCallback cb)
    {
        this->m_connectionCallback = std::move(cb);
    }

    /// @brief 注册消息回调业务
    /// @param cb
    void setMessageCallback(MessageCallback cb)
    {
        this->m_messageCallback = std::move(cb);
    }

    /// @brief 注册写操作完成业务
    /// @param cb
    void setWriteCompleteCallback(WriteCompleteCallback cb)
    {
        this->m_writeCompleteCallback = std::move(cb);
    }

    /// @brief 执行mainloop事件循环
    void start();

private:
    /// @brief mainloop中创建新连接之后进行tcp连接回调设置, 向subloop进行分配
    /// @param sockfd
    /// @param clientAddr
    void handleNewConnection(int sockfd, InetAddr &clientAddr);

    /// @brief 获取下一个loop的索引
    /// @return 返回下一个loop的索引
    size_t nextLoopIndex();

    /// @brief 关闭tcp连接的操作
    void handleCloseConnection(const TcpConnectionPtr &conn);

private:
    EventLoop *m_loop;
    std::string m_serverIp;
    std::string m_serverPort;
    InetAddr m_serverAddr;

    std::unique_ptr<Acceptor> m_serverAcceptor; // 运行在mainloop中 进行新客户端的连接监听
    EventLoopPtrVector m_subLoopVec;            // 存储subLoop
    std::unique_ptr<ThreadPool> m_threadPool;   // 内存池 里面的每个线程运行着subloop的loop事件循环
    int m_threadNums;                           // 线程数量

    std::atomic_size_t m_nextSubLoopIdx{0}; // 下一个subloop的索引 从 m_subLoopVec 中取即可
    std::atomic_bool m_isRuning{false};     // 是否开启了服务

    ConnectionPtrMap m_connMap; // 记录所有连接的Connection

    // 外界注册的业务函数
    ConnectionCallback m_connectionCallback;       // 连接回调
    MessageCallback m_messageCallback;             // 消息回调
    WriteCompleteCallback m_writeCompleteCallback; // 写完成回调
};

#endif // TCP_SERVER_H