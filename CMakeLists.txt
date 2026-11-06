cmake_minimum_required(VERSION 3.22)
project(ForesightTD1 VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

option(TD1_BUILD_TESTS  "Build td1-core tests" ON)
option(TD1_BUILD_PLUGIN "Build the JUCE plugin" ON)
option(TD1_ENABLE_CLAP  "Build CLAP via clap-juce-extensions (fetched from GitHub)" OFF)

add_subdirectory(core)

if(TD1_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(TD1_BUILD_PLUGIN)
  add_subdirectory(plugin)
endif()
