# Changelog

## [0.8.10] - UNRELEASED

- Fixed unhandled exception around UTF8 chars when diagnostics were enabled
- Fixed diagnostics JSON having some data in incorrect places due to stale pointers

## [0.8.9] - 2025-02-10

- Added a "Enable Diagnostics" advanced option to generate files to help with support
- Added "Check Paths" toggle for advanced options in the TruePBR patcher
- Added "Print Nonexistent Paths" toggle for advanced options in the TruePBR patcher

## [0.8.8] - 2025-02-04

- Added "Fix Mesh Lighting" pre-patcher (Thanks to Catnyss for their article!)
- Fixed bug where texture sets of duplicate NIFs were not being patched even if an alternate texture didn't exist
- Changed alpha message to say "beta"

## [0.8.7] - 2025-02-02

- Added --full-dump CLI argument to ParallaxGen.exe to generate a full crash dump
- Fixed PBR matching bug when multiple full paths were specified as well as a short path
- Disabled patching for CM on shaders with anisotropic_lighting and soft_lighting
- Re-enabled CM patching on texture sets with slots in 3,6,7
- Fixed non-ascii chars not working for paths in modorganizer.ini
- Issues with plugins will now be printed to console instead of an unhandled exception

## [0.8.6] - 2025-01-28

- 3D name is no longer considered for matching alternate textures
- ParallaxGen.esp now holds new TXST records only
- ParallaxGen_X.esp (where X is sequential numbers) will be created automatically as required based on master limit
- Plugin patching for armor weight checks is now verified that it is actually armor instead of just a NIF named as such (very rare edge case)

## [0.8.5] - 2025-01-27

- Fixed "_1" armor being renamed to "_0" in some cases in the plugin
- Fixed some records being copied to ParallaxGen.esp that were not needed
- Fixed non-armor plugin patching being broken
- Reverted localization change on parallaxgen.esp

## [0.8.4] - 2025-01-26

- Fixed some statics in plugins not being patched
- Fixed only one model of two in armor and armor addon records being patched
- Fixed localization breaking for overridden records in ParallaxGen.esp
- Fixed duplicate meshes not being created past PG1
- Fixed vanilla texture sets not being included in duplicate mesh consideration
- Fixed 1st person armor models not being patched
- Fixed duplicate meshes not being created for _0 or_1 armor meshes depending on which is in the plugin

## [0.8.3] - 2025-01-22

- Fixed some duplicate meshes saving with no data

## [0.8.2] - 2025-01-19

- Added `converttohdr` patcher for pgtools
- Temporarily disabled default patcher because it is causing issues, it will come back with revamped conflict resolution system in 0.9.0
- Removed the GPU Acceleration option, which is now required
- Fixed some custom paths in modorganizer.ini with %BASE_DIR% in them that were not properly parsed
- Specular flag will be set if CM map has glosiness
- Fixed duplicate modkeys crash
- Fixed mesh slot size being modified even if nothing is being patched
- Fixed duplicate meshes not being saved in some cases
- Hopefully fixed conversion error crash

## [0.8.1] - 2025-01-13

- Specular strength will be set to 1.0 for complex material
- Fixed repeated TXSTs not applying to STAT records on 2+ iterations
- Fixed new mods not being sorted according to shader type (including no shader)

## [0.8.0] - 2025-01-12

- Added pgtools.exe modding tools
- Added pgtools patcher to convert particle lights to light placer lights
- loadorder.txt is no longer required
- Mods that do not have any special shader type can be prioritized now
- Existing TXST records will no longer be patched, only new ones will be created
- Added support for PBR fuzz
- Added support for PBR hair
- When mixing shaders with alternate texture records meshes will now be duplicated where required
- Fixed 1PX black dynamic cubemap not being black which would cause really shiny surfaces in recent CS releases
- Parallax maps included in the pbr subdirectory will be considered a different "height pbr" texture type
- Closing mod sort dialog will now close the whole app
- BSMeshLODTriShapes will also be patched now
- Meshes that have higher priority than textures can be patched now
- Plugin patching will also be considered when evaluating mods for conflicts now
- Removed simplicity of snow warning as the mod is not inherently incompatible
- Fixed NULL output in ParallaxGen_Diff.json when PG patched nothing
- PBR patcher clears hair_soft_lighting flag now
- PBR texture swap JSONs are automatically generated
- Fixed yellow highlights staying in mod list after deselecting everything
- Fixed deleted texture maps not actually deleting
- Added additional help text to mod sort dialog
- PBR slot commands will have "textures\\" added to the beginning automatically if not already there
- Specular color will be set to white for complex material that has metalness
- Crash dumps will be generated automatically upon crashing
- PBR delete: true will now set alpha to 0 instead of deleting the shape

