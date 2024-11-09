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

  filesystem::path DefConfPath = ExePath / "cfg";
  // Loop through all files in DefConfPath recursively directoryiterator
  for (const auto &Entry : filesystem::recursive_directory_iterator(DefConfPath)) {
    if (filesystem::is_regular_file(Entry) && Entry.path().filename().extension() == ".json") {
      spdlog::debug(L"Loading ParallaxGen Config: {}", Entry.path().wstring());

      nlohmann::json J;
      if (!parseJSON(Entry.path(), getFileBytes(Entry.path()), J)) {
        continue;
      }

      if (!validateJSON(Entry.path(), J)) {
        continue;
      }

      if (!J.empty()) {
        replaceForwardSlashes(J);
        addConfigJSON(J);
        NumConfigs++;
      }
    }
  }

  // Print number of configs loaded
  spdlog::info("Loaded {} ParallaxGen configs successfully", NumConfigs);
}

auto ParallaxGenConfig::addConfigJSON(const nlohmann::json &J) -> void {
  // "dyncubemap_blocklist" field
  if (J.contains("dyncubemap_blocklist")) {
    for (const auto &Item : J["dyncubemap_blocklist"]) {
      DynCubemapBlocklist.insert(strToWstr(boost::to_lower_copy(Item.get<string>())));
    }
  }

  // "nif_blocklist" field
  if (J.contains("nif_blocklist")) {
    for (const auto &Item : J["nif_blocklist"]) {
      NIFBlocklist.insert(strToWstr(boost::to_lower_copy(Item.get<string>())));
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
      VanillaBSAList.insert(strToWstr(boost::to_lower_copy(Item.get<string>())));
    }
  }

  // "mod_order" field
  if (J.contains("mod_order")) {
    lock_guard<mutex> Lock(ModOrderMutex);

    ModOrder.clear();
    for (const auto &Item : J["mod_order"]) {
      auto Mod = strToWstr(Item.get<string>());
      ModOrder.push_back(Mod);
      ModPriority[Mod] = static_cast<int>(ModOrder.size() - 1);
    }
  }

  // "params" field
  if (J.contains("params")) {
    const auto &ParamJ = J["params"];

    // "game"
    ParamJ["game"]["dir"].get_to<filesystem::path>(Params.Game.Dir);
    ParamJ["game"]["type"].get_to<BethesdaGame::GameType>(Params.Game.Type);

    // "modmanager"
    ParamJ["modmanager"]["type"].get_to<ModManagerDirectory::ModManagerType>(Params.ModManager.Type);
    ParamJ["modmanager"]["mo2instancedir"].get_to<filesystem::path>(Params.ModManager.MO2InstanceDir);
    Params.ModManager.MO2Profile = strToWstr(ParamJ["modmanager"]["mo2profile"].get<string>());

    // "output"
    ParamJ["output"]["dir"].get_to<filesystem::path>(Params.Output.Dir);
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
  }
}

auto ParallaxGenConfig::parseJSON(const std::filesystem::path &JSONFile, const vector<std::byte> &Bytes,
                                  nlohmann::json &J) -> bool {
  // Parse JSON
  try {
    J = nlohmann::json::parse(Bytes);
  } catch (const nlohmann::json::parse_error &E) {
    spdlog::error(L"Error parsing JSON file {}: {}", JSONFile.wstring(), strToWstr(E.what()));
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
    spdlog::error(L"Invalid JSON file {}: {}", JSONFile.wstring(), strToWstr(E.what()));
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

void ParallaxGenConfig::saveUserConfig() const {
  // build output json
  nlohmann::json J;

  // Placeholders for future fields
  // TODO
  J["dyncubemap_blocklist"] = nlohmann::json::array();
  J["nif_blocklist"] = nlohmann::json::array();
  J["texture_maps"] = nlohmann::json::object();
  J["vanilla_bsas"] = nlohmann::json::array();

  vector<string> ModOrderNarrowStr;
  ModOrderNarrowStr.reserve(ModOrder.size());
  for (const auto &Mod : ModOrder) {
    ModOrderNarrowStr.push_back(wstrToStr(Mod));
  }
  J["mod_order"] = ModOrderNarrowStr;

  // Params

  // "game"
  J["params"]["game"]["dir"] = wstrToStr(Params.Game.Dir.wstring());
  J["params"]["game"]["type"] = Params.Game.Type;

  // "modmanager"
  J["params"]["modmanager"]["type"] = Params.ModManager.Type;
  J["params"]["modmanager"]["mo2instancedir"] = wstrToStr(Params.ModManager.MO2InstanceDir.wstring());
  J["params"]["modmanager"]["mo2profile"] = wstrToStr(Params.ModManager.MO2Profile);

  // "output"
  J["params"]["output"]["dir"] = wstrToStr(Params.Output.Dir.wstring());
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

  // write to file
  try {
    ofstream OutFile(getUserConfigFile());
    OutFile << J.dump(2) << endl;
    OutFile.close();
  } catch (const exception &E) {
    spdlog::error(L"Failed to save user config: {}", strToWstr(E.what()));
  }
}

auto ParallaxGenConfig::PGParams::getString() const -> wstring {
  wstring OutStr;
  OutStr += L"GameDir: " + Game.Dir.wstring() + L"\n";
  OutStr += L"GameType: " + strToWstr(BethesdaGame::getStrFromGameType(Game.Type)) + L"\n";
  OutStr += L"ModManagerType: " + strToWstr(ModManagerDirectory::getStrFromModManagerType(ModManager.Type)) + L"\n";
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
