#include "FileUtility.h"
#include <string.h>
#include <iostream>

FileUtility::FileUtility(const char *filename)
    : m_writenBytes(0)
{
    this->m_file = fopen(filename, "a");
    if(!this->m_file)
        perror("file open fail");

    // 设置文件缓冲区 当这个缓冲区写满时会自动触发刷写（flush）到磁盘当中
    ::setbuffer(this->m_file, this->m_buffer, sizeof(this->m_buffer));
}

FileUtility::~FileUtility()
{
    if (this->m_file)
        fclose(this->m_file);
}

void FileUtility::append(const char *data, int len)
{
    size_t writen_bytes = 0;
    size_t remain = len - writen_bytes;
    while (writen_bytes < len)
    {
        off_t ret_len = this->write(data + writen_bytes, remain);
        // 这里应该判断返回值是否为零 因为存在写入的数据少于指定长度的情况
        if (ret_len == 0 || ret_len < remain)
        {
            if (ferror(this->m_file))
            {
                ::perror("FileUtility::append fwrite_unlocked error\n");
                clearerr(this->m_file); // 清楚文件指针的错误标志
            }
            break;
        }
        writen_bytes += ret_len;
    }
    this->m_writenBytes += writen_bytes;
}

void FileUtility::flush()
{
    if (this->m_file)
    {
        ::fflush(this->m_file);
        this->m_writenBytes = 0;
    }
}

size_t FileUtility::WritenBytes()
{
    return this->m_writenBytes;
}

size_t FileUtility::freeSize()
{
    return sizeof(this->m_buffer) - this->m_writenBytes;
}

size_t FileUtility::write(const char *data, int len)
{
    if (len <= 0) return 0;
    // fwrite 是线程安全的
    size_t ret_len = fwrite(data, 1, len, this->m_file);
    // fwrite 是线程不安全的
    // size_t ret_len = ::fwrite_unlocked(data, 1, len, this->m_file);
    return ret_len;
}
