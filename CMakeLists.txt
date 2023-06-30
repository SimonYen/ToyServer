cmake_minimum_required(VERSION 3.0)
#设置c++标准
set(CMAKE_CXX_STANDARD 11)

project(toyserver)

#添加源文件目录
aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC)
#添加头文件目录
include_directories(${PROJECT_SOURCE_DIR}/include)
#设置可执行程序存放的目录
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

add_executable(server main.cpp ${SRC})