#include "patchers/PatcherTruePBR.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenUtil.hpp"

#include <Shaders.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <cstddef>
#include <spdlog/spdlog.h>

using namespace std;

PatcherTruePBR::PatcherTruePBR(filesystem::path NIFPath, nifly::NifFile *NIF, ParallaxGenDirectory *PGD)
    : PGD(PGD), NIFPath(std::move(NIFPath)), NIF(NIF) {}

auto PatcherTruePBR::getTruePBRConfigs() -> map<size_t, nlohmann::json> & {
  static map<size_t, nlohmann::json> TruePBRConfigs = {};
  return TruePBRConfigs;
}

auto PatcherTruePBR::getPathLookupJSONs() -> map<size_t, nlohmann::json> & {
  static map<size_t, nlohmann::json> PathLookupJSONs = {};
  return PathLookupJSONs;
}

auto PatcherTruePBR::getTruePBRDiffuseInverse() -> map<string, vector<size_t>> & {
  static map<string, vector<size_t>> TruePBRDiffuseInverse = {};
  return TruePBRDiffuseInverse;
}

auto PatcherTruePBR::getTruePBRNormalInverse() -> map<string, vector<size_t>> & {
  static map<string, vector<size_t>> TruePBRNormalInverse = {};
  return TruePBRNormalInverse;
}

auto PatcherTruePBR::getPathLookupCache() -> unordered_map<tuple<string, string>, bool, TupleStrHash> & {
  static unordered_map<tuple<string, string>, bool, TupleStrHash> PathLookupCache = {};
  return PathLookupCache;
}

void PatcherTruePBR::loadPatcherBuffers(const map<size_t, nlohmann::json> &PBRJSONs) {
  getTruePBRConfigs() = PBRJSONs;

  // Create helper vectors
  for (const auto &Config : PBRJSONs) {
    // "match_normal" attribute
    if (Config.second.contains("match_normal")) {
      string RevNormal = Config.second["match_normal"].get<string>();
      reverse(RevNormal.begin(), RevNormal.end());

      getTruePBRNormalInverse()[boost::to_lower_copy(RevNormal)].push_back(Config.first);
      continue;
    }

    // "match_diffuse" attribute
    if (Config.second.contains("match_diffuse")) {
      string RevDiffuse = Config.second["match_diffuse"].get<string>();
      reverse(RevDiffuse.begin(), RevDiffuse.end());

      getTruePBRDiffuseInverse()[boost::to_lower_copy(RevDiffuse)].push_back(Config.first);
    }

    // "path_contains" attribute
    if (Config.second.contains("path_contains")) {
      getPathLookupJSONs()[Config.first] = Config.second;
    }
  }
}

auto PatcherTruePBR::shouldApply(const uint32_t &ShapeBlockID, const array<string, NUM_TEXTURE_SLOTS> &SearchPrefixes,
                                 bool &EnableResult,
                                 map<size_t, tuple<nlohmann::json, string>> &TruePBRData) -> ParallaxGenTask::PGResult {
  spdlog::trace(L"NIF: {} | Shape: {} | PBR | Starting checking", NIFPath.wstring(), ShapeBlockID);

  // Stores the json filename that gets priority over this shape
  string PriorityJSONFile;

  // "match_normal" attribute: Binary search for normal map
  getSlotMatch(ShapeBlockID, TruePBRData, PriorityJSONFile, SearchPrefixes[1], getTruePBRNormalInverse(),
               L"match_normal");

  // "match_diffuse" attribute: Binary search for diffuse map
  getSlotMatch(ShapeBlockID, TruePBRData, PriorityJSONFile, SearchPrefixes[0], getTruePBRDiffuseInverse(),
               L"match_diffuse");

  // "path_contains" attribute: Linear search for path_contains
  getPathContainsMatch(ShapeBlockID, TruePBRData, PriorityJSONFile, SearchPrefixes[0]);

  // Find stuff for logging
  const auto NumConfigs = TruePBRData.size();
  EnableResult = NumConfigs > 0;
  if (EnableResult) {
    spdlog::trace(L"NIF: {} | Shape: {} | PBR | {} PBR Configs matched", NIFPath.wstring(), ShapeBlockID, NumConfigs);
  } else {
    spdlog::trace(L"NIF: {} | Shape: {} | PBR | No PBR Configs matched", NIFPath.wstring(), ShapeBlockID);
  }

  return ParallaxGenTask::PGResult::SUCCESS;
}

