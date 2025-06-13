#include "HttpContext.h"
#include "Buffer.h" 
#include <algorithm> 
#include <sstream>

// searchCRLF �� parseRequest �������ֲ���...

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
 * @brief [�ؼ��޸�] ������ Nginx Ĭ��ҳ����ȫ��ͬ�� HTTP ��Ӧ
 */
std::string HttpContext::makeResponse()
{
    // 1. ���� Nginx Ĭ��ҳ��� HTML ����
    // ʹ�� C++11 ��ԭʼ�ַ��������� (raw string literal) R"()" ���Է���ش�����к������ַ�
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
    response.reserve(body.size() + 256); // Ԥ���㹻�ռ�

    // 2. ���� HTTP ��Ӧͷ
    response += "HTTP/1.1 200 OK\r\n";
    
    // 3. [�޸�] �� Content-Type �޸�Ϊ text/html
    response += "Content-Type: text/html\r\n";
    
    // 4. [�޸�] �����µ� body ���� Content-Length
    response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    
    // 5. ���� keep-alive ��֧�ָ����ܲ���
    response += "Connection: keep-alive\r\n";

    response += "\r\n"; // Header �� Body �ķָ���
    
    // 6. ���� HTML ��Ӧ��
    response += body;

    return response;
}
