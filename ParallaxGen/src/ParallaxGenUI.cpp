#include "ParallaxGenUI.hpp"

#include <boost/algorithm/string/join.hpp>
#include <wx/app.h>
#include <wx/arrstr.h>
#include <wx/event.h>
#include <wx/gdicmn.h>
#include <wx/msw/statbox.h>
#include <wx/msw/stattext.h>
#include <wx/sizer.h>
#include <wx/toplevel.h>

#include "GUI/LauncherWindow.hpp"
#include "GUI/ModSortDialog.hpp"
#include "ParallaxGenConfig.hpp"
#include "ParallaxGenUtil.hpp"

using namespace std;

// ParallaxGenUI class

bool ParallaxGenUI::UIExitTriggered = false;

void ParallaxGenUI::init() {
  wxApp::SetInstance(new wxApp()); // NOLINT(cppcoreguidelines-owning-memory)
  if (!wxEntryStart(nullptr, nullptr)) {
    throw runtime_error("Failed to initialize wxWidgets");
  }
}

auto ParallaxGenUI::showLauncher(const ParallaxGenConfig::PGParams &OldParams) -> ParallaxGenConfig::PGParams {
  auto *Launcher = new LauncherWindow(OldParams); // NOLINT(cppcoreguidelines-owning-memory)
  if (Launcher->ShowModal() == wxID_OK) {
    return Launcher->getParams();
  }

  return {};
}

auto ParallaxGenUI::selectModOrder(
    const std::unordered_map<std::wstring, tuple<std::set<NIFUtil::ShapeShader>, unordered_set<wstring>>> &Conflicts,
    const std::vector<std::wstring> &ExistingMods) -> std::vector<std::wstring> {

  vector<wstring> FinalModOrder;
  if (ExistingMods.size() == 0) {
    std::vector<std::pair<NIFUtil::ShapeShader, std::wstring>> ModOrder;
    for (const auto &[Mod, Value] : Conflicts) {
      const auto &ShaderSet = std::get<0>(Value);
      if (!ShaderSet.empty()) {
        ModOrder.emplace_back(*prev(ShaderSet.end()), Mod); // Get the first shader
      }
    }
    // no existing load order, sort for defaults
    // Sort by ShapeShader first, and then by key name lexicographically
    std::sort(ModOrder.begin(), ModOrder.end(), [](const auto &Lhs, const auto &Rhs) {
      if (Lhs.first == Rhs.first) {
        return Lhs.second < Rhs.second; // Secondary sort by wstring key
      }
      return Lhs.first < Rhs.first; // Primary sort by ShapeShader
    });

    for (const auto &[Shader, Mod] : ModOrder) {
      FinalModOrder.push_back(Mod);
    }
  } else {
    vector<wstring> NewMods;
    // Add existing mod order
    for (const auto &Mod : ExistingMods) {
      // check if existing mods contain the mod
      if (Conflicts.find(Mod) != Conflicts.end()) {
        // add to final mod order
        FinalModOrder.push_back(Mod);
      }
    }

    // Add new load mods
    for (const auto &[Mod, Data] : Conflicts) {
      // check if mod is in existing order
      if (find(ExistingMods.begin(), ExistingMods.end(), Mod) == ExistingMods.end()) {
        // add to new mods
        FinalModOrder.insert(FinalModOrder.begin(), Mod);
      }
    }
  }

  // split into vectors
  vector<wstring> ModStrs;
  vector<wstring> ShaderCombinedStrs;
  vector<bool> IsNew;
  unordered_map<wstring, unordered_set<wstring>> ConflictTracker;

  // first loop through existing order to restore is

  for (const auto &Mod : FinalModOrder) {
    ModStrs.push_back(Mod);

    vector<wstring> ShaderStrs;
    for (const auto &Shader : get<0>(Conflicts.at(Mod))) {
      ShaderStrs.insert(ShaderStrs.begin(), ParallaxGenUtil::UTF8toUTF16(NIFUtil::getStrFromShader(Shader)));
    }

    auto ShaderStr = boost::join(ShaderStrs, L",");
    ShaderCombinedStrs.push_back(ShaderStr);

    // check if mod is in existing order
    IsNew.push_back(find(ExistingMods.begin(), ExistingMods.end(), Mod) == ExistingMods.end());

    // add to conflict tracker
    ConflictTracker.insert({Mod, get<1>(Conflicts.at(Mod))});
  }

  ModSortDialog Dialog(ModStrs, ShaderCombinedStrs, IsNew, ConflictTracker);
  if (Dialog.ShowModal() == wxID_OK) {
    return Dialog.getSortedItems();
  }

  return ModStrs; // Return the original order if cancelled or closed
}
