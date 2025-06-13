// =================================================================
// �ļ�: HttpContext.h (�Ż���)
// =================================================================
#ifndef HTTP_CONTEXT_H
#define HTTP_CONTEXT_H

#include <string>
#include <unordered_map>

// ǰ������ Buffer �࣬������ͷ�ļ����������� Buffer.h
class Buffer;

class HttpContext
{
public:
    HttpContext() = default;

    /**
     * @brief [�����Ż�] �����������ڽ��� Buffer* ָ��
     * ֱ���� Buffer ���ڴ��Ͻ��н����������˽����������������� std::string��
     * @param buf ָ�����HTTP�������ݵĻ�������ָ��
     * @return ����ɹ�������һ��������HTTP����ͷ���򷵻� true�����򷵻� false��
     */
    bool parseRequest(Buffer* buf);

    // ����һ���򵥵� HTTP ��Ӧ
    std::string makeResponse();

private:
    std::string method_;
    std::string path_;
    std::string version_;
    // ������� std::unordered_map<std::string, std::string> headers_; ������ͷ����Ϣ
};

#endif // HTTP_CONTEXT_H