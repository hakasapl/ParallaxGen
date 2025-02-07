set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

include_directories("include")
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS include/*.hpp)
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

# Add the library
add_library(PGLib SHARED ${SOURCES} ${HEADERS})

# Add Packages
find_package(spdlog REQUIRED CONFIG)
find_package(bsa REQUIRED CONFIG)
find_package(Boost REQUIRED COMPONENTS thread locale)
find_package(directxtk REQUIRED)
find_package(directxtex REQUIRED CONFIG)
find_package(miniz REQUIRED CONFIG)
find_package(nlohmann_json REQUIRED CONFIG)
find_package(nlohmann_json_schema_validator REQUIRED)
find_package(nifly REQUIRED CONFIG)
find_package(cpptrace REQUIRED CONFIG)

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
target_link_libraries(PGLib PUBLIC
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
    cpptrace::cpptrace
)
target_include_directories(PGLib PUBLIC include)

# Asset moving
add_custom_command(TARGET PGLib POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -rf $<TARGET_FILE_DIR:PGLib>/shaders
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:PGLib>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:PGLib>/shaders
)

# include and link mutagen wrapper output
add_dependencies(PGLib PGMutagen)
include_directories(${PGMutagen_BINARY_DIR})
target_link_libraries(PGLib PUBLIC ${PGMutagen_BINARY_DIR}/PGMutagenNE.lib)

# TODO redo install step

# Install DLLs from mutagenwrapper
install(DIRECTORY ${PGMutagen_BINARY_DIR}/
        DESTINATION .
        FILES_MATCHING PATTERN "*.dll")

install(FILES ${PGMutagen_BINARY_DIR}/PGMutagen.runtimeconfig.json DESTINATION . )

install(FILES $<TARGET_RUNTIME_DLLS:PGLib> DESTINATION . )

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
add_dependencies(${PARALLAXGENLIB_TEST_NAME} PGLib)

file(WRITE ${CMAKE_BINARY_DIR}/copyDLLs.cmake
  "file(COPY \${CMAKE_ARGV3}
        DESTINATION \${CMAKE_ARGV4}
        FILES_MATCHING PATTERN *.dll)\n"
)

# copy mutagen wrapper DLLS to test file directory
add_custom_command(TARGET ${PARALLAXGENLIB_TEST_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/copyDLLs.cmake ${PGMutagen_BINARY_DIR}/ $<TARGET_FILE_DIR:${PARALLAXGENLIB_TEST_NAME}>
)

add_custom_command(TARGET ${PARALLAXGENLIB_TEST_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${PGMutagen_BINARY_DIR}/PGMutagen.runtimeconfig.json
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
  PGLib
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(${PARALLAXGENLIB_TEST_NAME}
  WORKING_DIRECTORY $<TARGET_FILE_DIR:PGLib>
)
