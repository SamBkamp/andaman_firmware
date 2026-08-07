# The minimum CMake version required by ESP-IDF (commonly 3.16 or 3.22 depending on IDF version)
cmake_minimum_required(VERSION 4.0)

# Pull in core ESP-IDF build functionality and macros
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define your project name (creates your_project_name.bin and .elf)
project(andaman_firmware)
