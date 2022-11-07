cmake_minimum_required(VERSION 3.22)
project(sysy_compiler)

set(CMAKE_CXX_STANDARD 17)

#
# compile options
#

option(LOG_OUTPUT "show log info" OFF)
option(TEST_LEXER "test lexer using the lexer test file" OFF)
option(TEST_PARSER "test parser using test file" OFF)
option(TEST_COMPETITION "test all competition testcases" OFF)

#
# frontend part
#

set(FRONTEND src/frontend)

# flex & bison target
# https://cmake.org/cmake/help/latest/module/FindBISON.html
find_package(BISON)
bison_target(SysY_parser ${FRONTEND}/parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp
        DEFINES_FILE ${CMAKE_CURRENT_BINARY_DIR}/parser.h)
#        COMPILE_FLAGS "-Wcounterexamples")
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# find llvm package
# https://llvm.org/docs/CMake.html#embedding-llvm-in-your-project
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# https://stackoverflow.com/questions/61188470/what-is-a-correct-way-to-solve-undefined-reference-to-undefined-reference-to-ll
llvm_map_components_to_libnames(llvm_libs ${LLVM_TARGETS_TO_BUILD} support core irreader passes codegen mc mcparser option)

#
# compiler part
#

file(GLOB_RECURSE COMPILER_SRC src/*.cpp)
list(APPEND HEADERS src)
list(APPEND HEADERS src/frontend)
list(APPEND HEADERS src/passes)

if (LOG_OUTPUT)
    add_compile_definitions(CONF_LOG_OUTPUT)
endif ()

add_executable(sysy_compiler
        ${COMPILER_SRC}
        ${BISON_SysY_parser_OUTPUTS}
        )

target_include_directories(sysy_compiler PRIVATE ${HEADERS})

target_link_libraries(sysy_compiler ${llvm_libs})

#
# testing
#

enable_testing()

if (TEST_LEXER)
    function(add_lexer_test test_name file_name)
        add_test(NAME ${test_name} COMMAND
                sysy_compiler ${CMAKE_CURRENT_SOURCE_DIR}/test/lexer/${file_name}
                )
    endfunction()
    add_lexer_test(lexer_test_1 1.sy)
    add_lexer_test(lexer_test_2 2.sy)
endif ()

if (TEST_PARSER)
    function(add_parser_test test_name file_name)
        add_test(NAME ${test_name} COMMAND
                sysy_compiler ${CMAKE_CURRENT_SOURCE_DIR}/test/parser/${file_name}
                )
    endfunction()
    add_parser_test(parser_test_1 1.sy)
endif ()

if (TEST_COMPETITION)
    function(add_competition_test test_name file_name)
        add_test(NAME ${test_name} COMMAND
                sysy_compiler ${CMAKE_CURRENT_SOURCE_DIR}/test/competition/${file_name}
                )
    endfunction()

    function(add_competition_test_by_level level_name)
        file(GLOB competition_test_files
                ${CMAKE_CURRENT_SOURCE_DIR}/test/competition/${level_name}/*.sy
                )
        foreach (competition_test_file ${competition_test_files})
            get_filename_component(competition_test_name ${competition_test_file} NAME_WE)
            add_competition_test(
                    "${level_name}#${competition_test_name}"
                    ${level_name}/${competition_test_name}.sy
            )
        endforeach ()
    endfunction()

    add_competition_test_by_level(level1-1)
    add_competition_test_by_level(level1-2)
    add_competition_test_by_level(level2-1)
    add_competition_test_by_level(level2-2)
    add_competition_test_by_level(level2-3)
    add_competition_test_by_level(level2-4)
    add_competition_test_by_level(level2-5)
    add_competition_test_by_level(level2-6)

endif ()
