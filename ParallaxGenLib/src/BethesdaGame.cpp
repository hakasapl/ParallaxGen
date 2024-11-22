#include "BethesdaGame.hpp"
#include "ParallaxGenUtil.hpp"

#include <spdlog/spdlog.h>

#include <guiddef.h>
#include <shlobj.h>

#include <objbase.h>
#include <windows.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>

#include <boost/algorithm/string/join.hpp>

#include <cstdlib>

using namespace std;

BethesdaGame::BethesdaGame(GameType GameType, const bool &Logging, const filesystem::path &GamePath,
                           const filesystem::path &AppDataPath, const filesystem::path &DocumentPath)
    : ObjGameType(GameType), Logging(Logging) {
  if (GamePath.empty()) {
    // If the game path is empty, find
    this->GamePath = findGamePathFromSteam(GameType);
  } else {
    // If the game path is not empty, use the provided game path
    this->GamePath = GamePath;
  }

  if (this->GamePath.empty()) {
    // If the game path is still empty, throw an exception
    if (this->Logging) {
      spdlog::critical("Unable to locate game data path. Please specify the game data path "
                       "manually using the -d argument.");
      exit(1);
    } else {
      throw runtime_error("Game path not found");
    }
  }

  // check if path actually exists
  if (!filesystem::exists(this->GamePath)) {
    // If the game path does not exist, throw an exception
    if (this->Logging) {
      spdlog::critical(L"Game path does not exist: {}", this->GamePath.wstring());
      exit(1);
    } else {
      throw runtime_error("Game path does not exist");
    }
  }

  this->GameDataPath = this->GamePath / "Data";

  if (!isGamePathValid(this->GamePath, GameType)) {
    // If the game path does not contain Skyrim.esm, throw an exception
    if (this->Logging) {
      spdlog::critical(L"Game data location is invalid: {} does not contain Skyrim.esm", this->GameDataPath.wstring());
      exit(1);
    } else {
      throw runtime_error("Game data path does not contain Skyrim.esm");
    }
  }

  // Define appdata path
  if (AppDataPath.empty()) {
    GameAppDataPath = getGameAppdataSystemPath();
  } else {
    GameAppDataPath = AppDataPath;
  }

  // Define document path
  if (DocumentPath.empty()) {
    GameDocumentPath = getGameDocumentSystemPath();
  } else {
    GameDocumentPath = DocumentPath;
  }
}

auto BethesdaGame::isGamePathValid(const std::filesystem::path &GamePath, const GameType &Type) -> bool {
  // Check if the game path is valid
  const auto GameDataPath = GamePath / "Data";

  const auto CheckPath = GameDataPath / getDataCheckFile(Type);
  return filesystem::exists(CheckPath);
}

// Statics
auto BethesdaGame::getINILocations() const -> ININame {
  if (ObjGameType == BethesdaGame::GameType::SKYRIM_SE) {
    return ININame{"skyrim.ini", "skyrimprefs.ini", "skyrimcustom.ini"};
  }

  if (ObjGameType == BethesdaGame::GameType::SKYRIM_GOG) {
    return ININame{"skyrim.ini", "skyrimprefs.ini", "skyrimcustom.ini"};
  }

  if (ObjGameType == BethesdaGame::GameType::SKYRIM_VR) {
    return ININame{"skyrim.ini", "skyrimprefs.ini", "skyrimcustom.ini"};
  }

  if (ObjGameType == BethesdaGame::GameType::SKYRIM) {
    return ININame{"skyrim.ini", "skyrimprefs.ini", "skyrimcustom.ini"};
  }

  if (ObjGameType == BethesdaGame::GameType::ENDERAL) {
    return ININame{"enderal.ini", "enderalprefs.ini", "enderalcustom.ini"};
  }

  if (ObjGameType == BethesdaGame::GameType::ENDERAL_SE) {
    return ININame{"enderal.ini", "enderalprefs.ini", "enderalcustom.ini"};
  }

  return {};
}

