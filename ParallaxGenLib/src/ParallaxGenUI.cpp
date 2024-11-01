#include "ParallaxGenUI.hpp"

#include <boost/algorithm/string/join.hpp>
#include <list>
#include <wx/gdicmn.h>

#include "ParallaxGenUtil.hpp"

using namespace std;

// class ModSortDialog
ModSortDialog::ModSortDialog(const std::vector<std::wstring> &Mods, const std::vector<std::wstring> &Shaders,
                             const std::vector<bool> &IsNew,
                             const std::unordered_map<std::wstring, std::unordered_set<std::wstring>> &Conflicts)
    : wxDialog(nullptr, wxID_ANY, "Set Mod Priority", wxDefaultPosition, wxSize(300, 400),
               wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP),
      ScrollTimer(this), ConflictsMap(Conflicts), SortAscending(true) {
  Bind(wxEVT_TIMER, &ModSortDialog::onTimer, this, ScrollTimer.GetId());

  auto *MainSizer = new wxBoxSizer(wxVERTICAL);
  // Create the ListCtrl
  ListCtrl = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(250, 300), wxLC_REPORT);
  ListCtrl->InsertColumn(0, "Mod");
  ListCtrl->InsertColumn(1, "Shader");
  ListCtrl->InsertColumn(2, "Priority");

  ListCtrl->Bind(wxEVT_LIST_ITEM_SELECTED, &ModSortDialog::onItemSelected, this);
  ListCtrl->Bind(wxEVT_LIST_COL_CLICK, &ModSortDialog::onColumnClick, this);

  ListCtrl->Bind(wxEVT_LEFT_DOWN, &ModSortDialog::onMouseLeftDown, this);
  ListCtrl->Bind(wxEVT_MOTION, &ModSortDialog::onMouseMotion, this);
  ListCtrl->Bind(wxEVT_LEFT_UP, &ModSortDialog::onMouseLeftUp, this);

  // Add items to ListCtrl and highlight specific ones
  for (size_t I = 0; I < Mods.size(); ++I) {
    long Index = ListCtrl->InsertItem(static_cast<long>(I), Mods[I]);
    ListCtrl->SetItem(Index, 1, Shaders[I]);
    ListCtrl->SetItem(Index, 2, std::to_string(I));
    if (IsNew[I]) {
      ListCtrl->SetItemBackgroundColour(Index, *wxGREEN); // Highlight color
      OriginalBackgroundColors[Mods[I]] = *wxGREEN;       // Store the original color using the mod name
    } else {
      OriginalBackgroundColors[Mods[I]] = *wxWHITE; // Store the original color using the mod name
    }
  }

  // Calculate minimum width for each column
  int Col1Width = calculateColumnWidth(0);
  ListCtrl->SetColumnWidth(0, Col1Width);
  int Col2Width = calculateColumnWidth(1);
  ListCtrl->SetColumnWidth(1, Col2Width);
  int Col3Width = calculateColumnWidth(2);
  ListCtrl->SetColumnWidth(2, Col3Width);

  int ScrollBarWidth = wxSystemSettings::GetMetric(wxSYS_VSCROLL_X);
  int TotalWidth = Col1Width + Col2Width + Col3Width + 40 + ScrollBarWidth; // Extra padding

  // Add wrapped message at the top
  static const std::wstring Message = L"The following mods have been detected as potential conflicts. Please set the "
                                      L"priority order for these mods. Mods that are highlighted in green are new mods "
                                      L"that do not exist in the saved order and may need to be sorted.";
  // Create the wxStaticText and set wrapping
  auto *MessageText = new wxStaticText(this, wxID_ANY, Message, wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
  MessageText->Wrap(TotalWidth - 40); // Adjust wrapping width

  // Let wxWidgets automatically calculate the best height based on wrapped text
  MessageText->SetMinSize(wxSize(TotalWidth - 40, MessageText->GetBestSize().y));

  // Add the static text to the main sizer
  MainSizer->Add(MessageText, 0, wxEXPAND | wxALL, 10);

  // Adjust dialog width to match the total width of columns and padding
  SetSizeHints(TotalWidth, 0); // Adjust minimum width and height
  SetSize(TotalWidth, 400);    // Set dialog size

  MainSizer->Add(ListCtrl, 1, wxEXPAND | wxALL, 10);

  // Add OK button
  auto *OkButton = new wxButton(this, wxID_OK, "OK");
  MainSizer->Add(OkButton, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 10);

  SetSizer(MainSizer);

  ListCtrlHeaderHeight = getHeaderHeight();
}

