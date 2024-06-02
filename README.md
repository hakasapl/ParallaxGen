# ParallaxGen

NIF dynamic patcher to enable Parallax for Skyrim SE, also known as my excuse to start learning C++. Many parallax texture mods on the nexus do not provide meshes with parallax enabled. Some of this can be remedied by mods like Lux Orbis with parallax meshes, but even that doesn't cover everything and many meshes are still not enabled for parallax properly. ParallaxGen hopes to provide a dynamic method of generating parallax meshes for any load order.

This project is in a very early beta for its initial release. Use at your own risk. The app shouldn't mess with your data folder at all, but as mentioned it's in beta so this is not a 100% guarantee. I appreciate anyone's help in getting this tested initially. Another more common risk that may come up is a badly configured mesh which might cause game crashes. In this case it would be great if you could provide a crash log from a crash logger mod of your choice, as well as the log of ParallaxGen, which is stored as a file in the same directory as `ParallaxGen.exe`. It would be helpful to enable trace logging for any issues with the CLI argument `-vv`.

## Features

* Converts meshes to support parallax dynamically based on what parallax or complex material textures you have in your modlist
* Doesn't modify meshes where a corresponding parallax texture cannot be found
* Support for regular parallax
* Supports complex material (experimental, needs to be enabled with cli argument `--enable-complex-material`)
* Patches meshes even if they are not loose files (in BSAs)

## Usage

1. Download ParallaxGen and drop all files in the zip to a folder of your choice (make sure this is a new empty folder)
1. If using mod organizer, add `ParallaxGen.exe` as an executable to your instance
1. Run `ParallaxGen.exe` from either MO2 or the folder (if using Vortex) and generate meshes
1. A file `ParallaxGen_Output/ParallaxGen_Output.zip` should exist which you can import into your mod manager of choice
1. These meshes should win every conflict, even lighting mods. The only exception is complex material meshes unless you are running with `--enable-complex-material`
1. If you need to re-generate meshes you'll need to delete the existing `ParallaxGen_Output` mod from your mod manager before rerunning `ParallaxGen.exe`

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
* User interface

## Contributing

This is my first endeavour into C++ so any experienced C++ and/or skyrim modders that are willing to contribute or code review are more than welcome. Thank you in advnace! This is a CMake project with VCPKG for packages. Supported IDEs are Visual Studio 2022 or Visual Studio Code.

### Visual Studio Code

* Visual Studio 2022 with desktop development for C++ is still required as this project uses the `MSVC` toolchain.
* The [CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools) and [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) extensions are required.
* VS code must be run via the VS 2022 commandline (ie. `code .` from the CLI). This initializes all env vars to use MSVC toolchain.

### Visual Studio 2022

You should be able to just open the directory in Visual Studio and everything should automatically work.

### Initializing Project

1. Get git submodules: `git submodule init` and `git submodule update`
1. Configure CMake

## Acknowldgements

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
