#include "patchers/PatcherDefault.hpp"

#include <Geometry.hpp>
#include <boost/algorithm/string.hpp>

#include "Logger.hpp"
#include "NIFUtil.hpp"

using namespace std;

auto PatcherDefault::getFactory() -> PatcherShader::PatcherShaderFactory {
  return [](const filesystem::path& NIFPath, nifly::NifFile *NIF) -> unique_ptr<PatcherShader> {
    return make_unique<PatcherDefault>(NIFPath, NIF);
  };
}

auto PatcherDefault::getShaderType() -> NIFUtil::ShapeShader {
  return NIFUtil::ShapeShader::NONE;
}

PatcherDefault::PatcherDefault(filesystem::path NIFPath, nifly::NifFile *NIF)
    : PatcherShader(std::move(NIFPath), NIF, "Default") {
}

auto PatcherDefault::canApply([[maybe_unused]] NiShape &NIFShape) -> bool {
  return true;
}

auto PatcherDefault::shouldApply(nifly::NiShape &NIFShape, std::vector<PatcherMatch> &Matches) -> bool {
  return shouldApply(getTextureSet(NIFShape), Matches);
}

auto PatcherDefault::shouldApply(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                         std::vector<PatcherMatch> &Matches) -> bool {
  Matches.clear();

  // Loop through slots
  for (size_t Slot = 0; Slot < NUM_TEXTURE_SLOTS; Slot++) {
    if (OldSlots[Slot].empty()) {
      continue;
    }

    // Check if file exists
    if (!getPGD()->isFile(OldSlots[Slot])) {
      continue;
    }

    // Add match
    PatcherMatch CurMatch;
    CurMatch.MatchedPath = OldSlots[Slot];
    CurMatch.MatchedFrom.insert(static_cast<NIFUtil::TextureSlots>(Slot));
    Matches.push_back(CurMatch);
  }

  return !Matches.empty();
}

auto PatcherDefault::applyPatch(nifly::NiShape &NIFShape, [[maybe_unused]] const PatcherMatch &Match,
                                        [[maybe_unused]] bool &NIFModified, [[maybe_unused]] bool &ShapeDeleted) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> {
  return getTextureSet(NIFShape);
}

auto PatcherDefault::applyPatchSlots(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &OldSlots,
                                             [[maybe_unused]] const PatcherMatch &Match) -> std::array<std::wstring, NUM_TEXTURE_SLOTS> {
  return OldSlots;
}

void PatcherDefault::applyShader(nifly::NiShape &NIFShape, bool &NIFModified) {
}

auto PatcherDefault::applyNeutral(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &Slots)
    -> std::array<std::wstring, NUM_TEXTURE_SLOTS> {
  return Slots;
}
