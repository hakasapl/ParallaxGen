#include "patchers/PatcherTruePBR.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenUtil.hpp"

#include <Shaders.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <cstddef>
#include <spdlog/spdlog.h>
#include <string>

using namespace std;

// Statics
ParallaxGenDirectory *PatcherTruePBR::PGD;

PatcherTruePBR::PatcherTruePBR(filesystem::path NIFPath, nifly::NifFile *NIF)
    : NIFPath(std::move(NIFPath)), NIF(NIF) {}

auto PatcherTruePBR::getTruePBRConfigs() -> map<size_t, nlohmann::json> & {
  static map<size_t, nlohmann::json> TruePBRConfigs = {};
  return TruePBRConfigs;
}

auto PatcherTruePBR::getPathLookupJSONs() -> map<size_t, nlohmann::json> & {
  static map<size_t, nlohmann::json> PathLookupJSONs = {};
  return PathLookupJSONs;
}

auto PatcherTruePBR::getTruePBRDiffuseInverse() -> map<wstring, vector<size_t>> & {
  static map<wstring, vector<size_t>> TruePBRDiffuseInverse = {};
  return TruePBRDiffuseInverse;
}

auto PatcherTruePBR::getTruePBRNormalInverse() -> map<wstring, vector<size_t>> & {
  static map<wstring, vector<size_t>> TruePBRNormalInverse = {};
  return TruePBRNormalInverse;
}

auto PatcherTruePBR::getPathLookupCache() -> unordered_map<tuple<wstring, wstring>, bool, TupleStrHash> & {
  static unordered_map<tuple<wstring, wstring>, bool, TupleStrHash> PathLookupCache = {};
  return PathLookupCache;
}

auto PatcherTruePBR::getTruePBRConfigFilenameFields() -> vector<string> {
  static const vector<string> PGConfigFilenameFields = {"match_normal", "match_diffuse", "rename"};
  return PGConfigFilenameFields;
}

void PatcherTruePBR::loadPatcherBuffers(const std::vector<std::filesystem::path> &PBRJSONs, ParallaxGenDirectory *PGD) {
  PatcherTruePBR::PGD = PGD;

  size_t ConfigOrder = 0;
  for (const auto &Config : PBRJSONs) {
    // check if Config is valid
    auto ConfigFileBytes = PGD->getFile(Config);
    string ConfigFileStr(reinterpret_cast<const char *>(ConfigFileBytes.data()), ConfigFileBytes.size());

    try {
      nlohmann::json J = nlohmann::json::parse(ConfigFileStr);
      // loop through each Element
      for (auto &Element : J) {
        // Preprocessing steps here
        if (Element.contains("texture")) {
          Element["match_diffuse"] = Element["texture"];
        }

        Element["json"] = Config.string();

        // loop through filename Fields
        for (const auto &Field : getTruePBRConfigFilenameFields()) {
          if (Element.contains(Field) && !boost::istarts_with(Element[Field].get<string>(), "\\")) {
            Element[Field] = Element[Field].get<string>().insert(0, 1, '\\');
          }
        }

        spdlog::trace("TruePBR Config {} Loaded: {}", ConfigOrder, Element.dump());
        getTruePBRConfigs()[ConfigOrder++] = Element;
      }
    } catch (nlohmann::json::parse_error &E) {
      spdlog::error(L"Unable to parse TruePBR Config file {}: {}", Config.wstring(), ParallaxGenUtil::strToWstr(E.what()));
      continue;
    }
  }

  spdlog::info("Found {} TruePBR entries", getTruePBRConfigs().size());

  // Create helper vectors
  for (const auto &Config : getTruePBRConfigs()) {
    // "match_normal" attribute
    if (Config.second.contains("match_normal")) {
      auto RevNormal = ParallaxGenUtil::strToWstr(Config.second["match_normal"].get<string>());
      RevNormal = NIFUtil::getTexBase(RevNormal);
      reverse(RevNormal.begin(), RevNormal.end());

      getTruePBRNormalInverse()[boost::to_lower_copy(RevNormal)].push_back(Config.first);
      continue;
    }

    // "match_diffuse" attribute
    if (Config.second.contains("match_diffuse")) {
      auto RevDiffuse = ParallaxGenUtil::strToWstr(Config.second["match_diffuse"].get<string>());
      RevDiffuse = NIFUtil::getTexBase(RevDiffuse);
      reverse(RevDiffuse.begin(), RevDiffuse.end());

      getTruePBRDiffuseInverse()[boost::to_lower_copy(RevDiffuse)].push_back(Config.first);
    }

    // "path_contains" attribute
    if (Config.second.contains("path_contains")) {
      getPathLookupJSONs()[Config.first] = Config.second;
    }
  }
}

