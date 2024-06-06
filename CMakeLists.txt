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
set(PARALLAXGEN_VERSION 0.2.4)
project(${PROJECT_NAME} VERSION ${PARALLAXGEN_VERSION})

# Define preprocessor macro with the version number
add_compile_definitions(PARALLAXGEN_VERSION="${PARALLAXGEN_VERSION}")

# Include other Directories
include_directories("include")

# Disables conflicting min/max macros in Windows.h
add_compile_definitions(NOMINMAX)
add_compile_definitions(WIN32_LEAN_AND_MEAN)
add_compile_definitions(NOSOCKETTYPE)

# Add Executables
set(HEADERS
    "include/BethesdaGame/BethesdaGame.hpp"
    "include/BethesdaDirectory/BethesdaDirectory.hpp"
    "include/ParallaxGen/ParallaxGen.hpp"
    "include/ParallaxGenUtil/ParallaxGenUtil.hpp"
    "include/ParallaxGenDirectory/ParallaxGenDirectory.hpp"
)

set(SOURCES
    "src/main/main.cpp"
    "src/BethesdaGame/BethesdaGame.cpp"
    "src/BethesdaDirectory/BethesdaDirectory.cpp"
    "src/ParallaxGen/ParallaxGen.cpp"
    "src/ParallaxGenUtil/ParallaxGenUtil.cpp"
    "src/ParallaxGenDirectory/ParallaxGenDirectory.cpp"
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Pass build type to compiler
if(CMAKE_BUILD_TYPE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BUILD_TYPE=${CMAKE_BUILD_TYPE})
endif()

# Externals
# Setup Folders
set(EXTRN_BUILD_DIR ${CMAKE_BINARY_DIR}/external/blds)
set(EXTRN_INSTALL_DIR ${CMAKE_BINARY_DIR}/external/installed)
file(MAKE_DIRECTORY ${EXTRN_BUILD_DIR})
file(MAKE_DIRECTORY ${EXTRN_INSTALL_DIR})
# Set CMAKE module path to include external install dir
set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${EXTRN_INSTALL_DIR})

# Function to configure, build, and install the external project
function(setup_external PACKAGE_NAME PACKAGE_SOURCE_DIR CONF_ARGS)
    # Split CONF_ARGS into a list of arguments
    separate_arguments(CONF_ARGS_LIST UNIX_COMMAND "${CONF_ARGS}")

    message(STATUS "Setting up external package ${PACKAGE_NAME}...")
    set(PACKAGE_BUILD_DIR ${EXTRN_BUILD_DIR}/${PACKAGE_NAME})
    file(MAKE_DIRECTORY ${PACKAGE_BUILD_DIR})
    execute_process(COMMAND ${CMAKE_COMMAND} -S ${PACKAGE_SOURCE_DIR} -B ${PACKAGE_BUILD_DIR} -DCMAKE_INSTALL_PREFIX=${EXTRN_INSTALL_DIR} ${CONF_ARGS_LIST})
    execute_process(COMMAND ${CMAKE_COMMAND} --build ${PACKAGE_BUILD_DIR} --target install --config ${CMAKE_BUILD_TYPE})
endfunction()

# Add Packages
find_package(bsa REQUIRED CONFIG)
find_package(spdlog REQUIRED CONFIG)
find_package(CLI11 REQUIRED CONFIG)
find_package(Boost REQUIRED)
find_package(directxtex REQUIRED CONFIG)
find_package(miniz REQUIRED CONFIG)

setup_external("nifly" "${CMAKE_SOURCE_DIR}/external/nifly" "-DBUILD_TESTING=OFF")
find_package(nifly REQUIRED CONFIG)

target_link_libraries(${PROJECT_NAME} PRIVATE
    bsa::bsa
    spdlog::spdlog
    CLI11::CLI11
    Boost::boost
    Microsoft::DirectXTex
    nifly
    miniz::miniz
)

# Install the executable and DLLs
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION . )
install(FILES $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> DESTINATION . )

# manual copy DLLs not caught by TARGET_RUNTIME_DLLS
install(FILES ${CMAKE_BINARY_DIR}/lz4.dll DESTINATION . )
install(FILES ${CMAKE_BINARY_DIR}/zlib1.dll DESTINATION . )
