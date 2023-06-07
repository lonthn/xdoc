cmake_minimum_required(VERSION 3.22)

project(xdoc)

set(TARGET_NAME ${PROJECT_NAME})
set(CMAKE_CXX_STANDARD 11)

add_library(${TARGET_NAME} document.cpp)