auto PatcherTruePBR::shouldApply(nifly::NiShape *NIFShape, const array<wstring, NUM_TEXTURE_SLOTS> &SearchPrefixes,
                                 bool &EnableResult,
                                 map<size_t, tuple<nlohmann::json, wstring>> &TruePBRData) -> ParallaxGenTask::PGResult {
  // Prep
  auto *NIFShader = NIF->GetShader(NIFShape);
  auto *const NIFShaderBSLSP = dynamic_cast<BSLightingShaderProperty *>(NIFShader);
  auto TextureSetBlockID = NIFShaderBSLSP->TextureSetRef()->index;

  // Check if we already matched this texture set
  if (MatchedTextureSets.find(TextureSetBlockID) != MatchedTextureSets.end()) {
    TruePBRData = MatchedTextureSets[TextureSetBlockID];
    EnableResult = true;
    return ParallaxGenTask::PGResult::SUCCESS;
  }

  const auto ShapeBlockID = NIF->GetBlockID(NIFShape);
  wstring LogPrefix = L"NIF: " + NIFPath.wstring() + L" | Shape: " + to_wstring(ShapeBlockID) + L" | ";

  spdlog::trace(L"{}PBR | Starting checking", LogPrefix);

  if (shouldApplySlots(LogPrefix, SearchPrefixes, NIFPath, TruePBRData)) {
    EnableResult = true;
    spdlog::trace(L"{}PBR | {} PBR Configs matched", TruePBRData.size(), LogPrefix);

    // Add to good to go set
    MatchedTextureSets[TextureSetBlockID] = TruePBRData;
  } else {
    EnableResult = false;
    spdlog::trace(L"{}PBR | No PBR Configs matched", LogPrefix);
  }

  return ParallaxGenTask::PGResult::SUCCESS;
}

auto PatcherTruePBR::shouldApplySlots(const wstring &LogPrefix, const std::array<std::wstring, NUM_TEXTURE_SLOTS> &SearchPrefixes, const std::wstring &NIFPath, std::map<size_t, std::tuple<nlohmann::json, std::wstring>> &TruePBRData) -> bool {
  // Stores the json filename that gets priority over this shape
  wstring PriorityJSONFile;

  // "match_normal" attribute: Binary search for normal map
  getSlotMatch(LogPrefix, TruePBRData, PriorityJSONFile, SearchPrefixes[1], getTruePBRNormalInverse(),
               L"match_normal", NIFPath);

  // "match_diffuse" attribute: Binary search for diffuse map
  getSlotMatch(LogPrefix, TruePBRData, PriorityJSONFile, SearchPrefixes[0], getTruePBRDiffuseInverse(),
               L"match_diffuse", NIFPath);

  // "path_contains" attribute: Linear search for path_contains
  getPathContainsMatch(LogPrefix, TruePBRData, PriorityJSONFile, SearchPrefixes[0], NIFPath);

  return TruePBRData.size() > 0;
}

auto PatcherTruePBR::getSlotMatch(const wstring &LogPrefix, map<size_t, tuple<nlohmann::json, wstring>> &TruePBRData,
                                  wstring &PriorityJSONFile, const wstring &TexName,
                                  const map<wstring, vector<size_t>> &Lookup, const wstring &SlotLabel, const wstring &NIFPath) -> void {
  // binary search for map
  auto MapReverse = boost::to_lower_copy(TexName);
  reverse(MapReverse.begin(), MapReverse.end());
  auto It = Lookup.lower_bound(MapReverse);

  // Check if match is 1 back
  if (It != Lookup.begin() && boost::starts_with(MapReverse, prev(It)->first)) {
    It = prev(It);
  } else if (It != Lookup.end() && boost::starts_with(MapReverse, It->first)) {
    // Check if match is current iterator, just continue here
  } else {
    // No match found
    spdlog::trace(L"{}PBR | {} | No PBR JSON match found for \"{}\"", LogPrefix, SlotLabel, TexName);
    return;
  }

  // Initialize CFG set
  set<size_t> Cfgs(It->second.begin(), It->second.end());

  // Go back to include others if we need to
  while (It != Lookup.begin() && boost::starts_with(MapReverse, prev(It)->first)) {
    It = prev(It);
    Cfgs.insert(It->second.begin(), It->second.end());
  }

  spdlog::trace(L"{}PBR | {} | Matched {} PBR JSONs for \"{}\"", LogPrefix, SlotLabel, Cfgs.size(), TexName);

  // Loop through all matches
  for (const auto &Cfg : Cfgs) {
    insertTruePBRData(LogPrefix, TruePBRData, PriorityJSONFile, TexName, Cfg, NIFPath);
  }
}

