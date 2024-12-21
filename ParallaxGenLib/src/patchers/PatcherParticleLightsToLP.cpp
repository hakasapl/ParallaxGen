#include "patchers/PatcherParticleLightsToLP.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenUtil.hpp"
#include "patchers/PatcherGlobal.hpp"

#include <Animation.hpp>
#include <BasicTypes.hpp>
#include <Geometry.hpp>
#include <Keys.hpp>
#include <Nodes.hpp>
#include <Object3d.hpp>
#include <Shaders.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <nlohmann/json.hpp>

#include <nifly/Particles.hpp>

#include <fstream>

#include "Logger.hpp"

using namespace std;

// statics
nlohmann::json PatcherParticleLightsToLP::LPJsonData;
mutex PatcherParticleLightsToLP::LPJsonDataMutex;

PatcherParticleLightsToLP::PatcherParticleLightsToLP(std::filesystem::path NIFPath, nifly::NifFile *NIF)
    : PatcherGlobal(std::move(NIFPath), NIF, "ParticleLightsToLP") {}

auto PatcherParticleLightsToLP::getFactory() -> PatcherGlobal::PatcherGlobalFactory {
  return [](const filesystem::path &NIFPath, nifly::NifFile *NIF) -> unique_ptr<PatcherGlobal> {
    return make_unique<PatcherParticleLightsToLP>(NIFPath, NIF);
  };
}

auto PatcherParticleLightsToLP::applyPatch(bool &NIFModified) -> bool {
  // Loop through all blocks to find alpha properties
  // Determine if NIF has attached havok animations
  vector<NiObject *> NIFBlockTree;
  getNIF()->GetTree(NIFBlockTree);

  bool AppliedPatch = false;

  for (NiObject *NIFBlock : NIFBlockTree) {
    if (!boost::iequals(NIFBlock->GetBlockName(), "NiBillboardNode")) {
      continue;
    }
    auto *const BillboardNode = dynamic_cast<nifly::NiBillboardNode *>(NIFBlock);

    // Get children
    set<NiRef *> ChildRefs;
    BillboardNode->GetChildRefs(ChildRefs);

    if (ChildRefs.empty()) {
      // no children
      continue;
    }

    // get relevant objects
    nifly::NiShape *Shape = nullptr;
    nifly::NiAlphaProperty *AlphaProperty = nullptr;
    nifly::BSEffectShaderProperty *EffectShader = nullptr;
    nifly::NiParticleSystem *ParticleSystem = nullptr;

    // Loop through children and assign whatever is found
    for (auto *ChildRef : ChildRefs) {
      if (Shape == nullptr) {
        Shape = getNIF()->GetHeader().GetBlock<NiShape>(ChildRef);
      }

      if (ParticleSystem == nullptr) {
        ParticleSystem = getNIF()->GetHeader().GetBlock<NiParticleSystem>(ChildRef);
      }
    }

    if (Shape == nullptr) {
      // No shape available
      continue;
    }

    if (ParticleSystem != nullptr) {
      // This node has a particle system
      // Find the alpha property and effect shader from particle system
      const auto PSAlphaPropertyRef = ParticleSystem->alphaPropertyRef;
      if (!PSAlphaPropertyRef.IsEmpty() && AlphaProperty == nullptr) {
        AlphaProperty = getNIF()->GetHeader().GetBlock<NiAlphaProperty>(PSAlphaPropertyRef);
      }

      const auto PSShaderPropertyRef = ParticleSystem->shaderPropertyRef;
      if (!PSShaderPropertyRef.IsEmpty() && EffectShader == nullptr) {
        EffectShader = getNIF()->GetHeader().GetBlock<BSEffectShaderProperty>(PSShaderPropertyRef);
      }
    }

    if (ParticleSystem == nullptr) {
      // This node does not have a particle system
      // Find the alpha property and effect shader from shape
      auto *const ShapeAlphaPropertyRef = Shape->AlphaPropertyRef();
      if (!ShapeAlphaPropertyRef->IsEmpty() && AlphaProperty == nullptr) {
        AlphaProperty = getNIF()->GetHeader().GetBlock<NiAlphaProperty>(ShapeAlphaPropertyRef);
      }

      auto *const ShapeShaderPropertyRef = Shape->ShaderPropertyRef();
      if (!ShapeShaderPropertyRef->IsEmpty() && EffectShader == nullptr) {
        EffectShader = getNIF()->GetHeader().GetBlock<BSEffectShaderProperty>(ShapeShaderPropertyRef);
      }
    }

    if (AlphaProperty == nullptr || EffectShader == nullptr) {
      // No alpha property or effect shader
      continue;
    }

    // Validate that all parameters are met for a particle light
    if (AlphaProperty->flags != 4109) {
      // no additive blending
      continue;
    }

    if ((EffectShader->shaderFlags1 & SLSF1_SOFT_EFFECT) == 0U) {
      // no soft effect shader flag
      continue;
    }

    Logger::trace(L"Found Particle Light: {}", ParallaxGenUtil::ASCIItoUTF16(NIFBlock->GetBlockName()));

    // Apply patch to this particle light
    if (applySinglePatch(BillboardNode, Shape, EffectShader)) {
      // Delete block if patch was applied
      NIFModified = true;
      const auto NIFBlockRef = nifly::NiRef(getNIF()->GetBlockID(NIFBlock));
      getNIF()->GetHeader().DeleteBlock(NIFBlockRef);
    }
  }

  return AppliedPatch;
}

