#include "HttpResponse.h"
#include "HttpParser.h"
#include <iostream>
#include <unistd.h>
#include <fstream>
#include <thread>
#include <chrono>
#include "myutils.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <ostream>
#include <sys/stat.h>
#include "Logger.h"

void HttpResponse::SetStatusLine(const std::string &_version, const int &_code, const std::string &_desc)
{
    this->m_version = _version;
    this->m_state_code = _code;
    this->m_state_desc = _desc;
}

void HttpResponse::SetResponseHeaderPair(const std::string &_key, const std::string &_value)
{
    auto result_pair = this->m_headers.insert({_key, _value}); // 返回 std::pair<iterator, bool>
    if (!result_pair.second)                                   // 第二个是插入的结果 插入失败
        LOG_WARNING("m_header insert error");
}

void HttpResponse::SetBody(const std::string &_msg)
{
    this->m_body = _msg;
}

std::string HttpResponse::GenerateResponse()
{
    this->m_response.clear(); // 存放的响应也应当及时清理
    this->m_headers.clear();  // 存放的响应头应该及时清理
    if (!this->m_http_parser->ParseDone())
    {
        LOG_INFO("http parse fail");
        return std::string("");
    }

    // 设置状态行 判断资源是否存在
    if (!this->m_http_parser->isExist())
    {
        this->SetStatusLine(this->m_http_parser->m_version, 404, "no resource");
        struct stat st;
        int ret = stat("./page/404.html", &st);
        if (ret == -1)
        {
            LOG_ERROR("stat error");
            return "";
        }
        this->SetResponseHeaderPair("Content-Length", std::to_string(st.st_size));
    }
    else
    {
        this->SetStatusLine(this->m_http_parser->m_version, 200, "ok");
        this->SetResponseHeaderPair("Content-Length", std::to_string(this->m_http_parser->GetFileSize()));
    }

    // 设置响应头
    LOG_DEBUG(std::string("requst path: ") + this->m_http_parser->GetPath());
    std::string content_type = GetFileContentType(this->m_http_parser->GetPath());
    this->SetResponseHeaderPair("Content-Type", content_type);

    if (this->m_http_parser->isKeepAlive())
        this->SetResponseHeaderPair("Connection", "keep-alive");
    else
        this->SetResponseHeaderPair("Connection", "close");

    this->SetResponseHeaderPair("Cache-Control", "no-cache");
    this->SetResponseHeaderPair("Pragma", "no-cache");

    // 拼接状态行
    this->m_response += this->m_version + " ";
    this->m_response += std::to_string(this->m_state_code) + " ";
    this->m_response += this->m_state_desc + "\r\n";

    // 拼接响应头
    for (const auto &pair : this->m_headers)
    {
        this->m_response += pair.first + ": ";
        this->m_response += pair.second + "\r\n";
    }
    // 拼接空行
    this->m_response += "\r\n";

    return this->m_response;
}

void HttpResponse::SendFile(const std::string &filepath, int cfd)
{
#if 0 // 手动读取文件并发送文件

    std::string line;
    std::fstream ifs(this->m_http_parser->GetPath());
    if(!ifs.is_open())
    {
        LOG_ERROR("open file error");
        return;
    }

    while(std::getline(ifs, line) && !line.empty())
    {
        // std::cout << "line: " << line << std::endl;
        int ret = write(cfd, line.c_str(), line.size());
        LOG_ERROR("write error");
        if(ret == -1)
            return;
        std::this_thread::sleep_for(std::chrono::microseconds(1)); // 每次发送之后稍微等待一段时间
    }

#else

    int file_fd = open(filepath.c_str(), O_RDONLY);
    if (file_fd == -1)
        LOG_ERROR("open file error");

    struct stat file_stat;
    if (fstat(file_fd, &file_stat) < 0)
    {
        LOG_ERROR("fstat file error");
        close(file_fd);
        return;
    }

    ssize_t size = lseek(file_fd, 0, SEEK_END); // 将文件指针指向文件末尾 计算偏移量也就是文件大小
    lseek(file_fd, 0, SEEK_SET);                // 将文件指针指向开头

    // sendfile发送大文件 循环发送 分块传输
    off_t offset = 0;
    ssize_t bytes_send;
    int buffer_szie = 1024;
    while (offset < file_stat.st_size)
    {
        bytes_send = sendfile(cfd, file_fd, &offset, buffer_szie);
        if (bytes_send < 0)
        {
            LOG_ERROR("sendfile error");
            close(file_fd);
            return;
        }
    }
    close(file_fd);

#endif
}

