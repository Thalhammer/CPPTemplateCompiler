cmake_minimum_required(VERSION 3.10)

project(CPPTemplateCompiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_definitions(-DPICOJSON_USE_INT64)
add_compile_options(-pie -fPIE)
add_compile_options(-Wl,-z,now -Wl,-z,relro)

add_compile_options(-Wall)
add_compile_options(-Wextra)
add_compile_options(-Werror)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weffc++" CACHE STRING "" FORCE)

option(BUILD_STATIC "Build statically" OFF)

# Enable Link-Time Optimization
if(NOT ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug"))
    include(CheckIPOSupported)
    check_ipo_supported(RESULT LTOAvailable)
endif()
if(LTOAvailable)
    message(STATUS "Link-time optimization enabled")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
if(BUILD_STATIC)
    message("Building static binary")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -static")
    set(CMAKE_LINK_SEARCH_START_STATIC 1)
    set(CMAKE_LINK_SEARCH_END_STATIC 1)
endif()


add_executable(cpptemplate
    ${CMAKE_CURRENT_SOURCE_DIR}/Generator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Parser.cpp
)
target_include_directories(cpptemplate
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

try_compile(HAS_FS "${CMAKE_BINARY_DIR}/temp" 
    "${CMAKE_SOURCE_DIR}/cmake/tests/has_fs.cpp" 
    CMAKE_FLAGS -DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_STANDARD_REQUIRED=ON
    LINK_LIBRARIES stdc++fs)
try_compile(HAS_FS_EXP "${CMAKE_BINARY_DIR}/temp" 
    "${CMAKE_SOURCE_DIR}/cmake/tests/has_fs_exp.cpp" 
    CMAKE_FLAGS -DCMAKE_CXX_STANDARD=17 -DCMAKE_CXX_STANDARD_REQUIRED=ON
    LINK_LIBRARIES stdc++fs)
if(HAS_FS)
    message(STATUS "Compiler has filesystem support")
    target_compile_definitions(cpptemplate PRIVATE -DWITH_FS)
elseif(HAS_FS_EXP)
    message(STATUS "Compiler has experimental filesystem support")
    target_compile_definitions(cpptemplate PRIVATE -DWITH_FS_EXP)
else()
    message(FATAL_ERROR "Compiler is missing filesystem capabilities")
endif(HAS_FS)

target_link_libraries(cpptemplate stdc++fs)

if (CMAKE_BUILD_TYPE STREQUAL Release)
    add_custom_command(TARGET cpptemplate POST_BUILD
            COMMENT "Strip CXX Executable cpptemplate"
            COMMAND ${CMAKE_STRIP} cpptemplate)
endif ()

install(TARGETS cpptemplate
        DESTINATION bin)