auto PatcherTruePBR::getPathContainsMatch(const wstring &LogPrefix,
                                          std::map<size_t, std::tuple<nlohmann::json, std::wstring>> &TruePBRData,
                                          std::wstring &PriorityJSONFile, const std::wstring &Diffuse, const wstring &NIFPath) -> void {
  // "patch_contains" attribute: Linear search for path_contains
  auto &Cache = getPathLookupCache();

  // Check for path_contains only if no name match because it's a O(n) operation
  size_t NumMatches = 0;
  for (const auto &Config : getPathLookupJSONs()) {
    // Check if in cache
    auto CacheKey = make_tuple(ParallaxGenUtil::strToWstr(Config.second["path_contains"].get<string>()), Diffuse);

    bool PathMatch = false;
    if (Cache.find(CacheKey) == Cache.end()) {
      // Not in cache, update it
      Cache[CacheKey] = boost::icontains(Diffuse, get<0>(CacheKey));
    }

    PathMatch = Cache[CacheKey];
    if (PathMatch) {
      NumMatches++;
      insertTruePBRData(LogPrefix, TruePBRData, PriorityJSONFile, Diffuse, Config.first, NIFPath);
    }
  }

  const wstring SlotLabel = L"path_contains";
  if (NumMatches > 0) {
    spdlog::trace(L"{}PBR | {} | Matched {} PBR JSONs for \"{}\"", LogPrefix, SlotLabel, NumMatches, Diffuse);
  } else {
    spdlog::trace(L"{}PBR | {} | No PBR JSON match found for \"{}\"", LogPrefix, SlotLabel, Diffuse);
  }
}

auto PatcherTruePBR::insertTruePBRData(const wstring &LogPrefix,
                                       std::map<size_t, std::tuple<nlohmann::json, std::wstring>> &TruePBRData,
                                       std::wstring &PriorityJSONFile, const wstring &TexName, size_t Cfg, const wstring &NIFPath) -> void {
  const auto CurCfg = getTruePBRConfigs()[Cfg];
  const auto CurJSON = ParallaxGenUtil::strToWstr(CurCfg["json"].get<string>());

  // Check if we should skip this due to nif filter (this is expsenive, so we do it last)
  if (CurCfg.contains("nif_filter") && !boost::icontains(NIFPath, CurCfg["nif_filter"].get<string>())) {
    spdlog::trace(L"{}PBR | {} | PBR JSON Rejected: nif_filter {}", LogPrefix, Cfg, NIFPath);
    return;
  }

  // Evaluate JSON priority
  if (PriorityJSONFile.empty()) {
    // Define priority file
    PriorityJSONFile = CurJSON;
    spdlog::trace(L"{}PBR | Setting priority JSON to {}", LogPrefix, PriorityJSONFile);
  }

  if (CurJSON != PriorityJSONFile) {
    // Only use first JSON (highest priority one)
    if (Cfg < TruePBRData.begin()->first) {
      // Checks if we should be replacing the priority JSON (if we found a higher priority one)
      TruePBRData.clear();
      PriorityJSONFile = CurJSON;
      spdlog::trace(L"{}PBR | Setting priority JSON to {}", LogPrefix, PriorityJSONFile);
    } else {
      spdlog::trace(L"{}PBR | PBR JSON Rejected {}: Losing JSON Priority", LogPrefix, Cfg);
      return;
    }
  }

  // Find and check prefix value
  // Add the PBR part to the texture path
  auto TexPath = TexName;
  if (boost::istarts_with(TexPath, "textures\\") && !boost::istarts_with(TexPath, "textures\\pbr\\")) {
    TexPath.replace(0, TEXTURE_STR_LENGTH, L"textures\\pbr\\");
  }

  // Get PBR path, which is the path without the matched field
  auto MatchedFieldStr =
      CurCfg.contains("match_normal") ? CurCfg["match_normal"].get<string>() : CurCfg["match_diffuse"].get<string>();
  auto MatchedFieldBase = NIFUtil::getTexBase(MatchedFieldStr);
  TexPath.erase(TexPath.length() - MatchedFieldBase.length(), MatchedFieldBase.length());

  auto MatchedField = ParallaxGenUtil::strToWstr(MatchedFieldStr);

  // "rename" attribute
  if (CurCfg.contains("rename")) {
    auto RenameField = CurCfg["rename"].get<string>();
    if (!boost::iequals(RenameField, MatchedField)) {
      MatchedField = ParallaxGenUtil::strToWstr(RenameField);
    }
  }

  spdlog::trace(L"{}PBR | {} | PBR texture path created: {}", LogPrefix, Cfg, MatchedField);

  // Check if named_field is a directory
  wstring MatchedPath = boost::to_lower_copy(TexPath + MatchedField);
  bool EnableTruePBR = (!CurCfg.contains("pbr") || CurCfg["pbr"]) && !MatchedPath.empty();
  if (!CurCfg.contains("delete") || !CurCfg["delete"].get<bool>()) {
    if (EnableTruePBR && !PGD->isPrefix(MatchedPath)) {
      spdlog::trace(L"{}PBR | {} | PBR JSON Rejected: Path {} is not a prefix", LogPrefix, Cfg, MatchedPath);
      return;
    }
  }

  // If no pbr clear MatchedPath
  if (!EnableTruePBR) {
    MatchedPath = L"";
  }

  spdlog::trace(L"{}PBR | {} | PBR JSON accepted", LogPrefix, Cfg);
  TruePBRData.insert({Cfg, {CurCfg, MatchedPath}});
}

