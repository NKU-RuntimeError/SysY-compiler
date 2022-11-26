#include <vector>
#include <functional>

namespace Memory {

    namespace Detail {
        std::vector<std::function<void()>> destructors;
    }

    using namespace Detail;

    void freeAll() {
        for (const auto &destructor: destructors) {
            destructor();
        }
        destructors.clear();
    }
}