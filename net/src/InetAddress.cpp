#include "InetAddress.h"



InetAddr::InetAddr(struct sockaddr_in addr )
{
    this->m_ip = inet_ntoa(addr.sin_addr);
    this->m_port = ntohs(addr.sin_port);
    int _af = addr.sin_family;
}

InetAddr::~InetAddr(){
    this->m_ip = "";
    this->m_port = -1;
}

void InetAddr::setSockAddr(sockaddr_in addr)
{
    this->m_ip = inet_ntoa(addr.sin_addr);
    this->m_port = ntohs(addr.sin_port);
    int _af = addr.sin_family;
}

std::string InetAddr::getIp(void)
{
    return this->m_ip.c_str();
}

std::string InetAddr::getPort(void)
{
    return std::to_string(this->m_port);
}

sockaddr_in InetAddr::toSocketAddr(void)
{
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(this->m_port);
    addr.sin_addr.s_addr = inet_addr(this->m_ip.c_str());
    return addr;
}

std::string InetAddr::getIpPort() const
{
    return this->m_ip + ":" + std::to_string(this->m_port);
}
