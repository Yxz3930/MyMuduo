#ifndef UTILS_H
#define UTILS_H
#include <iostream>
#include <fcntl.h>
#include <string>
#include <sstream>

void ErrorIf(bool condition, const char* msg);

bool isFdClosed(int fd);

template<typename T>
std::string ptrToString(T* ptr) {
    std::ostringstream oss;
    oss << "0x" << std::hex << reinterpret_cast<uintptr_t>(ptr);
    return oss.str();
}

#endif // UTILS_H
