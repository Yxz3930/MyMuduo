#include "myutils.h"
#include "Logger.h"

void ErrorIf(bool condition, const char *msg)
{
    if(condition)
        LOG_ERROR(msg);
    else
        return;
}

bool isFdClosed(int fd)
{
    return fcntl(fd, F_GETFD) == -1; // 失败说明 fd 无效 fd已经被关闭了
}
