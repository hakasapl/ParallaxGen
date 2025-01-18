#include "Patchers/PatcherShader.hpp"

#include "NIFUtil.hpp"
#include "ParallaxGenUtil.hpp"
#include <BasicTypes.hpp>
#include <Shaders.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

using namespace std;

// statics

unordered_map<tuple<filesystem::path, uint32_t>, PatcherShader::PatchedTextureSet,
    PatcherShader::PatchedTextureSetsHash>
    PatcherShader::s_patchedTextureSets;
mutex PatcherShader::s_patchedTextureSetsMutex;

// Constructor
PatcherShader::PatcherShader(filesystem::path nifPath, nifly::NifFile* nif, string patcherName)
    : PatcherMesh(std::move(nifPath), nif, std::move(patcherName))
{
}

auto PatcherShader::getTextureSet(nifly::NiShape& nifShape) -> array<wstring, NUM_TEXTURE_SLOTS>
{
    const lock_guard<mutex> lock(s_patchedTextureSetsMutex);

    auto* const nifShader = getNIF()->GetShader(&nifShape);
    const auto texturesetBlockID = getNIF()->GetBlockID(getNIF()->GetHeader().GetBlock(nifShader->TextureSetRef()));
    const auto nifShapeKey = make_tuple(getNIFPath(), texturesetBlockID);

    // check if in patchedtexturesets
    if (s_patchedTextureSets.find(nifShapeKey) != s_patchedTextureSets.end()) {
        return s_patchedTextureSets[nifShapeKey].original;
    }

    // get the texture slots
    return NIFUtil::getTextureSlots(getNIF(), &nifShape);
}

auto PatcherShader::setTextureSet(
    nifly::NiShape& nifShape, const array<wstring, NUM_TEXTURE_SLOTS>& textures, bool& nifModified) -> void
{
    const lock_guard<mutex> lock(s_patchedTextureSetsMutex);

    auto* const nifShader = getNIF()->GetShader(&nifShape);
    const auto textureSetBlockID = getNIF()->GetBlockID(getNIF()->GetHeader().GetBlock(nifShader->TextureSetRef()));
    const auto nifShapeKey = make_tuple(getNIFPath(), textureSetBlockID);

    if (s_patchedTextureSets.find(nifShapeKey) != s_patchedTextureSets.end()) {
        // This texture set has been patched before
        uint32_t newBlockID = 0;

        // already been patched, check if it is the same
        for (const auto& [possibleTexRecordID, possibleTextures] : s_patchedTextureSets[nifShapeKey].patchResults) {
            if (possibleTextures == textures) {
                newBlockID = possibleTexRecordID;

                if (newBlockID == textureSetBlockID) {
                    return;
                }

                break;
            }
        }

        // Add a new texture set to the NIF
        if (newBlockID == 0) {
            auto newTextureSet = make_unique<nifly::BSShaderTextureSet>();
            newTextureSet->textures.resize(NUM_TEXTURE_SLOTS);
            for (uint32_t i = 0; i < textures.size(); i++) {
                newTextureSet->textures[i] = ParallaxGenUtil::utf16toASCII(textures.at(i));
            }

            // Set shader reference
            newBlockID = getNIF()->GetHeader().AddBlock(std::move(newTextureSet));
        }

        auto* const nifShaderBSLSP = dynamic_cast<nifly::BSLightingShaderProperty*>(nifShader);
        const NiBlockRef<BSShaderTextureSet> newBlockRef(newBlockID);
        nifShaderBSLSP->textureSetRef = newBlockRef;

        s_patchedTextureSets[nifShapeKey].patchResults[newBlockID] = textures;
        nifModified = true;
        return;
    }

    // set original for future use
    const auto slots = NIFUtil::getTextureSlots(getNIF(), &nifShape);
    s_patchedTextureSets[nifShapeKey].original = slots;

    // set the texture slots for the shape like normal
    NIFUtil::setTextureSlots(getNIF(), &nifShape, textures, nifModified);

    // update the patchedtexturesets
    s_patchedTextureSets[nifShapeKey].patchResults[textureSetBlockID] = textures;
}
