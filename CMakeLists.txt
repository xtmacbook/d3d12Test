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


set (COMMON_SOURCE 
    src/common/App.cpp
    src/common/D3DContext.cpp
    src/common/Data.cpp
    src/common/DDSTextureLoader.cpp
    src/common/FrameResource.cpp
    src/common/GameTimer.cpp
    src/common/Geometry.cpp
    src/common/GeometryGenerator.cpp
    src/common/MathHelper.cpp
    src/common/Util.cpp
)

set (COMMON_SOURCE_HEADS
    src/common/App.h
    src/common/D3DContext.h
    src/common/Util.h
    src/common/Data.h
    src/common/GameTimer.h
    src/common/DDSTextureLoader.h
    src/common/FrameResource.h
    src/common/d3dx12.h
    src/common/Geometry.h
    src/common/GeometryGenerator.h
    src/common/MathHelper.h
)

set (CONTEXT_INTERFACE_SOURCE
    src/context/interface/BlendContextInterface.cpp
    src/context/interface/FrameResourceContextInterface.cpp
    src/context/interface/GeomtryContextInterface.cpp
    src/context/interface/LayoutInterface.cpp
    src/context/interface/TexContextInterface.cpp
)
set (CONTEXT_INTERFACE_SOURCE_HEADS
    src/context/interface/BlendContextInterface.h
    src/context/interface/FrameResourceContextInterface.h
    src/context/interface/GeomtryContextInterface.h
    src/context/interface/LayoutInterface.h
)

set (CONTEXT_SOURCE
    src/context/BlendContext.cpp
    src/context/ComContext.cpp
    src/context/FrameResourceContext.cpp
    src/context/MainD3dContext.cpp
    src/context/MirrorWithStencil.cpp
    src/context/TestFrameResourceContext.cpp
    src/context/TexContext.cpp
    src/context/Waves.cpp
    src/context/WavesContext.cpp
    src/context/GSContext.cpp
)
set (CONTEXT_SOURCE_HEADS
    src/context/BlendContext.h
    src/context/ComContext.h
    src/context/FrameResourceContext.h
    src/context/MainD3dContext.h
    src/context/MirrorWithStencil.h
    src/context/TestFrameResourceContext.h
    src/context/TexContext.h
    src/context/Waves.h
    src/context/WavesContext.cpp
    src/context/GSContext.h
)


source_group ("Source Files/common" FILES ${COMMON_SOURCE})
source_group ("Header Files/common" FILES ${COMMON_SOURCE_HEADS})  

source_group ("Source Files/context/interface" FILES ${CONTEXT_INTERFACE_SOURCE})
source_group ("Header Files/context/interface" FILES ${CONTEXT_INTERFACE_SOURCE_HEADS})

source_group ("Source Files/context" FILES ${CONTEXT_SOURCE})
source_group ("Header Files/context" FILES ${CONTEXT_SOURCE_HEADS})

INCLUDE_DIRECTORIES(include)

set (RESOURCES
)

add_executable(  
        ${PROJECT_NAME}
        WIN32
        ${COMMON_SOURCE}
        ${COMMON_SOURCE_HEADS}
        ${CONTEXT_INTERFACE_SOURCE}
        ${CONTEXT_INTERFACE_SOURCE_HEADS}
        ${CONTEXT_SOURCE}
        ${CONTEXT_SOURCE_HEADS}
        src/osgEarth.cpp
        src/MainApp.cpp
        
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    d3d11.lib d3d12.lib dxgi.lib d3dcompiler.lib dxguid.lib
    user32.lib gdi32.lib kernel32.lib
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
