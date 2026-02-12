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
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME nucleo_l476RG_sx1280)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# ------------------------------------------------------------------
# SX1280 sources
# ------------------------------------------------------------------
set(SX1280_SRC
    radio_driver/sx1280/sx1280.c
    radio_driver/sx1280/sx1280-hal.c
    radio_driver/hw-hal/src/hw.c
    radio_driver/hw-hal/src/hw-gpio.c
    radio_driver/hw-hal/src/hw-spi.c
    # radio_driver/hw-hal/src/hw-rtc.c
    radio_driver/hw-hal/src/hw-lptim.c
    # radio_driver/hw-hal/src/hw-usart.c
    radio_driver/hw-hal/src/utilities.c
    radio_driver/rf_switch/rf_switch_cg2214m6.c
)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME}
    Core/Src/main.c
    ${SX1280_SRC}
)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    radio_app/main_pingpong.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    radio_driver/sx1280
    radio_driver/sx1280/rangingCorrection
    radio_driver/hw-hal/inc
    radio_driver/hw-hal/inc/boards
    radio_driver/rf_switch

    # Core/Inc
    # Core/Src
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
    NUCLEO_L476RG
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)
