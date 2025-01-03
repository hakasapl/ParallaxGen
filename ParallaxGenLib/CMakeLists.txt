# Add Executables
set(HEADERS
    "include/BethesdaGame.hpp"
    "include/BethesdaDirectory.hpp"
    "include/Logger.hpp"
    "include/ModManagerDirectory.hpp"
    "include/NIFUtil.hpp"
    "include/ParallaxGen.hpp"
    "include/ParallaxGenD3D.hpp"
    "include/ParallaxGenPlugin.hpp"
    "include/ParallaxGenTask.hpp"
    "include/ParallaxGenUtil.hpp"
    "include/ParallaxGenWarnings.hpp"
    "include/ParallaxGenDirectory.hpp"
    "include/patchers/PatcherShader.hpp"
    "include/patchers/PatcherComplexMaterial.hpp"
    "include/patchers/PatcherTruePBR.hpp"
    "include/patchers/PatcherVanillaParallax.hpp"
    "include/patchers/PatcherShaderTransform.hpp"
    "include/patchers/PatcherUpgradeParallaxToCM.hpp"
    "include/patchers/Patcher.hpp"
    "include/patchers/PatcherUtil.hpp"
    "include/patchers/PatcherGlobal.hpp"
    "include/patchers/PatcherParticleLightsToLP.hpp"
    "include/patchers/PatcherDefault.hpp"
)

set(SOURCES
    "src/BethesdaGame.cpp"
    "src/BethesdaDirectory.cpp"
    "src/Logger.cpp"
    "src/ModManagerDirectory.cpp"
    "src/NIFUtil.cpp"
    "src/ParallaxGen.cpp"
    "src/ParallaxGenD3D.cpp"
    "src/ParallaxGenPlugin.cpp"
    "src/ParallaxGenTask.cpp"
    "src/ParallaxGenUtil.cpp"
    "src/ParallaxGenWarnings.cpp"
    "src/ParallaxGenDirectory.cpp"
    "src/patchers/PatcherShader.cpp"
    "src/patchers/PatcherComplexMaterial.cpp"
    "src/patchers/PatcherTruePBR.cpp"
    "src/patchers/PatcherVanillaParallax.cpp"
    "src/patchers/PatcherShaderTransform.cpp"
    "src/patchers/PatcherUpgradeParallaxToCM.cpp"
    "src/patchers/Patcher.cpp"
    "src/patchers/PatcherUtil.cpp"
    "src/patchers/PatcherGlobal.cpp"
    "src/patchers/PatcherParticleLightsToLP.cpp"
    "src/patchers/PatcherDefault.cpp"
)

include_directories("include")
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Add the library
add_library(ParallaxGenLib SHARED ${SOURCES} ${HEADERS})

# Add Packages
find_package(spdlog REQUIRED CONFIG)
find_package(bsa REQUIRED CONFIG)
find_package(Boost REQUIRED COMPONENTS filesystem stacktrace_windbg thread locale)
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

# Asset moving
add_custom_command(TARGET ParallaxGenLib POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf $<TARGET_FILE_DIR:ParallaxGenLib>/shaders
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:ParallaxGenLib>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:ParallaxGenLib>/shaders
)

# include and link mutagen wrapper output
add_dependencies(ParallaxGenLib ParallaxGenMutagenWrapper)
include_directories(${PARALLAXGENMUTAGENWRAPPER_BINARY_DIR})
target_link_libraries(ParallaxGenLib PUBLIC ${PARALLAXGENMUTAGENWRAPPER_BINARY_DIR}/ParallaxGenMutagenWrapperNE.lib)

# TODO redo install step

# Install DLLs from mutagenwrapper
install(DIRECTORY ${PARALLAXGENMUTAGENWRAPPER_BINARY_DIR}/
        DESTINATION .
        FILES_MATCHING PATTERN "*.dll")

install(FILES ${PARALLAXGENMUTAGENWRAPPER_BINARY_DIR}/ParallaxGenMutagenWrapper.runtimeconfig.json DESTINATION . )

install(FILES $<TARGET_RUNTIME_DLLS:ParallaxGenLib> DESTINATION . )

# manual copy DLLs not caught by TARGET_RUNTIME_DLLS TODO why do I have to do this manually?
if (${CMAKE_BUILD_TYPE} STREQUAL "Release" OR ${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo" OR ${CMAKE_BUILD_TYPE} STREQUAL "MinSizeRel")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/lz4.dll DESTINATION . )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/zlib1.dll DESTINATION . )
elseif(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/lz4d.dll DESTINATION . )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/zlibd1.dll DESTINATION . )
endif()

# Test
find_package(GTest REQUIRED CONFIG)

set(PARALLAXGENLIB_TEST_NAME ParallaxGenLibTests)

set (TESTS
  "tests/CommonTests.cpp"
  "tests/ParallaxGenPluginTests.cpp"
  "tests/BethesdaGameTests.cpp"
  "tests/BethesdaDirectoryTests.cpp"
  "tests/ParallaxGenDirectoryTests.cpp"
  "tests/ParallaxGenD3DTests.cpp"
  "tests/BethesdaGameTestsSkyrimSEInstalled.cpp"
  "tests/NIFUtilTests.cpp")

add_executable(
  ${PARALLAXGENLIB_TEST_NAME}
  ${TESTS}
)
add_dependencies(${PARALLAXGENLIB_TEST_NAME} ParallaxGenLib)

file(WRITE ${CMAKE_BINARY_DIR}/copyDLLs.cmake
  "file(COPY \${CMAKE_ARGV3}
        DESTINATION \${CMAKE_ARGV4}
        FILES_MATCHING PATTERN *.dll)\n"
)

# copy mutagen wrapper DLLS to test file directory
add_custom_command(TARGET ${PARALLAXGENLIB_TEST_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/copyDLLs.cmake ${PARALLAXGENMUTAGENWRAPPER_BINARY_DIR}/ $<TARGET_FILE_DIR:${PARALLAXGENLIB_TEST_NAME}>
)

add_custom_command(TARGET ${PARALLAXGENLIB_TEST_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PARALLAXGENMUTAGENWRAPPER_BINARY_DIR}/ParallaxGenMutagenWrapper.runtimeconfig.json
        $<TARGET_FILE_DIR:${PARALLAXGENLIB_TEST_NAME}>
)

add_custom_command(TARGET ${PARALLAXGENLIB_TEST_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf $<TARGET_FILE_DIR:${PARALLAXGENLIB_TEST_NAME}>/env
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${PARALLAXGENLIB_TEST_NAME}>/env
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/env
        $<TARGET_FILE_DIR:${PARALLAXGENLIB_TEST_NAME}>/env
)

target_link_libraries(
  ${PARALLAXGENLIB_TEST_NAME}
  ParallaxGenLib
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(${PARALLAXGENLIB_TEST_NAME}
  WORKING_DIRECTORY $<TARGET_FILE_DIR:ParallaxGenLib>
)
