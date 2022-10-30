#include <vector>
#include <functional>

namespace Memory {
    std::vector<std::function<void()>> destructors;

    void freeAll() {
        for (const auto &destructor: destructors) {
            destructor();
        }
        destructors.clear();
    }
}