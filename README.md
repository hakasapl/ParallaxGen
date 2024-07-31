# ParallaxGen

NIF patcher to patch meshes for various things in your load order.

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

1. Install pre-commit from [here](https://pre-commit.com/)
1. Install pre-commit hook: `pre-commit install`
1. Get git submodules: `git submodule init` and `git submodule update`
1. Configure CMake
