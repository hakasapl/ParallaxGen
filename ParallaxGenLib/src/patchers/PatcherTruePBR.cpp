#include "patchers/PatcherTruePBR.hpp"

#include <Shaders.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <cstddef>
#include <memory>
#include <string>

#include "Logger.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenUtil.hpp"
#include "patchers/PatcherShader.hpp"

using namespace std;

PatcherTruePBR::PatcherTruePBR(filesystem::path NIFPath, nifly::NifFile *NIF)
    : PatcherShader(std::move(NIFPath), NIF, "TruePBR") {}

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

void PatcherTruePBR::loadStatics(const std::vector<std::filesystem::path> &PBRJSONs) {
  size_t ConfigOrder = 0;
  for (const auto &Config : PBRJSONs) {
    // check if Config is valid
    auto ConfigFileBytes = getPGD()->getFile(Config);
    string ConfigFileStr(reinterpret_cast<const char *>(ConfigFileBytes.data()), ConfigFileBytes.size());

    try {
      nlohmann::json J = nlohmann::json::parse(ConfigFileStr);
      // loop through each Element
      for (auto &Element : J) {
        // Preprocessing steps here
        if (Element.contains("texture")) {
          Element["match_diffuse"] = Element["texture"];
        }

        Element["json"] = ParallaxGenUtil::UTF16toUTF8(Config.wstring());

        // loop through filename Fields
        for (const auto &Field : getTruePBRConfigFilenameFields()) {
          if (Element.contains(Field) && !boost::istarts_with(Element[Field].get<string>(), "\\")) {
            Element[Field] = Element[Field].get<string>().insert(0, 1, '\\');
          }
        }

        Logger::trace(L"TruePBR Config {} Loaded: {}", ConfigOrder, ParallaxGenUtil::UTF8toUTF16(Element.dump()));
        getTruePBRConfigs()[ConfigOrder++] = Element;
      }
    } catch (nlohmann::json::parse_error &E) {
      Logger::error(L"Unable to parse TruePBR Config file {}: {}", Config.wstring(),
                    ParallaxGenUtil::ASCIItoUTF16(E.what()));
      continue;
    }
  }

  Logger::info(L"Found {} TruePBR entries", getTruePBRConfigs().size());

  // Create helper vectors
  for (const auto &Config : getTruePBRConfigs()) {
    // "match_normal" attribute
    if (Config.second.contains("match_normal")) {
      auto RevNormal = ParallaxGenUtil::UTF8toUTF16(Config.second["match_normal"].get<string>());
      RevNormal = NIFUtil::getTexBase(RevNormal);
      reverse(RevNormal.begin(), RevNormal.end());

      getTruePBRNormalInverse()[boost::to_lower_copy(RevNormal)].push_back(Config.first);
      continue;
    }

    // "match_diffuse" attribute
    if (Config.second.contains("match_diffuse")) {
      auto RevDiffuse = ParallaxGenUtil::UTF8toUTF16(Config.second["match_diffuse"].get<string>());
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

auto PatcherTruePBR::getFactory() -> PatcherShader::PatcherShaderFactory {
  return [](const filesystem::path& NIFPath, nifly::NifFile *NIF) -> unique_ptr<PatcherShader> {
    return make_unique<PatcherTruePBR>(NIFPath, NIF);
  };
}

auto PatcherTruePBR::getShaderType() -> NIFUtil::ShapeShader {
  return NIFUtil::ShapeShader::TRUEPBR;
}

auto PatcherTruePBR::canApply([[maybe_unused]] nifly::NiShape &NIFShape) -> bool {
  auto *const NIFShader = getNIF()->GetShader(&NIFShape);
  auto *const NIFShaderBSLSP = dynamic_cast<BSLightingShaderProperty *>(NIFShader);

  if (NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF1_FACEGEN_RGB_TINT)) {
    Logger::trace(L"Cannot Apply: Facegen RGB Tint");
    return false;
  }

  return true;
}

auto PatcherTruePBR::shouldApply(nifly::NiShape &NIFShape, std::vector<PatcherMatch> &Matches) -> bool {
  // Prep
  auto *NIFShader = getNIF()->GetShader(&NIFShape);
  auto *const NIFShaderBSLSP = dynamic_cast<BSLightingShaderProperty *>(NIFShader);

  Logger::trace(L"Starting checking");

  Matches.clear();

  // Find Old Slots
  auto OldSlots = getTextureSet(NIFShape);

  if (shouldApply(OldSlots, Matches)) {
    Logger::trace(L"{} PBR configs matched", Matches.size());
  } else {
    Logger::trace(L"No PBR Configs matched");
  }

  if (NIFUtil::hasShaderFlag(NIFShaderBSLSP, SLSF2_UNUSED01)) {
    // Check if RMAOS exists
    const auto RMAOSPath = OldSlots[static_cast<size_t>(NIFUtil::TextureSlots::ENVMASK)];
    if (!RMAOSPath.empty() && getPGD()->isFile(RMAOSPath)) {
      Logger::trace(L"This shape already has PBR");
      PatcherMatch Match;
      Match.MatchedPath = getNIFPath().wstring();
      Matches.insert(Matches.begin(), Match);
    }
  }

  return Matches.size() > 0;
}

auto PatcherTruePBR::shouldApply(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                 std::vector<PatcherMatch> &Matches) -> bool {
  // get search prefixes
  auto SearchPrefixes = NIFUtil::getSearchPrefixes(OldSlots);

  map<size_t, tuple<nlohmann::json, wstring>> TruePBRData;
  // "match_normal" attribute: Binary search for normal map
  getSlotMatch(TruePBRData, SearchPrefixes[1], getTruePBRNormalInverse(), L"match_normal", getNIFPath().wstring(), OldSlots);

  // "match_diffuse" attribute: Binary search for diffuse map
  getSlotMatch(TruePBRData, SearchPrefixes[0], getTruePBRDiffuseInverse(), L"match_diffuse", getNIFPath().wstring(), OldSlots);

  // "path_contains" attribute: Linear search for path_contains
  getPathContainsMatch(TruePBRData, SearchPrefixes[0], getNIFPath().wstring(), OldSlots);

  // Split data into individual JSONs
  unordered_map<wstring, map<size_t, tuple<nlohmann::json, wstring>>> TruePBROutputData;
  unordered_map<wstring, unordered_set<NIFUtil::TextureSlots>> TruePBRMatchedFrom;
  for (const auto &[Sequence, Data] : TruePBRData) {
    // get current JSON
    auto MatchedPath = ParallaxGenUtil::UTF8toUTF16(get<0>(Data)["json"].get<string>());

    // Add to output
    if (TruePBROutputData.find(MatchedPath) == TruePBROutputData.end()) {
      // If MatchedPath doesn't exist, insert it with an empty map and then add Sequence, Data
      TruePBROutputData.emplace(MatchedPath, map<size_t, tuple<nlohmann::json, wstring>>{});
    }
    TruePBROutputData[MatchedPath][Sequence] = Data;

    if (get<0>(Data).contains("match_normal")) {
      TruePBRMatchedFrom[MatchedPath].insert(NIFUtil::TextureSlots::NORMAL);
    } else {
      TruePBRMatchedFrom[MatchedPath].insert(NIFUtil::TextureSlots::DIFFUSE);
    }
  }

  // Convert output to vectors
  for (auto &[JSON, JSONData] : TruePBROutputData) {
    PatcherMatch Match;
    Match.MatchedPath = JSON;
    Match.MatchedFrom = TruePBRMatchedFrom[JSON];
    Match.ExtraData = make_shared<decltype(JSONData)>(JSONData);

    // check paths
    bool Valid = true;
    const auto NewSlots = applyPatchSlots(OldSlots, Match);
    for (size_t I = 0; I < NUM_TEXTURE_SLOTS; I++) {
      if (!NewSlots[I].empty() && !getPGD()->isFile(NewSlots[I])) {
        // Slot does not exist
        Logger::trace(L"PBR JSON Match: Result texture slot {} does not exist", NewSlots[I]);
        Valid = false;
      }
    }

    if (!Valid) {
      continue;
    }

    Matches.push_back(Match);
  }

  // Sort matches by ExtraData key minimum value (this preserves order of JSONs to be 0 having priority if mod order does not exist)
  sort(Matches.begin(), Matches.end(), [](const PatcherMatch &A, const PatcherMatch &B) {
    return get<0>(*(static_pointer_cast<map<size_t, tuple<nlohmann::json, wstring>>>(A.ExtraData)->begin())) >
           get<0>(*(static_pointer_cast<map<size_t, tuple<nlohmann::json, wstring>>>(B.ExtraData)->begin()));
  });

  // Check for no-JSON RMAOS
  if (TruePBRData.empty()) {
    auto RMAOSPath = OldSlots[static_cast<size_t>(NIFUtil::TextureSlots::ENVMASK)];
    // if not start with PBR add it for the check
    if (boost::istarts_with(RMAOSPath, "textures\\") && !boost::istarts_with(RMAOSPath, "textures\\pbr\\")) {
      RMAOSPath.replace(0, TEXTURE_STR_LENGTH, L"textures\\pbr\\");
    }

    if (getPGD()->getTextureType(RMAOSPath) == NIFUtil::TextureType::RMAOS) {
      Logger::trace(L"Found RMAOS without JSON: {}", RMAOSPath);
      PatcherMatch Match;
      Match.MatchedPath = RMAOSPath;
      Matches.insert(Matches.begin(), Match);
    }
  }

  return Matches.size() > 0;
}

auto PatcherTruePBR::getSlotMatch(map<size_t, tuple<nlohmann::json, wstring>> &TruePBRData, const wstring &TexName,
                                  const map<wstring, vector<size_t>> &Lookup, const wstring &SlotLabel,
                                  const wstring &NIFPath, const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots) -> void {
  // binary search for map
  auto MapReverse = boost::to_lower_copy(TexName);
  reverse(MapReverse.begin(), MapReverse.end());
  auto It = Lookup.lower_bound(MapReverse);

  // get the first element of the reverse path
  auto ReverseFile = MapReverse;
  auto Pos = ReverseFile.find_first_of(L"\\");
  if (Pos != wstring::npos) {
    ReverseFile = ReverseFile.substr(0, Pos);
  }

  // Check if match is 1 back
  if (It != Lookup.begin() && boost::starts_with(prev(It)->first, ReverseFile)) {
    It = prev(It);
  } else if (It != Lookup.end() && boost::starts_with(It->first, ReverseFile)) {
    // Check if match is current iterator, just continue here
  } else {
    // No match found
    Logger::trace(L"No PBR JSON match found for \"{}\":\"{}\"", SlotLabel, TexName);
    return;
  }

  bool FoundMatch = false;
  while (It != Lookup.end()) {
    if (boost::starts_with(MapReverse, It->first)) {
      FoundMatch = true;
      break;
    }

    if (It != Lookup.begin() && boost::starts_with(prev(It)->first, ReverseFile)) {
      It = prev(It);
    } else {
      break;
    }
  }

  if (!FoundMatch) {
    Logger::trace(L"No PBR JSON match found for \"{}\":\"{}\"", SlotLabel, TexName);
    return;
  }

  // Initialize CFG set
  set<size_t> Cfgs(It->second.begin(), It->second.end());

  // Go back to include others if we need to
  while (It != Lookup.begin() && boost::starts_with(MapReverse, prev(It)->first)) {
    It = prev(It);
    Cfgs.insert(It->second.begin(), It->second.end());
  }

  Logger::trace(L"Matched {} PBR JSONs for \"{}\":\"{}\"", Cfgs.size(), SlotLabel, TexName);

  // Loop through all matches
  for (const auto &Cfg : Cfgs) {
    insertTruePBRData(TruePBRData, TexName, Cfg, NIFPath, OldSlots);
  }
}

auto PatcherTruePBR::getPathContainsMatch(std::map<size_t, std::tuple<nlohmann::json, std::wstring>> &TruePBRData,
                                          const std::wstring &Diffuse, const wstring &NIFPath, const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots) -> void {
  // "patch_contains" attribute: Linear search for path_contains
  auto &Cache = getPathLookupCache();

  // Check for path_contains only if no name match because it's a O(n) operation
  size_t NumMatches = 0;
  for (const auto &Config : getPathLookupJSONs()) {
    // Check if in cache
    auto CacheKey = make_tuple(ParallaxGenUtil::UTF8toUTF16(Config.second["path_contains"].get<string>()), Diffuse);

    bool PathMatch = false;
    if (Cache.find(CacheKey) == Cache.end()) {
      // Not in cache, update it
      Cache[CacheKey] = boost::icontains(Diffuse, get<0>(CacheKey));
    }

    PathMatch = Cache[CacheKey];
    if (PathMatch) {
      NumMatches++;
      insertTruePBRData(TruePBRData, Diffuse, Config.first, NIFPath, OldSlots);
    }
  }

  const wstring SlotLabel = L"path_contains";
  if (NumMatches > 0) {
    Logger::trace(L"Matched {} PBR JSONs for \"{}\":\"{}\"", NumMatches, SlotLabel, Diffuse);
  } else {
    Logger::trace(L"No PBR JSON match found for \"{}\":\"{}\"", SlotLabel, Diffuse);
  }
}

auto PatcherTruePBR::insertTruePBRData(std::map<size_t, std::tuple<nlohmann::json, std::wstring>> &TruePBRData,
                                       const wstring &TexName, size_t Cfg, const wstring &NIFPath, const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots) -> void {
  const auto CurCfg = getTruePBRConfigs()[Cfg];

  // Check if we should skip this due to nif filter (this is expsenive, so we do it last)
  if (CurCfg.contains("nif_filter") && !boost::icontains(NIFPath, CurCfg["nif_filter"].get<string>())) {
    Logger::trace(L"Config {} PBR JSON Rejected: nif_filter {}", Cfg, NIFPath);
    return;
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

  auto MatchedField = ParallaxGenUtil::UTF8toUTF16(MatchedFieldStr);

  // "rename" attribute
  if (CurCfg.contains("rename")) {
    auto RenameField = CurCfg["rename"].get<string>();
    if (!boost::iequals(RenameField, MatchedField)) {
      MatchedField = ParallaxGenUtil::UTF8toUTF16(RenameField);
    }
  }

  Logger::trace(L"Config {} PBR texture path created: {}", Cfg, MatchedField);

  // Check if named_field is a directory
  wstring MatchedPath = boost::to_lower_copy(TexPath + MatchedField);
  bool EnableTruePBR = (!CurCfg.contains("pbr") || CurCfg["pbr"]) && !MatchedPath.empty();
  if (!EnableTruePBR) {
    MatchedPath = L"";
  }

  Logger::trace(L"Config {} PBR JSON accepted", Cfg);
  TruePBRData.insert({Cfg, {CurCfg, MatchedPath}});
}

auto PatcherTruePBR::applyPatch(nifly::NiShape &NIFShape, const PatcherMatch &Match, bool &NIFModified,
                                bool &ShapeDeleted) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> {
  auto NewSlots = getTextureSet(NIFShape);

  if (Match.MatchedPath == getNIFPath().wstring() || getPGD()->getTextureType(Match.MatchedPath) == NIFUtil::TextureType::RMAOS) {
    // already has PBR, just add PBR prefix to the slots if not already there
    for (size_t I = 0; I < NUM_TEXTURE_SLOTS; I++) {
      if (boost::istarts_with(NewSlots[I], "textures\\") && !boost::istarts_with(NewSlots[I], "textures\\pbr\\")) {
        NewSlots[I].replace(0, TEXTURE_STR_LENGTH, L"textures\\pbr\\");
      }
    }

    return NewSlots;
  }

  // get extra data from match
  auto ExtraData = static_pointer_cast<map<size_t, tuple<nlohmann::json, wstring>>>(Match.ExtraData);
  for (const auto &[Sequence, Data] : *ExtraData) {
    // apply one patch
    auto TruePBRData = get<0>(Data);
    auto MatchedPath = get<1>(Data);
    applyOnePatch(&NIFShape, TruePBRData, MatchedPath, NIFModified, ShapeDeleted, NewSlots);
  }

  return NewSlots;
}

auto PatcherTruePBR::applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                     const PatcherMatch &Match) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> {
  if (Match.MatchedPath == getNIFPath().wstring() || getPGD()->getTextureType(Match.MatchedPath) == NIFUtil::TextureType::RMAOS) {
    // already has PBR, just add PBR prefix to the slots if not already there
    auto NewSlots = OldSlots;
    for (size_t I = 0; I < NUM_TEXTURE_SLOTS; I++) {
      if (boost::istarts_with(OldSlots[I], "textures\\") && !boost::istarts_with(OldSlots[I], "textures\\pbr\\")) {
        NewSlots[I].replace(0, TEXTURE_STR_LENGTH, L"textures\\pbr\\");
      }
    }

    return NewSlots;
  }

  auto NewSlots = OldSlots;
  auto ExtraData = static_pointer_cast<map<size_t, tuple<nlohmann::json, wstring>>>(Match.ExtraData);
  for (const auto &[Sequence, Data] : *ExtraData) {
    auto TruePBRData = get<0>(Data);
    auto MatchedPath = get<1>(Data);
    applyOnePatchSlots(NewSlots, TruePBRData, MatchedPath);
  }

  return NewSlots;
}

auto PatcherTruePBR::applyNeutral(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &Slots)
    -> std::array<std::wstring, NUM_TEXTURE_SLOTS> {

  array<wstring, NUM_TEXTURE_SLOTS> NewSlots = Slots;

  // potentially existing height map can be used
  NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::GLOW)] = L"";
  NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::ENVMASK)] = L"textures\\parallaxgen\\neutral_rmaos.dds";
  NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::MULTILAYER)] = L"";
  NewSlots[static_cast<size_t>(NIFUtil::TextureSlots::BACKLIGHT)] = L"";

  return NewSlots;
}

