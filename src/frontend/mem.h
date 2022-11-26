#ifndef SYSY_COMPILER_FRONTEND_MEM_H
#define SYSY_COMPILER_FRONTEND_MEM_H

#include <vector>
#include <functional>

namespace Memory {

    namespace Detail {
        // 存储在mem.cpp中
        // 假设该结构不会被并发访问，因此不使用同步机制
        extern std::vector<std::function<void()>> destructors;
    }

    // 在创建AST节点时，使用该函数，通过完美转发，将参数传递给构造函数
    template<typename Ty, typename... Args>
    Ty *make(Args &&... args) {
        Ty *ptr = new Ty(std::forward<Args>(args)...);
        Detail::destructors.emplace_back([ptr] { delete ptr; });
        return ptr;
    }

    // 在整个AST不再使用时，调用该函数，释放AST占用的内存
    void freeAll();
}

#endif //SYSY_COMPILER_FRONTEND_MEM_H
