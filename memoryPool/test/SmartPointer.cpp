#include "SmartPointer.h"
using namespace memory_pool;



int main()
{

    std::shared_ptr<Student> ptr1 = make_shared_ptr_from_pool<Student>();
    std::cout << "name: " << ptr1->name << ", score: " << ptr1->score << std::endl;

    std::shared_ptr<Student> ptr2 = make_shared_ptr_from_pool<Student>("abc", 999);
    std::cout << "name: " << ptr2->name << ", score: " << ptr2->score << std::endl;

    std::cout << ptr1.use_count() << std::endl;
    std::cout << ptr2.use_count() << std::endl;

    ptr1.reset(new Student("hhh", 1));
    std::cout << "name: " << ptr1->name << ", score: " << ptr1->score << std::endl;

    ptr1 = ptr2;
    std::cout << ptr1.use_count() << std::endl;
    std::cout << ptr2.use_count() << std::endl;

    return 0;
}