cmake_minimum_required(VERSION 3.20)

set(PROJECT_NAME osgEarth)

project(${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 17)

add_compile_definitions(_UNICODE UNICODE)

# Uncomment the following line to add timeout to wait so that the
# registry handle can be released.  Not really needed and causes
# a very tiny bit of extra CPU to keep checking for exit condition.
#
#add_compile_definitions(_TIDY_TIMEOUT)

add_link_options(
    /SUBSYSTEM:WINDOWS #指定程序为 Windows 桌面窗口程序
    /DYNAMICBASE 
    Version.lib
    Comctl32.lib
    /MACHINE:X64
)


INCLUDE_DIRECTORIES(include)

add_executable(
        ${PROJECT_NAME} 
        src/winMain.cpp
        src/app.h
        src/app.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE) 

# 1. 设置目标文件名（对应右边的 $(ProjectName)）
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "${PROJECT_NAME}"
)

# 2. 设置输出目录（对应右边的 $(SolutionDir)$(Platform)\$(Configuration)\）
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/$<PLATFORM_ID>/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib/$<PLATFORM_ID>/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib/$<PLATFORM_ID>/$<CONFIG>"
)

# 3. 设置中间目录（对应右边的 $(Platform)$(Configuration)\）
set_target_properties(${PROJECT_NAME} PROPERTIES
    INTERMEDIATE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/obj/$<PLATFORM_ID>/$<CONFIG>/${PROJECT_NAME}"
)

# 4. 设置 C++ 语言标准（比如 C++14/17，对应右边的默认 ISO C++14）
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 14          # 改成 17 对应 C++17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
