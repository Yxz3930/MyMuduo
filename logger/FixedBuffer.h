#include <memory>
#include <stddef.h>
#include <cstring>

class FixedBuffer
{
public:
    explicit FixedBuffer(size_t size) : m_data(new char[size]), m_capacity(size), m_len(0) {}

    bool append(const char *data, size_t len)
    {
        if (available() >= len)
        {
            memcpy(m_data.get() + m_len, data, len);
            m_len += len;
            return true;
        }
        return false;
    }

    const char *data() const { return m_data.get(); }
    size_t length() const { return m_len; }
    void reset() { m_len = 0; }
    size_t available() const { return m_capacity - m_len; }

    bool empty() const { return m_len == 0; }

private:
    std::unique_ptr<char[]> m_data;
    size_t m_capacity;
    size_t m_len;
};
