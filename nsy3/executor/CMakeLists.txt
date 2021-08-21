cmake_minimum_required(VERSION 2.8)
project(nsy3executor)

find_package(docopt REQUIRED)

include(CheckCXXCompilerFlag)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# http://stackoverflow.com/a/33266748/3946766
function(enable_cxx_compiler_flag_if_supported flag)
    string(FIND "${CMAKE_CXX_FLAGS}" "${flag}" flag_already_set)
    if(flag_already_set EQUAL -1)
        check_cxx_compiler_flag("${flag}" flag_supported)
        if(flag_supported)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}" PARENT_SCOPE)
        endif()
    endif()
endfunction()

enable_cxx_compiler_flag_if_supported("-W")
enable_cxx_compiler_flag_if_supported("-Wall")
enable_cxx_compiler_flag_if_supported("-Wextra")
enable_cxx_compiler_flag_if_supported("-pedantic")
enable_cxx_compiler_flag_if_supported("-fdiagnostics-color=always")

set(PROJECT_FILES
        src/object.cpp
        src/thunk.cpp
        src/bytecode.cpp
        src/frame.cpp
        src/serialisation.cpp
        src/functionutils.cpp
        src/builtins.cpp
        src/executionengine.cpp
        src/exception.cpp
        src/main.cpp
)

add_executable(executor ${PROJECT_FILES})
target_link_libraries(executor docopt)

add_executable(executor_coverage ${PROJECT_FILES})
#target_compile_definitions(executor_coverage PRIVATE COVERAGE)
target_compile_options(executor_coverage PRIVATE
    "-fprofile-arcs"
    "-ftest-coverage"
    "-fno-elide-constructors"
    "-fno-inline"
    "-O0"
    "-g"
    "-fPIC"
    "-fno-default-inline"
    "--coverage"
    "-lgcov")
target_link_libraries(executor_coverage docopt gcov)