auto PatcherTruePBR::applyPatch(NiShape *NIFShape, nlohmann::json &TruePBRData, const std::wstring &MatchedPath,
                                bool &NIFModified, bool &ShapeDeleted) const -> ParallaxGenTask::PGResult {

  // enable TruePBR on shape
  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // Prep
  auto *NIFShader = NIF->GetShader(NIFShape);
  auto *const NIFShaderBSLSP = dynamic_cast<BSLightingShaderProperty *>(NIFShader);
  const bool EnableTruePBR = !MatchedPath.empty();
  const bool EnableEnvMapping = TruePBRData.contains("env_mapping") && TruePBRData["env_mapping"] && !EnableTruePBR;

  // "delete" attribute
  if (TruePBRData.contains("delete") && TruePBRData["delete"]) {
    NIF->DeleteShape(NIFShape);
    NIFModified = true;
    ShapeDeleted = true;
    return Result;
  }

  // "smooth_angle" attribute
  if (TruePBRData.contains("smooth_angle")) {
    NIF->CalcNormalsForShape(NIFShape, true, true, TruePBRData["smooth_angle"]);
    NIF->CalcTangentsForShape(NIFShape);
    NIFModified = true;
  }

  // "auto_uv" attribute
  if (TruePBRData.contains("auto_uv")) {
    vector<Triangle> Tris;
    NIFShape->GetTriangles(Tris);
    auto NewUVScale =
        autoUVScale(NIF->GetUvsForShape(NIFShape), NIF->GetVertsForShape(NIFShape), Tris) / TruePBRData["auto_uv"];
    NIFUtil::setShaderVec2(NIFShaderBSLSP->uvScale, NewUVScale, NIFModified);
  }

  // "vertex_colors" attribute
  if (TruePBRData.contains("vertex_colors")) {
    auto NewVertexColors = TruePBRData["vertex_colors"].get<bool>();
    if (NIFShape->HasVertexColors() != NewVertexColors) {
      NIFShape->SetVertexColors(NewVertexColors);
      NIFModified = true;
    }

    if (NIFShader->HasVertexColors() != NewVertexColors) {
      NIFShader->SetVertexColors(NewVertexColors);
      NIFModified = true;
    }
  }

  // "specular_level" attribute
  if (TruePBRData.contains("specular_level")) {
    auto NewSpecularLevel = TruePBRData["specular_level"].get<float>();
    if (NIFShader->GetGlossiness() != NewSpecularLevel) {
      NIFShader->SetGlossiness(NewSpecularLevel);
      NIFModified = true;
    }
  }

  // "subsurface_color" attribute
  if (TruePBRData.contains("subsurface_color") && TruePBRData["subsurface_color"].size() >= 3) {
    auto NewSpecularColor =
        Vector3(TruePBRData["subsurface_color"][0].get<float>(), TruePBRData["subsurface_color"][1].get<float>(),
                TruePBRData["subsurface_color"][2].get<float>());
    if (NIFShader->GetSpecularColor() != NewSpecularColor) {
      NIFShader->SetSpecularColor(NewSpecularColor);
      NIFModified = true;
    }
  }

  // "roughness_scale" attribute
  if (TruePBRData.contains("roughness_scale")) {
    auto NewRoughnessScale = TruePBRData["roughness_scale"].get<float>();
    if (NIFShader->GetSpecularStrength() != NewRoughnessScale) {
      NIFShader->SetSpecularStrength(NewRoughnessScale);
      NIFModified = true;
    }
  }

  // "subsurface_opacity" attribute
  if (TruePBRData.contains("subsurface_opacity")) {
    auto NewSubsurfaceOpacity = TruePBRData["subsurface_opacity"].get<float>();
    NIFUtil::setShaderFloat(NIFShaderBSLSP->softlighting, NewSubsurfaceOpacity, NIFModified);
  }

  // "displacement_scale" attribute
  if (TruePBRData.contains("displacement_scale")) {
    auto NewDisplacementScale = TruePBRData["displacement_scale"].get<float>();
    NIFUtil::setShaderFloat(NIFShaderBSLSP->rimlightPower, NewDisplacementScale, NIFModified);
  }

  // "EnvMapping" attribute
  if (EnableEnvMapping) {
    NIFUtil::setShaderType(NIFShader, BSLSP_ENVMAP, NIFModified);
    NIFUtil::setShaderFlag(NIFShaderBSLSP, SLSF1_ENVIRONMENT_MAPPING, NIFModified);
    NIFUtil::setShaderFlag(NIFShaderBSLSP, SLSF2_BACK_LIGHTING, NIFModified);
  }

  // "EnvMap_scale" attribute
  if (TruePBRData.contains("env_map_scale") && EnableEnvMapping) {
    auto NewEnvMapScale = TruePBRData["env_map_scale"].get<float>();
    NIFUtil::setShaderFloat(NIFShaderBSLSP->environmentMapScale, NewEnvMapScale, NIFModified);
  }

  // "EnvMap_scale_mult" attribute
  if (TruePBRData.contains("env_map_scale_mult") && EnableEnvMapping) {
    NIFShaderBSLSP->environmentMapScale *= TruePBRData["env_map_scale_mult"].get<float>();
    NIFModified = true;
  }

  // "cubemap" attribute
  if (TruePBRData.contains("cubemap") && EnableEnvMapping && !flag(TruePBRData, "lock_cubemap")) {
    auto NewCubemap = TruePBRData["cubemap"].get<string>();
    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::CUBEMAP, NewCubemap, NIFModified);
  }

  // "emmissive_scale" attribute
  if (TruePBRData.contains("emissive_scale")) {
    auto NewEmissiveScale = TruePBRData["emissive_scale"].get<float>();
    if (NIFShader->GetEmissiveMultiple() != NewEmissiveScale) {
      NIFShader->SetEmissiveMultiple(NewEmissiveScale);
      NIFModified = true;
    }
  }

  // "emmissive_color" attribute
  if (TruePBRData.contains("emissive_color") && TruePBRData["emissive_color"].size() >= 4) {
    auto NewEmissiveColor =
        Color4(TruePBRData["emissive_color"][0].get<float>(), TruePBRData["emissive_color"][1].get<float>(),
               TruePBRData["emissive_color"][2].get<float>(), TruePBRData["emissive_color"][3].get<float>());
    if (NIFShader->GetEmissiveColor() != NewEmissiveColor) {
      NIFShader->SetEmissiveColor(NewEmissiveColor);
      NIFModified = true;
    }
  }

  // "uv_scale" attribute
  if (TruePBRData.contains("uv_scale")) {
    auto NewUVScale = Vector2(TruePBRData["uv_scale"].get<float>(), TruePBRData["uv_scale"].get<float>());
    NIFUtil::setShaderVec2(NIFShaderBSLSP->uvScale, NewUVScale, NIFModified);
  }

  // "pbr" attribute
  if (EnableTruePBR) {
    // no pbr, we can return here
    enableTruePBROnShape(NIFShape, NIFShader, NIFShaderBSLSP, TruePBRData, MatchedPath, NIFModified);
  }

  // "SlotX" attributes
  for (int I = 0; I < NUM_TEXTURE_SLOTS - 1; I++) {
    string SlotName = "slot" + to_string(I + 1);
    if (TruePBRData.contains(SlotName)) {
      string NewSlot = TruePBRData[SlotName].get<string>();
      NIFUtil::setTextureSlot(NIF, NIFShape, static_cast<NIFUtil::TextureSlots>(I), NewSlot, NIFModified);
    }
  }

  return Result;
}

