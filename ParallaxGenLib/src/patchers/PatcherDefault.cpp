#include "patchers/PatcherDefault.hpp"

#include <Geometry.hpp>
#include <boost/algorithm/string.hpp>

#include "NIFUtil.hpp"

using namespace std;

auto PatcherDefault::getFactory() -> PatcherShader::PatcherShaderFactory
{
    return [](const filesystem::path& nifPath, nifly::NifFile* nif) -> unique_ptr<PatcherShader> {
        return make_unique<PatcherDefault>(nifPath, nif);
    };
}

auto PatcherDefault::getShaderType() -> NIFUtil::ShapeShader { return NIFUtil::ShapeShader::NONE; }

PatcherDefault::PatcherDefault(filesystem::path nifPath, nifly::NifFile* nif)
    : PatcherShader(std::move(nifPath), nif, "Default")
{
}

auto PatcherDefault::canApply([[maybe_unused]] NiShape& nifShape) -> bool { return true; }

auto PatcherDefault::shouldApply(nifly::NiShape& nifShape, std::vector<PatcherMatch>& matches) -> bool
{
    return shouldApply(getTextureSet(nifShape), matches);
}

auto PatcherDefault::shouldApply(
    const std::array<std::wstring, NUM_TEXTURE_SLOTS>& oldSlots, std::vector<PatcherMatch>& matches) -> bool
{
    matches.clear();

    // Loop through slots (only diffuse and normal)
    for (size_t slot = 0; slot <= 1; slot++) {
        if (oldSlots.at(slot).empty()) {
            continue;
        }

        // Check if file exists
        if (!getPGD()->isFile(oldSlots.at(slot))) {
            continue;
        }

        // Add match
        PatcherMatch curMatch;
        curMatch.matchedPath = oldSlots.at(slot);
        curMatch.matchedFrom.insert(static_cast<NIFUtil::TextureSlots>(slot));
        matches.push_back(curMatch);
    }

    return !matches.empty();
}

auto PatcherDefault::applyPatch(nifly::NiShape& nifShape, [[maybe_unused]] const PatcherMatch& match,
    [[maybe_unused]] bool& nifModified) -> std::array<std::wstring, NUM_TEXTURE_SLOTS>
{
    return getTextureSet(nifShape);
}

auto PatcherDefault::applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS>& oldSlots,
    [[maybe_unused]] const PatcherMatch& match) -> std::array<std::wstring, NUM_TEXTURE_SLOTS>
{
    return oldSlots;
}

void PatcherDefault::processNewTXSTRecord(const PatcherMatch& match, const std::string& edid) { }

void PatcherDefault::applyShader(nifly::NiShape& nifShape, bool& nifModified) { }
