# Changelog

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