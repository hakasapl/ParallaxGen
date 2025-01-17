#include "patchers/PatcherComplexMaterial.hpp"

#include <Shaders.hpp>
#include <boost/algorithm/string.hpp>

#include "Logger.hpp"
#include "NIFUtil.hpp"

using namespace std;

// Statics
std::vector<wstring> PatcherComplexMaterial::s_dynCubemapBlocklist;
bool PatcherComplexMaterial::s_disableMLP;

auto PatcherComplexMaterial::loadStatics(
    const bool& disableMLP, const std::vector<std::wstring>& dynCubemapBlocklist) -> void
{
    PatcherComplexMaterial::s_dynCubemapBlocklist = dynCubemapBlocklist;
    PatcherComplexMaterial::s_disableMLP = disableMLP;
}

auto PatcherComplexMaterial::getFactory() -> PatcherShader::PatcherShaderFactory
{
    return [](const filesystem::path& nifPath, nifly::NifFile* nif) -> unique_ptr<PatcherShader> {
        return make_unique<PatcherComplexMaterial>(nifPath, nif);
    };
}

auto PatcherComplexMaterial::getShaderType() -> NIFUtil::ShapeShader { return NIFUtil::ShapeShader::COMPLEXMATERIAL; }

PatcherComplexMaterial::PatcherComplexMaterial(filesystem::path nifPath, nifly::NifFile* nif)
    : PatcherShader(std::move(nifPath), nif, "ComplexMaterial")
{
}

auto PatcherComplexMaterial::canApply(NiShape& nifShape) -> bool
{
    // Prep
    Logger::trace(L"Starting checking");

    auto* nifShader = getNIF()->GetShader(&nifShape);

    // Get NIFShader type
    auto nifShaderType = static_cast<nifly::BSLightingShaderPropertyShaderType>(nifShader->GetShaderType());
    if (nifShaderType != BSLSP_DEFAULT && nifShaderType != BSLSP_ENVMAP && nifShaderType != BSLSP_PARALLAX
        && (nifShaderType != BSLSP_MULTILAYERPARALLAX || !s_disableMLP)) {
        Logger::trace(L"Shape Rejected: Incorrect NIFShader type");
        return false;
    }

    // check to make sure there are textures defined in slot 3 or 8
    if (nifShaderType != BSLSP_MULTILAYERPARALLAX
        && (!NIFUtil::getTextureSlot(getNIF(), &nifShape, NIFUtil::TextureSlots::GLOW).empty()
            || !NIFUtil::getTextureSlot(getNIF(), &nifShape, NIFUtil::TextureSlots::MULTILAYER).empty()
            || !NIFUtil::getTextureSlot(getNIF(), &nifShape, NIFUtil::TextureSlots::BACKLIGHT).empty())) {
        Logger::trace(L"Shape Rejected: Texture defined in slots 3,7,or 8");
        return false;
    }

    Logger::trace(L"Shape Accepted");
    return true;
}

auto PatcherComplexMaterial::shouldApply(nifly::NiShape& nifShape, std::vector<PatcherMatch>& matches) -> bool
{
    // Check for CM matches
    return shouldApply(getTextureSet(nifShape), matches);
}

auto PatcherComplexMaterial::shouldApply(
    const std::array<std::wstring, NUM_TEXTURE_SLOTS>& oldSlots, std::vector<PatcherMatch>& matches) -> bool
{
    static const auto cmBaseMap = getPGD()->getTextureMapConst(NIFUtil::TextureSlots::ENVMASK);

    matches.clear();

    // Search prefixes
    const auto searchPrefixes = NIFUtil::getSearchPrefixes(oldSlots);

    // Check if complex material file exists
    static const vector<int> slotSearch = { 1, 0 }; // Diffuse first, then normal
    filesystem::path baseMap;
    vector<NIFUtil::PGTexture> foundMatches;
    NIFUtil::TextureSlots matchedFromSlot = NIFUtil::TextureSlots::NORMAL;
    for (int slot : slotSearch) {
        baseMap = oldSlots.at(slot);
        if (baseMap.empty() || !getPGD()->isFile(baseMap)) {
            continue;
        }

        foundMatches.clear();
        foundMatches = NIFUtil::getTexMatch(searchPrefixes.at(slot), NIFUtil::TextureType::COMPLEXMATERIAL, cmBaseMap);

        if (!foundMatches.empty()) {
            // TODO should we be trying diffuse after normal too and present all options?
            matchedFromSlot = static_cast<NIFUtil::TextureSlots>(slot);
            break;
        }
    }

    PatcherMatch lastMatch; // Variable to store the match that equals OldSlots[Slot], if found
    for (const auto& match : foundMatches) {
        if (getPGD3D()->checkIfAspectRatioMatches(baseMap, match.path)) {
            PatcherMatch curMatch;
            curMatch.matchedPath = match.path;
            curMatch.matchedFrom.insert(matchedFromSlot);
            if (match.path == oldSlots[static_cast<size_t>(NIFUtil::TextureSlots::ENVMASK)]) {
                lastMatch = curMatch; // Save the match that equals OldSlots[Slot]
            } else {
                matches.push_back(curMatch); // Add other matches
            }
        }
    }

    if (!lastMatch.matchedPath.empty()) {
        matches.push_back(lastMatch); // Add the match that equals OldSlots[Slot]
    }

    return !matches.empty();
}

