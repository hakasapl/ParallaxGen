#pragma once

#include <memory>
#include <wx/arrstr.h>
#include <wx/dnd.h>
#include <wx/dragimag.h>
#include <wx/listbox.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/wx.h>
#include <wx/overlay.h>

#include <string>
#include <vector>

#include "NIFUtil.hpp"

class ModSortDialog : public wxDialog {
private:
  wxListCtrl *ListCtrl;
  std::unordered_map<std::wstring, std::unordered_set<std::wstring>> ConflictsMap;

  int DraggedIndex = -1;
  int TargetLineIndex = -1;  // Index where the line is drawn

  wxOverlay Overlay;
  std::vector<long> DraggedIndices;  // Vector to store indices of dragged items

public:
  ModSortDialog(const std::vector<std::wstring> &Mods, const std::vector<std::wstring> &Shaders,
                const std::vector<bool> &IsNew, const std::unordered_map<std::wstring, std::unordered_set<std::wstring>> &Conflicts);
  [[nodiscard]] auto getSortedItems() const -> std::vector<std::wstring>;

private:
  auto calculateColumnWidth(int ColIndex) -> int;
  void onMoveUp(wxCommandEvent &Event);
  void onMoveDown(wxCommandEvent &Event);
  void swapItems(long FirstIndex, long SecondIndex);
  void onItemSelected(wxListEvent &Event);
  void highlightConflictingItems(const std::wstring &SelectedMod);
  void onMouseLeftDown(wxMouseEvent &Event);
  void onMouseMotion(wxMouseEvent &Event);
  void onMouseLeftUp(wxMouseEvent &Event);
};

class ParallaxGenUI {
public:
  static void init();

  static auto selectModOrder(const std::unordered_map<std::wstring, std::tuple<std::set<NIFUtil::ShapeShader>, std::unordered_set<std::wstring>>> &Conflicts,
                             const std::vector<std::wstring> &ExistingMods) -> std::vector<std::wstring>;
};
