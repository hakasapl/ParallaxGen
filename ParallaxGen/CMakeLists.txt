# Add Files
set(SOURCES
    "src/main.cpp"
)

add_executable(ParallaxGen ${SOURCES})

# Packages
find_package(CLI11 REQUIRED CONFIG)

target_link_libraries(ParallaxGen PRIVATE
    ParallaxGenLib
    CLI11::CLI11
)

install(TARGETS ParallaxGen DESTINATION . )
install(FILES $<TARGET_RUNTIME_DLLS:ParallaxGen> DESTINATION . )

# manual copy DLLs not caught by TARGET_RUNTIME_DLLS TODO why do I have to do this manually?
if (${CMAKE_BUILD_TYPE} STREQUAL "Release" OR ${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo" OR ${CMAKE_BUILD_TYPE} STREQUAL "MinSizeRel")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/lz4.dll DESTINATION . )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/zlib1.dll DESTINATION . )
elseif(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/lz4d.dll DESTINATION . )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/zlibd1.dll DESTINATION . )
endif()
