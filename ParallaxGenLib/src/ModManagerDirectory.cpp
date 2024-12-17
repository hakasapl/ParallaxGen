#include "ModManagerDirectory.hpp"

#include <boost/algorithm/string.hpp>

#include <boost/algorithm/string/case_conv.hpp>
#include <fstream>
#include <regex>

#include <nlohmann/json.hpp>

#include <spdlog/spdlog.h>

#include "ParallaxGenUtil.hpp"

using namespace std;

ModManagerDirectory::ModManagerDirectory(const ModManagerType &MMType) : MMType(MMType) {}

auto ModManagerDirectory::getModFileMap() const -> const unordered_map<filesystem::path, wstring> & {
  return ModFileMap;
}

auto ModManagerDirectory::getMod(const filesystem::path &RelPath) const -> wstring {
  auto RelPathLower = filesystem::path(ParallaxGenUtil::ToLowerASCII(RelPath.wstring()));
  if (ModFileMap.contains(RelPathLower)) {
    return ModFileMap.at(RelPathLower);
  }

  return L"";
}

void ModManagerDirectory::populateModFileMapVortex(const filesystem::path &DeploymentDir) {
  // required file is vortex.deployment.json in the data folder
  spdlog::info("Populating mods from Vortex");

  const auto DeploymentFile = DeploymentDir / "vortex.deployment.json";

  if (!filesystem::exists(DeploymentFile)) {
    throw runtime_error("Vortex deployment file does not exist: " +
                        ParallaxGenUtil::UTF16toUTF8(DeploymentFile.wstring()));
  }

  ifstream VortexDepFileF(DeploymentFile);
  nlohmann::json VortexDeployment = nlohmann::json::parse(VortexDepFileF);
  VortexDepFileF.close();

  // Check that files field exists
  if (!VortexDeployment.contains("files")) {
    throw runtime_error("Vortex deployment file does not contain 'files' field: " +
                        ParallaxGenUtil::UTF16toUTF8(DeploymentFile.wstring()));
  }

  // loop through files
  for (const auto &File : VortexDeployment["files"]) {
    auto RelPath = filesystem::path(ParallaxGenUtil::UTF8toUTF16(File["relPath"].get<string>()));
    auto ModName = ParallaxGenUtil::UTF8toUTF16(File["source"].get<string>());

    // filter out modname suffix
    const static wregex VortexSuffixRe(L"-[0-9]+-.*");
    ModName = regex_replace(ModName, VortexSuffixRe, L"");

    AllMods.insert(ModName);

    // Update file map
    spdlog::trace(L"ModManagerDirectory | Adding Files to Map : {} -> {}", RelPath.wstring(), ModName);

    if (!ParallaxGenUtil::ContainsOnlyAscii(RelPath.wstring())) {
      spdlog::debug(L"Path {} from {} contains non-ASCII characters", RelPath.wstring(), DeploymentDir.wstring());
    }

    ModFileMap[ParallaxGenUtil::ToLowerASCII(RelPath.wstring())] = ModName;
  }
}

