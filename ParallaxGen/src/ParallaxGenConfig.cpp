#include "ParallaxGenConfig.hpp"

#include "BethesdaGame.hpp"
#include "ModManagerDirectory.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenUtil.hpp"

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <mutex>
#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <boost/algorithm/string/case_conv.hpp>

#include <exception>
#include <filesystem>
#include <fstream>
#include <string>

#include <cstdlib>

using namespace std;
using namespace ParallaxGenUtil;

ParallaxGenConfig::ParallaxGenConfig(std::filesystem::path ExePath) : ExePath(std::move(ExePath)) {}

auto ParallaxGenConfig::getConfigValidation() -> nlohmann::json {
  const static nlohmann::json PGConfigSchema = R"(
  {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "title": "parallaxgen-cfg",
    "type": "object",
    "properties": {
      "dyncubemap_blocklist": {
        "type": "array",
        "items": {
          "type": "string"
        }
      },
      "nif_blocklist": {
        "type": "array",
        "items": {
          "type": "string"
        }
      },
      "texture_map": {
        "type": "object",
        "additionalProperties": {
          "type": "string"
        }
      },
      "vanilla_bsas" : {
        "type" : "array",
        "items": {
            "type": "string"
        }
      },
      "mod_order": {
        "type" : "array",
        "items": {
            "type": "string"
        }
      },
      "params": {
        "type": "object"
      }
    }
  }
  )"_json;

  return PGConfigSchema;
}

auto ParallaxGenConfig::getDefaultConfigFile() const -> filesystem::path {
  // Get default config file
  static const filesystem::path DefaultConfigFile = ExePath / "cfg" / "default.json";
  return DefaultConfigFile;
}

auto ParallaxGenConfig::getUserConfigFile() const -> filesystem::path {
  // Get user config file
  static const filesystem::path UserConfigFile = ExePath / "cfg" / "user.json";
  return UserConfigFile;
}

void ParallaxGenConfig::loadConfig() {
  // Initialize Validator
  try {
    Validator.set_root_schema(getConfigValidation());
  } catch (const std::exception &E) {
    spdlog::critical("Unable to validate JSON validation: {}", E.what());
    exit(1);
  }

  size_t NumConfigs = 0;

  static const vector<filesystem::path> ConfigsToRead = {getDefaultConfigFile(), getUserConfigFile()};
  // Loop through all files in DefConfPath recursively directoryiterator
  for (const auto &Entry : ConfigsToRead) {
    if (!filesystem::exists(Entry)) {
      // don't load a config that doesn't exist
      continue;
    }

    spdlog::debug(L"Loading ParallaxGen Config: {}", Entry.wstring());

    nlohmann::json J;
    if (!parseJSON(Entry, getFileBytes(Entry), J)) {
      continue;
    }

    if (!validateJSON(Entry, J)) {
      continue;
    }

    if (!J.empty()) {
      replaceForwardSlashes(J);
      addConfigJSON(J);
      NumConfigs++;
    }

    if (Entry == getUserConfigFile()) {
      UserConfig = J;
    }
  }

  // Print number of configs loaded
  spdlog::info("Loaded {} ParallaxGen configs successfully", NumConfigs);
}