auto PatcherParticleLightsToLP::applySinglePatch(nifly::NiBillboardNode *Node, nifly::NiShape *Shape,
                                                 nifly::BSEffectShaderProperty *EffectShader) -> bool {
  // Start generating LP JSON
  nlohmann::json LPJson;

  // Set model path
  LPJson["models"] = nlohmann::json::array();

  // Remove "meshes\\" from start of path
  const auto NIFPath = boost::ireplace_first_copy(getNIFPath().string(), "meshes\\", "");
  LPJson["models"].push_back(NIFPath);

  // Create lights array
  LPJson["lights"] = nlohmann::json::array();

  // LightEntry will hold all JSON data for light
  nlohmann::json LightEntry;
  LightEntry["data"] = nlohmann::json::object();

  // Set position
  MatTransform GlobalPosition;
  getNIF()->GetNodeTransformToGlobal(Node->name.get(), GlobalPosition);

  // Setup points array
  LightEntry["points"] = nlohmann::json::array();
  LightEntry["points"].push_back(nlohmann::json::array({round(GlobalPosition.translation.x * 100.0) / 100.0,
                                                        round(GlobalPosition.translation.y * 100.0) / 100.0,
                                                        round(GlobalPosition.translation.z * 100.0) / 100.0}));

  // Set Light
  LightEntry["data"]["light"] = "MagicLightWhite01"; // Placeholder light that will be overridden

  // BSTriShape conversion
  auto *const ShapeBSTriShape = dynamic_cast<nifly::BSTriShape *>(Shape);
  if (ShapeBSTriShape == nullptr) {
    // not a BSTriShape
    // TODO support other shape types?
    return false;
  }

  const auto VertData = ShapeBSTriShape->vertData;
  const auto NumVerts = VertData.size();

  // set color
  auto BaseColor = EffectShader->baseColor;
  bool GetColorFromVertex = abs(BaseColor.r - 1.0) < 1e-5 && abs(BaseColor.g - 1.0) < 1e-5 &&
                            abs(BaseColor.b - 1.0) < 1e-5 && Shape->HasVertexColors();
  if (GetColorFromVertex) {
    // Reset basecolor
    BaseColor = {0, 0, 0, 0};

    // Get color from vertex
    for (const auto &Vertex : VertData) {
      BaseColor.r += static_cast<float>(Vertex.colorData[0]);
      BaseColor.g += static_cast<float>(Vertex.colorData[1]);
      BaseColor.b += static_cast<float>(Vertex.colorData[2]);
    }

    BaseColor.r /= static_cast<float>(NumVerts);
    BaseColor.g /= static_cast<float>(NumVerts);
    BaseColor.b /= static_cast<float>(NumVerts);
  } else {
    // Convert to 0-255
    BaseColor *= 255.0;
  }

  LightEntry["data"]["color"] = nlohmann::json::array(
      {static_cast<int>(BaseColor.r), static_cast<int>(BaseColor.g), static_cast<int>(BaseColor.b)});

  // Calculate radius from average of vertex distances
  double Radius = 0.0;
  for (const auto &Vertex : VertData) {
    // Find distance with pythagorean theorem
    const auto CurDistance = sqrt(pow(Vertex.vert.x, 2) + pow(Vertex.vert.y, 2) + pow(Vertex.vert.z, 2));
    // update radius with average
    Radius += CurDistance;
  }

  Radius /= static_cast<double>(NumVerts);

  LightEntry["data"]["radius"] = static_cast<int>(Radius);

  LightEntry["data"]["fade"] = round(EffectShader->baseColorScale * 100.0) / 100.0;

  // Get controllers
  auto ControllerRef = EffectShader->controllerRef;
  while (!ControllerRef.IsEmpty()) {
    auto *const Controller = getNIF()->GetHeader().GetBlock(ControllerRef);
    if (Controller == nullptr) {
      break;
    }

    string JSONField;
    const auto ControllerJson = getControllerJSON(Controller, JSONField);
    if (!ControllerJson.empty()) {
      LightEntry["data"][JSONField] = ControllerJson;
    }

    ControllerRef = Controller->nextControllerRef;
  }

  LPJson["lights"].push_back(LightEntry);

  // Save JSON
  {
    lock_guard<mutex> Lock(LPJsonDataMutex);
    PatcherParticleLightsToLP::LPJsonData.push_back(LPJson);
  }

  return true;
}

