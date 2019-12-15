cmake_minimum_required(VERSION 3.14)
find_package(PkgConfig)

project(blok C)

set(CMAKE_C_STANDARD 11)

pkg_check_modules(FUSE REQUIRED IMPORTED_TARGET fuse<3)

include_directories(include)
include_directories(/usr/include/fuse)

file(GLOB SOURCES "src/*.c")

add_executable(blok ${SOURCES})
target_link_libraries(blok PkgConfig::FUSE)

