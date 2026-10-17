cmake_minimum_required(VERSION 3.20)
project(SoftRam VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(SoftRam WIN32
    src/main.cpp
    src/memory.cpp
    src/memory.h
)

target_compile_definitions(SoftRam PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(SoftRam PRIVATE psapi shell32 user32 gdi32 advapi32)

if(MSVC)
    target_compile_options(SoftRam PRIVATE /W4 /permissive-)
else()
    target_compile_options(SoftRam PRIVATE -Wall -Wextra)
    target_link_options(SoftRam PRIVATE -municode)
endif()
