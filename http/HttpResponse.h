#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H


#include <string>
#include <unordered_map>
#include <memory>

class HttpParser;

class HttpResponse
{
public:
    std::string m_version; // HTTP版本
    int m_state_code; // 状态码
    std::string m_state_desc; // 状态描述符 是对状态码的解释
    std::unordered_map<std::string, std::string> m_headers; // 响应头 字符串键值对
    std::string m_body; // 响应体

private:
    std::string m_response; // 包括状态行、响应头、空行 没有响应体 响应体可能是大文件需要单独发送
    std::shared_ptr<HttpParser> m_http_parser; 


public:
    HttpResponse() {};
    HttpResponse(const std::shared_ptr<HttpParser>& _http_parser): m_http_parser(_http_parser) {}
    ~HttpResponse() {};


    /// @brief 设置状态行
    /// @param _version http版本号
    /// @param _code 状态码
    /// @param _desc 状态描述
    void SetStatusLine(const std::string& _version, const int& _code, const std::string& _desc);

    /// @brief 设置响应头中的每个键值对
    /// @param _key 键
    /// @param _value 值
    void SetResponseHeaderPair(const std::string& _key, const std::string& _value);

    /// @brief 设置响应体文本
    /// @param _msg 文本内容
    void SetBody(const std::string& _msg);

    /// @brief 生成http响应的前三部分：状态行、响应头、空行
    /// @param filepath 文件路径 用于读取文件大小
    /// @return 
    std::string GenerateResponse();

    /// @brief 向客户端发送文件/资源(只发送文件)
    /// @param cfd 客户端文件描述符
    void SendFile(const std::string& filepath, int cfd);

    /// @brief 向客户端发送http响应数据（包括 GenerateResponse()生成的前三部分和发送的文件等数据）
    /// @param fd 客户端通信文件描述符
    void SendHttpResponse(int cfd);


    /// @brief 根据文件路径 获取文件的类型即后缀
    /// @param path 
    /// @return 文件类型对应的Content-Type类型
    std::string GetFileContentType(const std::string& path);


};





#endif // HTTP_RESPONSE_H