void PatcherTruePBR::applyOnePatch(NiShape *NIFShape, nlohmann::json &TruePBRData, const std::wstring &MatchedPath,
                                   bool &NIFModified, bool &ShapeDeleted,
                                   std::array<std::wstring, NUM_TEXTURE_SLOTS> &NewSlots) {

  // Prep
  auto *NIFShader = getNIF()->GetShader(NIFShape);
  auto *const NIFShaderBSLSP = dynamic_cast<BSLightingShaderProperty *>(NIFShader);
  const bool EnableTruePBR = !MatchedPath.empty();
  const bool EnableEnvMapping = TruePBRData.contains("env_mapping") && TruePBRData["env_mapping"] && !EnableTruePBR;

  // "delete" attribute
  if (TruePBRData.contains("delete") && TruePBRData["delete"]) {
    getNIF()->DeleteShape(NIFShape);
    NIFModified = true;
    ShapeDeleted = true;
    return;
  }

  // "smooth_angle" attribute
  if (TruePBRData.contains("smooth_angle")) {
    getNIF()->CalcNormalsForShape(NIFShape, true, true, TruePBRData["smooth_angle"]);
    getNIF()->CalcTangentsForShape(NIFShape);
    NIFModified = true;
  }

  // "auto_uv" attribute
  if (TruePBRData.contains("auto_uv")) {
    vector<Triangle> Tris;
    NIFShape->GetTriangles(Tris);
    auto NewUVScale = autoUVScale(getNIF()->GetUvsForShape(NIFShape), getNIF()->GetVertsForShape(NIFShape), Tris) /
                      TruePBRData["auto_uv"];
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
    enableTruePBROnShape(NIFShape, NIFShader, NIFShaderBSLSP, TruePBRData, MatchedPath, NIFModified, NewSlots);
  }
}

