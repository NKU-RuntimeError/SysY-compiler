#include "IR.h"
#include "log.h"

namespace IR {
    Context ctx;

    void show() {
        log("IR") << "show IR" << std::endl;
        ctx.module.print(log_llvm(), nullptr);
    }
}
