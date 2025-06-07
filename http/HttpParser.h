#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <string>
#include <unordered_map>
#include <sys/stat.h>


class HttpParser
{
public:
    std::string m_method;
    std::string m_url;
    std::string m_version;
    std::unordered_map<std::string, std::string> m_headers;
    std::string m_body;

    struct stat m_st; // 记录客户端获取的资源特性
    bool is_exist; // 记录资源是否存在

private:
    bool m_parse_success{false};

public:

    bool ParseRequestline(const std::string& line);

    bool ParseRequestHeader(const std::string& line);

    bool ParseHttpRequest(const std::string& rawRequest);

    bool ParseDone();

    /// @brief 获取客户端请求的资源类型 是目录还是文件
    /// @return st.mode
    __mode_t GetSourceType();

    /// @brief 根据stat获取文件的大小 用于填写响应头中文件的长度
    /// @return st.st_size
    ssize_t GetFileSize();

    /// @brief 获取资源是否存在
    /// @return bool
    bool isExist();

    /// @brief 获取从http请求中得到的资源路径
    /// @return path
    std::string GetPath();

    /// @brief 根据请求头中的内容判断是否愮设置为keep-alive
    /// @return 
    bool isKeepAlive();

};





#endif // HTTP_PARSER_H