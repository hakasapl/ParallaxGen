#include "patchers/PatcherTruePBR.hpp"

#include <boost/algorithm/string.hpp>
#include <spdlog/spdlog.h>

using namespace std;

PatcherTruePBR::PatcherTruePBR(filesystem::path NIFPath, nifly::NifFile *NIF) : NIFPath(std::move(NIFPath)), NIF(NIF) {}

auto PatcherTruePBR::shouldApply(nifly::NiShape *NIFShape, const vector<nlohmann::json> &TPBRConfigs,
                                 const array<string, NUM_TEXTURE_SLOTS> &SearchPrefixes, bool &EnableResult,
                                 vector<tuple<nlohmann::json, string>> &TruePBRData) const
    -> ParallaxGenTask::PGResult {

  auto Result = ParallaxGenTask::PGResult::SUCCESS;
  const auto ShapeBlockID = NIF->GetBlockID(NIFShape);

  for (const auto &TruePBRCFG : TPBRConfigs) {
    string MatchedPath;

    // "path-contains" attribute
    bool ContainsMatch = TruePBRCFG.contains("path_contains") &&
                         boost::icontains(SearchPrefixes[0], TruePBRCFG["path_contains"].get<string>());

    bool NameMatch = false;
    if (TruePBRCFG.contains("match_normal") &&
        boost::iends_with(SearchPrefixes[1], TruePBRCFG["match_normal"].get<string>())) {
      NameMatch = true;
      MatchedPath = SearchPrefixes[1];
    }
    if (TruePBRCFG.contains("match_diffuse") &&
        boost::iends_with(SearchPrefixes[0], TruePBRCFG["match_diffuse"].get<string>())) {
      NameMatch = true;
      MatchedPath = SearchPrefixes[0];
    }

    if (!ContainsMatch && !NameMatch) {
      spdlog::trace(L"Rejecting shape {}: No matches", ShapeBlockID);
      EnableResult |= false;
      continue;
    }

    // "nif-filter" attribute
    if (TruePBRCFG.contains("nif_filter") &&
        !boost::icontains(NIFPath.wstring(), TruePBRCFG["nif_filter"].get<string>())) {
      spdlog::trace(L"Rejecting shape {}: NIF filter", ShapeBlockID);
      EnableResult |= false;
      continue;
    }

    EnableResult = true;
    TruePBRData.emplace_back(TruePBRCFG, MatchedPath);
  }

  return Result;
}

