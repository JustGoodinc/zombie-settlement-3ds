cmake_minimum_required(VERSION 3.16)
project(StayingAlive CXX)
if(WIN32)
    enable_language(RC)
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")  # static CRT: no VC++ redistributable needed
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Shared gameplay core: identical sources are compiled for the 3DS by the Makefile.
file(GLOB CORE_SOURCES
    src/core/*.cpp src/gameplay/*.cpp src/world/*.cpp src/settlement/*.cpp src/ui/*.cpp)
add_library(zs_core STATIC ${CORE_SOURCES})
if(MSVC)
    target_compile_options(zs_core PUBLIC /W3 /utf-8 /D_CRT_SECURE_NO_WARNINGS)
endif()

if(WIN32)
    add_executable(StayingAlive WIN32 src/platform/windows/main_win.cpp src/platform/windows/app.rc)
    target_link_libraries(StayingAlive zs_core winmm user32 gdi32 shell32)
endif()

add_executable(zs_tests tests/tests.cpp)
target_link_libraries(zs_tests zs_core)

add_executable(zs_tool tools/zs_tool.cpp)
target_link_libraries(zs_tool zs_core)

enable_testing()
add_test(NAME gameplay_tests COMMAND zs_tests)
