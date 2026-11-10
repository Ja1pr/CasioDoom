# CasioDoom - DOOM for the CASIO fx-9860GIII (gint / fxSDK add-in)
#
# Build with:   fxsdk build-fx
# The fxSDK provides the toolchain file and the CMake modules.

cmake_minimum_required(VERSION 3.15)
project(CasioDoom C)

include(GenerateG1A)
find_package(Gint 2.9 REQUIRED)

file(GLOB DOOM_SOURCES CONFIGURE_DEPENDS src/*.c)

add_executable(casiodoom ${DOOM_SOURCES} src/fx/i_fx.c)

target_compile_options(casiodoom PRIVATE
  -Os -std=gnu11 -Wall
  -Wno-unused-but-set-variable -Wno-parentheses -Wno-unused-variable
  -Wno-address -Wno-maybe-uninitialized -Wno-dangling-pointer
  -fno-strict-aliasing -ffunction-sections -fdata-sections)
target_link_options(casiodoom PRIVATE -Wl,--gc-sections)
target_link_libraries(casiodoom Gint::Gint)

if("${FXSDK_PLATFORM_LONG}" STREQUAL fx9860G)
  generate_g1a(TARGET casiodoom OUTPUT "CasioDoom.g1a"
    NAME "DOOM" ICON assets/icon.png)
else()
  message(FATAL_ERROR "CasioDoom only targets the fx-9860G family: "
    "use 'fxsdk build-fx'")
endif()
