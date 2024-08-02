#include "ParallaxGenDirectory.hpp"

#include <DirectXTex.h>
#include <spdlog/spdlog.h>

#include <boost/algorithm/string.hpp>
#include <regex>

#include "ParallaxGenUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;

ParallaxGenDirectory::ParallaxGenDirectory(BethesdaGame BG,
                                           filesystem::path ExePath)
    : BethesdaDirectory(BG, true), ExePath(std::move(ExePath)) {}

auto ParallaxGenDirectory::getPGConfigPath() -> wstring {
  static const wstring PGConfPath = L"parallaxgen";
  return PGConfPath;
}

auto ParallaxGenDirectory::getPGConfigValidation() -> nlohmann::json {
  static const string PGConfigValidationStr = R"(
  {
		"parallax_lookup": {
			"archive_blocklist": "^[^\\/\\\\]*$",
			"allowlist": ".*_.*\\.dds$",
			"blocklist": ".*"
		},
		"complexmaterial_lookup": {
			"archive_blocklist": "^[^\\/\\\\]*$",
			"allowlist": ".*_.*\\.dds$",
			"blocklist": ".*"
		},
		"truepbr_cfg_lookup": {
			"archive_blocklist": "^[^\\/\\\\]*$",
			"allowlist": ".*\\.json$",
			"blocklist": ".*"
		},
		"nif_lookup": {
			"archive_blocklist": "^[^\\/\\\\]*$",
			"allowlist": ".*\\.nif$",
			"blocklist": ".*"
		}
	}
  )";
  static const nlohmann::json PGConfigValidation =
      nlohmann::json::parse(PGConfigValidationStr);
  return PGConfigValidation;
}

auto ParallaxGenDirectory::getTruePBRConfigFilenameFields() -> vector<string> {
  static const vector<string> PGConfigFilenameFields = {
      "match_normal", "match_diffuse", "rename"};
  return PGConfigFilenameFields;
}

auto ParallaxGenDirectory::getDefaultCubemapPath() -> filesystem::path {
  static const filesystem::path DefCubemapPath =
      "textures\\cubemaps\\dynamic1pxcubemap_black.dds";
  return DefCubemapPath;
}

void ParallaxGenDirectory::findHeightMaps() {
  // find height maps
  spdlog::info("Finding parallax height maps");

  // Get relevant lists from Config
  vector<wstring> ParallaxAllowlist =
      jsonArrayToWString(PGConfig["parallax_lookup"]["allowlist"]);
  vector<wstring> ParallaxBlocklist =
      jsonArrayToWString(PGConfig["parallax_lookup"]["blocklist"]);
  vector<wstring> ParallaxArchiveBlocklist =
      jsonArrayToWString(PGConfig["parallax_lookup"]["archive_blocklist"]);

  // Find heightmaps
  HeightMaps = findFiles(true, ParallaxAllowlist, ParallaxBlocklist,
                         ParallaxArchiveBlocklist);
  spdlog::info("Found {} height maps", HeightMaps.size());
}