void HttpResponse::SendHttpResponse(int cfd)
{
    LOG_DEBUG("send http response starting");
    if (!this->m_http_parser->isExist())
    {
        // 如果资源不存在 直接发送前三部分然后退出 404
        this->GenerateResponse(); // 生成响应前三部分
        if (!isFdClosed(cfd))
        {
            write(cfd, this->m_response.c_str(), this->m_response.size());
            this->SendFile(this->m_http_parser->GetPath(), cfd);
        }
        LOG_DEBUG("sendfile 404.html done");
        return;
    }

    // 判断客户端获取的资源是目录还是文件
    if (S_ISDIR(this->m_http_parser->GetSourceType()))
    {
        // 如果是目录 则将目录中的内容发送给客户端
    }
    else
    {
        this->GenerateResponse();
        if (!isFdClosed(cfd))
        {
            write(cfd, this->m_response.c_str(), this->m_response.size()); // 发送响应前三部分
            // // 如果是文件则发送文件内容
            this->SendFile(this->m_http_parser->GetPath(), cfd); // 发送第四部分响应体(文件)
        }
        LOG_DEBUG(std::string("sendfile " + this->m_http_parser->GetPath() + " done"));
    }
    LOG_DEBUG("http response: ");
    LOG_DEBUG(this->m_response);
}

std::string HttpResponse::GetFileContentType(const std::string &path)
{
    ssize_t pos = path.find_last_of(".");
    if (pos == std::string::npos)
    {
        LOG_ERROR("get an invalid path, without ' . '");
        throw std::runtime_error("error");
    }

    std::string fileExtension = path.substr(pos);

// 进行fileExtension的判断
#if 1
    if (fileExtension == ".html" || fileExtension == ".htm")
    {
        return "text/html; charset=utf-8";
    }
    else if (fileExtension == ".css")
    {
        return "text/css";
    }
    else if (fileExtension == ".js")
    {
        return "application/javascript";
    }
    else if (fileExtension == ".json")
    {
        return "application/json";
    }
    else if (fileExtension == ".xml")
    {
        return "application/xml";
    }
    else if (fileExtension == ".png")
    {
        return "image/png";
    }
    else if (fileExtension == ".jpg" || fileExtension == ".jpeg")
    {
        return "image/jpeg";
    }
    else if (fileExtension == ".gif")
    {
        return "image/gif";
    }
    else if (fileExtension == ".bmp")
    {
        return "image/bmp";
    }
    else if (fileExtension == ".svg")
    {
        return "image/svg+xml";
    }
    else if (fileExtension == ".ico")
    {
        return "image/x-icon";
    }
    else if (fileExtension == ".pdf")
    {
        return "application/pdf";
    }
    else if (fileExtension == ".txt")
    {
        return "text/plain charset=utf-8";
    }
    else if (fileExtension == ".zip")
    {
        return "application/zip";
    }
    else if (fileExtension == ".tar")
    {
        return "application/x-tar";
    }
    else if (fileExtension == ".gz")
    {
        return "application/gzip";
    }
    else if (fileExtension == ".mp3")
    {
        return "audio/mpeg";
    }
    else if (fileExtension == ".mp4")
    {
        return "video/mp4";
    }
    else if (fileExtension == ".avi")
    {
        return "video/x-msvideo";
    }
    else if (fileExtension == ".wav")
    {
        return "audio/wav";
    }
    else if (fileExtension == ".doc")
    {
        return "application/msword";
    }
    else if (fileExtension == ".docx")
    {
        return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    }
    else if (fileExtension == ".ppt")
    {
        return "application/vnd.ms-powerpoint";
    }
    else if (fileExtension == ".pptx")
    {
        return "application/vnd.openxmlformats-officedocument.presentationml.presentation";
    }
    else if (fileExtension == ".xlsx")
    {
        return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    }
    else
    {
        return "text/plain"; // Default for text/plain
    }
#endif
}
