#include "HttpParser.h"
#include <sstream>
#include <iostream>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h> 
#include <algorithm>
#include "Logger.h"

std::string replace_invisible_chars(const std::string& str) {
    std::string result = str;
    
    // 遍历字符串，替换不可见字符
    for (size_t i = 0; i < result.length(); ++i) {
        if (result[i] == '\r') {
            result.replace(i, 1, "<CR>");
        }
        else if (result[i] == '\n') {
            result.replace(i, 1, "<LF>");
        }
        else if (result[i] == '\t') {
            result.replace(i, 1, "<TAB>");
        }
        else if (result[i] == ' ') {
            result.replace(i, 1, "<SPACE>");
        }
    }
    return result;
}



bool HttpParser::ParseRequestline(const std::string &line)
{
    std::istringstream istr(line);
    istr >> this->m_method >> this->m_url >> this->m_version;

    return !this->m_method.empty() && !this->m_url.empty() && !this->m_version.empty();
}

bool HttpParser::ParseRequestHeader(const std::string &line)
{
    ssize_t pos = line.find(":");
    if (pos == std::string::npos)
        return false;

    std::string key = line.substr(0, pos);
    std::string value = line.substr(pos+2);
    this->m_headers.insert({key, value});

    return true;
}

bool HttpParser::ParseHttpRequest(const std::string &rawRequest)
{
    LOG_DEBUG("parse http request starting");
    std::istringstream iStrRequest(rawRequest);
    std::string line;

    // 解析请求头
    if (std::getline(iStrRequest, line))
    {
        bool ret = this->ParseRequestline(line);
        if(!ret)
        {
            LOG_ERROR("parse request line error");
            return false;
        }
    }
    // 接下来可以分析一下http请求的各部分内容
    // 方法 目前只支持get
    if(strcasecmp(this->m_method.c_str(), "get") != 0)
        return false;
        
    // url路径
    if(this->m_url == "/")
        // this->m_url = "./";
        // this->m_url = "./page/test.txt";
        this->m_url = "./page/home.html";
        // this->m_url = "./page/comic_home.html";
    else 
        this->m_url = "./page/" + this->m_url.substr(1); // 去掉第一个字符 或者说 字符串右移一个位置

    // 获取文件属性
    int ret = stat(this->m_url.c_str(), &this->m_st);
    if(ret == -1)
    {
        // 文件不存在 -- 回复404页面
        LOG_WARNING(std::string(this->m_url + " request path is not exist"));
        this->m_url = "./page/404.html";
        this->is_exist = false; // 资源不存在
        this->m_parse_success = true;
        return true;
    }

    // 解析请求头
    while( std::getline(iStrRequest, line) && !line.empty() )
    {
        // // 清理掉行尾的回车符 `\r` 必须要进行'\r'字符的清理
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());

        if(line.empty())
            break;
        bool ret = this->ParseRequestHeader(line);
        if(!ret)
        {
            LOG_ERROR("parse request headers error");
            this->m_parse_success = false;
            return false;
        }
    }

    // 在请求头中查找Connection类型



    // 解析请求内容
    if(iStrRequest.peek() != EOF)
        std::getline(iStrRequest, this->m_body, '\0');

    this->m_parse_success = true;
    this->is_exist = true;
    return true;
}

bool HttpParser::ParseDone()
{
    return this->m_parse_success;
}

__mode_t HttpParser::GetSourceType()
{
    return this->m_st.st_mode;
}

ssize_t HttpParser::GetFileSize()
{
    return this->m_st.st_size;
}

bool HttpParser::isExist()
{
    return this->is_exist;
}

std::string HttpParser::GetPath()
{
    return this->m_url;
}

bool HttpParser::isKeepAlive()
{
    auto it = this->m_headers.find("Connection");
    if(it != this->m_headers.end())
        return (it->second == "keep-alive");
    else
        return (this->m_version == "HTTP/1.1"); // HTTP/1.1默认keep-alive
}
