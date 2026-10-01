cmake_minimum_required(VERSION 3.10)
project(file_analiz)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(file_analiz main.cpp
    scanfile/scanfile.h
    scanfile/scanfile.cpp)

target_include_directories(file_analiz PRIVATE includes)

target_link_libraries(file_analiz PRIVATE comdlg32)