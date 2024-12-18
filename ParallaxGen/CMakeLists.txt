# Add Files
set (HEADERS
    "include/ParallaxGenConfig.hpp"
    "include/ParallaxGenUI.hpp"
    "include/GUI/LauncherWindow.hpp"
    "include/GUI/ModSortDialog.hpp"
)

set(SOURCES
    "src/ParallaxGenConfig.cpp"
    "src/ParallaxGenUI.cpp"
    "src/GUI/LauncherWindow.cpp"
    "src/GUI/ModSortDialog.cpp"
    "src/main.cpp"
)

include_directories("include")
add_executable(ParallaxGen ${SOURCES} ${HEADERS} icon.rc)

# Packages
find_package(CLI11 REQUIRED CONFIG)
find_package(wxWidgets REQUIRED COMPONENTS core base)

target_link_libraries(ParallaxGen PRIVATE
    ParallaxGenLib
    CLI11::CLI11
    ${wxWidgets_LIBRARIES}
)

install(TARGETS ParallaxGen DESTINATION . )
install(FILES $<TARGET_RUNTIME_DLLS:ParallaxGen> DESTINATION . )

# manual copy DLLs not caught by TARGET_RUNTIME_DLLS TODO why do I have to do this manually?
if (${CMAKE_BUILD_TYPE} STREQUAL "Release" OR ${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo" OR ${CMAKE_BUILD_TYPE} STREQUAL "MinSizeRel")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/wxbase32u_vc_x64_custom.dll DESTINATION . )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/wxmsw32u_core_vc_x64_custom.dll DESTINATION . )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pcre2-16.dll DESTINATION . )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/jpeg62.dll DESTINATION . )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libpng16.dll DESTINATION . )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/tiff.dll DESTINATION . )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/liblzma.dll DESTINATION . )
elseif(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/wxbase32ud_vc_x64_custom.dll DESTINATION . )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/wxmsw32ud_core_vc_x64_custom.dll DESTINATION . )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/pcre2-16d.dll DESTINATION . )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/jpeg62.dll DESTINATION . )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libpng16d.dll DESTINATION . )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/tiffd.dll DESTINATION . )
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/liblzma.dll DESTINATION . )
endif()
