cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME stm32Demo)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# LVGL配置
add_definitions(-DLV_CONF_INCLUDE_SIMPLE)

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Collect LVGL source files (排除PC平台驱动、第三方库、非builtin stdlib)
file(GLOB_RECURSE LVGL_SOURCES "ThirdParty/LVGL/lvgl/src/*.c")
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/drivers/.*")
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/libs/(barcode|bmp|ffmpeg|freetype|frogfs|fsdrv|FT800-FT813|gif|gltf|gstreamer|libjpeg_turbo|libpng|libwebp|lodepng|lz4|nanovg|qrcode|rle|rlottie|svg|thorvg|tiny_ttf|tjpgd|vg_lite_driver)/.*")
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/osal/(cmsis_rtos2|freertos|linux|mqx|pthread|rtthread|sdl2|windows)\..*")
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/debugging/.*")
list(FILTER LVGL_SOURCES EXCLUDE REGEX ".*/stdlib/(clib|freertos|micropython|rtthread|custom)/.*")

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    Core/Src/lcd.c
    Core/Src/touch.c
    Core/Src/IS62WV512.c
    Core/Src/lv_port_disp.c
    Core/Src/lv_port_indev.c
    src/ui/ui_main.c
    src/font/my_siyuan_font.c
    ${LVGL_SOURCES}
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    Core/Inc
    ThirdParty/LVGL
    ThirdParty/LVGL/lvgl
    ThirdParty/LVGL/lvgl/src
    src/ui
    src/font
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
