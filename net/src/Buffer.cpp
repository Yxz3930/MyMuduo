#include "Buffer.h"


void Buffer::retrieve(size_t len)
{
    assert(len <= readableBytes());
    if (len < readableBytes())
    {
        m_readerIndex += len;
    }
    else
    {
        retrieveAll();
    }
}

void Buffer::retrieveAll()
{
    m_readerIndex = prependSize;
    m_writerIndex = prependSize;
}

// 将可读数据全部取出并转为 string
std::string Buffer::retrieveAllAsString()
{
    return retrieveAsString(readableBytes());
}

// 取出 len 长度的数据并转为 string
std::string Buffer::retrieveAsString(size_t len)
{
    assert(len <= readableBytes());
    std::string result(peek(), len);
    retrieve(len);
    return result;
}

// 向缓冲区中添加数据
void Buffer::append(const char *data, size_t len)
{
    ensureWritableBytes(len);
    std::copy(data, data + len, beginWrite());
    m_writerIndex += len;
}

void Buffer::append(const std::string &str)
{
    append(str.data(), str.length());
}

// Buffer::readFd 的实现
ssize_t Buffer::readFd(int fd, int* savedErrno) {
    char extrabuf[65536]; // 栈上缓冲区，用于应急
    struct iovec vec[2];
    const size_t writable = writableBytes();
    
    vec[0].iov_base = beginWrite();
    vec[0].iov_len = writable;
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);
    
    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if (n < 0) {
        *savedErrno = errno;
    } else if (static_cast<size_t>(n) <= writable) {
        m_writerIndex += n;
    } else {
        m_writerIndex = m_buffer.size();
        append(extrabuf, n - writable);
    }
    return n;
}