auto PatcherParticleLightsToLP::getControllerJSON(nifly::NiTimeController *Controller,
                                                  string &JSONField) -> nlohmann::json {
  nlohmann::json ControllerJson = nlohmann::json::object();

  if (Controller == nullptr) {
    return ControllerJson;
  }

  auto *const FloatController = dynamic_cast<nifly::BSEffectShaderPropertyFloatController *>(Controller);
  if (FloatController != nullptr) {
    if (FloatController->typeOfControlledVariable != 0) {
      // We don't care about this controller (not controlling emissive mult)
      return ControllerJson;
    }

    JSONField = "fadeController";
  }

  auto *const ColorController = dynamic_cast<nifly::BSEffectShaderPropertyColorController *>(Controller);
  if (ColorController != nullptr) {
    JSONField = "colorController";
  }

  if ((FloatController == nullptr && ColorController == nullptr) ||
      (FloatController != nullptr && ColorController != nullptr)) {
    // We don't care about this controller
    return ControllerJson;
  }

  const auto InterpRef = FloatController->interpolatorRef;
  if (InterpRef.IsEmpty()) {
    return ControllerJson;
  }

  // Find data block
  nifly::NiFloatData *FadeDataBlock = nullptr;
  nifly::NiPosData *ColorDataBlock = nullptr;

  auto *const FloatInterpolator = getNIF()->GetHeader().GetBlock<NiFloatInterpolator>(InterpRef);
  if (FloatInterpolator != nullptr) {
    const auto DataRef = FloatInterpolator->dataRef;
    FadeDataBlock = getNIF()->GetHeader().GetBlock<nifly::NiFloatData>(DataRef);
  }

  auto *const NiPoint3Interpolator = dynamic_cast<nifly::NiPoint3Interpolator *>(FloatInterpolator);
  if (NiPoint3Interpolator != nullptr) {
    const auto DataRef = NiPoint3Interpolator->dataRef;
    ColorDataBlock = getNIF()->GetHeader().GetBlock<nifly::NiPosData>(DataRef);
  }

  if ((FadeDataBlock == nullptr && FloatController != nullptr) ||
      (ColorDataBlock == nullptr && ColorController != nullptr)) {
    // Could not find data block
    return ControllerJson;
  }

  ControllerJson["keys"] = nlohmann::json::array();

  nifly::NiKeyType InterpolationType = nifly::NiKeyType::LINEAR_KEY;
  if (FloatController != nullptr) {
    InterpolationType = FadeDataBlock->data.GetInterpolationType();
  } else if (ColorController != nullptr) {
    InterpolationType = ColorDataBlock->data.GetInterpolationType();
  }

  // Get interpolation method
  switch (InterpolationType) {
  case nifly::NiKeyType::LINEAR_KEY:
    ControllerJson["interpolation"] = "Linear";
    break;
  case nifly::NiKeyType::QUADRATIC_KEY:
    ControllerJson["interpolation"] = "Cubic";
    break;
  case nifly::NiKeyType::NO_INTERP:
    ControllerJson["interpolation"] = "Step";
    break;
  default:
    // Linear interpolation default
    ControllerJson["interpolation"] = "Linear";
    break;
  }

  // Add keys to JSON
  uint32_t NumKeys = 0;
  if (FloatController != nullptr) {
    NumKeys = FadeDataBlock->data.GetNumKeys();
  } else if (ColorController != nullptr) {
    NumKeys = ColorDataBlock->data.GetNumKeys();
  }

  for (uint32_t I = 0; I < NumKeys; I++) {
    if (FloatController != nullptr) {
      const auto CurKey = FadeDataBlock->data.GetKey(static_cast<int>(I));

      ControllerJson["keys"].push_back(
          nlohmann::json::object({{"time", round(CurKey.time * 1000000.0) / 1000000.0},
                                  {"value", round(CurKey.value * 1000000.0) / 1000000.0},
                                  {"forward", round(CurKey.forward * 1000000.0) / 1000000.0},
                                  {"backward", round(CurKey.backward * 1000000.0) / 1000000.0}}));
    } else if (ColorController != nullptr) {
      const auto CurKey = ColorDataBlock->data.GetKey(static_cast<int>(I));

      ControllerJson["keys"].push_back(nlohmann::json::object(
          {{"time", round(CurKey.time * 1000000.0) / 1000000.0},
           {"color", nlohmann::json::array({static_cast<int>(CurKey.value.x), static_cast<int>(CurKey.value.y),
                                            static_cast<int>(CurKey.value.z)})},
           {"forward", nlohmann::json::array({static_cast<int>(CurKey.forward.x), static_cast<int>(CurKey.forward.y),
                                              static_cast<int>(CurKey.forward.z)})},
           {"backward", nlohmann::json::array({static_cast<int>(CurKey.backward.x), static_cast<int>(CurKey.backward.y),
                                               static_cast<int>(CurKey.backward.z)})}}));
    }
  }

  return ControllerJson;
}