auto ParallaxGenConfig::addConfigJSON(const nlohmann::json &J) -> void {
    // TODO UNIFORM check if to_lower_copy can handle UTF8
  // "dyncubemap_blocklist" field
  if (J.contains("dyncubemap_blocklist")) {
    for (const auto &Item : J["dyncubemap_blocklist"]) {
      DynCubemapBlocklist.insert(UTF8toUTF16(boost::to_lower_copy(Item.get<string>())));
    }
  }

  // "nif_blocklist" field
  if (J.contains("nif_blocklist")) {
    for (const auto &Item : J["nif_blocklist"]) {
      NIFBlocklist.insert(UTF8toUTF16(boost::to_lower_copy(Item.get<string>())));
    }
  }

  // "texture_map" field
  if (J.contains("texture_maps")) {
    for (const auto &Item : J["texture_maps"].items()) {
      ManualTextureMaps[boost::to_lower_copy(Item.key())] = NIFUtil::getTexTypeFromStr(Item.value().get<string>());
    }
  }

  // "vanilla_bsas" field
  if (J.contains("vanilla_bsas")) {
    for (const auto &Item : J["vanilla_bsas"]) {
      VanillaBSAList.insert(UTF8toUTF16(boost::to_lower_copy(Item.get<string>())));
    }
  }

  // "mod_order" field
  if (J.contains("mod_order")) {
    lock_guard<mutex> Lock(ModOrderMutex);

    ModOrder.clear();
    for (const auto &Item : J["mod_order"]) {
      auto Mod = UTF8toUTF16(Item.get<string>());
      ModOrder.push_back(Mod);
      ModPriority[Mod] = static_cast<int>(ModOrder.size() - 1);
    }
  }

  // "params" field
  if (J.contains("params")) {
    const auto &ParamJ = J["params"];

    // "game"
    Params.Game.Dir = UTF8toUTF16(ParamJ["game"]["dir"].get<string>());
    ParamJ["game"]["type"].get_to<BethesdaGame::GameType>(Params.Game.Type);

    // "modmanager"
    ParamJ["modmanager"]["type"].get_to<ModManagerDirectory::ModManagerType>(Params.ModManager.Type);
    ParamJ["modmanager"]["mo2instancedir"].get_to<filesystem::path>(Params.ModManager.MO2InstanceDir);
    Params.ModManager.MO2Profile = UTF8toUTF16(ParamJ["modmanager"]["mo2profile"].get<string>());

    // "output"
    Params.Output.Dir = UTF8toUTF16(ParamJ["output"]["dir"].get<string>());
    ParamJ["output"]["zip"].get_to<bool>(Params.Output.Zip);

    // "processing"
    ParamJ["processing"]["multithread"].get_to<bool>(Params.Processing.Multithread);
    ParamJ["processing"]["highmem"].get_to<bool>(Params.Processing.HighMem);
    ParamJ["processing"]["gpuacceleration"].get_to<bool>(Params.Processing.GPUAcceleration);
    ParamJ["processing"]["bsa"].get_to<bool>(Params.Processing.BSA);
    ParamJ["processing"]["pluginpatching"].get_to<bool>(Params.Processing.PluginPatching);
    ParamJ["processing"]["mapfrommeshes"].get_to<bool>(Params.Processing.MapFromMeshes);

    // "prepatcher"
    ParamJ["prepatcher"]["disablemlp"].get_to<bool>(Params.PrePatcher.DisableMLP);

    // "shaderpatcher"
    ParamJ["shaderpatcher"]["parallax"].get_to<bool>(Params.ShaderPatcher.Parallax);
    ParamJ["shaderpatcher"]["complexmaterial"].get_to<bool>(Params.ShaderPatcher.ComplexMaterial);
    ParamJ["shaderpatcher"]["truepbr"].get_to<bool>(Params.ShaderPatcher.TruePBR);

    // "shadertransforms"
    ParamJ["shadertransforms"]["parallaxtocm"].get_to<bool>(Params.ShaderTransforms.ParallaxToCM);

    // "postpatcher"
    ParamJ["postpatcher"]["optimizemeshes"].get_to<bool>(Params.PostPatcher.OptimizeMeshes);

    // Fill in empty values with defaults if needed
    if (Params.Game.Dir.empty()) {
      Params.Game.Dir = BethesdaGame::findGamePathFromSteam(Params.Game.Type);
    }

    if (Params.Output.Dir.empty()) {
      Params.Output.Dir = ExePath / "ParallaxGen_Output";
    }
  }
}

auto ParallaxGenConfig::parseJSON(const std::filesystem::path &JSONFile, const vector<std::byte> &Bytes,
                                  nlohmann::json &J) -> bool {
  // Parse JSON
  try {
    J = nlohmann::json::parse(Bytes);
  } catch (const nlohmann::json::parse_error &E) {
    spdlog::error(L"Error parsing JSON file {}: {}", JSONFile.wstring(), ASCIItoUTF16(E.what()));
    J = {};
    return false;
  }

  return true;
}

auto ParallaxGenConfig::validateJSON(const std::filesystem::path &JSONFile, const nlohmann::json &J) -> bool {
  // Validate JSON
  try {
    Validator.validate(J);
  } catch (const std::exception &E) {
    spdlog::error(L"Invalid JSON file {}: {}", JSONFile.wstring(), ASCIItoUTF16(E.what()));
    return false;
  }

  return true;
}

