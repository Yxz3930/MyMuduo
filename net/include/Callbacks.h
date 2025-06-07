#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <memory>
#include <functional>
#include <string>

class TimeStamp;
class TcpConnection;

// TcpConnection����ָ��
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
// ���ӻص�
using ConnectionCallback = std::function<void(const TcpConnectionPtr &)>;
// ��Ϣ�ص�
using MessageCallback = std::function<void(const TcpConnectionPtr &, std::string, TimeStamp)>;
// �رղ����ص�
using CloseCallback = std::function<void(const TcpConnectionPtr &)>;
// 
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr &)>;
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr &, size_t)>;

#endif // CALLBACKS_H