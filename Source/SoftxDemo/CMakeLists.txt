cmake_minimum_required(VERSION 3.15)

project(SoftXDemo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(SoftXDemo SoftxDemo.cpp)

target_link_libraries(SoftXDemo PRIVATE SoftX)

target_include_directories(SoftXDemo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../AfterMath/include
)

target_compile_definitions(SoftXDemo
    PRIVATE
        _CONSOLE
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<CONFIG:Release>:NDEBUG>
        $<$<CXX_COMPILER_ID:MSVC>:$<$<EQUAL:${CMAKE_SIZEOF_VOID_P},4>:WIN32>>
)

if(MSVC)
    target_compile_options(SoftXDemo PRIVATE /W3 /sdl)
    target_compile_options(SoftXDemo PRIVATE /FA /FR)
endif()

set_target_properties(SoftXDemo PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
)

set_target_properties(SoftXDemo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/binaries"
)
