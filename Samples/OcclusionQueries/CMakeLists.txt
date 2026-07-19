cmake_minimum_required(VERSION 3.15)
project(OcclusionQueries LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)

add_executable(OcclusionQueries WIN32 main.cpp)

target_link_libraries(OcclusionQueries PRIVATE SoftX)

target_include_directories(OcclusionQueries
    PRIVATE
        ${CMAKE_SOURCE_DIR}/Source/AfterMath/include
)

set_target_properties(OcclusionQueries PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/$<CONFIG>"
)
