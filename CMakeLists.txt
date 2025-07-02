cmake_minimum_required(VERSION 3.10)
project(camstream)

find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

add_executable(app main.cpp)
target_link_libraries(app ${OpenCV_LIBS})

