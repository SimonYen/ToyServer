cmake_minimum_required(VERSION 3.0)
#设置c++标准
set(CMAKE_CXX_STANDARD 17)

project(toyserver)

#添加源文件目录
aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC)
#添加头文件目录
include_directories(${PROJECT_SOURCE_DIR}/include)
#设置可执行程序存放的目录
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR})#与代码保持一致

#找到mysql库
include(FindPkgConfig)
pkg_check_modules(LIBMYSQLCLIENT REQUIRED mysqlclient)

add_executable(server main.cpp ${SRC})
target_link_libraries(server PRIVATE mysqlclient)