## [0.7.3] - 2024-12-09

- Added a warning for simplicity of snow users if PBR or CM is enabled (SoS is incompatible with these shaders)
- Fixed thread gridlock that casued mesh patching to get suck occasionally
- Fixed PBR JSON delete: true not working
- Fixed uncaught exception if mod folder does not exist in MO2
- Fixed dyncubemap blocklist not saving/loading correctly in the GUI
- Fixed dyncubemap blocklist not showing up on start in the GUI if advanced options is enabled
- Fixed uncaught exception when a NIF has non-ASCII chars as a texture slot

## [0.7.2] - 2024-12-07

- Added mesh allowlist
- Added mesh allowlist, mesh blocklist, texture maps, vanilla bsa list, and dyncubemap blocklist to GUI advanced options
- If using MO2 you now have the option to use the MO2 left pane (loose file) order for PG order
- Added ESMify option for ParallaxGen.esp
- Fixed unicode character handling
- Added critical error if outputting to MO2 mod and mod is enabled in MO2 VFS
- Added critical error if DynDoLOD output is activated
- Added "save config", "load config", and "restore config" buttons to the launcher GUI
- MO2 selection will respect custom paths for mods and profiles folder now
- Fixed exceptions when plugin patching is not enabled
- PBR prefix check accounts for slot commands now too
- Improved warning output for texture mismatches
- Fixed case where multiple PBR entries did not apply together
- Fixed PBR slot check to check at the end of applying all entries for the match
- When PBR is the only shader patcher selected "map textures from meshes" will be automatically unselected
- INI files in the data folder will be read for BSA loading now
- Advanced is now a checkbox with persistence in the launcher GUI
- Fixed failed shader upgrade applying the wrong shader
- Shader transform errors don't post more than once now
- Exceptions in threads will trigger exceptions in main thread now to prevent error spam
- Texture TXST missing warnings are changed to debug level
- If mesh texture set has less than 9 slots it will be resized to 9 slots automatically while patching
- New texture sets will be created if required for two different shader types in meshes now

## [0.7.1] - 2024-11-18

- Fixed PBR applying to shapes with facegen RGB tint
- Fixed exception when file not found in BSA archive for CM check
- Added workaround for MO2 operator++ crash
- Max number of logs has been increased from 100 to 1000
- Fixed warning for missing textures for a texture set when upgrading shaders
- User.json custom entries are no longer deleted

## [0.7.0] - 2024-11-15

- Added a launcher GUI
- All CLI arguments except --autostart and -v/-vv have been removed in favor of the persistent GUI
- Added a mod manager priority conflict resolution system with UI
- Complex material will set env map scale to 1.0 now
- Fixed PBR bug when there were two overlapping matches
- Default textures are applied when TXST record cannot be patched for a shader
- Output directory will only delete items that parallaxgen might have generates (meshes folder, textures folder, PG files)
- BSLODTriShapes are now patched too
- Upgrade shaders only upgrades what is required now
- If user does not have .NET framework an error is posted now
- PBR ignore Skin Tint and Face Tint types now
- meta.ini is not deleted when deleting previous output
- Fixed plugin patching bug that would result in some alternature texture records referencing the wrong TXST record
- Fixed NIF block sorting breaking 3d index in plugins
- ParallaxGen.esp will be flagged as light if possible
- Load order configs in the ParallaxGen folder will no longer do anything - use cfg/user.json instead

## [0.6.0] - 2024-10-06

- Added plugin patching
- Fixed PBR rename not functions with certain suffixes
- Fixed ParallaxGen thinking RMAOS files were CM
- -vv logging mode will only log to file now not the terminal buffer
- Added a function to classify vanilla BSAs in the config, which will ignore complex material and parallax files from them
- Fixed retrieval of game installation directories from the registry
- Normal is matched before diffuse for CM and parallax now
- If multiple textures have the same prefix, a smarter choice will be made based on the existing value
- Only active plugins will be considered when plugin patching and loading BSAs now
- Meshes are only considered in the "meshes" folder now
- Textures are only considered in the "textures" folder now
- Fixed "textures\" being added to end of slots in very rare edge cases

## [0.5.8] - 2024-09-21

- Fixed PBR bug when multilayer: false is defined
- Fixed upgrade-shader not generating mipmaps
- Fixed PBR bug with duplicate texture sets
- New method for mapping textures to types by searching in NIFs
- Added --disable-mlp flag to turn MLP into complex material where possible
- Removed weapons/armor from dynamic cubemap blocklist
- Added --high-mem option for faster processing in exchange for high memory usage
- Added wide string support in NIF filenames
- Fixed issue of blank DDS files being checked for aspect ratio
- CM will be rejected on shapes with textures in slots 3, 7, or 8 now
- Added icon to parallaxgen.exe
- At the end parallaxgen will now report the time it took to run the patcher

