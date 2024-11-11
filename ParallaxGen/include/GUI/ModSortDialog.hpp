#pragma once

#include <wx/arrstr.h>
#include <wx/dnd.h>
#include <wx/dragimag.h>
#include <wx/listbox.h>
#include <wx/listctrl.h>
#include <wx/msw/textctrl.h>
#include <wx/overlay.h>
#include <wx/sizer.h>
#include <wx/wx.h>

#include <string>
#include <unordered_set>
#include <vector>

/**
 * @brief wxDialog that allows the user to sort the mods in the order they want
 */
class ModSortDialog : public wxDialog {
private:
  wxListCtrl *ListCtrl;         /** Main list object that stores all the mods */
  wxTimer ScrollTimer;          /** Timer that is responsible for autoscroll */
  int ListCtrlHeaderHeight = 0; /** Stores list ctrl header height for use in autoscroll */

  //
  // Item Highlighting
  //
  std::unordered_map<std::wstring, std::unordered_set<std::wstring>>
      ConflictsMap; /** Stores the conflicts for each mod for highlighting */
  std::unordered_map<std::wstring, wxColour>
      OriginalBackgroundColors; /** Stores the original highlight of elements to be able to restore it later */

  //
  // Dragging
  //
  int TargetLineIndex = -1;         /** Stores the index of the element where an element is being dropped */
  std::vector<long> DraggedIndices; /** Stores the indices being dragged in the case of multi selection */
  wxOverlay Overlay;                /** Overlay used to paint guide lines for dragging */

  bool SortAscending; /** Stores whether the list is in asc or desc order */

public:
  /**
   * @brief Construct a new Mod Sort Dialog object
   *
   * @param Mods vector of mod strings
   * @param Shaders vector of shader strings
   * @param IsNew vector of bools indicating whether each mod is new or not (for highlighting)
   * @param Conflicts map that stores mod conflicts for highlighting
   */
  ModSortDialog(const std::vector<std::wstring> &Mods, const std::vector<std::wstring> &Shaders,
                const std::vector<bool> &IsNew,
                const std::unordered_map<std::wstring, std::unordered_set<std::wstring>> &Conflicts);

  /**
   * @brief Get the list of sorted mods (meant to be called after the user presses okay)
   *
   * @return std::vector<std::wstring> list of sorted mods
   */
  [[nodiscard]] auto getSortedItems() const -> std::vector<std::wstring>;

private:
  /**
   * @brief Event handler that triggers when an item is selected in the list (highlighting)
   *
   * @param Event wxWidgets event object
   */
  void onItemSelected(wxListEvent &Event);

  /**
   * @brief Event handler that triggers when the left mouse button is pressed down (dragging)
   *
   * @param Event wxWidgets event object
   */
  void onMouseLeftDown(wxMouseEvent &Event);

  /**
   * @brief Event handler that triggers when the mouse is moved (dragging)
   *
   * @param Event wxWidgets event object
   */
  void onMouseMotion(wxMouseEvent &Event);

  /**
   * @brief Event handler that triggers when the left mouse button is released (dragging)
   *
   * @param Event wxWidgets event object
   */
  void onMouseLeftUp(wxMouseEvent &Event);

  /**
   * @brief Event handler that triggers when a column is clicked (changing from asc to desc order)
   *
   * @param Event wxWidgets event object
   */
  void onColumnClick(wxListEvent &Event);

  /**
   * @brief Event handler that triggers from timer for autoscrolling while dragging
   *
   * @param Event wxWidgets event object
   */
  void onTimer(wxTimerEvent &Event);

  /**
   * @brief Get the Header Height for positioning
   *
   * @return int Header height
   */
  auto getHeaderHeight() -> int;

  /**
   * @brief Calculates the width of a column in the list
   *
   * @param ColIndex Index of column to calculate
   * @return int Width of column
   */
  auto calculateColumnWidth(int ColIndex) -> int;

  /**
   * @brief Highlights the conflicting items for a selected mod
   *
   * @param SelectedMod Mod that is selected
   */
  void highlightConflictingItems(const std::wstring &SelectedMod);

  /**
   * @brief Draws a drop indicator during drag and drop
   *
   * @param TargetIndex Index of the target drop item
   */
  void drawDropIndicator(int TargetIndex);

  /**
   * @brief Resets indices for the list after drag or sort
   */
  void resetIndices();

  /**
   * @brief Reverses the order of the list
   */
  void reverseListOrder();
};
