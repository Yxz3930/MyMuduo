#ifndef SMART_POINTER_H
#define SMART_POINTER_H

#include "MemoryPool.h"
#include <iostream>
#include <string>
#include <memory> // 智能指针
using namespace memory_pool;


// 自定义删除器
template<typename T>
class PoolDeleter
{
public:
    void operator()(T* p)
    {
        p->~T();
        MemoryPool::deallocate(p, sizeof(T));
    }
};


//  shared_ptr shared_ptr不需要传入删除器
template <class T, class... Args>
std::shared_ptr<T> make_shared_from_pool(Args &&...args)
{
    // 分配内存
    void *addr = MemoryPool::allocate(sizeof(T));

    // 构造对象 placement new
    T *ptr = new (addr) T(std::forward<Args>(args)...);

    // 自定义删除器 结合内存池使用
    auto deleter = [](T *p)
    {
        p->~T();
        MemoryPool::deallocate(p, sizeof(T));
    };

    // 智能指针包装 返回
    return std::shared_ptr<T>(ptr, deleter);
}

template <class T, class... Args>
std::unique_ptr<T, PoolDeleter<T>> make_unique_from_pool(Args &&...args)
{
    // 分配内存
    void *addr = MemoryPool::allocate(sizeof(T));

    // 构造对象 placement new
    T *ptr = new (addr) T(std::forward<Args>(args)...);

    // // 自定义删除器 结合内存池使用
    // auto deleter = [](T *p)
    // {
    //     p->~T();
    //     MemoryPool::deallocate(p, sizeof(T));
    // };

    // 智能指针包装 返回
    return std::unique_ptr<T, PoolDeleter<T>>(ptr);
}



struct Student
{
    std::string name;
    double score;
    Student() : name(""), score(0.0) {}
    Student(std::string _name, double _score) : name(_name), score(_score) {}
    ~Student()
    {
        name = "";
        score = 0.0;
    }
};

#endif // SMART_POINTER_H