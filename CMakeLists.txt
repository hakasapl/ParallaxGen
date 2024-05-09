# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Project Vars
set(PROJECT_NAME "ParallaxGen")
set(PROJECT_URI "com.github.hakasapl.ParallaxGen")

# Initialize Project
project(${PROJECT_NAME})

# Find Files
file(GLOB_RECURSE SOURCES src/main/*.cpp src/main/*.h)
file(GLOB_RECURSE DATA data/*)

add_executable(${PROJECT_NAME} ${SOURCES})

# Add Packages
find_package(bsa REQUIRED CONFIG)
target_link_libraries(${PROJECT_NAME} PUBLIC bsa::bsa)