void ParallaxGenDirectory::findComplexMaterialMaps() {
  spdlog::info("Finding complex material maps");

  // Get relevant lists from Config
  vector<wstring> ComplexMaterialAllowlist =
      jsonArrayToWString(PGConfig["complexmaterial_lookup"]["allowlist"]);
  vector<wstring> ComplexMaterialBlocklist =
      jsonArrayToWString(PGConfig["complexmaterial_lookup"]["blocklist"]);
  vector<wstring> ComplexMaterialArchiveBlocklist = jsonArrayToWString(
      PGConfig["complexmaterial_lookup"]["archive_blocklist"]);

  // find complex material maps
  vector<filesystem::path> EnvMaps =
      findFiles(true, ComplexMaterialAllowlist, ComplexMaterialBlocklist,
                ComplexMaterialArchiveBlocklist);

  // loop through env maps
  for (const auto &EnvMap : EnvMaps) {
    // check if env map is actually a complex material map
    const vector<std::byte> EnvMapData = getFile(EnvMap);

    // load image into directxtex
    DirectX::ScratchImage Image;
    HRESULT HR =
        DirectX::LoadFromDDSMemory(EnvMapData.data(), EnvMapData.size(),
                                   DirectX::DDS_FLAGS_NONE, nullptr, Image);
    if (FAILED(HR)) {
      spdlog::warn(L"Failed to load DDS from memory: {} - skipping",
                   EnvMap.wstring());
      continue;
    }

    // check if image is a complex material map
    if (!Image.IsAlphaAllOpaque()) {
      // if alpha channel is used, there is parallax data
      // this won't work on complex matterial maps that don't make use of
      // complex parallax I'm not sure there's a way to check for those other
      // cases
      spdlog::trace(L"Adding {} as a complex material map", EnvMap.wstring());
      ComplexMaterialMaps.push_back(EnvMap);
    }
  }

  spdlog::info("Found {} complex material maps", ComplexMaterialMaps.size());
}

void ParallaxGenDirectory::findMeshes() {
  // find Meshes
  spdlog::info("Finding Meshes");

  // get relevant lists
  vector<wstring> MeshAllowlist =
      jsonArrayToWString(PGConfig["nif_lookup"]["allowlist"]);
  vector<wstring> MeshBlocklist =
      jsonArrayToWString(PGConfig["nif_lookup"]["blocklist"]);
  vector<wstring> MeshArchiveBlocklist =
      jsonArrayToWString(PGConfig["nif_lookup"]["archive_blocklist"]);

  Meshes = findFiles(true, MeshAllowlist, MeshBlocklist, MeshArchiveBlocklist);
  spdlog::info("Found {} Meshes", Meshes.size());
}

void ParallaxGenDirectory::findTruePBRConfigs() {
  // TODO more logging here
  // Find True PBR Configs
  spdlog::info("Finding TruePBR Configs");

  // get relevant lists
  vector<wstring> TruePBRAllowlist =
      jsonArrayToWString(PGConfig["truepbr_cfg_lookup"]["allowlist"]);
  vector<wstring> TruePBRBlocklist =
      jsonArrayToWString(PGConfig["truepbr_cfg_lookup"]["blocklist"]);
  vector<wstring> TruePBRArchiveBlocklist =
      jsonArrayToWString(PGConfig["truepbr_cfg_lookup"]["archive_blocklist"]);

  auto ConfigFiles = findFiles(true, TruePBRAllowlist, TruePBRBlocklist,
                               TruePBRArchiveBlocklist);

  // loop through and parse Configs
  for (auto &Config : ConfigFiles) {
    // check if Config is valid
    auto ConfigFileBytes = getFile(Config);
    string ConfigFileStr(reinterpret_cast<const char *>(ConfigFileBytes.data()),
                         ConfigFileBytes.size());

    try {
      nlohmann::json J = nlohmann::json::parse(ConfigFileStr);
      // loop through each Element
      for (auto &Element : J) {
        // Preprocessing steps here
        if (Element.contains("texture")) {
          Element["match_diffuse"] = Element["texture"];
        }

        // loop through filename Fields
        for (const auto &Field : getTruePBRConfigFilenameFields()) {
          if (Element.contains(Field) &&
              !boost::istarts_with(Element[Field].get<string>(), "\\")) {
            Element[Field] = Element[Field].get<string>().insert(0, 1, '\\');
          }
        }

        TruePBRConfigs.push_back(Element);
      }
    } catch (nlohmann::json::parse_error &E) {
      spdlog::error(L"Unable to parse TruePBR Config file {}: {}",
                    Config.wstring(), stringToWstring(E.what()));
      continue;
    }
  }

  spdlog::info("Found {} TruePBR entries", TruePBRConfigs.size());
}

