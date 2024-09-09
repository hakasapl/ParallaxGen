cmake_minimum_required (VERSION 3.8)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# For MSVC
add_compile_options("/Zc:__cplusplus")

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

# Disables conflicting min/max macros in Windows.h
add_compile_definitions(NOMINMAX)
add_compile_definitions(WIN32_LEAN_AND_MEAN)
add_compile_definitions(NOSOCKETTYPE)
add_compile_definitions(_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)

# Project Vars
set(PROJECT_NAME "ParallaxGen")

# Initialize Project
set(PARALLAXGEN_VERSION 0.5.7)
project(${PROJECT_NAME} VERSION ${PARALLAXGEN_VERSION})
add_compile_definitions(PARALLAXGEN_VERSION="${PARALLAXGEN_VERSION}")

# Setup Folders
set(EXTRN_BUILD_DIR ${CMAKE_BINARY_DIR}/external/blds)
set(EXTRN_INSTALL_DIR ${CMAKE_BINARY_DIR}/external/installed)
file(MAKE_DIRECTORY ${EXTRN_BUILD_DIR})
file(MAKE_DIRECTORY ${EXTRN_INSTALL_DIR})
# Set CMAKE module path to include external install dir
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${EXTRN_INSTALL_DIR})

# NIFLY
message(STATUS "Install NIFLY external...")
execute_process(COMMAND ${CMAKE_COMMAND} -S ${CMAKE_SOURCE_DIR}/external/nifly -B ${EXTRN_BUILD_DIR}/nifly -DCMAKE_INSTALL_PREFIX=${EXTRN_INSTALL_DIR} -DBUILD_TESTING=OFF)
execute_process(COMMAND ${CMAKE_COMMAND} --build ${EXTRN_BUILD_DIR}/nifly --target install --config ${CMAKE_BUILD_TYPE})

# Submodules
add_subdirectory(ParallaxGen)
add_subdirectory(ParallaxGenLib)

# Install Global Things
# Copy Folders
file(COPY ${CMAKE_SOURCE_DIR}/ParallaxGen/assets DESTINATION ${CMAKE_BINARY_DIR}/)
file(COPY ${CMAKE_SOURCE_DIR}/ParallaxGen/cfg DESTINATION ${CMAKE_BINARY_DIR}/)
file(COPY ${CMAKE_SOURCE_DIR}/ParallaxGenLib/shaders DESTINATION ${CMAKE_BINARY_DIR}/)

# Copy assets to install
install(DIRECTORY ${CMAKE_BINARY_DIR}/assets DESTINATION . )
install(DIRECTORY ${CMAKE_BINARY_DIR}/cfg DESTINATION . )
install(DIRECTORY ${CMAKE_BINARY_DIR}/shaders DESTINATION . )
