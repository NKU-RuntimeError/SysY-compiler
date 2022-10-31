#include "IR.h"
#include "log.h"

namespace IR {
    Context context;

    void show() {
        log("IR") << "show IR" << std::endl;
        context.module.print(llvm::errs(), nullptr);
    }
}