void ParallaxGenDirectory::loadPGConfig(const bool &LoadDefault) {
  spdlog::info("Loading ParallaxGen Configs from load order");

  // Load default Config
  if (LoadDefault) {
    filesystem::path DefConfPath = ExePath / "cfg/default.json";
    if (!filesystem::exists(DefConfPath)) {
      spdlog::error(L"Default Config not found at {}", DefConfPath.wstring());
      exitWithUserInput(1);
    }

    mergeJSONSmart(PGConfig, nlohmann::json::parse(getFileBytes(DefConfPath)),
                   getPGConfigValidation());
  }

  // Load Configs from load order
  size_t CfgCount = 0;
  auto PGConfigs = findFiles(true, {getPGConfigPath() + L"\\**.json"});
  for (auto &CurCfg : PGConfigs) {
    try {
      auto ParsedJson = nlohmann::json::parse(getFile(CurCfg));
      mergeJSONSmart(PGConfig, ParsedJson, getPGConfigValidation());
      CfgCount++;
    } catch (nlohmann::json::parse_error &E) {
      spdlog::warn(L"Failed to parse ParallaxGen Config file {}: {}",
                   CurCfg.wstring(), stringToWstring(E.what()));
      continue;
    }
  }

  // Loop through each Element in JSON
  replaceForwardSlashes(PGConfig);

  spdlog::info("Loaded {} ParallaxGen Configs from load order", CfgCount);
}

void ParallaxGenDirectory::addHeightMap(const filesystem::path &Path) {
  filesystem::path PathLower = getPathLower(Path);

  // add to vector
  addUniqueElement(HeightMaps, PathLower);
}

void ParallaxGenDirectory::addComplexMaterialMap(const filesystem::path &Path) {
  filesystem::path PathLower = getPathLower(Path);

  // add to vector
  addUniqueElement(ComplexMaterialMaps, PathLower);
}

void ParallaxGenDirectory::addMesh(const filesystem::path &Path) {
  filesystem::path PathLower = getPathLower(Path);

  // add to vector
  addUniqueElement(Meshes, PathLower);
}

auto ParallaxGenDirectory::isHeightMap(const filesystem::path &Path) const
    -> bool {
  return find(HeightMaps.begin(), HeightMaps.end(), getPathLower(Path)) !=
         HeightMaps.end();
}

auto ParallaxGenDirectory::isComplexMaterialMap(
    const filesystem::path &Path) const -> bool {
  return find(ComplexMaterialMaps.begin(), ComplexMaterialMaps.end(),
              getPathLower(Path)) != ComplexMaterialMaps.end();
}

auto ParallaxGenDirectory::isMesh(const filesystem::path &Path) const -> bool {
  return find(Meshes.begin(), Meshes.end(), getPathLower(Path)) != Meshes.end();
}

auto ParallaxGenDirectory::defCubemapExists() -> bool {
  return isFile(getDefaultCubemapPath());
}

auto ParallaxGenDirectory::getHeightMaps() const -> vector<filesystem::path> {
  return HeightMaps;
}

auto ParallaxGenDirectory::getComplexMaterialMaps() const
    -> vector<filesystem::path> {
  return ComplexMaterialMaps;
}

auto ParallaxGenDirectory::getMeshes() const -> vector<filesystem::path> {
  return Meshes;
}

auto ParallaxGenDirectory::getTruePBRConfigs() const -> vector<nlohmann::json> {
  return TruePBRConfigs;
}

auto ParallaxGenDirectory::getHeightMapFromBase(const string &Base) const
    -> string {
  return matchBase(Base, HeightMaps).string();
}

auto ParallaxGenDirectory::getComplexMaterialMapFromBase(
    const string &Base) const -> string {
  return matchBase(Base, ComplexMaterialMaps).string();
}

