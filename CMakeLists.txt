cmake_minimum_required(VERSION 3.22)
project(sysy_compiler)

set(CMAKE_CXX_STANDARD 17)

#
# compile options
#

option(LOG_OUTPUT "show log info" OFF)

#
# frontend part
#

set(FRONTEND src/frontend)

# flex & bison target
# https://cmake.org/cmake/help/latest/module/FindBISON.html
find_package(BISON)
bison_target(SysY_parser ${FRONTEND}/parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
        DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# find llvm package
# https://llvm.org/docs/CMake.html#embedding-llvm-in-your-project
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

llvm_map_components_to_libnames(llvm_libs support core irreader)

#
# compiler part
#

file(GLOB_RECURSE COMPILER_SRC src/*.cpp)
list(APPEND HEADERS src)
list(APPEND HEADERS src/frontend)

if(LOG_OUTPUT)
    add_compile_definitions(CONF_LOG_OUTPUT)
endif()

add_executable(sysy_compiler
        ${COMPILER_SRC}
        ${BISON_SysY_parser_OUTPUTS}
)

target_include_directories(sysy_compiler PRIVATE ${HEADERS})

target_link_libraries(sysy_compiler ${llvm_libs})

