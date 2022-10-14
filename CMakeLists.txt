cmake_minimum_required(VERSION 3.22)
project(sysy_compiler)

set(CMAKE_CXX_STANDARD 17)

#
# frontend part
#

set(FRONTEND src/frontend)

# flex & bison target
find_package(FLEX)
find_package(BISON)
flex_target(SysY_lexer ${FRONTEND}/lexer.l ${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp
        DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/lexer.h)
bison_target(SysY_parser ${FRONTEND}/parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
        DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.h)
add_flex_bison_dependency(SysY_lexer SysY_parser)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

#
# compiler part
#

file(GLOB_RECURSE COMPILER_SRC src/*.cpp)
list(APPEND HEADERS src)
list(APPEND HEADERS src/frontend)

add_executable(sysy_compiler
        ${COMPILER_SRC}
        ${BISON_SysY_parser_OUTPUTS}
        ${FLEX_SysY_lexer_OUTPUTS}
)

target_include_directories(sysy_compiler PRIVATE ${HEADERS})