void ParallaxGenDirectory::mergeJSONSmart(nlohmann::json &Target,
                                          const nlohmann::json &Source,
                                          const nlohmann::json &Validation) {
  // recursively merge json objects while preseving lists
  for (const auto &[Key, Value] : Source.items()) {
    if (Value.is_object()) {
      // This is a JSON object
      if (!Target.contains(Key)) {
        // Create an object if it doesn't exist in the Target
        Target[Key] = nlohmann::json::object();
      }

      // Recursion
      if (!Validation.contains(Key)) {
        // No Validation for this Key
        spdlog::warn("Skipping unknown Field in ParallaxGen Config: {}", Key);
        continue;
      }

      mergeJSONSmart(Target[Key], Value, Validation[Key]);
    } else if (Value.is_array()) {
      // This is a list

      if (!Target.contains(Key)) {
        // Create an array if it doesn't exist in the Target
        Target[Key] = nlohmann::json::array();
      }

      // Loop through each item in array and add only if it doesn't already
      // exist in the list
      for (const auto &Item : Value) {
        if (std::find(Target[Key].begin(), Target[Key].end(), Item) ==
            Target[Key].end()) {
          // Validation
          if (!validateJSON(Item, Validation, Key)) {
            continue;
          }

          // Add item to Target
          Target[Key].push_back(Item);
        }
      }
    } else {
      // This is a single object

      // Validation
      if (!validateJSON(Value, Validation, Key)) {
        continue;
      }

      // Add item to Target
      Target[Key] = Value;
    }
  }
}

auto ParallaxGenDirectory::validateJSON(const nlohmann::json &Item,
                                        const nlohmann::json &Validation,
                                        const string &Key) -> bool {
  if (!Validation.contains(Key)) {
    // No Validation for this Key
    spdlog::warn("Skipping unknown Field in ParallaxGen Config: {}", Key);
    return false;
  }

  if (Validation[Key].is_string()) {
    // regex check
    regex CHKPattern(Validation[Key].get<string>());
    string CHKPatternStr = Item.get<string>();

    // Check that regex matches, log if not and return
    if (!regex_match(CHKPatternStr, CHKPattern)) {
      spdlog::warn(
          "Invalid Value in ParallaxGen Config Key {}: {}. Value must match "
          "regex pattern {}",
          Key, Item.get<string>(), Validation[Key].get<string>());
      return false;
    }
  }

  return true;
}

auto ParallaxGenDirectory::jsonArrayToWString(const nlohmann::json &JSONArray)
    -> vector<wstring> {
  vector<wstring> Result;

  if (JSONArray.is_array()) {
    for (const auto &Item : JSONArray) {
      if (Item.is_string()) {
        Result.push_back(stringToWstring(Item.get<string>()));
      }
    }
  }

  return Result;
}

void ParallaxGenDirectory::replaceForwardSlashes(nlohmann::json &JSON) {
  if (JSON.is_string()) {
    auto &Str = JSON.get_ref<string &>();
    for (auto &Ch : Str) {
      if (Ch == '/') {
        Ch = '\\';
      }
    }
  } else if (JSON.is_object()) {
    for (const auto &Item : JSON.items()) {
      replaceForwardSlashes(Item.value());
    }
  } else if (JSON.is_array()) {
    for (auto &Element : JSON) {
      replaceForwardSlashes(Element);
    }
  }
}

// TODO what about suffixes like 01MASK.dds?
auto ParallaxGenDirectory::matchBase(const string &Base,
                                     const vector<filesystem::path> &SearchList)
    -> filesystem::path {
  for (const auto &Search : SearchList) {
    auto SearchStr = Search.wstring();
    if (boost::istarts_with(SearchStr, Base)) {
      size_t Pos = SearchStr.find_last_of(L'_');
      auto MapBase = SearchStr.substr(0, Pos);
      if (Pos != wstring::npos && boost::iequals(MapBase, Base)) {
        return Search;
      }
    }
  }

  return filesystem::path();
}
