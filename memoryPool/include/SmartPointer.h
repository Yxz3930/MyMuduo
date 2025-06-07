#ifndef SMART_POINTER_H
#define SMART_POINTER_H

#include "MemoryPool.h"
#include <iostream>
#include <string>
#include <memory> // ����ָ��
using namespace memory_pool;

// ����ڴ��������ָ��

template <class T, class... Args>
std::shared_ptr<T> make_shared_ptr_from_pool(Args &&...args)
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
std::unique_ptr<T> make_unique_ptr_from_pool(Args &&...args)
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
    return std::unique_ptr<T>(ptr, deleter);
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