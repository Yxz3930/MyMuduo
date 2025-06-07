#ifndef SOCKET_H
#define SOCKET_H

#include <iostream>
#include <sys/socket.h>
#include <utility>
class InetAddr;

/**
 * Socket 类
 * 属性包含监听文件描述符和通信文件描述符(accept获取)
 * 方法包含socket, bind, listen, accept等
 */

/// @brief Socket类 管理socket fd的所有权, 但是要注意Socket类对象需要从外部传入一个文件描述符 然后析构时会自动关闭对应的fd
///        这里的socket fd的创建是由上层的Acceptr或者工厂函数来完成 这样会更灵活 避免了只能在构造函数中创建文件描述符的情况
///        当然 也可以在构造函数中创建fd 这样也没有什么问题
class Socket
{
private:
    int m_fd{-1};

public:
    explicit Socket(int sockfd)
        : m_fd(sockfd) {}

    // Socket() { this->createNoBlocFd(); }

    ~Socket();

    int fd() {return this->m_fd;}

    // 通过socket获取监听文件描述符
    int createSocketFd();

    /// @brief 用于Acceptor中服务器端文件描述符获取
    /// @return fd
    int createNoBlocFd();

    // bind
    int bind(InetAddr *addr);

    // listen
    int listen();

    /// @brief 服务器端接收到客户端连接之后 返回cfd同时将客户端地址写入clientAddr当中
    /// @param clientAddr 保存客户端的地址
    /// @return 
    int accept(InetAddr* clientAddr);

    // accept
    InetAddr accept();

    // accept同时返回得到的字符串以及客户端地址 参数int仅仅是为了区分 默认设置为0
    std::pair<int, InetAddr> accept(int para = 0);

    /// @brief 客户端用于连接服务端的方法
    /// @param server_addr 服务器段的地址
    /// @return 
    int connect(InetAddr *server_addr);

    /// @brief socket文件描述符关闭本段写操作功能 本端不能发送数据 远端回收到FIN报文 但本地仍可以进行接收操作 属于关闭tcp连接中4次挥手中的一步
    void shutdownWrite();

    /// @brief 设置是否禁用Nagle 算法 Nagle 算法用于减少网络上传输的小数据包数量
    /// @param on 
    void setTcpNoDelay(bool on);

    /// @brief 设置是否可以重复绑定同一个地址
    /// @param on 
    void setReuseAddr(bool on);

    /// @brief 设置是否可以重复绑定同一个端口
    /// @param on 
    void setReusePort(bool on);

    /// @brief 设置在已经连接的套接字上定期传输消息
    /// @param on 
    void setKeepAlive(bool on);
};

#endif // SOCKET_H
