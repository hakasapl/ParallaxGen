# Changelog

## [0.3.3] - 2024-07-15

- Fixed parallax heightmaps not applying correctly for some meshes
- Aspect ratio of texture maps are checked for complex material now too
- Added additional error handling during BSA read step
- CLI arguments are now printed to console
- Wrong CLI arguments results in a graceful termination now

## [0.3.2] - 2024-07-15

- Fixed --optimize-meshes CLI arg not doing anything

## [0.3.1] - 2024-07-15

- Added option to optimize meshes that are generated
- Fixed issue where ignoring parallax caused nothing to work

## [0.3.0] - 2024-07-14

- Before enabling parallax on mesh heightmap and diffuse map are now checked to make sure they are the same aspect ratio
- Complex parallax checking is now enabled by default
- Added a -o CLI argument to allow the user to specify the output directory
- App will throw a critical error if the output directory is the game data directory
- Fixed bug where loadorder.txt would sometimes not be found
- complex material will now remove parallax flags and texture maps from meshes if required

## [0.2.4] - 2024-06-06

- Generation now sorts nif blocks before saving

## [0.2.3] - 2024-06-05

- Fixed crash from failure to write BSA file to memory buffer

## [0.2.2] - 2024-06-05

- Ignore shaders with back ligting, which seems to cause flickering

## [0.2.1] - 2024-06-05

- Fixed BSAs not being detected due to case insensitivity
- Made --no-zip enable --no-cleanup by default
- NIF processing logs will not show the block id of shapes
- getFile now logs which BSA it's pulling a file from
- Fixed crash that would occur if a shape doesn't have a shader
- Generation will now ignore shaders with the DECAL or DYNAMIC DECAL shader flag set

## [0.2.0] - 2024-06-04

- Added support for enderal and enderal se
- Fixed crash when reading invalid INI file

## [0.1.8] - 2024-06-04

- Fixed log level for havok animation warning to trace
- Added additional logging for pre-generation
- Added additional error handling for missing INI files

## [0.1.7] - 2024-06-03

- Added a helper log message for which file to import at the end of generation
- Meshes now don't process if there are attached havok physics

## [0.1.6] - 2024-06-03

- Added wstring support for all file paths
- Added additional trace logging for building file map
- Stopped using memoryio file buffer, using ifstream now
- After generation state file is now cleaned up properly

## [0.1.5] - 2024-06-03

- Fixed an issue that would cause hangs or crashes when loading invalid or corrupt NIFs

## [0.1.4] - 2024-06-03

- Set log to flush on whatever the verbosity mode is set to (trace, debug, or info). Should help reproducing some issues.

## [0.1.3] - 2024-06-02

- Fixed CLI arg requesting game data path instead of game path.
- Made some messages more descriptive
- Made some CLI arg helps more descriptive
- Added a check to look for Skyrim.esm in the data folder

## [0.1.2] - 2024-06-02

- Added error handling for unable to find ParallaxGen.exe
- Added error handling for invalid data paths
- Added error handling for logger initialization
- Added error handling for registry lookups
- Logs now flush on every INFO level message
- Enabled logging for BethesdaGame
- Fixed log message for deleteOutputDir()

## [0.1.1] - 2024-06-02

- Added log flush every 3 seconds to prevent log from being lost on app crash
- Added error message if loadorder.txt doesn't exist

## [0.1.0] - 2024-06-02

- Initial release