void PatcherTruePBR::applyOnePatchSlots(std::array<std::wstring, NUM_TEXTURE_SLOTS> &Slots,
                                        const nlohmann::json &TruePBRData, const std::wstring &MatchedPath) {
  if (MatchedPath.empty()) {
    return;
  }

  // "lock_diffuse" attribute
  if (!flag(TruePBRData, "lock_diffuse")) {
    auto NewDiffuse = MatchedPath + L".dds";
    Slots[static_cast<size_t>(NIFUtil::TextureSlots::DIFFUSE)] = NewDiffuse;
  }

  // "lock_normal" attribute
  if (!flag(TruePBRData, "lock_normal")) {
    auto NewNormal = MatchedPath + L"_n.dds";
    Slots[static_cast<size_t>(NIFUtil::TextureSlots::NORMAL)] = NewNormal;
  }

  // "emissive" attribute
  if (TruePBRData.contains("emissive") && !flag(TruePBRData, "lock_emissive")) {
    wstring NewGlow;
    if (TruePBRData["emissive"].get<bool>()) {
      NewGlow = MatchedPath + L"_g.dds";
    }

    Slots[static_cast<size_t>(NIFUtil::TextureSlots::GLOW)] = NewGlow;
  }

  // "parallax" attribute
  if (TruePBRData.contains("parallax") && !flag(TruePBRData, "lock_parallax")) {
    wstring NewParallax;
    if (TruePBRData["parallax"].get<bool>()) {
      NewParallax = MatchedPath + L"_p.dds";
    }

    Slots[static_cast<size_t>(NIFUtil::TextureSlots::PARALLAX)] = NewParallax;
  }

  // "cubemap" attribute
  if (TruePBRData.contains("cubemap") && !flag(TruePBRData, "lock_cubemap")) {
    auto NewCubemap = ParallaxGenUtil::UTF8toUTF16(TruePBRData["cubemap"].get<string>());
    Slots[static_cast<size_t>(NIFUtil::TextureSlots::CUBEMAP)] = NewCubemap;
  } else {
    Slots[static_cast<size_t>(NIFUtil::TextureSlots::CUBEMAP)] = L"";
  }

  // "lock_rmaos" attribute
  if (!flag(TruePBRData, "lock_rmaos")) {
    auto NewRMAOS = MatchedPath + L"_rmaos.dds";
    Slots[static_cast<size_t>(NIFUtil::TextureSlots::ENVMASK)] = NewRMAOS;
  }

  // "lock_cnr" attribute
  if (!flag(TruePBRData, "lock_cnr")) {
    // "coat_normal" attribute
    wstring NewCNR;
    if (TruePBRData.contains("coat_normal") && TruePBRData["coat_normal"].get<bool>()) {
      NewCNR = MatchedPath + L"_cnr.dds";
    }

    Slots[static_cast<size_t>(NIFUtil::TextureSlots::MULTILAYER)] = NewCNR;
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

    Slots[static_cast<size_t>(NIFUtil::TextureSlots::BACKLIGHT)] = NewSubsurface;
  }

  // "SlotX" attributes
  for (int I = 0; I < NUM_TEXTURE_SLOTS - 1; I++) {
    string SlotName = "slot" + to_string(I + 1);
    if (TruePBRData.contains(SlotName)) {
      string NewSlot = TruePBRData[SlotName].get<string>();
      Slots[I] = ParallaxGenUtil::UTF8toUTF16(NewSlot);
    }
  }
}