## [0.5.7] - 2024-09-05

- Fixed sorting issue that would result in some patches being missed
- Updated nifly library to the latest commit (fixes undefined behavior with badly configured NIFs)
- Logging is more detailed now in -vv mode for mesh patching

## [0.5.6] - 2024-09-03

- PBR multilayer: false is now processed correctly
- More robust CLI argument validation
- Runtime for parallax and CM is now n*log(n) worst-case instead of n^2
- Runtime for truepbr average case is n*log(n) instead of n^2
- Introduced multi-threading for mesh generation
- Added --no-multithread CLI argument
- Added --no-bsa CLI argument to avoid reading any BSAs
- Textures that have non-ASCII chars are skipped because NIFs can't use them
- Fixed TruePBR case issue with Texture being capital T
- upgrade-shaders will now check for _em.dds files when checking if an existing vanilla env mask exists
- actors, effects, and interface folders now included in mesh search
- Diff JSON file is generated with mesh patch results (crc32 hash comparisons)
- PARALLAXGEN_DONT_DELETE file is removed from output and replaced by diff file
- PBR will now not apply if the result prefix doesn't exists
- Logs are now stored in "ParallaxGenLogs" and use a rolling log system to make it more manageable
- Added PBR glint support

## [0.5.5] - 2024-08-10

- Fixed TruePBR nif_filter handling
- Fixed TruePBR slotX handling

## [0.5.4] - 2024-08-08

- Fixed complex material applying on PBR meshes
- Fixed output zip file capitalization
- Added error handling for failing to load BSA

## [0.5.3] - 2024-08-07

- BC1 is no longer considered for Complex Material
- Fixed CM lookup memory leak on the GPU
- UTF-16/8 optimizations

## [0.5.2] - 2024-08-06

- Fixed rename issue for truepbr patching
- No longer checking for mask.dds files for CM

## [0.5.1] - 2024-08-04

- Removed loading screen exclusions from NIF config
- findFilesBySuffix now is findFiles and uses globs exclusively
- _resourcepack.bsa is now ignored for complex material
- Creation club BSAs are now ignored for complex material
- _em.dds files are now checked if they are complex material
- mask.dds files are not checked if they are complex material
- User-defined generic suffixes is now possible
- Added JSON validation for ParallaxGen configs
- Complex material lookup is much smarter now
- Added --no-gpu option to disable GPU use

## [0.5.0] - 2024-07-28

- Initial TruePBR implementation
- Added allowlist and blocklist support
- Added --autostart CLI argument to skip the "Press Enter" prompt
- Existing ParallaxGen meshes in load order is checked after output dir is deleted
- Fixed exit prompt to refer to ENTER specifically

## [0.4.7] - 2024-07-27

- GPU code will now verify textures are a power of 2
- Added "skyrimgog" game type
- Top level directory is now checked to make sure a NIF comes from "meshes", dds from "textures"
- Fixed parallax or complex material texture paths not being set correctly for some edge case NIFs
- Fixed aspect ratio checks happening more than once for the same pair
- Output directory + data dir path checking is done using std::filesystem now instead of string comparison
- Fixed bugs where shaders wouldn't compile when in wrong working directory
- Diffuse maps are checked to make sure they exist now before patching mesh
- Fixed help not showing up with -h or --help argument

## [0.4.6] - 2024-07-23

- Added additional error handling for GPU code

## [0.4.5] - 2024-07-23

- Shaders are no longer pre-compiled, they are compiled at runtime
- Skinned meshes are only checked for vanilla parallax now
- Havok animations are only checked for vanilla parallax now

## [0.4.4] - 2024-07-23

- Fixed non ASCII characters in loose file extension causing crashes
- Added global exception handler w/ stack trace
- Dynamic cubemaps overwrite oold cubemap value for CM meshes now
- Shaders are in their own folder now

## [0.4.3] - 2024-07-22

- Fixed parallax being applied on soft and rim lighting

## [0.4.2] - 2024-07-22

- ParallaxGen no longer patches LOD
- Added dynamic cubemaps support

## [0.4.1] - 2024-07-18

- Fixed already generated complex parallax maps regenerating if a heightmap was also included

## [0.4.0] - 2024-07-18

- Added --upgrade-shader argument to enable upgrading vanilla parallax to complex material
- Implemented DX11 into --upgrade-shader process
- Lots of code cleanup and optimization
- Fixed some typos in log messages

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
