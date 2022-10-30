#include <utility>
#include <llvm/Support/JSON.h>
#include "log.h"
#include "AST.h"

namespace AST {

    Base *root;

    void show() {
        llvm::json::Value json = std::move(root->toJSON());
        log("AST") << "show AST:" << std::endl <<
                   llvm::formatv("{0:2}", json).str() << std::endl;
    }
}