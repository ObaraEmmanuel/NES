cmake_minimum_required(VERSION 3.16)
project(nes)

set(CMAKE_C_STANDARD 11)
set(BUILD_DIR build)

IF (WIN32)
  set(SDL2_DIR vendor/SDL2)
ENDIF()

find_package(SDL2 REQUIRED)
file(GLOB SRC src/*.c src/mappers/*.c)

include_directories(src src/mappers ${SDL2_INCLUDE_DIRS})

add_executable(nes ${SRC})
target_link_libraries(nes ${SDL2_LIBRARIES})

IF (NOT WIN32)
  target_link_libraries(nes m)
ENDIF()

if(WIN32)
  get_filename_component(SDL2_DLL_NAME "${SDL2_DLL}" NAME)
  add_custom_command(TARGET nes POST_BUILD
          MAIN_DEPENDENCY "${SDL2_DLL}"
          BYPRODUCTS "${SDL2_DLL_NAME}"
          COMMENT "Copying SDL2 DLL"
          COMMAND "${CMAKE_COMMAND}" -E copy "${SDL2_DLL}" "$<TARGET_FILE_DIR:nes>/${SDL2_DLL_NAME}"
          )
endif()
