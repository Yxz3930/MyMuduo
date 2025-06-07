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
    
    // �����ַ������滻���ɼ��ַ�
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

    // ��������ͷ
    if (std::getline(iStrRequest, line))
    {
        bool ret = this->ParseRequestline(line);
        if(!ret)
        {
            LOG_ERROR("parse request line error");
            return false;
        }
    }
    // ���������Է���һ��http����ĸ���������
    // ���� Ŀǰֻ֧��get
    if(strcasecmp(this->m_method.c_str(), "get") != 0)
        return false;
        
    // url·��
    if(this->m_url == "/")
        // this->m_url = "./";
        // this->m_url = "./page/test.txt";
        this->m_url = "./page/home.html";
        // this->m_url = "./page/comic_home.html";
    else 
        this->m_url = "./page/" + this->m_url.substr(1); // ȥ����һ���ַ� ����˵ �ַ�������һ��λ��

    // ��ȡ�ļ�����
    int ret = stat(this->m_url.c_str(), &this->m_st);
    if(ret == -1)
    {
        // �ļ������� -- �ظ�404ҳ��
        LOG_WARNING(std::string(this->m_url + " request path is not exist"));
        this->m_url = "./page/404.html";
        this->is_exist = false; // ��Դ������
        this->m_parse_success = true;
        return true;
    }

    // ��������ͷ
    while( std::getline(iStrRequest, line) && !line.empty() )
    {
        // // �������β�Ļس��� `\r` ����Ҫ����'\r'�ַ�������
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

    // ������ͷ�в���Connection����



    // ������������
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
        return (this->m_version == "HTTP/1.1"); // HTTP/1.1Ĭ��keep-alive
}
