#include "HttpContext.h"
#include "Buffer.h" 
#include <algorithm> 
#include <sstream>

// searchCRLF 和 parseRequest 函数保持不变...

const char* searchCRLF(const char* begin, const char* end) {
    const char* crlf = "\r\n";
    const char* found = std::search(begin, end, crlf, crlf + 2);
    return found == end ? nullptr : found;
}

bool HttpContext::parseRequest(Buffer* buf)
{
    const char* kDoubleCRLF = "\r\n\r\n";
    const char* end_of_headers = std::search(buf->peek(),
                                             buf->peek() + buf->readableBytes(),
                                             kDoubleCRLF,
                                             kDoubleCRLF + 4);

    if (end_of_headers == buf->peek() + buf->readableBytes()) {
        return false;
    }

    const char* start_of_line = buf->peek();
    const char* end_of_line = searchCRLF(start_of_line, end_of_headers);
    if (!end_of_line) {
        return false;
    }

    std::string request_line(start_of_line, end_of_line);
    std::istringstream iss(request_line);
    iss >> method_ >> path_ >> version_;

    buf->retrieve(end_of_headers + 4 - buf->peek());

    return true;
}


/**
 * @brief [关键修改] 生成与 Nginx 默认页面完全相同的 HTTP 响应
 */
std::string HttpContext::makeResponse()
{
    // 1. 定义 Nginx 默认页面的 HTML 内容
    // 使用 C++11 的原始字符串字面量 (raw string literal) R"()" 可以方便地处理多行和特殊字符
    std::string body = R"(<html>
<head>
<title>Welcome to nginx!</title>
<style>
    body {
        width: 35em;
        margin: 0 auto;
        font-family: Tahoma, Verdana, Arial, sans-serif;
    }
</style>
</head>
<body>
<h1>Welcome to nginx!</h1>
<p>If you see this page, the nginx web server is successfully installed and
working. Further configuration is required.</p>

<p>For online documentation and support please refer to
<a href="http://nginx.org/">nginx.org</a>.<br/>
Commercial support is available at
<a href="http://nginx.com/">nginx.com</a>.</p>

<p><em>Thank you for using nginx.</em></p>
</body>
</html>)";

    std::string response;
    response.reserve(body.size() + 256); // 预留足够空间

    // 2. 构建 HTTP 响应头
    response += "HTTP/1.1 200 OK\r\n";
    
    // 3. [修改] 将 Content-Type 修改为 text/html
    response += "Content-Type: text/html\r\n";
    
    // 4. [修改] 根据新的 body 计算 Content-Length
    response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    
    // 5. 保持 keep-alive 以支持高性能测试
    response += "Connection: keep-alive\r\n";

    response += "\r\n"; // Header 和 Body 的分隔符
    
    // 6. 附加 HTML 响应体
    response += body;

    return response;
}
