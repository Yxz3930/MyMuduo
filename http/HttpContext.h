// =================================================================
// 文件: HttpContext.h (优化版)
// =================================================================
#ifndef HTTP_CONTEXT_H
#define HTTP_CONTEXT_H

#include <string>
#include <unordered_map>

// 前向声明 Buffer 类，避免在头文件中引入整个 Buffer.h
class Buffer;

class HttpContext
{
public:
    HttpContext() = default;

    /**
     * @brief [核心优化] 解析函数现在接收 Buffer* 指针
     * 直接在 Buffer 的内存上进行解析，避免了将整个缓冲区拷贝到 std::string。
     * @param buf 指向包含HTTP请求数据的缓冲区的指针
     * @return 如果成功解析出一个完整的HTTP请求头，则返回 true；否则返回 false。
     */
    bool parseRequest(Buffer* buf);

    // 生成一个简单的 HTTP 响应
    std::string makeResponse();

private:
    std::string method_;
    std::string path_;
    std::string version_;
    // 可以添加 std::unordered_map<std::string, std::string> headers_; 来保存头部信息
};

#endif // HTTP_CONTEXT_H