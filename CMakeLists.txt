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
    src/common/Struct.cpp
    src/common/DDSTextureLoader.cpp
    src/common/FrameResource.cpp
    src/common/GameTimer.cpp
    src/common/Geometry.cpp
    src/common/GeometryGenerator.cpp
    src/common/MathHelper.cpp
    src/common/Util.cpp
    src/common/BlurFilter.cpp
    src/common/ModelIO.cpp
    src/common/Sky.cpp
)

set (COMMON_SOURCE_HEADS
    src/common/App.h
    src/common/D3DContext.h
    src/common/Util.h
    src/common/Struct.h
    src/common/BufferStruct.h
    src/common/GameTimer.h
    src/common/DDSTextureLoader.h
    src/common/FrameResource.h
    src/common/d3dx12.h
    src/common/Geometry.h
    src/common/GeometryGenerator.h
    src/common/MathHelper.h
    src/common/BlurFilter.h
    src/common/ModelIO.h
    src/common/ObjLoader.h
    src/common/Sky.h
)

set (CONTEXT_INTERFACE_SOURCE
    src/context/interface/BlendContextInterface.cpp
    src/context/interface/FrameResourceContextInterface.cpp
    src/context/interface/GeomtryContextInterface.cpp
    src/context/interface/LayoutInterface.cpp
    src/context/interface/TexContextInterface.cpp
    src/context/interface/ComputeShaderInterface.cpp
)
set (CONTEXT_INTERFACE_SOURCE_HEADS
    src/context/interface/BlendContextInterface.h
    src/context/interface/FrameResourceContextInterface.h
    src/context/interface/GeomtryContextInterface.h
    src/context/interface/LayoutInterface.h
    src/context/interface/ComputeShaderInterface.h
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
    src/context/ComputeShader.cpp
    src/context/TessllationContext.cpp
    src/context/DynamicIndexContext.cpp
    src/context/DynamicIndexComContex.cpp
    src/context/InstanceContext.cpp
    src/context/CubeMapContext.cpp
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
    src/context/WavesContext.h
    src/context/GSContext.h
    src/context/ComputeShader.h
    src/context/TessllationContext.h
    src/context/DynamicIndexContext.h
    src/context/DynamicIndexComContex.h
    src/context/InstanceContext.h
    src/context/CubeMapContext.h
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
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
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

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "${PROJECT_NAME}"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/$<PLATFORM_ID>/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib/$<PLATFORM_ID>/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib/$<PLATFORM_ID>/$<CONFIG>"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    INTERMEDIATE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/obj/$<PLATFORM_ID>/$<CONFIG>/${PROJECT_NAME}"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 14           
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
