#include "Patchers/PatcherShader.hpp"

#include "NIFUtil.hpp"
#include "ParallaxGenUtil.hpp"
#include "patchers/Patcher.hpp"
#include <BasicTypes.hpp>
#include <Shaders.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

using namespace std;

// statics

unordered_map<tuple<filesystem::path, uint32_t>, PatcherShader::PatchedTextureSet, PatcherShader::PatchedTextureSetsHash>
    PatcherShader::PatchedTextureSets;
mutex PatcherShader::PatchedTextureSetsMutex;

// Constructor
PatcherShader::PatcherShader(filesystem::path NIFPath, nifly::NifFile *NIF, string PatcherName)
    : Patcher(std::move(NIFPath), NIF, std::move(PatcherName)) {}

auto PatcherShader::getTextureSet(nifly::NiShape &NIFShape) -> array<wstring, NUM_TEXTURE_SLOTS> {
  lock_guard<mutex> Lock(PatchedTextureSetsMutex);

  auto *const NIFShader = getNIF()->GetShader(&NIFShape);
  const auto TexturesetBlockID = getNIF()->GetBlockID(getNIF()->GetHeader().GetBlock(NIFShader->TextureSetRef()));
  const auto NIFShapeKey = make_tuple(getNIFPath(), TexturesetBlockID);

  // check if in patchedtexturesets
  if (PatchedTextureSets.find(NIFShapeKey) != PatchedTextureSets.end()) {
    return PatchedTextureSets[NIFShapeKey].Original;
  }

  // get the texture slots
  const auto Slots = NIFUtil::getTextureSlots(getNIF(), &NIFShape);
  PatchedTextureSets[NIFShapeKey].Original = Slots;
  return Slots;
}

auto PatcherShader::setTextureSet(nifly::NiShape &NIFShape, const array<wstring, NUM_TEXTURE_SLOTS> &Textures, bool &NIFModified) -> void {
  lock_guard<mutex> Lock(PatchedTextureSetsMutex);

  auto *const NIFShader = getNIF()->GetShader(&NIFShape);
  const auto TextureSetBlockID = getNIF()->GetBlockID(getNIF()->GetHeader().GetBlock(NIFShader->TextureSetRef()));
  const auto NIFShapeKey = make_tuple(getNIFPath(), TextureSetBlockID);

  if (PatchedTextureSets.find(NIFShapeKey) != PatchedTextureSets.end()) {
    uint32_t NewBlockID = 0;

    // already been patched, check if it is the same
    for (const auto &[PossibleTexRecordID, PossibleTextures] : PatchedTextureSets[NIFShapeKey].PatchResults) {
      if (PossibleTextures == Textures) {
        NewBlockID = PossibleTexRecordID;

        if (NewBlockID == TextureSetBlockID) {
          return;
        }

        break;
      }
    }

    // Add a new texture set to the NIF
    if (NewBlockID == 0) {
      auto NewTextureSet = make_unique<nifly::BSShaderTextureSet>();
      NewTextureSet->textures.resize(9);
      for (uint32_t I = 0; I < Textures.size(); I++) {
        NewTextureSet->textures[I] = ParallaxGenUtil::UTF16toASCII(Textures[I]);
      }

      // Set shader reference
      NewBlockID = getNIF()->GetHeader().AddBlock(std::move(NewTextureSet));
    }

    auto *const NIFShaderBSLSP = dynamic_cast<nifly::BSLightingShaderProperty *>(NIFShader);
    NiBlockRef<BSShaderTextureSet> NewBlockRef(NewBlockID);
    NIFShaderBSLSP->textureSetRef = NewBlockRef;

    PatchedTextureSets[NIFShapeKey].PatchResults[NewBlockID] = Textures;
    NIFModified = true;
    return;
  }

  // set the texture slots for the shape like normal
  NIFUtil::setTextureSlots(getNIF(), &NIFShape, Textures, NIFModified);

  // update the patchedtexturesets
  PatchedTextureSets[NIFShapeKey].PatchResults[TextureSetBlockID] = Textures;
}