void ParallaxGenConfig::replaceForwardSlashes(nlohmann::json &JSON) {
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

auto ParallaxGenConfig::getNIFBlocklist() const -> const unordered_set<wstring> & { return NIFBlocklist; }

auto ParallaxGenConfig::getDynCubemapBlocklist() const -> const unordered_set<wstring> & { return DynCubemapBlocklist; }

auto ParallaxGenConfig::getManualTextureMaps() const -> const unordered_map<filesystem::path, NIFUtil::TextureType> & {
  return ManualTextureMaps;
}

auto ParallaxGenConfig::getVanillaBSAList() const -> const std::unordered_set<std::wstring> & { return VanillaBSAList; }

auto ParallaxGenConfig::getModOrder() -> vector<wstring> {
  lock_guard<mutex> Lock(ModOrderMutex);
  return ModOrder;
}

auto ParallaxGenConfig::getModPriority(const wstring &Mod) -> int {
  lock_guard<mutex> Lock(ModOrderMutex);
  if (ModPriority.contains(Mod)) {
    return ModPriority[Mod];
  }

  return -1;
}

auto ParallaxGenConfig::getModPriorityMap() -> unordered_map<wstring, int> {
  lock_guard<mutex> Lock(ModOrderMutex);
  return ModPriority;
}

void ParallaxGenConfig::setModOrder(const vector<wstring> &ModOrder) {
  lock_guard<mutex> Lock(ModOrderMutex);
  this->ModOrder = ModOrder;

  ModPriority.clear();
  for (size_t I = 0; I < ModOrder.size(); I++) {
    ModPriority[ModOrder[I]] = static_cast<int>(I);
  }

  saveUserConfig();
}

auto ParallaxGenConfig::getParams() const -> PGParams { return Params; }

void ParallaxGenConfig::setParams(const PGParams &Params) {
  this->Params = Params;
  saveUserConfig();
}

auto ParallaxGenConfig::validateParams(const PGParams &Params, vector<string> &Errors) -> bool {
  // Game
  if (Params.Game.Dir.empty()) {
    Errors.emplace_back("Game Location is required.");
  }

  if (!BethesdaGame::isGamePathValid(Params.Game.Dir, Params.Game.Type)) {
    Errors.emplace_back("Game Location is not valid.");
  }

  // Mod Manager
  if (Params.ModManager.Type == ModManagerDirectory::ModManagerType::ModOrganizer2) {
    if (Params.ModManager.MO2InstanceDir.empty()) {
      Errors.emplace_back("MO2 Instance Location is required");
    }

    if (!filesystem::exists(Params.ModManager.MO2InstanceDir)) {
      Errors.emplace_back("MO2 Instance Location does not exist");
    }

    if (Params.ModManager.MO2Profile.empty()) {
      Errors.emplace_back("MO2 Profile is required");
    }

    // Check if the MO2 profile exists
    filesystem::path ProfilesDir = Params.ModManager.MO2InstanceDir / "profiles" / Params.ModManager.MO2Profile;
    if (!filesystem::exists(ProfilesDir)) {
      Errors.emplace_back("MO2 Profile does not exist");
    }
  }

  // Output
  if (Params.Output.Dir.empty()) {
    Errors.emplace_back("Output Location is required");
  }

  // Processing

  // Pre-Patchers

  // Shader Patchers

  // Shader Transforms
  if (Params.ShaderTransforms.ParallaxToCM && (!Params.ShaderPatcher.Parallax || !Params.ShaderPatcher.ComplexMaterial)) {
    Errors.emplace_back("Upgrade Parallax to Complex Material requires both the Complex Material and Parallax shader patchers");
  }

  if (Params.ShaderTransforms.ParallaxToCM && !Params.Processing.GPUAcceleration) {
    Errors.emplace_back("Upgrade Parallax to Complex Material requires GPU acceleration to be enabled");
  }

  // Post-Patchers

  return Errors.empty();
}

void ParallaxGenConfig::saveUserConfig() {
  // build output json
  nlohmann::json J = UserConfig;

  vector<string> ModOrderNarrowStr;
  ModOrderNarrowStr.reserve(ModOrder.size());
  for (const auto &Mod : ModOrder) {
    ModOrderNarrowStr.push_back(UTF16toUTF8(Mod));
  }
  J["mod_order"] = ModOrderNarrowStr;

  // Params

  // "game"
  J["params"]["game"]["dir"] = UTF16toUTF8(Params.Game.Dir.wstring());
  J["params"]["game"]["type"] = Params.Game.Type;

  // "modmanager"
  J["params"]["modmanager"]["type"] = Params.ModManager.Type;
  J["params"]["modmanager"]["mo2instancedir"] = UTF16toUTF8(Params.ModManager.MO2InstanceDir.wstring());
  J["params"]["modmanager"]["mo2profile"] = UTF16toUTF8(Params.ModManager.MO2Profile);

  // "output"
  J["params"]["output"]["dir"] = UTF16toUTF8(Params.Output.Dir.wstring());
  J["params"]["output"]["zip"] = Params.Output.Zip;

  // "processing"
  J["params"]["processing"]["multithread"] = Params.Processing.Multithread;
  J["params"]["processing"]["highmem"] = Params.Processing.HighMem;
  J["params"]["processing"]["gpuacceleration"] = Params.Processing.GPUAcceleration;
  J["params"]["processing"]["bsa"] = Params.Processing.BSA;
  J["params"]["processing"]["pluginpatching"] = Params.Processing.PluginPatching;
  J["params"]["processing"]["mapfrommeshes"] = Params.Processing.MapFromMeshes;

  // "prepatcher"
  J["params"]["prepatcher"]["disablemlp"] = Params.PrePatcher.DisableMLP;

  // "shaderpatcher"
  J["params"]["shaderpatcher"]["parallax"] = Params.ShaderPatcher.Parallax;
  J["params"]["shaderpatcher"]["complexmaterial"] = Params.ShaderPatcher.ComplexMaterial;
  J["params"]["shaderpatcher"]["truepbr"] = Params.ShaderPatcher.TruePBR;

  // "shadertransforms"
  J["params"]["shadertransforms"]["parallaxtocm"] = Params.ShaderTransforms.ParallaxToCM;

  // "postpatcher"
  J["params"]["postpatcher"]["optimizemeshes"] = Params.PostPatcher.OptimizeMeshes;

  // Update UserConfig var
  UserConfig = J;

  // write to file
  try {
    ofstream OutFile(getUserConfigFile());
    OutFile << J.dump(2) << endl;
    OutFile.close();
  } catch (const exception &E) {
    spdlog::error("Failed to save user config: {}", E.what());
  }
}

auto ParallaxGenConfig::PGParams::getString() const -> wstring {
  wstring OutStr;
  OutStr += L"GameDir: " + Game.Dir.wstring() + L"\n";
  OutStr += L"GameType: " + UTF8toUTF16(BethesdaGame::getStrFromGameType(Game.Type)) + L"\n";
  OutStr += L"ModManagerType: " + UTF8toUTF16(ModManagerDirectory::getStrFromModManagerType(ModManager.Type)) + L"\n";
  OutStr += L"MO2InstanceDir: " + ModManager.MO2InstanceDir.wstring() + L"\n";
  OutStr += L"MO2Profile: " + ModManager.MO2Profile + L"\n";
  OutStr += L"OutputDir: " + Output.Dir.wstring() + L"\n";
  OutStr += L"ZipOutput: " + to_wstring(static_cast<int>(Output.Zip)) + L"\n";
  OutStr += L"Multithread: " + to_wstring(static_cast<int>(Processing.Multithread)) + L"\n";
  OutStr += L"HighMem: " + to_wstring(static_cast<int>(Processing.HighMem)) + L"\n";
  OutStr += L"GPUAcceleration: " + to_wstring(static_cast<int>(Processing.GPUAcceleration)) + L"\n";
  OutStr += L"BSA: " + to_wstring(static_cast<int>(Processing.BSA)) + L"\n";
  OutStr += L"PluginPatching: " + to_wstring(static_cast<int>(Processing.PluginPatching)) + L"\n";
  OutStr += L"MapFromMeshes: " + to_wstring(static_cast<int>(Processing.MapFromMeshes)) + L"\n";
  OutStr += L"DisableMLP: " + to_wstring(static_cast<int>(PrePatcher.DisableMLP)) + L"\n";
  OutStr += L"Parallax: " + to_wstring(static_cast<int>(ShaderPatcher.Parallax)) + L"\n";
  OutStr += L"ComplexMaterial: " + to_wstring(static_cast<int>(ShaderPatcher.ComplexMaterial)) + L"\n";
  OutStr += L"TruePBR: " + to_wstring(static_cast<int>(ShaderPatcher.TruePBR)) + L"\n";
  OutStr += L"ParallaxToCM: " + to_wstring(static_cast<int>(ShaderTransforms.ParallaxToCM)) + L"\n";
  OutStr += L"OptimizeMeshes: " + to_wstring(static_cast<int>(PostPatcher.OptimizeMeshes));

  return OutStr;
}
