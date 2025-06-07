#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H


#include <string>
#include <unordered_map>
#include <memory>

class HttpParser;

class HttpResponse
{
public:
    std::string m_version; // HTTP�汾
    int m_state_code; // ״̬��
    std::string m_state_desc; // ״̬������ �Ƕ�״̬��Ľ���
    std::unordered_map<std::string, std::string> m_headers; // ��Ӧͷ �ַ�����ֵ��
    std::string m_body; // ��Ӧ��

private:
    std::string m_response; // ����״̬�С���Ӧͷ������ û����Ӧ�� ��Ӧ������Ǵ��ļ���Ҫ��������
    std::shared_ptr<HttpParser> m_http_parser; 


public:
    HttpResponse() {};
    HttpResponse(const std::shared_ptr<HttpParser>& _http_parser): m_http_parser(_http_parser) {}
    ~HttpResponse() {};


    /// @brief ����״̬��
    /// @param _version http�汾��
    /// @param _code ״̬��
    /// @param _desc ״̬����
    void SetStatusLine(const std::string& _version, const int& _code, const std::string& _desc);

    /// @brief ������Ӧͷ�е�ÿ����ֵ��
    /// @param _key ��
    /// @param _value ֵ
    void SetResponseHeaderPair(const std::string& _key, const std::string& _value);

    /// @brief ������Ӧ���ı�
    /// @param _msg �ı�����
    void SetBody(const std::string& _msg);

    /// @brief ����http��Ӧ��ǰ�����֣�״̬�С���Ӧͷ������
    /// @param filepath �ļ�·�� ���ڶ�ȡ�ļ���С
    /// @return 
    std::string GenerateResponse();

    /// @brief ��ͻ��˷����ļ�/��Դ(ֻ�����ļ�)
    /// @param cfd �ͻ����ļ�������
    void SendFile(const std::string& filepath, int cfd);

    /// @brief ��ͻ��˷���http��Ӧ���ݣ����� GenerateResponse()���ɵ�ǰ�����ֺͷ��͵��ļ������ݣ�
    /// @param fd �ͻ���ͨ���ļ�������
    void SendHttpResponse(int cfd);


    /// @brief �����ļ�·�� ��ȡ�ļ������ͼ���׺
    /// @param path 
    /// @return �ļ����Ͷ�Ӧ��Content-Type����
    std::string GetFileContentType(const std::string& path);


};





#endif // HTTP_RESPONSE_H