#ifndef SYSY_COMPILER_LOG_H
#define SYSY_COMPILER_LOG_H

#include <iostream>
#include <string_view>

std::ostream &log(std::string_view module);

std::ostream &err(std::string_view module);

#endif //SYSY_COMPILER_LOG_H
