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
#include <unordered_set>

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

auto ParallaxGenConfig::getDefaultParams() -> PGParams {
  PGParams OutParams;

  // Game
  OutParams.Game.Dir = BethesdaGame::findGamePathFromSteam(BethesdaGame::GameType::SKYRIM_SE);

  // Output
  OutParams.Output.Dir = ExePath / "ParallaxGen_Output";

  // Mesh Rules
  static const vector<wstring> DefaultMeshBlocklist = {
      L"*\\cameras\\*", L"*\\dyndolod\\*", L"*\\lod\\*", L"*\\magic\\*", L"*\\markers\\*", L"*\\mps\\*", L"*\\sky\\*"};
  OutParams.MeshRules.BlockList = DefaultMeshBlocklist;

  // Texture Rules
  static const vector<wstring> DefaultVanillaBSAList = {L"Skyrim - Textures0.bsa",
                                                        L"Skyrim - Textures1.bsa",
                                                        L"Skyrim - Textures2.bsa",
                                                        L"Skyrim - Textures3.bsa",
                                                        L"Skyrim - Textures4.bsa",
                                                        L"Skyrim - Textures5.bsa",
                                                        L"Skyrim - Textures6.bsa",
                                                        L"Skyrim - Textures7.bsa",
                                                        L"Skyrim - Textures8.bsa",
                                                        L"Project Clarity AIO Half Res Packed.bsa",
                                                        L"Project Clarity AIO Half Res Packed - Textures.bsa",
                                                        L"Project Clarity AIO Half Res Packed0 - Textures.bsa",
                                                        L"Project Clarity AIO Half Res Packed1 - Textures.bsa",
                                                        L"Project Clarity AIO Half Res Packed2 - Textures.bsa",
                                                        L"Project Clarity AIO Half Res Packed3 - Textures.bsa",
                                                        L"Project Clarity AIO Half Res Packed4 - Textures.bsa",
                                                        L"Project Clarity AIO Half Res Packed5 - Textures.bsa",
                                                        L"Project Clarity AIO Half Res Packed6 - Textures.bsa"};
  OutParams.TextureRules.VanillaBSAList = DefaultVanillaBSAList;

  return OutParams;
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

  if (filesystem::exists(getUserConfigFile())) {
    // don't load a config that doesn't exist
    spdlog::debug(L"Loading ParallaxGen Config: {}", getUserConfigFile().wstring());

    nlohmann::json J;
    bool ProcessJSON = true;
    if (!parseJSON(getUserConfigFile(), getFileBytes(getUserConfigFile()), J)) {
      ProcessJSON = false;
    }

    if (!validateJSON(getUserConfigFile(), J)) {
      ProcessJSON = false;
    }

    if (ProcessJSON) {
      if (!J.empty()) {
        replaceForwardSlashes(J);
        addConfigJSON(J);
        NumConfigs++;
      }

      if (getUserConfigFile() == getUserConfigFile()) {
        UserConfig = J;
      }
    }
  } else {
    Params = getDefaultParams();
  }

  // Print number of configs loaded
  spdlog::info("Loaded {} ParallaxGen configs successfully", NumConfigs);
}

