#ifndef INETADDRESS_H
#define INETADDRESS_H

#include <iostream>
#include <arpa/inet.h>
#include <string>

class InetAddr
{
private:
    std::string m_ip; // ip��ַ Ĭ�ϳ�ʼ��Ϊ"" ���ʮ������ʽ��ip�ַ�����ʽ
    int m_port{-1};   // �˿ں� �����ֽ���

public:
    InetAddr() : m_ip(""), m_port(-1) {}
    InetAddr(std::string ip_address, int port) : m_ip(ip_address), m_port(port) {}
    InetAddr(struct sockaddr_in addr);
    ~InetAddr();

    /// @brief ����addr����
    /// @param addr
    void setSockAddr(sockaddr_in addr);

    // ��ȡip��ַ���ʮ�����ַ���
    std::string getIp(void);

    // ��ȡ�˿ں�sock
    std::string getPort(void);

    // ת��Ϊsockaddr_in�ĸ�ʽ
    sockaddr_in toSocketAddr(void);

    /// @brief ����const�����ȡip��port
    /// @return ip:port 
    std::string getIpPort() const; 
};

#endif // INETADDRESS_H