void ModManagerDirectory::populateModFileMapMO2(const filesystem::path &InstanceDir, const wstring &Profile,
                                                const filesystem::path &OutputDir) {
  // required file is modlist.txt in the profile folder

  spdlog::info("Populating mods from Mod Organizer 2");

  // First read modorganizer.ini in the instance folder to get the profiles and mods folders
  filesystem::path MO2IniFile = InstanceDir / L"modorganizer.ini";
  if (!filesystem::exists(MO2IniFile)) {
    throw runtime_error("Mod Organizer 2 ini file does not exist: " +
                        ParallaxGenUtil::UTF16toUTF8(MO2IniFile.wstring()));
  }

  // Find MO2 paths from ModOrganizer.ini
  filesystem::path ProfileDir;
  filesystem::path ModDir;
  filesystem::path BaseDir;

  ifstream MO2IniFileF(MO2IniFile);
  string MO2IniLine;
  while (getline(MO2IniFileF, MO2IniLine)) {
    if (MO2IniLine.starts_with("profiles_directory=")) {
      ProfileDir = InstanceDir / MO2IniLine.substr(19);
    } else if (MO2IniLine.starts_with("mod_directory=")) {
      ModDir = InstanceDir / MO2IniLine.substr(14);
    } else if (MO2IniLine.starts_with("base_directory=")) {
      BaseDir = InstanceDir / MO2IniLine.substr(15);
    }
  }

  MO2IniFileF.close();

  if (ProfileDir.empty()) {
    if (BaseDir.empty()) {
      ProfileDir = InstanceDir / "profiles";
    } else {
      ProfileDir = BaseDir / "profiles";
    }
  }

  if (ModDir.empty()) {
    if (BaseDir.empty()) {
      ModDir = InstanceDir / "mods";
    } else {
      ModDir = BaseDir / "mods";
    }
  }

  // Find location of modlist.txt
  const auto ModListFile = ProfileDir / Profile / "modlist.txt";
  if (!filesystem::exists(ModListFile)) {
    throw runtime_error("Mod Organizer 2 modlist.txt file does not exist: " +
                        ParallaxGenUtil::UTF16toUTF8(ModListFile.wstring()));
  }

  ifstream ModListFileF(ModListFile);

  // loop through modlist.txt
  string ModStr;
  while (getline(ModListFileF, ModStr)) {
    wstring Mod = ParallaxGenUtil::UTF8toUTF16(ModStr);
    if (Mod.empty()) {
      // skip empty lines
      continue;
    }

    if (Mod.starts_with(L"-") || Mod.starts_with(L"*")) {
      // Skip disabled and uncontrolled mods
      continue;
    }

    if (Mod.starts_with(L"#")) {
      // Skip comments
      continue;
    }

    if (Mod.ends_with(L"_separator")) {
      // Skip separators
      continue;
    }

    // loop through all files in mod
    Mod.erase(0, 1); // remove +
    const auto CurModDir = ModDir / Mod;

    // Check if mod folder exists
    if (!filesystem::exists(CurModDir)) {
      spdlog::warn(L"Mod directory from modlist.txt does not exist: {}", CurModDir.wstring());
      continue;
    }

    // check if mod dir is output dir
    if (filesystem::equivalent(CurModDir, OutputDir)) {
      spdlog::critical(L"If outputting to MO2 you must disable the mod {} first to prevent issues with MO2 VFS", Mod);
      exit(1);
    }

    AllMods.insert(Mod);
    InferredOrder.insert(InferredOrder.begin(), Mod);

    try {
      for (const auto &File :
           filesystem::recursive_directory_iterator(CurModDir, filesystem::directory_options::skip_permission_denied)) {
        if (!filesystem::is_regular_file(File)) {
          continue;
        }

        // skip meta.ini file
        if (boost::iequals(File.path().filename().wstring(), L"meta.ini")) {
          continue;
        }

        auto RelPath = filesystem::relative(File, CurModDir);
        spdlog::trace(L"ModManagerDirectory | Adding Files to Map : {} -> {}", RelPath.wstring(), Mod);

        if (!ParallaxGenUtil::ContainsOnlyAscii(RelPath.wstring())) {
          spdlog::debug(L"Path {} in directory {} contains non-ASCII characters", RelPath.wstring(), ModDir.wstring());
        }

        filesystem::path RelPathLower = ParallaxGenUtil::ToLowerASCII(RelPath.wstring());
        // check if already in map
        if (ModFileMap.contains(RelPathLower)) {
          continue;
        }

        ModFileMap[RelPathLower] = Mod;
      }
    } catch (const filesystem::filesystem_error &E) {
      spdlog::error(L"Error reading mod directory {} (skipping): {}", Mod, ParallaxGenUtil::ASCIItoUTF16(E.what()));
    }
  }

  ModListFileF.close();
}

auto ModManagerDirectory::getModManagerTypes() -> vector<ModManagerType> {
  return {ModManagerType::None, ModManagerType::Vortex, ModManagerType::ModOrganizer2};
}

auto ModManagerDirectory::getStrFromModManagerType(const ModManagerType &Type) -> string {
  const static auto ModManagerTypeToStrMap =
      unordered_map<ModManagerType, string>{{ModManagerType::None, "None"},
                                            {ModManagerType::Vortex, "Vortex"},
                                            {ModManagerType::ModOrganizer2, "Mod Organizer 2"}};

  if (ModManagerTypeToStrMap.contains(Type)) {
    return ModManagerTypeToStrMap.at(Type);
  }

  return ModManagerTypeToStrMap.at(ModManagerType::None);
}

auto ModManagerDirectory::getModManagerTypeFromStr(const string &Type) -> ModManagerType {
  const static auto ModManagerStrToTypeMap =
      unordered_map<string, ModManagerType>{{"None", ModManagerType::None},
                                            {"Vortex", ModManagerType::Vortex},
                                            {"Mod Organizer 2", ModManagerType::ModOrganizer2}};

  if (ModManagerStrToTypeMap.contains(Type)) {
    return ModManagerStrToTypeMap.at(Type);
  }

  return ModManagerStrToTypeMap.at("None");
}

auto ModManagerDirectory::getMO2ProfilesFromInstanceDir(const filesystem::path &InstanceDir) -> vector<wstring> {
  // First read modorganizer.ini in the instance folder to get the profiles and mods folders
  filesystem::path MO2IniFile = InstanceDir / L"modorganizer.ini";
  if (!filesystem::exists(MO2IniFile)) {
    return {};
  }

  filesystem::path ProfileDir;
  filesystem::path BaseDir;

  ifstream MO2IniFileF(MO2IniFile);
  string MO2IniLine;
  while (getline(MO2IniFileF, MO2IniLine)) {
    if (MO2IniLine.starts_with("profiles_directory=")) {
      ProfileDir = InstanceDir / MO2IniLine.substr(19);
    } else if (MO2IniLine.starts_with("base_directory=")) {
      BaseDir = InstanceDir / MO2IniLine.substr(15);
    }
  }

  MO2IniFileF.close();

  if (ProfileDir.empty()) {
    if (BaseDir.empty()) {
      ProfileDir = InstanceDir / "profiles";
    } else {
      ProfileDir = BaseDir / "profiles";
    }
  }

  // check if the "profiles" folder exists
  if (!filesystem::exists(ProfileDir)) {
    // set instance directory text to red
    return {};
  }

  // Find all directories within "profiles"
  vector<wstring> Profiles;
  for (const auto &Entry : filesystem::directory_iterator(ProfileDir)) {
    if (Entry.is_directory()) {
      Profiles.push_back(Entry.path().filename().wstring());
    }
  }

  return Profiles;
}

auto ModManagerDirectory::getInferredOrder() const -> const vector<wstring> & { return InferredOrder; }
