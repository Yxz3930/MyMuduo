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

    struct stat m_st; // ��¼�ͻ��˻�ȡ����Դ����
    bool is_exist; // ��¼��Դ�Ƿ����

private:
    bool m_parse_success{false};

public:

    bool ParseRequestline(const std::string& line);

    bool ParseRequestHeader(const std::string& line);

    bool ParseHttpRequest(const std::string& rawRequest);

    bool ParseDone();

    /// @brief ��ȡ�ͻ����������Դ���� ��Ŀ¼�����ļ�
    /// @return st.mode
    __mode_t GetSourceType();

    /// @brief ����stat��ȡ�ļ��Ĵ�С ������д��Ӧͷ���ļ��ĳ���
    /// @return st.st_size
    ssize_t GetFileSize();

    /// @brief ��ȡ��Դ�Ƿ����
    /// @return bool
    bool isExist();

    /// @brief ��ȡ��http�����еõ�����Դ·��
    /// @return path
    std::string GetPath();

    /// @brief ��������ͷ�е������ж��Ƿ������Ϊkeep-alive
    /// @return 
    bool isKeepAlive();

};





#endif // HTTP_PARSER_H