auto PatcherTruePBR::applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots, const nlohmann::json &TruePBRData, const std::wstring &MatchedPath) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> {
  // TODO don't duplicate this code
  if (MatchedPath.empty()) {
    return OldSlots;
  }

  array<wstring, NUM_TEXTURE_SLOTS> NewSlots = OldSlots;

  // "lock_diffuse" attribute
  if (!flag(TruePBRData, "lock_diffuse")) {
    auto NewDiffuse = MatchedPath + L".dds";
    NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::DIFFUSE)] = NewDiffuse;
  }

  // "lock_normal" attribute
  if (!flag(TruePBRData, "lock_normal")) {
    auto NewNormal = MatchedPath + L"_n.dds";
    NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::NORMAL)] = NewNormal;
  }

  // "emissive" attribute
  if (TruePBRData.contains("emissive") && !flag(TruePBRData, "lock_emissive")) {
    wstring NewGlow;
    if (TruePBRData["emissive"].get<bool>()) {
      NewGlow = MatchedPath + L"_g.dds";
    }

    NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::GLOW)] = NewGlow;
  }

  // "parallax" attribute
  if (TruePBRData.contains("parallax") && !flag(TruePBRData, "lock_parallax")) {
    wstring NewParallax;
    if (TruePBRData["parallax"].get<bool>()) {
      NewParallax = MatchedPath + L"_p.dds";
    }

    NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::PARALLAX)] = NewParallax;
  }

  NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::CUBEMAP)] = L"";

  // "lock_rmaos" attribute
  if (!flag(TruePBRData, "lock_rmaos")) {
    auto NewRMAOS = MatchedPath + L"_rmaos.dds";
    NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::ENVMASK)] = NewRMAOS;
  }

  // "lock_cnr" attribute
  if (!flag(TruePBRData, "lock_cnr")) {
    // "coat_normal" attribute
    wstring NewCNR;
    if (TruePBRData.contains("coat_normal") && TruePBRData["coat_normal"].get<bool>()) {
      NewCNR = MatchedPath + L"_cnr.dds";
    }

    NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::TINT)] = NewCNR;
  }

  // "lock_subsurface" attribute
  if (!flag(TruePBRData, "lock_subsurface")) {
    // "subsurface_foliage" attribute
    wstring NewSubsurface;
    if ((TruePBRData.contains("subsurface_foliage") && TruePBRData["subsurface_foliage"].get<bool>()) ||
        (TruePBRData.contains("subsurface") && TruePBRData["subsurface"].get<bool>()) ||
        (TruePBRData.contains("coat_diffuse") && TruePBRData["coat_diffuse"].get<bool>())) {
      NewSubsurface = MatchedPath + L"_s.dds";
    }

    NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::BACKLIGHT)] = NewSubsurface;
  }

  return NewSlots;
}