auto PatcherTruePBR::getSlotMatch(const uint32_t &ShapeBlockID, map<size_t, tuple<nlohmann::json, string>> &TruePBRData,
                                  string &PriorityJSONFile, const string &TexName,
                                  const map<string, vector<size_t>> &Lookup, const wstring &SlotLabel) -> void {
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
    spdlog::trace(L"NIF: {} | Shape: {} | PBR | {} | No PBR JSON match found for \"{}\"", NIFPath.wstring(),
                  ShapeBlockID, SlotLabel, ParallaxGenUtil::stringToWstring(TexName));
    return;
  }

  // Initialize CFG set
  set<size_t> Cfgs(It->second.begin(), It->second.end());

  // Go back to include others if we need to
  while (It != Lookup.begin() && boost::starts_with(MapReverse, prev(It)->first)) {
    It = prev(It);
    Cfgs.insert(It->second.begin(), It->second.end());
  }

  spdlog::trace(L"NIF: {} | Shape: {} | PBR | {} | Matched {} PBR JSONs for \"{}\"", NIFPath.wstring(), ShapeBlockID,
                SlotLabel, Cfgs.size(), ParallaxGenUtil::stringToWstring(TexName));

  // Loop through all matches
  for (const auto &Cfg : Cfgs) {
    insertTruePBRData(ShapeBlockID, TruePBRData, PriorityJSONFile, TexName, Cfg);
  }
}

auto PatcherTruePBR::getPathContainsMatch(const uint32_t &ShapeBlockID,
                                          std::map<size_t, std::tuple<nlohmann::json, std::string>> &TruePBRData,
                                          std::string &PriorityJSONFile, const std::string &Diffuse) -> void {
  // "patch_contains" attribute: Linear search for path_contains
  auto &Cache = getPathLookupCache();

  // Check for path_contains only if no name match because it's a O(n) operation
  size_t NumMatches = 0;
  for (const auto &Config : getPathLookupJSONs()) {
    // Check if in cache
    auto CacheKey = make_tuple(Config.second["path_contains"].get<string>(), Diffuse);

    bool PathMatch = false;
    if (Cache.find(CacheKey) == Cache.end()) {
      // Not in cache, update it
      Cache[CacheKey] = boost::icontains(Diffuse, get<0>(CacheKey));
    }

    PathMatch = Cache[CacheKey];
    if (PathMatch) {
      NumMatches++;
      insertTruePBRData(ShapeBlockID, TruePBRData, PriorityJSONFile, Diffuse, Config.first);
    }
  }

  wstring SlotLabel = L"path_contains";
  if (NumMatches > 0) {
    spdlog::trace(L"NIF: {} | Shape: {} | PBR | {} | Matched {} PBR JSONs for \"{}\"", NIFPath.wstring(), ShapeBlockID,
                  SlotLabel, NumMatches, ParallaxGenUtil::stringToWstring(Diffuse));
  } else {
    spdlog::trace(L"NIF: {} | Shape: {} | PBR | {} | No PBR JSON match found for \"{}\"", NIFPath.wstring(),
                  ShapeBlockID, SlotLabel, ParallaxGenUtil::stringToWstring(Diffuse));
  }
}