void PatcherTruePBR::enableTruePBROnShape(NiShape *NIFShape, NiShader *NIFShader,
                                          BSLightingShaderProperty *NIFShaderBSLSP, nlohmann::json &TruePBRData,
                                          const wstring &MatchedPath, bool &NIFModified,
                                          std::array<std::wstring, NUM_TEXTURE_SLOTS> &NewSlots) {
  applyOnePatchSlots(NewSlots, TruePBRData, MatchedPath);
  setTextureSet(*NIFShape, NewSlots, NIFModified);

  // "emissive" attribute
  if (TruePBRData.contains("emissive") && !flag(TruePBRData, "lock_emissive")) {
    NIFUtil::configureShaderFlag(NIFShaderBSLSP, SLSF1_EXTERNAL_EMITTANCE, TruePBRData["emissive"].get<bool>(),
                                 NIFModified);
  }

  // revert to default NIFShader type, remove flags used in other types
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF1_ENVIRONMENT_MAPPING, NIFModified);
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF1_PARALLAX, NIFModified);
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF2_GLOW_MAP, NIFModified);

  // Enable PBR flag
  NIFUtil::setShaderFlag(NIFShaderBSLSP, SLSF2_UNUSED01, NIFModified);

  if (TruePBRData.contains("subsurface_foliage") && TruePBRData["subsurface_foliage"].get<bool>() &&
      TruePBRData.contains("subsurface") && TruePBRData["subsurface"].get<bool>()) {
    Logger::error(L"Error: Subsurface and foliage NIFShader chosen at once, undefined behavior!");
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
      auto NewCoatColor = Vector3(TruePBRData["coat_color"][0].get<float>(), TruePBRData["coat_color"][1].get<float>(),
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

auto PatcherTruePBR::flag(const nlohmann::json &JSON, const char *Key) -> bool {
  return JSON.contains(Key) && JSON[Key];
}
