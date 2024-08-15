cmake_minimum_required(VERSION 3.16)
project(nes)

set(CMAKE_C_STANDARD 11)
set(BUILD_DIR build)

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

set(SDL_DIR ${CMAKE_SOURCE_DIR}/external/SDL)
set(SDL_ttf_DIR ${CMAKE_SOURCE_DIR}/external/SDL_ttf)
option(TTF_WITH_HARFBUZZ "use harfbuzz to improve text shaping" ON)

IF(ANDROID)
  # This is the built in NDK logging library
  find_library(log-lib log)
  option(BUILD_STATIC_LIBS "Build shared libraries" OFF)
  option(BUILD_SHARED_LIBS "Build shared libraries" ON)
ELSE()
  option(BUILD_STATIC_LIBS "Build shared libraries" ON)
  option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
ENDIF()

add_subdirectory(${SDL_DIR})
add_subdirectory(${SDL_ttf_DIR})

set (SRC
        src/apu.c
        src/controller.c
        src/cpu6502.c
        src/debugtools.c
        src/emulator.c
        src/gamepad.c
        src/genie.c
        src/gfx.c
        src/mmu.c
        src/ppu.c
        src/timers.c
        src/trace.c
        src/touchpad.c
        src/trace.c
        src/utils.c
        src/main.c

        src/mappers/axrom.c
        src/mappers/cnrom.c
        src/mappers/gnrom.c
        src/mappers/mapper.c
        src/mappers/mmc1.c
        src/mappers/uxrom.c
        src/mappers/mmc3.c
        src/mappers/colordreams.c
        src/nsf.c
        src/biquad.c
        src/biquad.h
        src/font.h
)
include_directories(src src/mappers vendor/include ${SDL_DIR}/include ${SDL_ttf_DIR})


IF(ANDROID)
  add_library(
          nes
          SHARED
          ${SRC}
  )
  target_link_libraries(
          nes
          SDL2
          SDL2_ttf
          ${log-lib}
  )
ELSE()
  IF(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    enable_language("RC")
    set (WIN32_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/app.rc)
    add_executable(nes WIN32 ${SRC} ${WIN32_RESOURCES})
  ELSE()
    add_executable(nes ${SRC})
  ENDIF ()
  target_link_libraries(nes SDL2::SDL2main SDL2::SDL2-static SDL2_ttf::SDL2_ttf-static)
ENDIF()

IF (NOT WIN32)
  target_link_libraries(nes m)
ELSE()
  target_link_libraries(nes winmm.lib)
ENDIF()
