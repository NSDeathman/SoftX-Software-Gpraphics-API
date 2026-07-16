cmake_minimum_required(VERSION 3.15)
project(SoftX LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

add_library(SoftX STATIC)

set(SOFTX_SOURCES
    src/CPUDetector.cpp
    src/Device.cpp
    src/DeviceContext.cpp
    src/DeviceContextDebug.cpp
    src/DeviceContextDraw.cpp
    src/FrameBuffer.cpp
    src/TextureRGBA32F.cpp
    src/OcclusionQuery.cpp
    src/QueryRasterizerAVX.cpp
    src/QueryRasterizerFactory.cpp
    src/QueryRasterizerScalar.cpp
    src/QueryRasterizerSSE.cpp
    src/RasterizerAVX.cpp
    src/RasterizerFactory.cpp
    src/RasterizerScalar.cpp
    src/RasterizerSSE.cpp
    src/Renderer.cpp
)

set(SOFTX_PRIVATE_HEADERS
    src/CPUDetector.h
    src/QueryRasterizerAVX.h
    src/QueryRasterizerFactory.h
    src/QueryRasterizerScalar.h
    src/QueryRasterizerSSE.h
    src/RasterizerAVX.h
    src/RasterizerCommon.h
    src/RasterizerFactory.h
    src/RasterizerScalar.h
    src/RasterizerSSE.h
    src/Renderer.h
    src/ThreadPool.h
    src/ThreadPoolManager.h
)

set(SOFTX_PUBLIC_HEADERS
    include/DepthBuffer.h
    include/DepthTextureView.h
    include/Device.h
    include/DeviceContext.h
    include/Exceptions.h
    include/FrameBuffer.h
    include/LibInternal.h
    include/OcclusionQuery.h
    include/QueryRasterizerInterface.h
    include/RasterizerInterface.h
    include/RenderTargetInterface.h
    include/RenderTargetTexture.h
    include/SoftX.h
    include/TextureRGBA32F.h
    include/TextureInterface.h
    include/ThirdPartyIncluding.h
    include/Types.h
)

target_sources(SoftX PRIVATE ${SOFTX_SOURCES} ${SOFTX_PRIVATE_HEADERS})
target_sources(SoftX PUBLIC ${SOFTX_PUBLIC_HEADERS})

target_include_directories(SoftX
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../AfterMath/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(SoftX
    PRIVATE
        SOFTX_IMPLEMENTATION
        _LIB
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<CONFIG:Release>:NDEBUG>
        $<$<CXX_COMPILER_ID:MSVC>:$<$<EQUAL:${CMAKE_SIZEOF_VOID_P},4>:WIN32>>
)

if(MSVC)
    add_compile_options(/W4 /sdl)
    add_compile_options(/fp:fast)
    target_compile_options(SoftX PRIVATE
        $<$<CONFIG:Release>:/GL /FR>
    )
endif()

set_target_properties(SoftX PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
    UNITY_BUILD ON
)

set_target_properties(SoftX PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Source/SoftX/lib/$<IF:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>,x64,x86>"
)

source_group("Source Files\\Core"          FILES src/Device.cpp src/DeviceContext.cpp src/DeviceContextDebug.cpp src/DeviceContextDraw.cpp src/Renderer.cpp src/OcclusionQuery.cpp)
source_group("Source Files\\Rasterizer"    FILES src/RasterizerAVX.cpp src/RasterizerFactory.cpp src/RasterizerScalar.cpp src/RasterizerSSE.cpp
                                                 src/QueryRasterizerAVX.cpp src/QueryRasterizerFactory.cpp src/QueryRasterizerScalar.cpp src/QueryRasterizerSSE.cpp)
source_group("Source Files\\Tools"         FILES src/CPUDetector.cpp)
source_group("Source Files\\Resources"     FILES src/FrameBuffer.cpp src/TextureRGBA32F.cpp)

source_group("Header Files\\Core"          FILES src/Renderer.h)
source_group("Header Files\\Rasterizer"    FILES src/RasterizerAVX.h src/RasterizerCommon.h src/RasterizerFactory.h src/RasterizerScalar.h src/RasterizerSSE.h
                                                 src/QueryRasterizerAVX.h src/QueryRasterizerFactory.h src/QueryRasterizerScalar.h src/QueryRasterizerSSE.h)
source_group("Header Files\\Tools"         FILES src/CPUDetector.h src/ThreadPool.h src/ThreadPoolManager.h)
source_group("Header Files\\API"           FILES include/SoftX.h include/Device.h include/DeviceContext.h include/OcclusionQuery.h include/Types.h
                                                 include/DepthBuffer.h include/DepthTextureView.h include/FrameBuffer.h include/Exceptions.h
                                                 include/RenderTargetInterface.h include/RenderTargetTexture.h include/TextureRGBA32F.h include/TextureInterface.h
                                                 include/LibInternal.h include/ThirdPartyIncluding.h include/QueryRasterizerInterface.h include/RasterizerInterface.h)