auto BethesdaGame::getDocumentLocation() const -> filesystem::path {
  if (ObjGameType == BethesdaGame::GameType::SKYRIM_SE) {
    return "My Games/Skyrim Special Edition";
  }

  if (ObjGameType == BethesdaGame::GameType::SKYRIM_GOG) {
    return "My Games/Skyrim Special Edition GOG";
  }

  if (ObjGameType == BethesdaGame::GameType::SKYRIM_VR) {
    return "My Games/Skyrim VR";
  }

  if (ObjGameType == BethesdaGame::GameType::SKYRIM) {
    return "My Games/Skyrim";
  }

  if (ObjGameType == BethesdaGame::GameType::ENDERAL) {
    return "My Games/Enderal";
  }

  if (ObjGameType == BethesdaGame::GameType::ENDERAL_SE) {
    return "My Games/Enderal Special Edition";
  }

  return {};
}

auto BethesdaGame::getAppDataLocation() const -> filesystem::path {
  if (ObjGameType == BethesdaGame::GameType::SKYRIM_SE) {
    return "Skyrim Special Edition";
  }

  if (ObjGameType == BethesdaGame::GameType::SKYRIM_GOG) {
    return "Skyrim Special Edition GOG";
  }

  if (ObjGameType == BethesdaGame::GameType::SKYRIM_VR) {
    return "Skyrim VR";
  }

  if (ObjGameType == BethesdaGame::GameType::SKYRIM) {
    return "Skyrim";
  }

  if (ObjGameType == BethesdaGame::GameType::ENDERAL) {
    return "Enderal";
  }

  if (ObjGameType == BethesdaGame::GameType::ENDERAL_SE) {
    return "Enderal Special Edition";
  }

  return {};
}

auto BethesdaGame::getSteamGameID() const -> int {
  if (ObjGameType == BethesdaGame::GameType::SKYRIM_SE) {
    return STEAMGAMEID_SKYRIM_SE;
  }

  if (ObjGameType == BethesdaGame::GameType::SKYRIM_VR) {
    return STEAMGAMEID_SKYRIM_VR;
  }

  if (ObjGameType == BethesdaGame::GameType::SKYRIM) {
    return STEAMGAMEID_SKYRIM;
  }

  if (ObjGameType == BethesdaGame::GameType::ENDERAL) {
    return STEAMGAMEID_ENDERAL;
  }

  if (ObjGameType == BethesdaGame::GameType::ENDERAL_SE) {
    return STEAMGAMEID_ENDERAL_SE;
  }

  return {};
}

auto BethesdaGame::getDataCheckFile(const GameType &Type) -> filesystem::path {
  if (Type == BethesdaGame::GameType::SKYRIM_SE) {
    return "Skyrim.esm";
  }

  if (Type == BethesdaGame::GameType::SKYRIM_GOG) {
    return "Skyrim.esm";
  }

  if (Type == BethesdaGame::GameType::SKYRIM_VR) {
    return "SkyrimVR.esm";
  }

  if (Type == BethesdaGame::GameType::SKYRIM) {
    return "Skyrim.esm";
  }

  if (Type == BethesdaGame::GameType::ENDERAL) {
    return "Enderal - Forgotten Stories.esm";
  }

  if (Type == BethesdaGame::GameType::ENDERAL_SE) {
    return "Enderal - Forgotten Stories.esm";
  }

  return {};
}

auto BethesdaGame::getGameType() const -> BethesdaGame::GameType { return ObjGameType; }

auto BethesdaGame::getGamePath() const -> filesystem::path {
  // Get the game path from the registry
  // If the game is not found, return an empty string
  return GamePath;
}