auto PatcherTruePBR::applyPatch(NiShape *NIFShape, nlohmann::json &TruePBRData, const std::string &MatchedPath,
                                bool &NIFModified) const -> ParallaxGenTask::PGResult {

  // enable TruePBR on shape
  auto Result = ParallaxGenTask::PGResult::SUCCESS;

  // Prep
  auto *NIFShader = NIF->GetShader(NIFShape);
  auto *const NIFShaderBSLSP = dynamic_cast<BSLightingShaderProperty *>(NIFShader);
  bool EnableTruePBR = (!TruePBRData.contains("pbr") || TruePBRData["pbr"]) && !MatchedPath.empty();
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
  if ((!TruePBRData.contains("pbr") || TruePBRData["pbr"].get<bool>()) && !MatchedPath.empty()) {
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

  // Add the PBR part to the texture path
  string TexPath = string(MatchedPath);
  if (!boost::istarts_with(TexPath, "textures\\pbr\\")) {
    boost::replace_first(TexPath, "textures\\", "textures\\pbr\\");
  }

  // Get PBR path, which is the path without the matched field
  string MatchedField = TruePBRData.contains("match_normal") ? TruePBRData["match_normal"].get<string>()
                                                             : TruePBRData["match_diffuse"].get<string>();
  TexPath.erase(TexPath.length() - MatchedField.length(), MatchedField.length());

  // "rename" attribute
  string NamedField = MatchedField;
  if (TruePBRData.contains("rename")) {
    NamedField = TruePBRData["rename"].get<string>();
  }

  // "lock_diffuse" attribute
  if (!flag(TruePBRData, "lock_diffuse")) {
    auto NewDiffuse = TexPath + NamedField + ".dds";
    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::Diffuse, NewDiffuse, NIFModified);
  }

  // "lock_normal" attribute
  if (!flag(TruePBRData, "lock_normal")) {
    auto NewNormal = TexPath + NamedField + "_n.dds";
    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::Normal, NewNormal, NIFModified);
  }

  // "emissive" attribute
  if (TruePBRData.contains("emissive") && !flag(TruePBRData, "lock_emissive")) {
    string NewGlow;
    if (TruePBRData["emissive"].get<bool>()) {
      NewGlow = TexPath + NamedField + "_g.dds";
    }

    NIFUtil::configureShaderFlag(NIFShaderBSLSP, SLSF1_EXTERNAL_EMITTANCE, TruePBRData["emissive"].get<bool>(),
                                 NIFModified);
    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::Glow, NewGlow, NIFModified);
  }

  // "parallax" attribute
  if (TruePBRData.contains("parallax") && !flag(TruePBRData, "lock_parallax")) {
    string NewParallax;
    if (TruePBRData["parallax"].get<bool>()) {
      NewParallax = TexPath + NamedField + "_p.dds";
    }

    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::Parallax, NewParallax, NIFModified);
  }

  string Slot4;
  NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::Cubemap, Slot4, NIFModified);

  // "lock_rmaos" attribute
  if (!flag(TruePBRData, "lock_rmaos")) {
    auto NewRMAOS = TexPath + NamedField + "_rmaos.dds";
    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::EnvMask, NewRMAOS, NIFModified);
  }

  // "lock_cnr" attribute
  if (!flag(TruePBRData, "lock_cnr")) {
    // "coat_normal" attribute
    string NewCNR;
    if (TruePBRData.contains("coat_normal") && TruePBRData["coat_normal"].get<bool>()) {
      NewCNR = TexPath + NamedField + "_cnr.dds";
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
      NewSubsurface = TexPath + NamedField + "_s.dds";
    }

    NIFUtil::setTextureSlot(NIF, NIFShape, NIFUtil::TextureSlots::Backlight, NewSubsurface, NIFModified);
  }

  // revert to default NIFShader type, remove flags used in other types
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF1_ENVIRONMENT_MAPPING, NIFModified);
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF1_PARALLAX, NIFModified);
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF2_GLOW_MAP, NIFModified);
  NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF2_BACK_LIGHTING, NIFModified);

  // Enable PBR flag
  NIFUtil::setShaderFlag(NIFShaderBSLSP, SLSF2_UNUSED01, NIFModified);

  if (TruePBRData.contains("subsurface_foliage") && TruePBRData["subsurface_foliage"].get<bool>() &&
      TruePBRData.contains("subsurface") && TruePBRData["subsurface"].get<bool>()) {
    spdlog::error("Error: Subsurface and foliage NIFShader chosen at once, undefined behavior!");
  }

  // "subsurface_foliage" attribute
  if (TruePBRData.contains("subsurface_foliage")) {
    NIFUtil::configureShaderFlag(NIFShaderBSLSP, SLSF2_SOFT_LIGHTING, TruePBRData["subsurface_foliage"].get<bool>(),
                                 NIFModified);
  }

  // "subsurface" attribute
  if (TruePBRData.contains("subsurface")) {
    NIFUtil::configureShaderFlag(NIFShaderBSLSP, SLSF2_RIM_LIGHTING, TruePBRData["subsurface"].get<bool>(),
                                 NIFModified);
  }

  // "multilayer" attribute
  if (TruePBRData.contains("multilayer") && TruePBRData["multilayer"]) {
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
  } else {
    // Revert to default NIFShader type
    NIFUtil::setShaderType(NIFShader, BSLSP_DEFAULT, NIFModified);
    NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF2_MULTI_LAYER_PARALLAX, NIFModified);
    NIFUtil::clearShaderFlag(NIFShaderBSLSP, SLSF2_BACK_LIGHTING, NIFModified);
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
