#ifndef SYSY_COMPILER_FRONTEND_TYPE_H
#define SYSY_COMPILER_FRONTEND_TYPE_H

#include <optional>
#include <llvm/IR/Value.h>

enum class Typename {
    VOID,

    // 按照优先顺序排列，序号越高，优先级越大，便于进行类型转换
    BOOL,
    INT,
    FLOAT,
};

namespace TypeSystem {
    Typename fromValue(llvm::Value *value);

    llvm::Value *cast(llvm::Value *value, Typename wantType);

    llvm::Type *get(Typename type);

    llvm::Type *get(Typename type, const std::vector<std::optional<int>> &size);
}

#endif //SYSY_COMPILER_FRONTEND_TYPE_H
