# Root CMakeLists for an ESP-IDF project.
# The build system lives in IDF_PATH; this just names the project.
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_ir_parking)
