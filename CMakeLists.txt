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

# Project Vars
set(PROJECT_NAME "ParallaxGen")
set(PROJECT_URI "com.github.hakasapl.ParallaxGen")

# Initialize Project
set(PARALLAXGEN_VERSION 0.5.1)
project(${PROJECT_NAME} VERSION ${PARALLAXGEN_VERSION})

# Export compile commands for use with clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Define preprocessor macro with the version number
add_compile_definitions(PARALLAXGEN_VERSION="${PARALLAXGEN_VERSION}")

# Include other Directories
include_directories("include")

# Disables conflicting min/max macros in Windows.h
add_compile_definitions(NOMINMAX)
add_compile_definitions(WIN32_LEAN_AND_MEAN)
add_compile_definitions(NOSOCKETTYPE)
add_compile_definitions(_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)

# Add Executables
set(HEADERS
    "include/BethesdaGame.hpp"
    "include/BethesdaDirectory.hpp"
    "include/NIFUtil.hpp"
    "include/ParallaxGen.hpp"
    "include/ParallaxGenConfig.hpp"
    "include/ParallaxGenD3D.hpp"
    "include/ParallaxGenTask.hpp"
    "include/ParallaxGenUtil.hpp"
    "include/ParallaxGenDirectory.hpp"
    "include/patchers/PatcherComplexMaterial.hpp"
    "include/patchers/PatcherTruePBR.hpp"
    "include/patchers/PatcherVanillaParallax.hpp"
)

set(SOURCES
    "src/main.cpp"
    "src/BethesdaGame.cpp"
    "src/BethesdaDirectory.cpp"
    "src/NIFUtil.cpp"
    "src/ParallaxGen.cpp"
    "src/ParallaxGenConfig.cpp"
    "src/ParallaxGenD3D.cpp"
    "src/ParallaxGenTask.cpp"
    "src/ParallaxGenUtil.cpp"
    "src/ParallaxGenDirectory.cpp"
    "src/patchers/PatcherComplexMaterial.cpp"
    "src/patchers/PatcherTruePBR.cpp"
    "src/patchers/PatcherVanillaParallax.cpp"
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

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
find_package(spdlog REQUIRED CONFIG)
find_package(bsa REQUIRED CONFIG)
find_package(CLI11 REQUIRED CONFIG)
find_package(Boost REQUIRED COMPONENTS filesystem stacktrace_windbg)
find_package(directxtk REQUIRED)
find_package(directxtex REQUIRED CONFIG)
find_package(miniz REQUIRED CONFIG)
find_package(nlohmann_json REQUIRED CONFIG)
find_package(nlohmann_json_schema_validator REQUIRED)

setup_external("nifly" "${CMAKE_SOURCE_DIR}/external/nifly" "-DBUILD_TESTING=OFF")
find_package(nifly REQUIRED CONFIG)

set(DirectXTK_LIBS
    d3d11.lib
    dxgi.lib
    dxguid.lib
    uuid.lib
    kernel32.lib
    user32.lib
    comdlg32.lib
    advapi32.lib
    shell32.lib
    ole32.lib
    oleaut32.lib
    D3DCompiler.lib
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    spdlog::spdlog
    bsa::bsa
    CLI11::CLI11
    ${Boost_LIBRARIES}
    nifly
    miniz::miniz
    Microsoft::DirectXTex
    ${DirectXTK_LIBS}
    Microsoft::DirectXTK
    Shlwapi
    nlohmann_json::nlohmann_json
    nlohmann_json_schema_validator::validator
)

# Copy Folders
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR}/)
file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR}/)
file(COPY ${CMAKE_SOURCE_DIR}/cfg DESTINATION ${CMAKE_BINARY_DIR}/)

# Install the executable and DLLs
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION . )
install(FILES $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> DESTINATION . )

# manual copy DLLs not caught by TARGET_RUNTIME_DLLS
install(FILES ${CMAKE_BINARY_DIR}/lz4.dll DESTINATION . )
install(FILES ${CMAKE_BINARY_DIR}/zlib1.dll DESTINATION . )

# Copy compiled shaders to install
install(DIRECTORY ${CMAKE_BINARY_DIR}/shaders DESTINATION . )

# Copy assets to install
install(DIRECTORY ${CMAKE_BINARY_DIR}/assets DESTINATION . )

# Copy cfg to install
install(DIRECTORY ${CMAKE_BINARY_DIR}/cfg DESTINATION . )