// EVENT HANDLERS

void ModSortDialog::onItemSelected(wxListEvent &Event) {
  long Index = Event.GetIndex();
  std::wstring SelectedMod = ListCtrl->GetItemText(Index).ToStdWstring();
  highlightConflictingItems(SelectedMod);
}

void ModSortDialog::highlightConflictingItems(const std::wstring &SelectedMod) {
  // Clear previous highlights and restore original colors
  for (long I = 0; I < ListCtrl->GetItemCount(); ++I) {
    std::wstring ItemText = ListCtrl->GetItemText(I).ToStdWstring();
    auto It = OriginalBackgroundColors.find(ItemText);
    if (It != OriginalBackgroundColors.end()) {
      ListCtrl->SetItemBackgroundColour(I, It->second); // Restore original color
    } else {
      ListCtrl->SetItemBackgroundColour(I, *wxWHITE); // Fallback to white if not found
    }
  }

  // Highlight selected item and its conflicts
  auto ConflictSet = ConflictsMap.find(SelectedMod);
  if (ConflictSet != ConflictsMap.end()) {
    for (long I = 0; I < ListCtrl->GetItemCount(); ++I) {
      std::wstring ItemText = ListCtrl->GetItemText(I).ToStdWstring();
      if (ItemText == SelectedMod || ConflictSet->second.contains(ItemText)) {
        ListCtrl->SetItemBackgroundColour(I, *wxYELLOW); // Highlight color
      }
    }
  }
}

