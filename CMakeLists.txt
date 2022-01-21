cmake_minimum_required(VERSION 3.21)

project(example CXX)
set(CMAKE_CXX_STANDARD 14)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
set(CONAN_DISABLE_CHECK_COMPILER True)
conan_basic_setup()


add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PUBLIC main.cpp)

conan_target_link_libraries(${PROJECT_NAME})