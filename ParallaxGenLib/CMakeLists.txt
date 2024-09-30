# Add Executables
set(HEADERS
    "include/BethesdaGame.hpp"
    "include/BethesdaDirectory.hpp"
    "include/NIFUtil.hpp"
    "include/ParallaxGen.hpp"
    "include/ParallaxGenConfig.hpp"
    "include/ParallaxGenD3D.hpp"
    "include/ParallaxGenPlugin.hpp"
    "include/ParallaxGenTask.hpp"
    "include/ParallaxGenUtil.hpp"
    "include/ParallaxGenDirectory.hpp"
    "include/patchers/PatcherComplexMaterial.hpp"
    "include/patchers/PatcherTruePBR.hpp"
    "include/patchers/PatcherVanillaParallax.hpp"
)

set(SOURCES
    "src/BethesdaGame.cpp"
    "src/BethesdaDirectory.cpp"
    "src/NIFUtil.cpp"
    "src/ParallaxGen.cpp"
    "src/ParallaxGenConfig.cpp"
    "src/ParallaxGenD3D.cpp"
    "src/ParallaxGenPlugin.cpp"
    "src/ParallaxGenTask.cpp"
    "src/ParallaxGenUtil.cpp"
    "src/ParallaxGenDirectory.cpp"
    "src/patchers/PatcherComplexMaterial.cpp"
    "src/patchers/PatcherTruePBR.cpp"
    "src/patchers/PatcherVanillaParallax.cpp"
)

include_directories("include")
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Add the library
add_library(ParallaxGenLib SHARED ${SOURCES} ${HEADERS})

# Add Packages
find_package(spdlog REQUIRED CONFIG)
find_package(bsa REQUIRED CONFIG)
find_package(Boost REQUIRED COMPONENTS filesystem stacktrace_windbg thread)
find_package(directxtk REQUIRED)
find_package(directxtex REQUIRED CONFIG)
find_package(miniz REQUIRED CONFIG)
find_package(nlohmann_json REQUIRED CONFIG)
find_package(nlohmann_json_schema_validator REQUIRED)
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
target_link_libraries(ParallaxGenLib PUBLIC
    spdlog::spdlog
    bsa::bsa
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
target_include_directories(ParallaxGenLib PUBLIC include)

# DNNE setup
set(DNNE_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/ParallaxGenMutagenWrapper/bin/Release/net8.0)
include_directories(${DNNE_OUTPUT_DIR})
target_link_libraries(ParallaxGenLib PUBLIC ${DNNE_OUTPUT_DIR}/ParallaxGenMutagenWrapperNE.lib)
file(GLOB PGMutagenWrapperDLLs "${DNNE_OUTPUT_DIR}/*.dll")
install(FILES ${PGMutagenWrapperDLLs} DESTINATION . )
install(FILES ${DNNE_OUTPUT_DIR}/ParallaxGenMutagenWrapper.runtimeconfig.json DESTINATION . )

install(FILES $<TARGET_RUNTIME_DLLS:ParallaxGenLib> DESTINATION . )

# manual copy DLLs not caught by TARGET_RUNTIME_DLLS TODO why do I have to do this manually?
if (${CMAKE_BUILD_TYPE} STREQUAL "Release" OR ${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo" OR ${CMAKE_BUILD_TYPE} STREQUAL "MinSizeRel")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/lz4.dll DESTINATION . )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/zlib1.dll DESTINATION . )
elseif(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/lz4d.dll DESTINATION . )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/zlibd1.dll DESTINATION . )
endif()