auto BethesdaGame::getGameRegistryPath(const GameType &Type) -> std::string {
  const string BasePathSkyrim = R"(SOFTWARE\WOW6432Node\bethesda softworks\)";
  const string BasePathEnderal = R"(Software\Sure AI\)";

  const std::map<GameType, string> GameToPathMap{{GameType::SKYRIM_SE, BasePathSkyrim + "Skyrim Special Edition"},
                                                 {GameType::SKYRIM, BasePathSkyrim + "Skyrim"},
                                                 {GameType::SKYRIM_VR, BasePathSkyrim + "Skyrim VR"},
                                                 {GameType::ENDERAL, BasePathEnderal + "Enderal"},
                                                 {GameType::ENDERAL_SE, BasePathEnderal + "EnderalSE"}};

  if (GameToPathMap.contains(Type)) {
    return GameToPathMap.at(Type);
  }

  return {};
}

auto BethesdaGame::getGameDataPath() const -> filesystem::path { return GameDataPath; }

auto BethesdaGame::findGamePathFromSteam(const GameType &Type) -> filesystem::path {
  // TODO UNICODE get file path as UNICODE

  // Find the game path from the registry
  // If the game is not found, return an empty string

  HKEY BaseHKey = (Type == GameType::ENDERAL || Type == GameType::ENDERAL_SE) ? HKEY_CURRENT_USER
                                                                                            : HKEY_LOCAL_MACHINE;
  const string RegPath = getGameRegistryPath(Type);

  char Data[REG_BUFFER_SIZE]{};
  DWORD DataSize = REG_BUFFER_SIZE;

  LONG Result = RegGetValueA(BaseHKey, RegPath.c_str(), "Installed Path", RRF_RT_REG_SZ, nullptr, Data, &DataSize);
  if (Result == ERROR_SUCCESS) {
    return {Data};
  }

  return {};
}

auto BethesdaGame::getINIPaths() const -> BethesdaGame::ININame {
  BethesdaGame::ININame Output = getINILocations();
  const filesystem::path GameDocsPath = getGameDocumentSystemPath();

  // normal ini file
  Output.INI = GameDocumentPath / Output.INI;
  Output.INIPrefs = GameDocumentPath / Output.INIPrefs;
  Output.INICustom = GameDocumentPath / Output.INICustom;

  return Output;
}

auto BethesdaGame::getLoadOrderFile() const -> filesystem::path {
  const filesystem::path GameLoadOrderFile = GameAppDataPath / "loadorder.txt";
  return GameLoadOrderFile;
}

auto BethesdaGame::getPluginsFile() const -> filesystem::path {
  const filesystem::path GamePluginsFile = GameAppDataPath / "plugins.txt";
  return GamePluginsFile;
}

auto BethesdaGame::getActivePlugins(const bool &TrimExtension) const -> vector<wstring> {
  vector<wstring> OutputLO;

  // Build set of plugins that are actually active
  unordered_map<string, bool> ActivePlugins;

  // Get the plugins file
  const filesystem::path PluginsFile = getPluginsFile();
  ifstream PluginsFileHandle(PluginsFile, 1);
  if (!PluginsFileHandle.is_open()) {
    if (Logging) {
      spdlog::critical("Unable to open plugins.txt");
      exit(1);
    } else {
      throw runtime_error("Unable to open plugins.txt");
    }
  }

  // loop through each line of loadorder.txt
  string Line;
  while (getline(PluginsFileHandle, Line)) {
    // Ignore lines that start with '#', which are comment lines
    if (Line.empty() || Line[0] == '#') {
      continue;
    }

    if (Line.starts_with("*")) {
      // this is an active plugin
      Line = Line.substr(1);
      ActivePlugins[Line] = true;
    } else {
      // this is not an active plugin
      ActivePlugins[Line] = false;
    }
  }

  // close file handle
  PluginsFileHandle.close();

  // Get the loadorder file
  const filesystem::path LoadOrderFile = getLoadOrderFile();
  ifstream LoadOrderFileHandle(LoadOrderFile, 1);
  if (!LoadOrderFileHandle.is_open()) {
    if (Logging) {
      spdlog::critical("Unable to open loadorder.txt");
      exit(1);
    } else {
      throw runtime_error("Unable to open loadorder.txt");
    }
  }

  // loop through each line of loadorder.txt
  while (getline(LoadOrderFileHandle, Line)) {
    // Ignore lines that start with '#', which are comment lines
    if (Line.empty() || Line[0] == '#') {
      continue;
    }

    if (ActivePlugins.find(Line) != ActivePlugins.end() && !ActivePlugins[Line]) {
      // this is not an active plugin
      continue;
    }

    // Remove extension from line
    if (TrimExtension) {
      Line = Line.substr(0, Line.find_last_of('.'));
    }

    // Add to output list
    OutputLO.push_back(ParallaxGenUtil::UTF8toUTF16(Line));
  }

  // close file handle
  LoadOrderFileHandle.close();

  if (Logging) {
    wstring LoadOrderStr = boost::algorithm::join(OutputLO, L",");
    spdlog::debug(L"Active Plugin Load Order: {}", LoadOrderStr);
  }

  return OutputLO;
}

