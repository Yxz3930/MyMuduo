#ifndef SMART_POINTER_H
#define SMART_POINTER_H

#include "MemoryPool.h"
#include <iostream>
#include <string>
#include <memory> // ����ָ��
using namespace memory_pool;


// �Զ���ɾ����
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


//  shared_ptr shared_ptr����Ҫ����ɾ����
template <class T, class... Args>
std::shared_ptr<T> make_shared_from_pool(Args &&...args)
{
    // �����ڴ�
    void *addr = MemoryPool::allocate(sizeof(T));

    // ������� placement new
    T *ptr = new (addr) T(std::forward<Args>(args)...);

    // �Զ���ɾ���� ����ڴ��ʹ��
    auto deleter = [](T *p)
    {
        p->~T();
        MemoryPool::deallocate(p, sizeof(T));
    };

    // ����ָ���װ ����
    return std::shared_ptr<T>(ptr, deleter);
}

template <class T, class... Args>
std::unique_ptr<T, PoolDeleter<T>> make_unique_from_pool(Args &&...args)
{
    // �����ڴ�
    void *addr = MemoryPool::allocate(sizeof(T));

    // ������� placement new
    T *ptr = new (addr) T(std::forward<Args>(args)...);

    // // �Զ���ɾ���� ����ڴ��ʹ��
    // auto deleter = [](T *p)
    // {
    //     p->~T();
    //     MemoryPool::deallocate(p, sizeof(T));
    // };

    // ����ָ���װ ����
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