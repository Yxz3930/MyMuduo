#ifndef BUFFER_H
#define BUFFER_H

#include <vector>
#include <string>
#include <algorithm> // for std::swap
#include <cassert>
#include <sys/uio.h> // for readv

/// @brief 一个专为网络I/O设计的高效缓冲区类
///
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size()
///
/// 这是模仿 muduo 设计的一个简化版 Buffer。
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

    // 返回缓冲区中可读数据的起始地址
    const char *peek() const { return beginRead(); }

    // 从缓冲区中取出 len 长度的数据 实际并没有取出来而是直接对索引进行操作进行覆盖了
    void retrieve(size_t len);

    // 这个也是 直接对索引操作 重新归位了
    void retrieveAll();

    // 取出 len 长度的数据并转为 string
    std::string retrieveAsString(size_t len);

    // 将可读数据全部取出并转为 string
    std::string retrieveAllAsString();

    // 向缓冲区中添加数据
    void append(const char *data, size_t len);

    void append(const std::string &str);

    // 从fd读取数据，这是最关键的性能提升点
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
            // 空间不足，需要扩容
            m_buffer.resize(m_writerIndex + len);
        }
        else
        {
            // 空间足够，只需移动数据，腾出后面的 writable 空间
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
