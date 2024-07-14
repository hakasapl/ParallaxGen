# ParallaxGen

NIF patcher to enable parallax on meshes in your load order.

See the [Nexus Page](https://www.nexusmods.com/skyrimspecialedition/mods/120946) for full description.

## Contributing

This is my first endeavor into C++ so any experienced C++ and/or skyrim modders that are willing to contribute or code review are more than welcome. Thank you in advance! This is a CMake project with VCPKG for packages. Supported IDEs are Visual Studio 2022 or Visual Studio Code.

### Visual Studio Code

* Visual Studio 2022 with desktop development for C++ is still required as this project uses the `MSVC` toolchain.
* The [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) and [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) extensions are required.
* VS code must be run via the VS 2022 commandline (ie. `code .` from the CLI). This initializes all env vars to use MSVC toolchain.

### Visual Studio 2022

You should be able to just open the directory in Visual Studio and everything should automatically work.

### Initializing Project

1. Get git submodules: `git submodule init` and `git submodule update`
1. Configure CMake

## Acknowledgements

A ton of very useful libraries were used in this project, including:

* [cli11](https://github.com/CLIUtils/CLI11) for CLI argument processing
* [spdlog](https://github.com/gabime/spdlog) for logging
* [DirectXTex](https://github.com/microsoft/DirectXTex) for analyzing dds environment maps
* [rsm-bsa](https://github.com/Ryan-rsm-McKenzie/bsa) for reading BSA files
* [nifly](https://github.com/ousnius/nifly) for modifying NIF files
* [miniz](https://github.com/richgel999/miniz) for creating the output zip file
* [boost](https://www.boost.org/) for several helpful features used throughout

A special thanks to:

* [Spongeman131](https://www.nexusmods.com/skyrimspecialedition/users/1366316) for their SSEedit script [NIF Batch Processing Script](https://www.nexusmods.com/skyrimspecialedition/mods/33846), which was instrumental on figuring out how to modify meshes to enable parallax
* [Kulharin](https://www.nexusmods.com/skyrimspecialedition/users/930789) for their SSEedit script [itAddsComplexMaterialParallax](https://www.nexusmods.com/skyrimspecialedition/mods/96777/?tab=files), which served the same purpose for figuring out how to modify meshes to enable complex material
* The folks on the community shaders discord who helped me understand a lot of the different parallax/shader implementations