void ModSortDialog::onMouseLeftDown(wxMouseEvent &Event) {
  int Flags = 0;
  long ItemIndex = ListCtrl->HitTest(Event.GetPosition(), Flags);

  if (ItemIndex != wxNOT_FOUND) {
    // Select the item if it's not already selected
    if ((ListCtrl->GetItemState(ItemIndex, wxLIST_STATE_SELECTED) & wxLIST_STATE_SELECTED) == 0) {
      ListCtrl->SetItemState(ItemIndex, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    }

    // Capture the indices of all selected items for dragging
    DraggedIndices.clear();
    long SelectedItem = -1;
    while ((SelectedItem = ListCtrl->GetNextItem(SelectedItem, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED)) !=
           wxNOT_FOUND) {
      DraggedIndices.push_back(SelectedItem);
    }

    // Set the initial drag index
    DraggedIndex = ItemIndex;
  }

  ScrollTimer.Start(250);
  Event.Skip();
}

void ModSortDialog::onMouseLeftUp(wxMouseEvent &Event) {
  if (!DraggedIndices.empty() && TargetLineIndex != -1) {
    Overlay.Reset(); // Clear the overlay when the drag operation is complete

    // Sort indices to maintain the order during removal
    std::sort(DraggedIndices.begin(), DraggedIndices.end());

    // Capture item data for all selected items
    std::vector<std::pair<wxString, wxString>> ItemData;
    std::vector<wxColour> BackgroundColors;

    for (auto Index : DraggedIndices) {
      ItemData.emplace_back(ListCtrl->GetItemText(Index, 0), ListCtrl->GetItemText(Index, 1));
      BackgroundColors.push_back(ListCtrl->GetItemBackgroundColour(Index));
    }

    // Remove items from their original positions
    for (int I = static_cast<int>(DraggedIndices.size()) - 1; I >= 0; --I) {
      ListCtrl->DeleteItem(DraggedIndices[I]);
      // Adjust TargetLineIndex if items were removed from above it
      if (DraggedIndices[I] < TargetLineIndex) {
        TargetLineIndex--;
      }
    }

    // Insert items at the new position
    long InsertPos = TargetLineIndex;
    for (size_t I = 0; I < ItemData.size(); ++I) {
      long NewIndex = ListCtrl->InsertItem(InsertPos, ItemData[I].first);
      ListCtrl->SetItem(NewIndex, 1, ItemData[I].second);
      ListCtrl->SetItemBackgroundColour(NewIndex, BackgroundColors[I]);
      InsertPos++;
    }

    // reset priority values
    resetIndices();

    // Reset indices to prepare for the next drag
    DraggedIndices.clear();
    DraggedIndex = -1;
    TargetLineIndex = -1;
  }

  // Stop the timer when the drag operation ends
  if (ScrollTimer.IsRunning()) {
    ScrollTimer.Stop();
  }

  Event.Skip();
}

void ModSortDialog::onMouseMotion(wxMouseEvent &Event) {
  if (!DraggedIndices.empty() && Event.LeftIsDown()) {
    int Flags = 0;
    auto DropTargetIndex = ListCtrl->HitTest(Event.GetPosition(), Flags);

    if (DropTargetIndex != wxNOT_FOUND) {
      wxRect ItemRect;
      ListCtrl->GetItemRect(DropTargetIndex, ItemRect);

      // Check if the mouse is in the top or bottom half of the item
      const int MidPointY = ItemRect.GetTop() + (ItemRect.GetHeight() / 2);
      const auto CurPosition = Event.GetPosition().y;
      bool TargetingTopHalf = CurPosition > MidPointY;

      if (TargetingTopHalf) {
        DropTargetIndex++;
      }

      // Draw the line only if the target index has changed
      drawDropIndicator(DropTargetIndex);
      TargetLineIndex = DropTargetIndex;
    } else {
      // Clear overlay if not hovering over a valid item
      Overlay.Reset();
      TargetLineIndex = -1;
    }
  }
  Event.Skip();
}

void ModSortDialog::onTimer(wxTimerEvent &Event) {
  // Get the current mouse position relative to the ListCtrl
  wxPoint MousePos = ScreenToClient(wxGetMousePosition());
  wxRect ListCtrlRect = ListCtrl->GetRect();

  // Check if the mouse is within the ListCtrl bounds
  if (ListCtrlRect.Contains(MousePos)) {
    const int ScrollMargin = 20; // Margin to trigger scrolling
    const int MouseY = MousePos.y;

    if (MouseY < ListCtrlRect.GetTop() + ScrollMargin + ListCtrlHeaderHeight) {
      // Scroll up if the mouse is near the top edge
      ListCtrl->ScrollLines(-1);
    } else if (MouseY > ListCtrlRect.GetBottom() - ScrollMargin) {
      // Scroll down if the mouse is near the bottom edge
      ListCtrl->ScrollLines(1);
    }
  }
}

// HELPERS

auto ModSortDialog::getHeaderHeight() -> int {
  if (ListCtrl->GetItemCount() > 0) {
    wxRect FirstItemRect;
    if (ListCtrl->GetItemRect(0, FirstItemRect)) {
      // The top of the first item minus the top of the client area gives the header height
      return FirstItemRect.GetTop() - ListCtrl->GetClientRect().GetTop();
    }
  }
  return 0; // Fallback if the list is empty
}

void ModSortDialog::drawDropIndicator(int TargetIndex) {
  wxClientDC DC(ListCtrl);
  wxDCOverlay DCOverlay(Overlay, &DC);
  DCOverlay.Clear(); // Clear the existing overlay to avoid double lines

  wxRect ItemRect;
  int LineY = -1;

  // Validate TargetIndex before calling GetItemRect()
  if (TargetIndex >= 0 && TargetIndex < ListCtrl->GetItemCount()) {
    if (ListCtrl->GetItemRect(TargetIndex, ItemRect)) {
      LineY = ItemRect.GetTop();
    }
  } else if (TargetIndex >= ListCtrl->GetItemCount()) {
    // Handle drawing at the end of the list (after the last item)
    if (ListCtrl->GetItemRect(ListCtrl->GetItemCount() - 1, ItemRect)) {
      LineY = ItemRect.GetBottom();
    }
  }

  // Ensure LineY is set correctly before drawing
  if (LineY != -1) {
    DC.SetPen(wxPen(*wxBLACK, 2)); // Draw the line with a width of 2 pixels
    DC.DrawLine(ItemRect.GetLeft(), LineY, ItemRect.GetRight(), LineY);
  }
}

void ModSortDialog::resetIndices() {
  // loop through each item in list and set col 3
  for (long I = 0; I < ListCtrl->GetItemCount(); ++I) {
    if (SortAscending) {
      ListCtrl->SetItem(I, 2, std::to_string(I));
    } else {
      ListCtrl->SetItem(I, 2, std::to_string(ListCtrl->GetItemCount() - I - 1));
    }
  }
}

void ModSortDialog::onColumnClick(wxListEvent &Event) {
  int Column = Event.GetColumn();

  // Toggle sort order if the same column is clicked, otherwise reset to ascending
  if (Column == 2) {
    // Only sort priority col
    SortAscending = !SortAscending;

    // Reverse the order of every item
    reverseListOrder();
  }

  Event.Skip();
}

void ModSortDialog::reverseListOrder() {
  // Store all items in a vector
  std::vector<std::vector<wxString>> Items;
  std::vector<wxColour> BackgroundColors;

  for (long I = 0; I < ListCtrl->GetItemCount(); ++I) {
    std::vector<wxString> Row;
    Row.reserve(ListCtrl->GetColumnCount());
    for (int Col = 0; Col < ListCtrl->GetColumnCount(); ++Col) {
      Row.push_back(ListCtrl->GetItemText(I, Col));
    }
    Items.push_back(Row);

    // Store original background color using the mod name
    std::wstring ItemText = Row[0].ToStdWstring();
    auto It = OriginalBackgroundColors.find(ItemText);
    if (It != OriginalBackgroundColors.end()) {
      BackgroundColors.push_back(It->second);
    } else {
      BackgroundColors.push_back(*wxWHITE); // Default color if not found
    }
  }

  // Clear the ListCtrl
  ListCtrl->DeleteAllItems();

  // Insert items back in reverse order and set background colors
  for (size_t I = 0; I < Items.size(); ++I) {
    long NewIndex = ListCtrl->InsertItem(ListCtrl->GetItemCount(), Items[Items.size() - 1 - I][0]);
    for (int Col = 1; Col < ListCtrl->GetColumnCount(); ++Col) {
      ListCtrl->SetItem(NewIndex, Col, Items[Items.size() - 1 - I][Col]);
    }

    // Set background color for the new item
    ListCtrl->SetItemBackgroundColour(NewIndex, BackgroundColors[Items.size() - 1 - I]);
  }
}

auto ModSortDialog::calculateColumnWidth(int ColIndex) -> int {
  int MaxWidth = 0;
  wxClientDC DC(ListCtrl);
  DC.SetFont(ListCtrl->GetFont());

  for (int I = 0; I < ListCtrl->GetItemCount(); ++I) {
    wxString ItemText = ListCtrl->GetItemText(I, ColIndex);
    int Width = 0;
    int Height = 0;
    DC.GetTextExtent(ItemText, &Width, &Height);
    if (Width > MaxWidth) {
      MaxWidth = Width;
    }
  }
  return MaxWidth + 20; // Add some padding
}

auto ModSortDialog::getSortedItems() const -> std::vector<std::wstring> {
  std::vector<std::wstring> SortedItems;
  for (long I = 0; I < ListCtrl->GetItemCount(); ++I) {
    SortedItems.push_back(ListCtrl->GetItemText(I).ToStdWstring());
  }

  if (!SortAscending) {
    // reverse items if descending
    std::reverse(SortedItems.begin(), SortedItems.end());
  }

  return SortedItems;
}

// ParallaxGenUI class

void ParallaxGenUI::init() {
  wxApp::SetInstance(new wxApp());
  if (!wxEntryStart(nullptr, nullptr)) {
    throw runtime_error("Failed to initialize wxWidgets");
  }
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
    for (const auto &[Mod, Value] : Conflicts) {
      // check if existing mods contain the mod
      if (std::find(ExistingMods.begin(), ExistingMods.end(), Mod) == ExistingMods.end()) {
        // add to final mod order
        FinalModOrder.push_back(Mod);
      } else {
        // add to new mods
        NewMods.push_back(Mod);
      }
    }

    // add newmods to finalmodorder
    FinalModOrder.insert(FinalModOrder.begin(), NewMods.begin(), NewMods.end());
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
      ShaderStrs.insert(ShaderStrs.begin(), ParallaxGenUtil::strToWstr(NIFUtil::getStrFromShader(Shader)));
    }

    auto ShaderStr = boost::join(ShaderStrs, L",");
    ShaderCombinedStrs.push_back(ShaderStr);

    // check if mod is in existing order
    IsNew.push_back(false);

    // add to conflict tracker
    ConflictTracker.insert({Mod, get<1>(Conflicts.at(Mod))});
  }

  ModSortDialog Dialog(ModStrs, ShaderCombinedStrs, IsNew, ConflictTracker);
  if (Dialog.ShowModal() == wxID_OK) {
    return Dialog.getSortedItems();
  }

  return ModStrs; // Return the original order if cancelled or closed
}
