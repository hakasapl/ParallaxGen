# ParallaxGen

NIF dynamic patcher to enable Parallax for Skyrim SE, also known as my excuse to start learning C++.

This is still in its early stages and could cause issues in your game (sometimes a badly configured mesh can cause game crashes). When filing a bug report please provide a log with trace output with `-vv` argument, as well as any other relevant details. For example, the path and mod of a mesh that didn't get patched correctly, crash logs from a crash logger mod, etc. I appreciate your help ironing this out!

## Features

* Converts meshes to support parallax dynamically based on what parallax or complex material textures you have in your modlist
* Doesn't modify meshes where a corresponding parallax texture cannot be found
* Support for regular parallax and experimental complex material
* Patches meshes even if they are not loose files

## Usage

### Mod Organizer

Just put `ParallaxGen.exe` in any folder then add it as an executable in MO2. Run it from MO2, then add the `ParallaxGen_Output.zip`, which is created in the `ParallaxGen_Output` folder in the same directory as `ParallaxGen.exe` file as a mod in your load order.

### Vortex

Put `ParallaxGen.exe` anywhere and run it. Afterwards add the `ParallaxGen_Output.zip`, which is created in the `ParallaxGen_Output` folder in the same directory as `ParallaxGen.exe` file as a mod in your load order.

### Mod Order

The output should overwrite any mesh in your load order, except for complex material meshes (unless you enable that argument).

### CLI arguments

```
Options:
  -h,--help                   Print this help message and exit
  -v [0]                      Verbosity level -v for DEBUG data or -vv for TRACE data (warning: TRACE data is very verbose)
  -d,--data-dir TEXT          Manually specify of Skyrim data directory (ie. <Skyrim SE Directory>/Data)
  -g,--game-type TEXT         Specify game type [skyrimse, skyrim, or skyrimvr]
  --no-zip                    Don't zip the output meshes
  --no-cleanup                Don't delete files after zipping
  --ignore-parallax           Don't generate any parallax meshes
  --enable-complex-material   Generate any complex material meshes (Experimental!)
```

## How it Works

1. Builds an abstraction of the bethesda data directory so that it can read both BSAs (in the correct order) and loose files
1. Finds all height maps (`_p.dds` files), complex material maps if enabled (`_m.dds` that have alpha layer), and meshes (`.nif files).
1. Loops through each mesh and looks for matching textures (additional checks here to avoid changing meshes it's not supposed to)
1. Patches meshes as needed and saves NIFs in `ParallaxGen_Output`
1. Zips everything (uncompressed) in `ParallaxGen_Output` for easier adding to mod managers

## Limitations

* The way ParallaxGen finds meshes to update is by checking if the normal or diffuse map with the `_p.dds` or `_m.dds` texture exists. If for whatever reason the maps are not named according to that convention, this app will miss it.
* Uncommonly, some mods will have texture maps defined in the plugin and not in the mesh (or it overrides the mesh). These will need an esp patch to work properly (example: Capital Whiterun Expansion does this)

## Future Work

* Ability to manually blocklist files on a per-mod basis by providing custom blocklist in the data directory (allows mods to specify what shouldn't be touched in their package)
* Multi-threading for faster mesh processing

## Contributing

This is my first endeavour into C++ so any experiences C++ and/or skyrim modders that are willing to contribute or code review are more than welcome. This is a CMake project with VCPKG for packages. Supported IDEs are Visual Studio 2022 or Visual Studio Code.

### Visual Studio Code

* Visual Studio 2022 with desktop development for C++ is still required as this project uses the `MSVC` toolchain.
* The [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) and [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) extensions are required.
* VS code must be run via the VS 2022 commandline (ie. `code .` from the CLI). This initializes all env vars to use MSVC toolchain.

### Visual Studio 2022

You should be able to just open the directory in Visual Studio and everything should automatically work.

### Initializing Project

1. Get git submodules: `git submodule init` and `git submodule update`
1. Configure CMake
