#pragma once

#include <memory>
#include <wx/arrstr.h>
#include <wx/dnd.h>
#include <wx/dragimag.h>
#include <wx/listbox.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/wx.h>

#include <string>
#include <vector>

#include "NIFUtil.hpp"

class ModSortDialog : public wxDialog {
private:
  wxListCtrl *ListCtrl;

public:
  ModSortDialog(const std::vector<std::wstring> &Mods, const std::vector<std::wstring> &Shaders,
                const std::vector<bool> &IsNew);
  [[nodiscard]] auto getSortedItems() const -> std::vector<std::wstring>;

private:
  auto calculateColumnWidth(int ColIndex) -> int;
  void onMoveUp(wxCommandEvent &Event);
  void onMoveDown(wxCommandEvent &Event);
  void swapItems(long FirstIndex, long SecondIndex);
};

class ParallaxGenUI {
public:
  static void init();

  static auto selectModOrder(const std::unordered_map<std::wstring, std::set<NIFUtil::ShapeShader>> &Conflicts,
                             const std::vector<std::wstring> &ExistingMods) -> std::vector<std::wstring>;
};