void PatcherParticleLightsToLP::finalize() {
  lock_guard<mutex> Lock(LPJsonDataMutex);

  // Check if output JSON is empty
  if (LPJsonData.empty()) {
    return;
  }

  // Key type for grouping: the stringified "models" array.
  std::unordered_map<std::string, std::unordered_map<std::string, std::vector<nlohmann::json>>> ModelsToDataPoints;

  // --- Step 1: Group by "models". ---
  for (auto &Obj : LPJsonData) {
    // Convert the entire "models" array to a string key for grouping.
    std::string ModelsKey = Obj["models"].dump();

    // Retrieve or create the DataGroup for these models.
    auto &DataGroup = ModelsToDataPoints[ModelsKey];

    // --- Step 2: For each light, group by "data". ---
    for (auto &Light : Obj["lights"]) {
      // The entire "data" object as a string key:
      std::string DataKey = Light["data"].dump();

      // The "points" array might have multiple points,
      // but typically in your example there's only one per object.
      // We'll append them all to the vector in dataGroup.
      for (auto &P : Light["points"]) {
        DataGroup[DataKey].push_back(P);
      }
    }
  }

  // --- Step 3: Build the merged output. ---
  // We'll produce a JSON array of objects, each with "models" and "lights".
  nlohmann::json MergedOutput = nlohmann::json::array();

  for (auto &Kv : ModelsToDataPoints) {
    const auto &ModelsKey = Kv.first;
    const auto &DataGroup = Kv.second;

    // Reconstruct the "models" array from the key.
    nlohmann::json ModelsJson = nlohmann::json::parse(ModelsKey);

    // Build "lights" by iterating over each distinct dataKey
    nlohmann::json LightsArray = nlohmann::json::array();

    for (const auto &DataKv : DataGroup) {
      std::string DataKey = DataKv.first;
      // Reconstruct the data object from string
      nlohmann::json DataObj = nlohmann::json::parse(DataKey);

      // The merged points for this data
      const auto &AllPoints = DataKv.second; // vector<json>

      // Create the light entry
      nlohmann::json LightEntry;
      LightEntry["data"] = DataObj;
      LightEntry["points"] = nlohmann::json::array();

      // Append all points
      for (const auto &Pt : AllPoints) {
        LightEntry["points"].push_back(Pt);
      }

      // Add this light entry to the lights array
      LightsArray.push_back(LightEntry);
    }

    // Now assemble the final group object
    nlohmann::json GroupObj;
    GroupObj["models"] = ModelsJson;
    GroupObj["lights"] = LightsArray;

    // Add to the final merged output
    MergedOutput.push_back(GroupObj);
  }

  const auto OutputJSON = getPGD()->getGeneratedPath() / "LightPlacer/parallaxgen.json";

  // Create directories for parent path
  filesystem::create_directories(OutputJSON.parent_path());

  // Save JSON to file
  ofstream LPJsonFile(OutputJSON);
  LPJsonFile << MergedOutput.dump(2) << endl;
  LPJsonFile.close();
}
