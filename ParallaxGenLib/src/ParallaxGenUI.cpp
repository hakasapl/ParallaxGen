#include "ParallaxGenUI.hpp"

#include <boost/algorithm/string/join.hpp>

#include "ParallaxGenUtil.hpp"

using namespace std;

// class ModSortDialog
ModSortDialog::ModSortDialog(const std::vector<std::wstring> &Mods, const std::vector<std::wstring> &Shaders,
                             const std::vector<bool> &IsNew)
    : wxDialog(nullptr, wxID_ANY, "Set Mod Priority", wxDefaultPosition, wxSize(300, 400),
               wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP) {
  auto *MainSizer = new wxBoxSizer(wxVERTICAL);
  // Create the ListCtrl
  ListCtrl = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(250, 300), wxLC_REPORT | wxLC_SINGLE_SEL);
  ListCtrl->InsertColumn(0, "Mod");
  ListCtrl->InsertColumn(1, "Shader");

  // Add items to ListCtrl and highlight specific ones
  for (size_t I = 0; I < Mods.size(); ++I) {
    long Index = ListCtrl->InsertItem(static_cast<long>(I), Mods[I]);
    ListCtrl->SetItem(Index, 1, Shaders[I]);
    if (IsNew[I]) {
      ListCtrl->SetItemBackgroundColour(Index, *wxGREEN); // Highlight color
    }
  }

  // Calculate minimum width for each column
  int Col1Width = calculateColumnWidth(0);
  int Col2Width = calculateColumnWidth(1);
  int ScrollBarWidth = wxSystemSettings::GetMetric(wxSYS_VSCROLL_X);
  int TotalWidth = Col1Width + Col2Width + 40 + ScrollBarWidth; // Extra padding

  // Add wrapped message at the top
  static const std::wstring Message = L"The following mods have been detected as potential conflicts. Please set the "
                                      L"priority order for these mods. Mods that are highlighted in green are new mods "
                                      L"that do not exist in the saved order and may need to be sorted.";
  auto *MessageText = new wxStaticText(this, wxID_ANY, Message, wxDefaultPosition, wxSize(TotalWidth - 20, -1));
  MessageText->Wrap(TotalWidth - 20);
  MainSizer->Add(MessageText, 0, wxEXPAND | wxALL, 10);

  // Set column widths
  ListCtrl->SetColumnWidth(0, Col1Width);
  ListCtrl->SetColumnWidth(1, Col2Width);

  // Adjust dialog width to match the total width of columns and padding
  SetSizeHints(TotalWidth, 600); // Adjust minimum width and height
  SetSize(TotalWidth, 600);      // Set dialog size

  MainSizer->Add(ListCtrl, 1, wxEXPAND | wxALL, 10);

  // Add Up/Down buttons for reordering
  auto *ButtonSizer = new wxBoxSizer(wxHORIZONTAL);
  auto *UpButton = new wxButton(this, wxID_ANY, "Move Up (Lower Priority)");
  auto *DownButton = new wxButton(this, wxID_ANY, "Move Down (Higher Priority)");
  ButtonSizer->Add(UpButton, 0, wxALL, 5);
  ButtonSizer->Add(DownButton, 0, wxALL, 5);
  MainSizer->Add(ButtonSizer, 0, wxALIGN_CENTER_HORIZONTAL);

  // Add OK button
  auto *OkButton = new wxButton(this, wxID_OK, "OK");
  MainSizer->Add(OkButton, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 10);

  SetSizer(MainSizer);

  // Event bindings
  UpButton->Bind(wxEVT_BUTTON, &ModSortDialog::onMoveUp, this);
  DownButton->Bind(wxEVT_BUTTON, &ModSortDialog::onMoveDown, this);
}

