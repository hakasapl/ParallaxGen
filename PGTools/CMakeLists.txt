# Add Files
set (HEADERS
)

set(SOURCES
    "src/main.cpp"
)

include_directories("include")
add_executable(pgtools ${SOURCES} ${HEADERS})

# Packages
find_package(CLI11 REQUIRED CONFIG)

target_link_libraries(pgtools PRIVATE
    ParallaxGenLib
    CLI11::CLI11
)

install(TARGETS pgtools DESTINATION . )
install(FILES $<TARGET_RUNTIME_DLLS:pgtools> DESTINATION . )