auto PatcherTruePBR::enableTruePBROnShape(NiShape *NIFShape, NiShader *NIFShader,
                                          BSLightingShaderProperty *NIFShaderBSLSP, nlohmann::json &TruePBRData,
                                          const wstring &MatchedPath,
                                          bool &NIFModified) const -> ParallaxGenTask::PGResult {

  // enable TruePBR on shape
  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // "lock_diffuse" attribute
  if (!flag(TruePBRData, "lock_diffuse")) {
    auto NewDiffuse = MatchedPath + L".dds";
    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::DIFFUSE, NewDiffuse, NIFModified);
  }

  // "lock_normal" attribute
  if (!flag(TruePBRData, "lock_normal")) {
    auto NewNormal = MatchedPath + L"_n.dds";
    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::NORMAL, NewNormal, NIFModified);
  }

  // "emissive" attribute
  if (TruePBRData.contains("emissive") && !flag(TruePBRData, "lock_emissive")) {
    wstring NewGlow;
    if (TruePBRData["emissive"].get<bool>()) {
      NewGlow = MatchedPath + L"_g.dds";
    }

    NIFUtil::configureShaderFlag(NIFShaderBSLSP, SLSF1_EXTERNAL_EMITTANCE, TruePBRData["emissive"].get<bool>(),
                                 NIFModified);
    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::GLOW, NewGlow, NIFModified);
  }

  // "parallax" attribute
  if (TruePBRData.contains("parallax") && !flag(TruePBRData, "lock_parallax")) {
    wstring NewParallax;
    if (TruePBRData["parallax"].get<bool>()) {
      NewParallax = MatchedPath + L"_p.dds";
    }

    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::PARALLAX, NewParallax, NIFModified);
  }

  const string Slot4;
  NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::CUBEMAP, Slot4, NIFModified);

  // "lock_rmaos" attribute
  if (!flag(TruePBRData, "lock_rmaos")) {
    auto NewRMAOS = MatchedPath + L"_rmaos.dds";
    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::ENVMASK, NewRMAOS, NIFModified);
  }

  // "lock_cnr" attribute
  if (!flag(TruePBRData, "lock_cnr")) {
    // "coat_normal" attribute
    wstring NewCNR;
    if (TruePBRData.contains("coat_normal") && TruePBRData["coat_normal"].get<bool>()) {
      NewCNR = MatchedPath + L"_cnr.dds";
    }

    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::TINT, NewCNR, NIFModified);
  }

  // "lock_subsurface" attribute
  if (!flag(TruePBRData, "lock_subsurface")) {
    // "subsurface_foliage" attribute
    wstring NewSubsurface;
    if ((TruePBRData.contains("subsurface_foliage") && TruePBRData["subsurface_foliage"].get<bool>()) ||
        (TruePBRData.contains("subsurface") && TruePBRData["subsurface"].get<bool>()) ||
        (TruePBRData.contains("coat_diffuse") && TruePBRData["coat_diffuse"].get<bool>())) {
      NewSubsurface = MatchedPath + L"_s.dds";
    }

    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::BACKLIGHT, NewSubsurface, NIFModified);
  }

  // revert to default NIFShader type, remove flags used in other types
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF1_ENVIRONMENT_MAPPING, NIFModified);
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF1_PARALLAX, NIFModified);
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF2_GLOW_MAP, NIFModified);

  // Enable PBR flag
  NIFUtil::setShaderFlag(NIFShaderBSLSP, SLSF2_UNUSED01, NIFModified);

  if (TruePBRData.contains("subsurface_foliage") && TruePBRData["subsurface_foliage"].get<bool>() &&
      TruePBRData.contains("subsurface") && TruePBRData["subsurface"].get<bool>()) {
    spdlog::error("Error: Subsurface and foliage NIFShader chosen at once, undefined behavior!");
  }

  // "subsurface" attribute
  if (TruePBRData.contains("subsurface")) {
    NIFUtil::configureShaderFlag(NIFShaderBSLSP, SLSF2_RIM_LIGHTING, TruePBRData["subsurface"].get<bool>(),
                                 NIFModified);
  }

  // "multilayer" attribute
  bool EnableMultiLayer = false;
  if (TruePBRData.contains("multilayer") && TruePBRData["multilayer"]) {
    EnableMultiLayer = true;

    NIFUtil::setShaderType(NIFShader, BSLSP_MULTILAYERPARALLAX, NIFModified);
    NIFUtil::setShaderFlag(NIFShaderBSLSP, SLSF2_MULTI_LAYER_PARALLAX, NIFModified);

    // "coat_color" attribute
    if (TruePBRData.contains("coat_color") && TruePBRData["coat_color"].size() >= 3) {
      auto NewCoatColor =
          Vector3(TruePBRData["coat_color"][0].get<float>(), TruePBRData["coat_color"][1].get<float>(),
                  TruePBRData["coat_color"][2].get<float>());
      if (NIFShader->GetSpecularColor() != NewCoatColor) {
        NIFShader->SetSpecularColor(NewCoatColor);
        NIFModified = true;
      }
    }

    // "coat_specular_level" attribute
    if (TruePBRData.contains("coat_specular_level")) {
      auto NewCoatSpecularLevel = TruePBRData["coat_specular_level"].get<float>();
      NIFUtil::setShaderFloat(NIFShaderBSLSP->parallaxRefractionScale, NewCoatSpecularLevel, NIFModified);
    }

    // "coat_roughness" attribute
    if (TruePBRData.contains("coat_roughness")) {
      auto NewCoatRoughness = TruePBRData["coat_roughness"].get<float>();
      NIFUtil::setShaderFloat(NIFShaderBSLSP->parallaxInnerLayerThickness, NewCoatRoughness, NIFModified);
    }

    // "coat_strength" attribute
    if (TruePBRData.contains("coat_strength")) {
      auto NewCoatStrength = TruePBRData["coat_strength"].get<float>();
      NIFUtil::setShaderFloat(NIFShaderBSLSP->softlighting, NewCoatStrength, NIFModified);
    }

    // "coat_diffuse" attribute
    if (TruePBRData.contains("coat_diffuse")) {
      NIFUtil::configureShaderFlag(NIFShaderBSLSP, SLSF2_EFFECT_LIGHTING, TruePBRData["coat_diffuse"].get<bool>(),
                                    NIFModified);
    }

    // "coat_parallax" attribute
    if (TruePBRData.contains("coat_parallax")) {
      NIFUtil::configureShaderFlag(NIFShaderBSLSP, SLSF2_SOFT_LIGHTING, TruePBRData["coat_parallax"].get<bool>(),
                                    NIFModified);
    }

    // "coat_normal" attribute
    if (TruePBRData.contains("coat_normal")) {
      NIFUtil::configureShaderFlag(NIFShaderBSLSP, SLSF2_BACK_LIGHTING, TruePBRData["coat_normal"].get<bool>(),
                                    NIFModified);
    }

    // "inner_uv_scale" attribute
    if (TruePBRData.contains("inner_uv_scale")) {
      auto NewInnerUVScale =
          Vector2(TruePBRData["inner_uv_scale"].get<float>(), TruePBRData["inner_uv_scale"].get<float>());
      NIFUtil::setShaderVec2(NIFShaderBSLSP->parallaxInnerLayerTextureScale, NewInnerUVScale, NIFModified);
    }
  } else if (TruePBRData.contains("glint")) {
    // glint is enabled
    const auto &GlintParams = TruePBRData["glint"];

    // Set shader type to MLP
    NIFUtil::setShaderType(NIFShader, BSLSP_MULTILAYERPARALLAX, NIFModified);
    // Enable Glint with FitSlope flag
    NIFUtil::setShaderFlag(NIFShaderBSLSP, SLSF2_FIT_SLOPE, NIFModified);

    // Glint parameters
    if (GlintParams.contains("screen_space_scale")) {
      NIFUtil::setShaderFloat(NIFShaderBSLSP->parallaxInnerLayerThickness, GlintParams["screen_space_scale"],
                              NIFModified);
    }

    if (GlintParams.contains("log_microfacet_density")) {
      NIFUtil::setShaderFloat(NIFShaderBSLSP->parallaxRefractionScale, GlintParams["log_microfacet_density"],
                              NIFModified);
    }

    if (GlintParams.contains("microfacet_roughness")) {
      NIFUtil::setShaderFloat(NIFShaderBSLSP->parallaxInnerLayerTextureScale.u, GlintParams["microfacet_roughness"],
                              NIFModified);
    }

    if (GlintParams.contains("density_randomization")) {
      NIFUtil::setShaderFloat(NIFShaderBSLSP->parallaxInnerLayerTextureScale.v, GlintParams["density_randomization"],
                              NIFModified);
    }
  } else {
    // Revert to default NIFShader type
    NIFUtil::setShaderType(NIFShader, BSLSP_DEFAULT, NIFModified);
  }

  if (!EnableMultiLayer) {
    // Clear multilayer flags
    NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF2_MULTI_LAYER_PARALLAX, NIFModified);
    NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF2_BACK_LIGHTING, NIFModified);
    NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF2_SOFT_LIGHTING, NIFModified);
  }

  return Result;
}

