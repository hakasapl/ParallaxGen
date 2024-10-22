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

void ModManagerDirectory::populateInfo(const filesystem::path &RequiredFile, const filesystem::path &StagingDir) {
  this->StagingDir = StagingDir;
  this->RequiredFile = RequiredFile;
}

void ModManagerDirectory::populateModFileMap() {
  switch (MMType) {
    case ModManagerType::ModOrganizer2:
      populateModFileMapMO2();
      break;
    case ModManagerType::Vortex:
      populateModFileMapVortex();
      break;
    default:
      break;
  }
}

auto ModManagerDirectory::getModFileMap() const -> const unordered_map<filesystem::path, wstring> & {
  return ModFileMap;
}

auto ModManagerDirectory::getMod(const filesystem::path &RelPath) const -> wstring {
  auto RelPathLower = filesystem::path(boost::to_lower_copy(RelPath.wstring()));
  if (ModFileMap.contains(RelPathLower)) {
    return ModFileMap.at(RelPathLower);
  }

  return L"";
}

void ModManagerDirectory::populateModFileMapVortex() {
  // required file is vortex.deployment.json in the data folder
  spdlog::info("Populating mods from Vortex");

  if (!filesystem::exists(RequiredFile)) {
    throw runtime_error("Vortex deployment file does not exist: " + RequiredFile.string());
  }

  ifstream VortexDepFileF(RequiredFile);
  nlohmann::json VortexDeployment = nlohmann::json::parse(VortexDepFileF);
  VortexDepFileF.close();

  // Populate staging dir
  StagingDir = VortexDeployment["stagingPath"].get<string>();

  // Check that files field exists
  if (!VortexDeployment.contains("files")) {
    throw runtime_error("Vortex deployment file does not contain 'files' field: " + RequiredFile.string());
  }

  // loop through files
  for (const auto &File : VortexDeployment["files"]) {
    auto RelPath = filesystem::path(File["relPath"].get<string>());
    auto ModName = ParallaxGenUtil::strToWstr(File["source"].get<string>());

    // filter out modname suffix
    const static wregex VortexSuffixRe(L"-[0-9]+-.*");
    ModName = regex_replace(ModName, VortexSuffixRe, L"");

    // Update file map
    spdlog::trace(L"ModManagerDirectory | Adding Files to Map : {} -> {}", RelPath.wstring(), ModName);
    ModFileMap[boost::to_lower_copy(RelPath.wstring())] = boost::to_lower_copy(ModName);
  }
}

void ModManagerDirectory::populateModFileMapMO2() {
  // required file is modlist.txt in the mods folder

  spdlog::info("Populating mods from Mod Organizer 2");

  if (!filesystem::exists(RequiredFile)) {
    throw runtime_error("Mod Organizer 2 modlist.txt file does not exist: " + RequiredFile.string());
  }

  wifstream ModListFileF(RequiredFile);

  // loop through modlist.txt
  wstring Mod;
  while (getline(ModListFileF, Mod)) {
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
    Mod.erase(0, 1);  // remove +
    auto ModDir = StagingDir / Mod;
    if (!filesystem::exists(ModDir)) {
      spdlog::warn(L"Mod directory from modlist.txt does not exist: {}", ModDir.wstring());
      continue;
    }

    for (const auto &File : filesystem::recursive_directory_iterator(ModDir, filesystem::directory_options::skip_permission_denied)) {
      if (!filesystem::is_regular_file(File)) {
        continue;
      }

      // check if already in map
      if (ModFileMap.contains(filesystem::relative(File, ModDir))) {
        continue;
      }

      // skip meta.ini file
      if (boost::iequals(File.path().filename().wstring(), L"meta.ini")) {
        continue;
      }

      auto RelPath = filesystem::relative(File, ModDir);
      spdlog::trace(L"ModManagerDirectory | Adding Files to Map : {} -> {}", RelPath.wstring(), Mod);

      filesystem::path RelPathLower = boost::to_lower_copy(RelPath.wstring());
      // check if already in map
      if (ModFileMap.contains(RelPathLower)) {
        continue;
      }

      ModFileMap[RelPathLower] = boost::to_lower_copy(Mod);
    }
  }
}