auto PatcherTruePBR::insertTruePBRData(const uint32_t &ShapeBlockID,
                                       std::map<size_t, std::tuple<nlohmann::json, std::string>> &TruePBRData,
                                       std::string &PriorityJSONFile, const string &TexName, size_t Cfg) -> void {
  const auto CurCfg = getTruePBRConfigs()[Cfg];
  const auto CurJSON = CurCfg["json"].get<string>();

  // Check if we should skip this due to nif filter (this is expsenive, so we do it last)
  if (CurCfg.contains("nif_filter") && !boost::icontains(NIFPath.wstring(), CurCfg["nif_filter"].get<string>())) {
    spdlog::trace(L"NIF: {} | Shape: {} | PBR | {} | PBR JSON Rejected: nif_filter", NIFPath.wstring(), ShapeBlockID,
                  Cfg);
    return;
  }

  // Evaluate JSON priority
  if (PriorityJSONFile.empty()) {
    // Define priority file
    PriorityJSONFile = CurJSON;
    spdlog::trace(L"NIF: {} | Shape: {} | PBR | Setting priority JSON to {}", NIFPath.wstring(), ShapeBlockID,
                  ParallaxGenUtil::stringToWstring(PriorityJSONFile));
  }

  if (CurJSON != PriorityJSONFile) {
    // Only use first JSON (highest priority one)
    if (Cfg < TruePBRData.begin()->first) {
      // Checks if we should be replacing the priority JSON (if we found a higher priority one)
      TruePBRData.clear();
      PriorityJSONFile = CurJSON;
      spdlog::trace(L"NIF: {} | Shape: {} | PBR | Setting priority JSON to {}", NIFPath.wstring(), ShapeBlockID,
                    ParallaxGenUtil::stringToWstring(PriorityJSONFile));
    } else {
      spdlog::trace(L"NIF: {} | Shape: {} | PBR | PBR JSON Rejected {}: Losing JSON Priority", NIFPath.wstring(),
                    ShapeBlockID, Cfg);
      return;
    }
  }

  // Find and check prefix value
  // Add the PBR part to the texture path
  string TexPath = TexName;
  if (boost::istarts_with(TexPath, "textures\\") && !boost::istarts_with(TexPath, "textures\\pbr\\")) {
    TexPath.replace(0, TEXTURE_STR_LENGTH, "textures\\pbr\\");
  }

  // Get PBR path, which is the path without the matched field
  string MatchedField =
      CurCfg.contains("match_normal") ? CurCfg["match_normal"].get<string>() : CurCfg["match_diffuse"].get<string>();
  TexPath.erase(TexPath.length() - MatchedField.length(), MatchedField.length());

  // "rename" attribute
  if (CurCfg.contains("rename")) {
    MatchedField = CurCfg["rename"].get<string>();
  }

  spdlog::trace(L"NIF: {} | Shape: {} | PBR | {} | PBR texture path created: {}", NIFPath.wstring(), ShapeBlockID, Cfg,
                ParallaxGenUtil::stringToWstring(MatchedField));

  // Check if named_field is a directory
  string MatchedPath = boost::to_lower_copy(TexPath + MatchedField);
  bool EnableTruePBR = (!CurCfg.contains("pbr") || CurCfg["pbr"]) && !MatchedPath.empty();
  if (EnableTruePBR && !PGD->isPrefix(MatchedPath)) {
    spdlog::trace(L"NIF: {} | Shape: {} | PBR | {} | PBR JSON Rejected: Path {} is not a prefix", NIFPath.wstring(),
                  ShapeBlockID, Cfg, ParallaxGenUtil::stringToWstring(MatchedPath));
    return;
  }

  // If no pbr clear MatchedPath
  if (!EnableTruePBR) {
    MatchedPath = "";
  }

  spdlog::trace(L"NIF: {} | Shape: {} | PBR | {} | PBR JSON accepted", NIFPath.wstring(), ShapeBlockID, Cfg);
  TruePBRData.insert({Cfg, {CurCfg, MatchedPath}});
}