auto BethesdaGame::getGameDocumentSystemPath() const -> filesystem::path {
  filesystem::path DocPath = getSystemPath(FOLDERID_Documents);
  if (DocPath.empty()) {
    return {};
  }

  DocPath /= getDocumentLocation();
  return DocPath;
}

auto BethesdaGame::getGameAppdataSystemPath() const -> filesystem::path {
  filesystem::path AppDataPath = getSystemPath(FOLDERID_LocalAppData);
  if (AppDataPath.empty()) {
    return {};
  }

  AppDataPath /= getAppDataLocation();
  return AppDataPath;
}

auto BethesdaGame::getSystemPath(const GUID &FolderID) -> filesystem::path {
  PWSTR Path = nullptr;
  const HRESULT Result = SHGetKnownFolderPath(FolderID, 0, nullptr, &Path);
  if (SUCCEEDED(Result)) {
    wstring OutPath(Path);
    CoTaskMemFree(Path); // Free the memory allocated for the path

    return OutPath;
  }

  // Handle error
  return {};
}

auto BethesdaGame::getGameTypes() -> vector<GameType> {
  const static auto GameTypes = vector<GameType>{GameType::SKYRIM_SE, GameType::SKYRIM_GOG, GameType::SKYRIM,
                                                 GameType::SKYRIM_VR, GameType::ENDERAL, GameType::ENDERAL_SE};
  return GameTypes;
}

auto BethesdaGame::getStrFromGameType(const GameType &Type) -> string {
  const static auto GameTypeToStrMap = unordered_map<GameType, string>{
      {GameType::SKYRIM_SE, "Skyrim SE"}, {GameType::SKYRIM_GOG, "Skyrim GOG"}, {GameType::SKYRIM, "Skyrim"},
      {GameType::SKYRIM_VR, "Skyrim VR"}, {GameType::ENDERAL, "Enderal"},       {GameType::ENDERAL_SE, "Enderal SE"}};

  if (GameTypeToStrMap.contains(Type)) {
    return GameTypeToStrMap.at(Type);
  }

  return GameTypeToStrMap.at(GameType::SKYRIM_SE);
}

auto BethesdaGame::getGameTypeFromStr(const string &Type) -> GameType {
  const static auto StrToGameTypeMap = unordered_map<string, GameType>{{"Skyrim SE", GameType::SKYRIM_SE},
                                                                            {"Skyrim GOG", GameType::SKYRIM_GOG},
                                                                            {"Skyrim", GameType::SKYRIM},
                                                                            {"Skyrim VR", GameType::SKYRIM_VR},
                                                                            {"Enderal", GameType::ENDERAL},
                                                                            {"Enderal SE", GameType::ENDERAL_SE}};

  if (StrToGameTypeMap.contains(Type)){return StrToGameTypeMap.at(Type);}

return StrToGameTypeMap.at("Skyrim SE");
}