auto ModSortDialog::getSortedItems() const -> std::vector<std::wstring> {
  std::vector<std::wstring> SortedItems;
  for (long I = 0; I < ListCtrl->GetItemCount(); ++I) {
    SortedItems.push_back(ListCtrl->GetItemText(I).ToStdWstring());
  }
  return SortedItems;
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

void ModSortDialog::onMoveUp(wxCommandEvent &Event) { // NOLINT
  int Selection = ListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  if (Selection > 0) {
    swapItems(Selection, Selection - 1);
    ListCtrl->SetItemState(Selection - 1, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);

    // Ensure the moved item is visible
    ListCtrl->EnsureVisible(Selection - 1);
  }
}

void ModSortDialog::onMoveDown(wxCommandEvent &Event) { // NOLINT
  int Selection = ListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  if (Selection != wxNOT_FOUND && Selection < ListCtrl->GetItemCount() - 1) {
    swapItems(Selection, Selection + 1);
    ListCtrl->SetItemState(Selection + 1, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);

    // Ensure the moved item is visible
    ListCtrl->EnsureVisible(Selection + 1);
  }
}

void ModSortDialog::swapItems(long FirstIndex, long SecondIndex) {
  // Swap text for both columns
  wxString FirstTextCol1 = ListCtrl->GetItemText(FirstIndex, 0);
  wxString FirstTextCol2 = ListCtrl->GetItemText(FirstIndex, 1);
  wxString SecondTextCol1 = ListCtrl->GetItemText(SecondIndex, 0);
  wxString SecondTextCol2 = ListCtrl->GetItemText(SecondIndex, 1);

  ListCtrl->SetItem(FirstIndex, 0, SecondTextCol1);
  ListCtrl->SetItem(FirstIndex, 1, SecondTextCol2);
  ListCtrl->SetItem(SecondIndex, 0, FirstTextCol1);
  ListCtrl->SetItem(SecondIndex, 1, FirstTextCol2);

  // Swap background colors to maintain highlights
  wxColour FirstColor = ListCtrl->GetItemBackgroundColour(FirstIndex);
  wxColour SecondColor = ListCtrl->GetItemBackgroundColour(SecondIndex);

  ListCtrl->SetItemBackgroundColour(FirstIndex, SecondColor);
  ListCtrl->SetItemBackgroundColour(SecondIndex, FirstColor);

  // Clear background color of the original position if needed
  if (FirstColor != SecondColor) {
    ListCtrl->SetItemBackgroundColour(FirstIndex, *wxWHITE);
  }
}

// ParallaxGenUI class

void ParallaxGenUI::init() {
  wxApp::SetInstance(new wxApp());
  if (!wxEntryStart(nullptr, nullptr)) {
    throw runtime_error("Failed to initialize wxWidgets");
  }
}

auto ParallaxGenUI::selectModOrder(const std::unordered_map<std::wstring, std::set<NIFUtil::ShapeShader>> &Conflicts,
                                   const std::vector<std::wstring> &ExistingMods) -> std::vector<std::wstring> {
  // split into vectors
  vector<wstring> ModStrs;
  vector<wstring> ShaderCombinedStrs;
  vector<bool> IsNew;

  // first loop through existing order to restore is
  auto ConflictsCopy = Conflicts;
  for (const auto &Mod : ExistingMods) {
    if (ConflictsCopy.contains(Mod)) {
      ModStrs.push_back(Mod);

      vector<wstring> ShaderStrs;
      for (const auto &Shader : Conflicts.at(Mod)) {
        ShaderStrs.push_back(ParallaxGenUtil::strToWstr(NIFUtil::getStrFromShader(Shader)));
      }

      auto ShaderStr = boost::join(ShaderStrs, L",");
      ShaderCombinedStrs.push_back(ShaderStr);

      // check if mod is in existing order
      IsNew.push_back(false);

      // delete from conflicts
      ConflictsCopy.erase(Mod);
    }
  }

  // now loop through conflicts to add new mods
  for (const auto &[Mod, Shaders] : ConflictsCopy) {
    ModStrs.insert(ModStrs.begin(), Mod);

    vector<wstring> ShaderStrs;
    for (const auto &Shader : ConflictsCopy.at(Mod)) {
      ShaderStrs.push_back(ParallaxGenUtil::strToWstr(NIFUtil::getStrFromShader(Shader)));
    }

    auto ShaderStr = boost::join(ShaderStrs, L",");
    ShaderCombinedStrs.insert(ShaderCombinedStrs.begin(), ShaderStr);

    // check if mod is in existing order
    IsNew.insert(IsNew.begin(), true);
  }

  ModSortDialog Dialog(ModStrs, ShaderCombinedStrs, IsNew);
  if (Dialog.ShowModal() == wxID_OK) {
    return Dialog.getSortedItems();
  }

  return ModStrs; // Return the original order if cancelled or closed
}