auto ParallaxGenConfig::addConfigJSON(const nlohmann::json &J) -> void {
  // TODO UNIFORM check if to_lower_copy can handle UTF8

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
    if (ParamJ.contains("game") && ParamJ["game"].contains("dir")) {
      Params.Game.Dir = UTF8toUTF16(ParamJ["game"]["dir"].get<string>());
    }
    if (ParamJ.contains("game") && ParamJ["game"].contains("type")) {
      ParamJ["game"]["type"].get_to<BethesdaGame::GameType>(Params.Game.Type);
    }

    // "modmanager"
    if (ParamJ.contains("modmanager") && ParamJ["modmanager"].contains("type")) {
      ParamJ["modmanager"]["type"].get_to<ModManagerDirectory::ModManagerType>(Params.ModManager.Type);
    }
    if (ParamJ.contains("modmanager") && ParamJ["modmanager"].contains("mo2instancedir")) {
      ParamJ["modmanager"]["mo2instancedir"].get_to<filesystem::path>(Params.ModManager.MO2InstanceDir);
    }
    if (ParamJ.contains("modmanager") && ParamJ["modmanager"].contains("mo2profile")) {
      Params.ModManager.MO2Profile = UTF8toUTF16(ParamJ["modmanager"]["mo2profile"].get<string>());
    }
    if (ParamJ.contains("modmanager") && ParamJ["modmanager"].contains("mo2useorder")) {
      ParamJ["modmanager"]["mo2useorder"].get_to<bool>(Params.ModManager.MO2UseOrder);
    }

    // "output"
    if (ParamJ.contains("output") && ParamJ["output"].contains("dir")) {
      Params.Output.Dir = UTF8toUTF16(ParamJ["output"]["dir"].get<string>());
    }
    if (ParamJ.contains("output") && ParamJ["output"].contains("zip")) {
      ParamJ["output"]["zip"].get_to<bool>(Params.Output.Zip);
    }

    // "advanced"
    if (ParamJ.contains("advanced")) {
      ParamJ["advanced"].get_to<bool>(Params.Advanced);
    }

    // "processing"
    if (ParamJ.contains("processing") && ParamJ["processing"].contains("multithread")) {
      ParamJ["processing"]["multithread"].get_to<bool>(Params.Processing.Multithread);
    }
    if (ParamJ.contains("processing") && ParamJ["processing"].contains("highmem")) {
      ParamJ["processing"]["highmem"].get_to<bool>(Params.Processing.HighMem);
    }
    if (ParamJ.contains("processing") && ParamJ["processing"].contains("gpuacceleration")) {
      ParamJ["processing"]["gpuacceleration"].get_to<bool>(Params.Processing.GPUAcceleration);
    }
    if (ParamJ.contains("processing") && ParamJ["processing"].contains("bsa")) {
      ParamJ["processing"]["bsa"].get_to<bool>(Params.Processing.BSA);
    }
    if (ParamJ.contains("processing") && ParamJ["processing"].contains("pluginpatching")) {
      ParamJ["processing"]["pluginpatching"].get_to<bool>(Params.Processing.PluginPatching);
    }
    if (ParamJ.contains("processing") && ParamJ["processing"].contains("pluginesmify")) {
      ParamJ["processing"]["pluginesmify"].get_to<bool>(Params.Processing.PluginESMify);
    }
    if (ParamJ.contains("processing") && ParamJ["processing"].contains("mapfrommeshes")) {
      ParamJ["processing"]["mapfrommeshes"].get_to<bool>(Params.Processing.MapFromMeshes);
    }

    // "prepatcher"
    if (ParamJ.contains("prepatcher") && ParamJ["prepatcher"].contains("disablemlp")) {
      ParamJ["prepatcher"]["disablemlp"].get_to<bool>(Params.PrePatcher.DisableMLP);
    }

    // "shaderpatcher"
    if (ParamJ.contains("shaderpatcher") && ParamJ["shaderpatcher"].contains("parallax")) {
      ParamJ["shaderpatcher"]["parallax"].get_to<bool>(Params.ShaderPatcher.Parallax);
    }
    if (ParamJ.contains("shaderpatcher") && ParamJ["shaderpatcher"].contains("complexmaterial")) {
      ParamJ["shaderpatcher"]["complexmaterial"].get_to<bool>(Params.ShaderPatcher.ComplexMaterial);
    }
    if (ParamJ.contains("shaderpatcher") && ParamJ["shaderpatcher"].contains("complexmaterialdyncubemapblocklist")) {
      for (const auto &Item : ParamJ["shaderpatcher"]["complexmaterialdyncubemapblocklist"]) {
        Params.ShaderPatcher.ShaderPatcherComplexMaterial.ListsDyncubemapBlocklist.push_back(
            UTF8toUTF16(Item.get<string>()));
      }
    }
    if (ParamJ.contains("shaderpatcher") && ParamJ["shaderpatcher"].contains("truepbr")) {
      ParamJ["shaderpatcher"]["truepbr"].get_to<bool>(Params.ShaderPatcher.TruePBR);
    }

    // "shadertransforms"
    if (ParamJ.contains("shadertransforms") && ParamJ["shadertransforms"].contains("parallaxtocm")) {
      ParamJ["shadertransforms"]["parallaxtocm"].get_to<bool>(Params.ShaderTransforms.ParallaxToCM);
    }

    // "postpatcher"
    if (ParamJ.contains("postpatcher") && ParamJ["postpatcher"].contains("optimizemeshes")) {
      ParamJ["postpatcher"]["optimizemeshes"].get_to<bool>(Params.PostPatcher.OptimizeMeshes);
    }

    // "meshrules"
    if (ParamJ.contains("meshrules") && ParamJ["meshrules"].contains("allowlist")) {
      for (const auto &Item : ParamJ["meshrules"]["allowlist"]) {
        Params.MeshRules.AllowList.push_back(UTF8toUTF16(Item.get<string>()));
      }
    }

    if (ParamJ.contains("meshrules") && ParamJ["meshrules"].contains("blocklist")) {
      for (const auto &Item : ParamJ["meshrules"]["blocklist"]) {
        Params.MeshRules.BlockList.push_back(UTF8toUTF16(Item.get<string>()));
      }
    }

    // "texturerules"
    if (ParamJ.contains("texturerules") && ParamJ["texturerules"].contains("texturemaps")) {
      for (const auto &Item : ParamJ["texturerules"]["texturemaps"].items()) {
        Params.TextureRules.TextureMaps.emplace_back(UTF8toUTF16(Item.key()),
                                                     NIFUtil::getTexTypeFromStr(Item.value().get<string>()));
      }
    }

    if (ParamJ.contains("texturerules") && ParamJ["texturerules"].contains("vanillabsalist")) {
      for (const auto &Item : ParamJ["texturerules"]["vanillabsalist"]) {
        Params.TextureRules.VanillaBSAList.push_back(UTF8toUTF16(Item.get<string>()));
      }
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
  // Helpers
  unordered_set<wstring> CheckSet;

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
    const auto Profiles = ModManagerDirectory::getMO2ProfilesFromInstanceDir(Params.ModManager.MO2InstanceDir);
    if (find(Profiles.begin(), Profiles.end(), Params.ModManager.MO2Profile) == Profiles.end()) {
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

  CheckSet.clear();
  for (const auto &Item : Params.ShaderPatcher.ShaderPatcherComplexMaterial.ListsDyncubemapBlocklist) {
    if (!CheckSet.insert(Item).second) {
      Errors.emplace_back("Duplicate entry in Complex Material Dyncubemap Block List: " + UTF16toUTF8(Item));
    }
  }

  // Shader Transforms
  if (Params.ShaderTransforms.ParallaxToCM &&
      (!Params.ShaderPatcher.Parallax || !Params.ShaderPatcher.ComplexMaterial)) {
    Errors.emplace_back(
        "Upgrade Parallax to Complex Material requires both the Complex Material and Parallax shader patchers");
  }

  if (Params.ShaderTransforms.ParallaxToCM && !Params.Processing.GPUAcceleration) {
    Errors.emplace_back("Upgrade Parallax to Complex Material requires GPU acceleration to be enabled");
  }

  // Post-Patchers

  // Mesh Rules
  CheckSet.clear();
  for (const auto &Item : Params.MeshRules.AllowList) {
    if (Item.empty()) {
      Errors.emplace_back("Empty entry in Mesh Allow List");
    }

    if (!CheckSet.insert(Item).second) {
      Errors.emplace_back("Duplicate entry in Mesh Allow List: " + UTF16toUTF8(Item));
    }
  }

  CheckSet.clear();
  for (const auto &Item : Params.MeshRules.BlockList) {
    if (Item.empty()) {
      Errors.emplace_back("Empty entry in Mesh Block List");
    }

    if (!CheckSet.insert(Item).second) {
      Errors.emplace_back("Duplicate entry in Mesh Block List: " + UTF16toUTF8(Item));
    }
  }

  // Texture Rules

  CheckSet.clear();
  for (const auto &[Key, Value] : Params.TextureRules.TextureMaps) {
    if (Key.empty()) {
      Errors.emplace_back("Empty key in Texture Rules");
    }

    if (!CheckSet.insert(Key).second) {
      Errors.emplace_back("Duplicate entry in Texture Rules: " + UTF16toUTF8(Key));
    }
  }

  CheckSet.clear();
  for (const auto &Item : Params.TextureRules.VanillaBSAList) {
    if (Item.empty()) {
      Errors.emplace_back("Empty entry in Vanilla BSA List");
    }

    if (!CheckSet.insert(Item).second) {
      Errors.emplace_back("Duplicate entry in Vanilla BSA List: " + UTF16toUTF8(Item));
    }
  }

  return Errors.empty();
}

void ParallaxGenConfig::saveUserConfig() {
  // build output json
  nlohmann::json J = UserConfig;

  // Mod Order
  J["mod_order"] = utf16VectorToUTF8(ModOrder);

  // Params

  // "game"
  J["params"]["game"]["dir"] = UTF16toUTF8(Params.Game.Dir.wstring());
  J["params"]["game"]["type"] = Params.Game.Type;

  // "modmanager"
  J["params"]["modmanager"]["type"] = Params.ModManager.Type;
  J["params"]["modmanager"]["mo2instancedir"] = UTF16toUTF8(Params.ModManager.MO2InstanceDir.wstring());
  J["params"]["modmanager"]["mo2profile"] = UTF16toUTF8(Params.ModManager.MO2Profile);
  J["params"]["modmanager"]["mo2useorder"] = Params.ModManager.MO2UseOrder;

  // "output"
  J["params"]["output"]["dir"] = UTF16toUTF8(Params.Output.Dir.wstring());
  J["params"]["output"]["zip"] = Params.Output.Zip;

  // "advanced"
  J["params"]["advanced"] = Params.Advanced;

  // "processing"
  J["params"]["processing"]["multithread"] = Params.Processing.Multithread;
  J["params"]["processing"]["highmem"] = Params.Processing.HighMem;
  J["params"]["processing"]["gpuacceleration"] = Params.Processing.GPUAcceleration;
  J["params"]["processing"]["bsa"] = Params.Processing.BSA;
  J["params"]["processing"]["pluginpatching"] = Params.Processing.PluginPatching;
  J["params"]["processing"]["pluginesmify"] = Params.Processing.PluginESMify;
  J["params"]["processing"]["mapfrommeshes"] = Params.Processing.MapFromMeshes;

  // "prepatcher"
  J["params"]["prepatcher"]["disablemlp"] = Params.PrePatcher.DisableMLP;

  // "shaderpatcher"
  J["params"]["shaderpatcher"]["parallax"] = Params.ShaderPatcher.Parallax;
  J["params"]["shaderpatcher"]["complexmaterial"] = Params.ShaderPatcher.ComplexMaterial;
  J["params"]["shaderpatcher"]["complexmaterialdyncubemapblocklist"] =
      utf16VectorToUTF8(Params.ShaderPatcher.ShaderPatcherComplexMaterial.ListsDyncubemapBlocklist);
  J["params"]["shaderpatcher"]["truepbr"] = Params.ShaderPatcher.TruePBR;

  // "shadertransforms"
  J["params"]["shadertransforms"]["parallaxtocm"] = Params.ShaderTransforms.ParallaxToCM;

  // "postpatcher"
  J["params"]["postpatcher"]["optimizemeshes"] = Params.PostPatcher.OptimizeMeshes;

  // "meshrules"
  J["params"]["meshrules"]["allowlist"] = utf16VectorToUTF8(Params.MeshRules.AllowList);
  J["params"]["meshrules"]["blocklist"] = utf16VectorToUTF8(Params.MeshRules.BlockList);

  // "texturerules"
  for (const auto &[Key, Value] : Params.TextureRules.TextureMaps) {
    J["params"]["texturerules"]["texturemaps"][UTF16toUTF8(Key)] = NIFUtil::getStrFromTexType(Value);
  }
  J["params"]["texturerules"]["vanillabsalist"] = utf16VectorToUTF8(Params.TextureRules.VanillaBSAList);

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

auto ParallaxGenConfig::utf16VectorToUTF8(const vector<wstring> &Vec) -> vector<string> {
  vector<string> Out;
  Out.reserve(Vec.size());
  for (const auto &Item : Vec) {
    Out.push_back(UTF16toUTF8(Item));
  }

  return Out;
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
