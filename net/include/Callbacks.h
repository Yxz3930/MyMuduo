#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <memory>
#include <functional>
#include <string>
#include "Buffer.h"

class TimeStamp;
class TcpConnection;

// TcpConnection智能指针
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
// 连接回调
using ConnectionCallback = std::function<void(const TcpConnectionPtr &)>;
// 消息回调
// using MessageCallback = std::function<void(const TcpConnectionPtr &, std::string, TimeStamp)>;
using MessageCallback = std::function<void(const TcpConnectionPtr &, Buffer*, TimeStamp)>;
// 关闭操作回调
using CloseCallback = std::function<void(const TcpConnectionPtr &)>;
// 
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr &)>;
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr &, size_t)>;

#endif // CALLBACKS_H