auto PatcherComplexMaterial::applyPatch(
    NiShape& nifShape, const PatcherMatch& match, bool& nifModified) -> std::array<std::wstring, NUM_TEXTURE_SLOTS>
{
    // Apply shader
    applyShader(nifShape, nifModified);

    // Check if specular should be white
    if (getPGD()->hasTextureAttribute(match.matchedPath, NIFUtil::TextureAttribute::CM_METALNESS)) {
        Logger::trace(L"Setting specular to white because CM has metalness");
        auto* nifShader = getNIF()->GetShader(&nifShape);
        auto* const nifShaderBSLSP = dynamic_cast<BSLightingShaderProperty*>(nifShader);
        NIFUtil::setShaderFloat(nifShaderBSLSP->specularColor.x, 1.0F, nifModified);
        NIFUtil::setShaderFloat(nifShaderBSLSP->specularColor.y, 1.0F, nifModified);
        NIFUtil::setShaderFloat(nifShaderBSLSP->specularColor.z, 1.0F, nifModified);
    }

    // Apply slots
    auto newSlots = applyPatchSlots(getTextureSet(nifShape), match);
    setTextureSet(nifShape, newSlots, nifModified);

    return newSlots;
}

auto PatcherComplexMaterial::applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS>& oldSlots,
    const PatcherMatch& match) -> std::array<std::wstring, NUM_TEXTURE_SLOTS>
{
    array<wstring, NUM_TEXTURE_SLOTS> newSlots = oldSlots;

    const auto matchedPath = match.matchedPath;

    newSlots[static_cast<size_t>(NIFUtil::TextureSlots::PARALLAX)] = L"";
    newSlots[static_cast<size_t>(NIFUtil::TextureSlots::ENVMASK)] = matchedPath;

    bool enableDynCubemaps
        = !(ParallaxGenDirectory::checkGlobMatchInVector(getNIFPath().wstring(), s_dynCubemapBlocklist)
            || ParallaxGenDirectory::checkGlobMatchInVector(matchedPath, s_dynCubemapBlocklist));
    if (enableDynCubemaps) {
        newSlots[static_cast<size_t>(NIFUtil::TextureSlots::CUBEMAP)]
            = L"textures\\cubemaps\\dynamic1pxcubemap_black.dds";
    }

    return newSlots;
}

void PatcherComplexMaterial::processNewTXSTRecord(const PatcherMatch& match, const std::string& edid) { }

void PatcherComplexMaterial::applyShader(NiShape& nifShape, bool& nifModified)
{
    auto* nifShader = getNIF()->GetShader(&nifShape);
    auto* const nifShaderBSLSP = dynamic_cast<BSLightingShaderProperty*>(nifShader);

    // Remove texture slots if disabling MLP
    if (s_disableMLP && nifShaderBSLSP->GetShaderType() == BSLSP_MULTILAYERPARALLAX) {
        NIFUtil::setTextureSlot(getNIF(), &nifShape, NIFUtil::TextureSlots::GLOW, "", nifModified);
        NIFUtil::setTextureSlot(getNIF(), &nifShape, NIFUtil::TextureSlots::MULTILAYER, "", nifModified);
        NIFUtil::setTextureSlot(getNIF(), &nifShape, NIFUtil::TextureSlots::BACKLIGHT, "", nifModified);

        NIFUtil::clearShaderFlag(nifShaderBSLSP, SLSF2_MULTI_LAYER_PARALLAX, nifModified);
    }

    // Set NIFShader type to env map
    NIFUtil::setShaderType(nifShader, BSLSP_ENVMAP, nifModified);
    NIFUtil::setShaderFloat(nifShaderBSLSP->environmentMapScale, 1.0F, nifModified);
    NIFUtil::setShaderFloat(nifShaderBSLSP->specularStrength, 1.0F, nifModified);

    // Set NIFShader flags
    NIFUtil::clearShaderFlag(nifShaderBSLSP, SLSF1_PARALLAX, nifModified);
    NIFUtil::clearShaderFlag(nifShaderBSLSP, SLSF2_UNUSED01, nifModified);
    NIFUtil::setShaderFlag(nifShaderBSLSP, SLSF1_ENVIRONMENT_MAPPING, nifModified);
}
