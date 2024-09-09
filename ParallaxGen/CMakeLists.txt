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
