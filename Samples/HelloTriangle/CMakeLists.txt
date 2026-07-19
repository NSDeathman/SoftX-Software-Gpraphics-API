cmake_minimum_required(VERSION 3.15)
project(HelloTriangle LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

add_executable(HelloTriangle WIN32 main.cpp)

target_link_libraries(HelloTriangle PRIVATE SoftX)

target_include_directories(HelloTriangle
    PRIVATE
        ${CMAKE_SOURCE_DIR}/Source/AfterMath/include
)

set_target_properties(HelloTriangle PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>"
)
