#ifndef SYSY_COMPILER_LOG_H
#define SYSY_COMPILER_LOG_H

#include <iostream>
#include <string_view>
#include <llvm/Support/raw_ostream.h>

std::ostream &log(std::string_view module);

std::ostream &err(std::string_view module);

llvm::raw_ostream &log_llvm();

#endif //SYSY_COMPILER_LOG_H
