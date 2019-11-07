cmake_minimum_required(VERSION 3.15)
find_package(PkgConfig)

project(blok C)


set(CMAKE_C_STANDARD 11)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_FILE_OFFSET_BITS=64 -lfuse -pthread")
#set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -D_FILE_OFFSET_BITS=64 -lfuse -pthread")

pkg_check_modules(FUSE REQUIRED IMPORTED_TARGET fuse<3)

include_directories(include)
include_directories(/usr/include/fuse)

file(GLOB SOURCES "src/*.c")

add_executable(blok ${SOURCES})
target_link_libraries(blok PkgConfig::FUSE)