//
// Helpers
//

auto PatcherTruePBR::abs2(Vector2 V) -> Vector2 { return {abs(V.u), abs(V.v)}; }

auto PatcherTruePBR::autoUVScale(const vector<Vector2> *UVs, const vector<Vector3> *Verts,
                                 vector<Triangle> &Tris) -> Vector2 {
  Vector2 Scale;
  for (const Triangle &T : Tris) {
    auto V1 = (*Verts)[T.p1];
    auto V2 = (*Verts)[T.p2];
    auto V3 = (*Verts)[T.p3];
    auto UV1 = (*UVs)[T.p1];
    auto UV2 = (*UVs)[T.p2];
    auto UV3 = (*UVs)[T.p3];

    // auto cross = (V2 - V1).cross(V3 - V1);
    // auto uv_cross = Vector3((UV2 - UV1).u, (UV2 - UV1).v,
    // 0).cross(Vector3((UV3 - UV1).u, (UV3 - UV1).v, 0)); auto s =
    // cross.length() / uv_cross.length(); scale += Vector2(s, s); auto s =
    // (abs(UV2 - UV1) / (V2 - V1).length() + abs(UV3 - UV1) / (V3 -
    // V1).length() + abs(UV2 - UV3) / (V2 - V3).length())/3; scale +=
    // Vector2(1.0 / s.u, 1.0 / s.v);
    auto S = (abs2(UV2 - UV1) + abs2(UV3 - UV1)) / ((V2 - V1).length() + (V3 - V1).length());
    Scale += Vector2(1.0F / S.u, 1.0F / S.v);
  }

  Scale *= 10.0 / 4.0; // NOLINT
  Scale /= static_cast<float>(Tris.size());
  Scale.u = min(Scale.u, Scale.v);
  Scale.v = min(Scale.u, Scale.v);

  return Scale;
}

auto PatcherTruePBR::flag(const nlohmann::json &JSON, const char *Key) -> bool { return JSON.contains(Key) && JSON[Key]; }
