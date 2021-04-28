cmake_minimum_required(VERSION 3.19)
project(nes)

set(CMAKE_C_STANDARD 11)
set(BUILD_DIR build)

find_package(SDL2 REQUIRED)
file(GLOB SRC src/*.c src/mappers/*.c)

include_directories(src src/mappers ${SDL2_INCLUDE_DIRS})

add_executable(nes ${SRC})
target_link_libraries(nes ${SDL2_LIBRARIES})
target_link_libraries(nes m)