auto PatcherTruePBR::applyPatch(NiShape *NIFShape, nlohmann::json &TruePBRData, const std::string &MatchedPath,
                                bool &NIFModified) const -> ParallaxGenTask::PGResult {

  // enable TruePBR on shape
  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // Prep
  auto *NIFShader = NIF->GetShader(NIFShape);
  auto *const NIFShaderBSLSP = dynamic_cast<BSLightingShaderProperty *>(NIFShader);
  bool EnableTruePBR = !MatchedPath.empty();
  bool EnableEnvMapping = TruePBRData.contains("env_mapping") && TruePBRData["env_mapping"] && !EnableTruePBR;

  // "delete" attribute
  if (TruePBRData.contains("delete") && TruePBRData["delete"]) {
    NIF->DeleteShape(NIFShape);
    NIFModified = true;
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
    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::Cubemap, NewCubemap, NIFModified);
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

auto PatcherTruePBR::enableTruePBROnShape(NiShape *NIFShape, NiShader *NIFShader,
                                          BSLightingShaderProperty *NIFShaderBSLSP, nlohmann::json &TruePBRData,
                                          const string &MatchedPath,
                                          bool &NIFModified) const -> ParallaxGenTask::PGResult {

  // enable TruePBR on shape
  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // "lock_diffuse" attribute
  if (!flag(TruePBRData, "lock_diffuse")) {
    auto NewDiffuse = MatchedPath + ".dds";
    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::Diffuse, NewDiffuse, NIFModified);
  }

  // "lock_normal" attribute
  if (!flag(TruePBRData, "lock_normal")) {
    auto NewNormal = MatchedPath + "_n.dds";
    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::Normal, NewNormal, NIFModified);
  }

  // "emissive" attribute
  if (TruePBRData.contains("emissive") && !flag(TruePBRData, "lock_emissive")) {
    string NewGlow;
    if (TruePBRData["emissive"].get<bool>()) {
      NewGlow = MatchedPath + "_g.dds";
    }

    NIFUtil::configureShaderFlag(NIFShaderBSLSP, SLSF1_EXTERNAL_EMITTANCE, TruePBRData["emissive"].get<bool>(),
                                 NIFModified);
    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::Glow, NewGlow, NIFModified);
  }

  // "parallax" attribute
  if (TruePBRData.contains("parallax") && !flag(TruePBRData, "lock_parallax")) {
    string NewParallax;
    if (TruePBRData["parallax"].get<bool>()) {
      NewParallax = MatchedPath + "_p.dds";
    }

    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::Parallax, NewParallax, NIFModified);
  }

  string Slot4;
  NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::Cubemap, Slot4, NIFModified);

  // "lock_rmaos" attribute
  if (!flag(TruePBRData, "lock_rmaos")) {
    auto NewRMAOS = MatchedPath + "_rmaos.dds";
    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::EnvMask, NewRMAOS, NIFModified);
  }

  // "lock_cnr" attribute
  if (!flag(TruePBRData, "lock_cnr")) {
    // "coat_normal" attribute
    string NewCNR;
    if (TruePBRData.contains("coat_normal") && TruePBRData["coat_normal"].get<bool>()) {
      NewCNR = MatchedPath + "_cnr.dds";
    }

    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::Tint, NewCNR, NIFModified);
  }

  // "lock_subsurface" attribute
  if (!flag(TruePBRData, "lock_subsurface")) {
    // "subsurface_foliage" attribute
    string NewSubsurface;
    if ((TruePBRData.contains("subsurface_foliage") && TruePBRData["subsurface_foliage"].get<bool>()) ||
        (TruePBRData.contains("subsurface") && TruePBRData["subsurface"].get<bool>()) ||
        (TruePBRData.contains("coat_diffuse") && TruePBRData["coat_diffuse"].get<bool>())) {
      NewSubsurface = MatchedPath + "_s.dds";
    }

    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::Backlight, NewSubsurface, NIFModified);
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
  if (TruePBRData.contains("multilayer")) {
    if (TruePBRData["multilayer"]) {
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

auto PatcherTruePBR::flag(nlohmann::json &JSON, const char *Key) -> bool { return JSON.contains(Key) && JSON[Key]; }
