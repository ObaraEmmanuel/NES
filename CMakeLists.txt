cmake_minimum_required(VERSION 3.16)
project(nes)

set(CMAKE_C_STANDARD 11)
set(BUILD_DIR build)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_definitions(DEBUGGING_ENABLED)
endif()

# General configuration
option(DISABLE_AUDIO "Disable audio output" OFF)
option(UNLOCK_FPS "Removes FPS caps to tv standard e.g. 60Hz for NTSC" OFF)

if (DISABLE_AUDIO)
  add_compile_definitions(nes PRIVATE DISABLE_AUDIO)
endif ()

if (UNLOCK_FPS)
  add_compile_definitions(nes PRIVATE DISABLE_AUDIO)
  add_compile_definitions(nes PRIVATE UNLOCK_FPS)
endif ()

message(STATUS "NES was configured as follows:")
message(STATUS "  Disable audio:  ${DISABLE_AUDIO}")
message(STATUS "  Unlock FPS:     ${UNLOCK_FPS}")

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

set(SDL_DIR ${CMAKE_SOURCE_DIR}/external/SDL)
set(SDL_ttf_DIR ${CMAKE_SOURCE_DIR}/external/SDL_ttf)
option(TTF_WITH_HARFBUZZ "use harfbuzz to improve text shaping" ON)

IF(ANDROID)
  # This is the built in NDK logging library
  find_library(log-lib log)
  option(BUILD_STATIC_LIBS "Build static libraries" OFF)
  option(BUILD_SHARED_LIBS "Build shared libraries" ON)
ELSE()
  option(BUILD_STATIC_LIBS "Build static libraries" ON)
  option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
ENDIF()

add_subdirectory(${SDL_DIR})
add_subdirectory(${SDL_ttf_DIR})

set (SRC
        src/apu.c
        src/controller.c
        src/cpu6502.c
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
        src/nsf.c
        src/nsf_gfx.c
        src/biquad.c
        src/mixer.c

        src/mappers/axrom.c
        src/mappers/cnrom.c
        src/mappers/gnrom.c
        src/mappers/mapper.c
        src/mappers/mapper.tmpl.c
        src/mappers/mmc1.c
        src/mappers/uxrom.c
        src/mappers/mmc3.c
        src/mappers/colordreams.c
        src/mappers/vrc1.c
        src/mappers/mapper185.c
        src/mappers/cprom.c
)
include_directories(
        src
        src/mappers
        ${SDL_DIR}/include/SDL3
        ${SDL_DIR}/include
        ${SDL_ttf_DIR}/include/SDL3_ttf
)


IF(ANDROID)
  add_library(
          nes
          SHARED
          ${SRC}
  )
  target_link_libraries(
          nes
          SDL3::SDL3-shared
          SDL3_ttf::SDL3_ttf-shared
          ${log-lib}
  )
  # O3 for release on all ABIs
  target_compile_options(nes PRIVATE $<$<CONFIG:Release>:-O3>)

  if(ANDROID_ABI STREQUAL "armeabi-v7a")
    # NEON is not on by default at minSdk 21
    target_compile_options(nes PRIVATE -mfpu=neon)
    # use thin LTO to work around regression in NDK >= 27
    target_compile_options(nes PRIVATE $<$<CONFIG:Release>:-flto=thin;-marm>)
    target_link_options(nes PRIVATE $<$<CONFIG:Release>:-flto=thin>)
  else()
    # LTO for release, everything except armeabi-v7a
    target_compile_options(nes PRIVATE $<$<CONFIG:Release>:-flto>)
    target_link_options(nes PRIVATE $<$<CONFIG:Release>:-flto>)
  endif()

ELSE()
  IF(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    enable_language("RC")
    set (WIN32_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/app.rc)
    add_executable(nes WIN32 ${SRC} ${WIN32_RESOURCES})
  ELSE()
    add_executable(nes ${SRC})
  ENDIF ()
  target_link_libraries(nes SDL3::SDL3-static SDL3_ttf::SDL3_ttf-static)
ENDIF()

IF (NOT WIN32)
  target_link_libraries(nes m)
ELSE()
  target_link_libraries(nes winmm.lib)
ENDIF()

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported)

if(ipo_supported)
  set_target_properties(nes PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()
