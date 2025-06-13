#ifndef BUFFER_H
#define BUFFER_H

#include <vector>
#include <string>
#include <algorithm> // for std::swap
#include <cassert>
#include <sys/uio.h> // for readv

/// @brief һ��רΪ����I/O��Ƶĸ�Ч��������
///
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size()
///
/// ����ģ�� muduo ��Ƶ�һ���򻯰� Buffer��
class Buffer
{
public:
    static const size_t prependSize = 8;
    static const size_t initialSize = 1024;

    explicit Buffer(size_t _initialSize = initialSize)
        : m_buffer(prependSize + _initialSize),
          m_readerIndex(prependSize),
          m_writerIndex(prependSize)
    {
    }

    size_t readableBytes() const { return m_writerIndex - m_readerIndex; }
    size_t writableBytes() const { return m_buffer.size() - m_writerIndex; }
    size_t prependableBytes() const { return m_readerIndex; }

    // ���ػ������пɶ����ݵ���ʼ��ַ
    const char *peek() const { return beginRead(); }

    // �ӻ�������ȡ�� len ���ȵ����� ʵ�ʲ�û��ȡ��������ֱ�Ӷ��������в������и�����
    void retrieve(size_t len);

    // ���Ҳ�� ֱ�Ӷ��������� ���¹�λ��
    void retrieveAll();

    // ȡ�� len ���ȵ����ݲ�תΪ string
    std::string retrieveAsString(size_t len);

    // ���ɶ�����ȫ��ȡ����תΪ string
    std::string retrieveAllAsString();

    // �򻺳������������
    void append(const char *data, size_t len);

    void append(const std::string &str);

    // ��fd��ȡ���ݣ�������ؼ�������������
    ssize_t readFd(int fd, int *savedErrno);

private:
    char *begin() { return &*m_buffer.begin(); }
    const char *begin() const { return &*m_buffer.begin(); }

    char* beginRead() { return begin() + m_readerIndex; }
    const char* beginRead() const { return begin() + m_readerIndex; }

    char *beginWrite() { return begin() + m_writerIndex; }
    const char *beginWrite() const { return begin() + m_writerIndex; }

    void ensureWritableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len);
        }
        assert(writableBytes() >= len);
    }

    void makeSpace(size_t len)
    {
        if (writableBytes() + prependableBytes() < len + prependSize)
        {
            // �ռ䲻�㣬��Ҫ����
            m_buffer.resize(m_writerIndex + len);
        }
        else
        {
            // �ռ��㹻��ֻ���ƶ����ݣ��ڳ������ writable �ռ�
            size_t readable = readableBytes();
            std::copy(begin() + m_readerIndex,
                      begin() + m_writerIndex,
                      begin() + prependSize);
            m_readerIndex = prependSize;
            m_writerIndex = m_readerIndex + readable;
            assert(readable == readableBytes());
        }
    }

private:
    std::vector<char> m_buffer;
    size_t m_readerIndex;
    size_t m_writerIndex;
};

#endif // BUFFER_H
