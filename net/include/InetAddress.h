#ifndef INETADDRESS_H
#define INETADDRESS_H

#include <iostream>
#include <arpa/inet.h>
#include <string>

class InetAddr
{
private:
    std::string m_ip; // ip地址 默认初始化为"" 点分十进制形式的ip字符串形式
    int m_port{-1};   // 端口号 主机字节序

public:
    InetAddr() : m_ip(""), m_port(-1) {}
    InetAddr(std::string ip_address, int port) : m_ip(ip_address), m_port(port) {}
    InetAddr(struct sockaddr_in addr);
    ~InetAddr();

    /// @brief 设置addr数据
    /// @param addr
    void setSockAddr(sockaddr_in addr);

    // 获取ip地址点分十进制字符串
    std::string getIp(void);

    // 获取端口号sock
    std::string getPort(void);

    // 转换为sockaddr_in的格式
    sockaddr_in toSocketAddr(void);

    /// @brief 用于const对象获取ip和port
    /// @return ip:port 
    std::string getIpPort() const; 
};

#endif // INETADDRESS_H