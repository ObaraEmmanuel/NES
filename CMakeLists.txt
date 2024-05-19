cmake_minimum_required(VERSION 3.16)
project(nes)

set(CMAKE_C_STANDARD 11)
set(BUILD_DIR build)

IF (WIN32 OR ANDROID)
  set(VENDOR_DIR ${CMAKE_SOURCE_DIR}/vendor)
  set(SDL2_DIR ${CMAKE_SOURCE_DIR}/vendor/SDL2)
  set(SDL2_TTF_DIR ${CMAKE_SOURCE_DIR}/vendor/SDL2_ttf)
ENDIF()


IF(ANDROID)
  option(BUILD_SHARED_LIBS "Build shared libraries" ON)
  option(TTF_WITH_HARFBUZZ "use harfbuzz to improve text shaping" ON)
  add_subdirectory(${SDL2_DIR})
  add_subdirectory(${SDL2_TTF_DIR})
  # This is the built in NDK logging library
  find_library(log-lib log)
ELSE()
  find_package(SDL2 REQUIRED)
ENDIF()

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

)

IF(WIN32 OR ANDROID)
  include_directories(src src/mappers vendor/include ${SDL2_DIR})
ELSE()
  include_directories(src src/mappers ${SDL2_INCLUDE_DIRS})
ENDIF()


IF(ANDROID)
  add_library(
          nes
          SHARED
          ${SRC}
  )
  target_link_libraries(
          nes
          #SDL2Main
          SDL2_ttf
          SDL2
          ${log-lib}
  )
ELSE()
  add_executable(nes ${SRC})
  target_link_libraries(nes ${SDL2_LIBRARIES})
ENDIF()

IF (NOT WIN32)
  target_link_libraries(nes m)
ELSE()
  target_link_libraries(nes winmm.lib)
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
