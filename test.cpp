#include <algorithm>
#include <atomic>
#include <iostream>

#ifdef NEW_DELETE

void* operator new(size_t s) {
    std::cout << __func__ << std::endl;
    return malloc(s);
}

void* operator new[](size_t s) {
    std::cout << __func__ << std::endl;
    return malloc(s);
}

void operator delete(void* ptr) {
    std::cout << __func__ << std::endl;
}

void operator delete[](void* ptr, size_t s) {
    std::cout << __func__ << std::endl;
}

#endif

class Foo {
 public:
    Foo() = default;

#ifdef NEW_DELETE

    void* operator new(size_t s) {
        std::cout << "Foo::" << __func__ << std::endl;
        return malloc(s);
    }

    void* operator new[](size_t s) {
        std::cout << "Foo::" << __func__ << std::endl;
        return malloc(s);
    }

    void operator delete(void* ptr) {
        std::cout << "Foo::" << __func__ << std::endl;
    }

    void operator delete[](void* ptr, size_t s) {
        std::cout << "Foo::" << __func__ << std::endl;
    }

#endif

 private:
    std::atomic<int> i_{0};
};

int main() {
    int* a = new int[10]{};
    delete a;

    auto* b = new Foo[10]{};
    delete[] b;
}