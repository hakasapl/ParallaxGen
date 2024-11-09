#include "ParallaxGenUI.hpp"

#include <boost/algorithm/string/join.hpp>
#include <list>
#include <wx/app.h>
#include <wx/arrstr.h>
#include <wx/gdicmn.h>
#include <wx/msw/statbox.h>
#include <wx/msw/stattext.h>
#include <wx/sizer.h>
#include <wx/toplevel.h>

#include "BethesdaGame.hpp"
#include "ParallaxGenConfig.hpp"
#include "ParallaxGenUtil.hpp"

using namespace std;

// class LauncherWindow
LauncherWindow::LauncherWindow(const ParallaxGenConfig::PGParams &Params)
    : wxDialog(nullptr, wxID_ANY, "ParallaxGen Options", wxDefaultPosition, wxSize(600, 800),
               wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP),
      InitParams(Params) {
  // Main sizer
  auto *MainSizer = new wxBoxSizer(wxHORIZONTAL); // NOLINT(cppcoreguidelines-owning-memory)

  // Left/Right sizers
  auto *LeftSizer = new wxBoxSizer(wxVERTICAL);  // NOLINT(cppcoreguidelines-owning-memory)
  LeftSizer->SetMinSize(wxSize(800, -1));
  auto *RightSizer = new wxBoxSizer(wxVERTICAL); // NOLINT(cppcoreguidelines-owning-memory)

  //
  // Left Panel
  //

  //
  // Game
  //
  auto *GameSizer = new wxStaticBoxSizer(wxVERTICAL, this, "Game");

  // Game Location
  auto *GameLocationLabel = new wxStaticText(this, wxID_ANY, "Location"); // NOLINT(cppcoreguidelines-owning-memory)
  GameLocationTextbox = new wxTextCtrl(this, wxID_ANY); // NOLINT(cppcoreguidelines-owning-memory)
  auto *GameLocationBrowseButton = new wxButton(this, wxID_ANY, "Browse");
  GameLocationBrowseButton->Bind(wxEVT_BUTTON, &LauncherWindow::onBrowseGameLocation, this);

  auto *GameLocationSizer = new wxBoxSizer(wxHORIZONTAL); // NOLINT(cppcoreguidelines-owning-memory)
  GameLocationSizer->Add(GameLocationTextbox, 1, wxEXPAND | wxALL, 5);
  GameLocationSizer->Add(GameLocationBrowseButton, 0, wxALL, 5);

  GameSizer->Add(GameLocationLabel, 0, wxLEFT | wxRIGHT | wxTOP, 5);
  GameSizer->Add(GameLocationSizer, 0, wxEXPAND);

  // Game Type
  auto *GameTypeLabel = new wxStaticText(this, wxID_ANY, "Type"); // NOLINT(cppcoreguidelines-owning-memory)
  GameSizer->Add(GameTypeLabel, 0, wxLEFT | wxRIGHT | wxTOP, 5);

  bool IsFirst = true;
  for (const auto &GameType : BethesdaGame::getGameTypes()) {
    auto *Radio = new wxRadioButton(this, wxID_ANY, BethesdaGame::getStrFromGameType(GameType), wxDefaultPosition,
                                    wxDefaultSize, IsFirst ? wxRB_GROUP : 0);
    IsFirst = false;
    GameTypeRadios[GameType] = Radio;
    GameSizer->Add(Radio, 0, wxALL, 5);
  }

  LeftSizer->Add(GameSizer, 0, wxEXPAND | wxALL, 5);

  //
  // Mod Manager
  //
  auto *ModManagerSizer =
      new wxStaticBoxSizer(wxVERTICAL, this, "Mod Manager"); // NOLINT(cppcoreguidelines-owning-memory)

  IsFirst = true;
  for (const auto &MMType : ModManagerDirectory::getModManagerTypes()) {
    auto *Radio = new wxRadioButton(this, wxID_ANY, ModManagerDirectory::getStrFromModManagerType(MMType),
                                    wxDefaultPosition, wxDefaultSize, IsFirst ? wxRB_GROUP : 0);
    IsFirst = false;
    ModManagerRadios[MMType] = Radio;
    ModManagerSizer->Add(Radio, 0, wxALL, 5);
    Radio->Bind(wxEVT_RADIOBUTTON, &LauncherWindow::updateModManagerOptions, this);
  }

  LeftSizer->Add(ModManagerSizer, 0, wxEXPAND | wxALL, 5);

  // MO2-specific controls (initially hidden)
  MO2OptionsSizer = new wxStaticBoxSizer(wxVERTICAL, this, "MO2 Options"); // NOLINT(cppcoreguidelines-owning-memory)

  auto *MO2InstanceLocationSizer = new wxBoxSizer(wxHORIZONTAL); // NOLINT(cppcoreguidelines-owning-memory)
  auto *MO2InstanceLocationLabel =
      new wxStaticText(this, wxID_ANY, "Instance Location"); // NOLINT(cppcoreguidelines-owning-memory)

  MO2InstanceLocationTextbox = new wxTextCtrl(this, wxID_ANY); // NOLINT(cppcoreguidelines-owning-memory)
  MO2InstanceLocationTextbox->Bind(wxEVT_TEXT, &LauncherWindow::onMO2InstanceLocationChange, this);

  auto *MO2InstanceBrowseButton = new wxButton(this, wxID_ANY, "Browse");
  MO2InstanceBrowseButton->Bind(wxEVT_BUTTON, &LauncherWindow::onBrowseMO2InstanceLocation, this);

  MO2InstanceLocationSizer->Add(MO2InstanceLocationTextbox, 1, wxEXPAND | wxALL, 5);
  MO2InstanceLocationSizer->Add(MO2InstanceBrowseButton, 0, wxALL, 5);

  // Dropdown for MO2 profile selection
  auto *MO2ProfileLabel = new wxStaticText(this, wxID_ANY, "Profile"); // NOLINT(cppcoreguidelines-owning-memory)
  MO2ProfileChoice = new wxChoice(this, wxID_ANY);                     // NOLINT(cppcoreguidelines-owning-memory)

  // Add the label and dropdown to MO2 options sizer
  MO2OptionsSizer->Add(MO2InstanceLocationLabel, 0, wxLEFT | wxRIGHT | wxTOP, 5);
  MO2OptionsSizer->Add(MO2InstanceLocationSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 0);
  MO2OptionsSizer->Add(MO2ProfileLabel, 0, wxLEFT | wxRIGHT | wxTOP, 5);
  MO2OptionsSizer->Add(MO2ProfileChoice, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

  // Add MO2 options to leftSizer but hide it initially
  ModManagerSizer->Add(MO2OptionsSizer, 0, wxEXPAND | wxALL, 5);
  if (Params.ModManager.Type != ModManagerDirectory::ModManagerType::ModOrganizer2) {
    // Hide initially if MO2 is not selected
    MO2OptionsSizer->Show(false);
  }

  //
  // Output
  //
  auto *OutputSizer = new wxStaticBoxSizer(wxVERTICAL, this, "Output");

  auto *OutputLocationLabel = new wxStaticText(this, wxID_ANY, "Location"); // NOLINT(cppcoreguidelines-owning-memory)
  OutputLocationTextbox = new wxTextCtrl(this, wxID_ANY); // NOLINT(cppcoreguidelines-owning-memory)
  auto *OutputLocationBrowseButton = new wxButton(this, wxID_ANY, "Browse");
  OutputLocationBrowseButton->Bind(wxEVT_BUTTON, &LauncherWindow::onBrowseOutputLocation, this);

  auto *OutputLocationSizer = new wxBoxSizer(wxHORIZONTAL); // NOLINT(cppcoreguidelines-owning-memory)
  OutputLocationSizer->Add(OutputLocationTextbox, 1, wxEXPAND | wxALL, 5);
  OutputLocationSizer->Add(OutputLocationBrowseButton, 0, wxALL, 5);

  OutputSizer->Add(OutputLocationLabel, 0, wxLEFT | wxRIGHT | wxTOP, 5);
  OutputSizer->Add(OutputLocationSizer, 0, wxEXPAND);

  OutputZipCheckbox = new wxCheckBox(this, wxID_ANY, "Zip Output"); // NOLINT(cppcoreguidelines-owning-memory)
  OutputSizer->Add(OutputZipCheckbox, 0, wxALL, 5);
  LeftSizer->Add(OutputSizer, 0, wxEXPAND | wxALL, 5);

  //
  // Advanced
  //
  AdvancedButton = new wxButton(this, wxID_ANY, "Show Advanced"); // NOLINT(cppcoreguidelines-owning-memory)
  LeftSizer->Add(AdvancedButton, 0, wxALIGN_LEFT | wxALL, 5);
  AdvancedButton->Bind(wxEVT_BUTTON, &LauncherWindow::onToggleAdvanced, this);

  // Processing Options (hidden by default)
  AdvancedOptionsSizer = new wxBoxSizer(wxVERTICAL); // NOLINT(cppcoreguidelines-owning-memory)

  //
  // Processing
  //
  auto *ProcessingOptionsSizer = new wxStaticBoxSizer(wxVERTICAL, this, "Processing Options");

  ProcessingPluginPatchingCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "Plugin Patching");
  ProcessingOptionsSizer->Add(ProcessingPluginPatchingCheckbox, 0, wxALL, 5);

  ProcessingMultithreadingCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "Multithreading");
  ProcessingOptionsSizer->Add(ProcessingMultithreadingCheckbox, 0, wxALL, 5);

  ProcessingHighMemCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "High Memory Usage");
  ProcessingOptionsSizer->Add(ProcessingHighMemCheckbox, 0, wxALL, 5);

  ProcessingGPUAccelerationCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "GPU Acceleration");
  ProcessingOptionsSizer->Add(ProcessingGPUAccelerationCheckbox, 0, wxALL, 5);

  ProcessingMapFromMeshesCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "Map Textures From Meshes");
  ProcessingOptionsSizer->Add(ProcessingMapFromMeshesCheckbox, 0, wxALL, 5);

  ProcessingBSACheckbox = new wxCheckBox(this, wxID_ANY, "Read BSAs"); // NOLINT(cppcoreguidelines-owning-memory)
  ProcessingOptionsSizer->Add(ProcessingBSACheckbox, 0, wxALL, 5);

  AdvancedOptionsSizer->Add(ProcessingOptionsSizer, 0, wxEXPAND | wxALL, 5);

  LeftSizer->Add(AdvancedOptionsSizer, 0, wxEXPAND | wxALL, 0);
  AdvancedOptionsSizer->Show(false); // Initially hidden

  //
  // Right Panel
  //

  //
  // Pre-Patchers
  //
  auto *PrePatcherSizer = new wxStaticBoxSizer(wxVERTICAL, this, "Pre-Patchers");

  PrePatcherDisableMLPCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "Disable Multi-Layer Parallax");
  PrePatcherSizer->Add(PrePatcherDisableMLPCheckbox, 0, wxALL, 5);

  RightSizer->Add(PrePatcherSizer, 0, wxEXPAND | wxALL, 5);

  //
  // Shader Patchers
  //
  auto *ShaderPatcherSizer = new wxStaticBoxSizer(wxVERTICAL, this, "Shader Patchers");

  ShaderPatcherParallaxCheckbox = new wxCheckBox(this, wxID_ANY, "Parallax"); // NOLINT(cppcoreguidelines-owning-memory)
  ShaderPatcherSizer->Add(ShaderPatcherParallaxCheckbox, 0, wxALL, 5);

  ShaderPatcherComplexMaterialCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "Complex Material");
  ShaderPatcherSizer->Add(ShaderPatcherComplexMaterialCheckbox, 0, wxALL, 5);

  ShaderPatcherTruePBRCheckbox = new wxCheckBox(this, wxID_ANY, "True PBR"); // NOLINT(cppcoreguidelines-owning-memory)
  ShaderPatcherSizer->Add(ShaderPatcherTruePBRCheckbox, 0, wxALL, 5);

  RightSizer->Add(ShaderPatcherSizer, 0, wxEXPAND | wxALL, 5);

  //
  // Shader Transforms
  //
  auto *ShaderTransformSizer = new wxStaticBoxSizer(wxVERTICAL, this, "Shader Transforms");

  ShaderTransformParallaxToCMCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "Upgrade Parallax to Complex Material");
  ShaderTransformSizer->Add(ShaderTransformParallaxToCMCheckbox, 0, wxALL, 5);

  RightSizer->Add(ShaderTransformSizer, 0, wxEXPAND | wxALL, 5);

  //
  // Post-Patchers
  //
  auto *PostPatcherSizer = new wxStaticBoxSizer(wxVERTICAL, this, "Post-Patchers");

  PostPatcherOptimizeMeshesCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "Optimize Meshes (Experimental)");
  PostPatcherSizer->Add(PostPatcherOptimizeMeshesCheckbox, 0, wxALL, 5);

  RightSizer->Add(PostPatcherSizer, 0, wxEXPAND | wxALL, 5);

  // Start Patching button on the right side
  OKButton = new wxButton(this, wxID_OK, "Start Patching"); // NOLINT(cppcoreguidelines-owning-memory)

  // Set font size for the "start patching" button
  wxFont OKButtonFont = OKButton->GetFont();
  OKButtonFont.SetPointSize(12); // Set font size to 12 (or any size you prefer)
  OKButton->SetFont(OKButtonFont);
  OKButton->Bind(wxEVT_BUTTON, &LauncherWindow::onOkButtonPressed, this);
  Bind(wxEVT_CLOSE_WINDOW, &LauncherWindow::onClose, this);

  RightSizer->Add(OKButton, 0, wxEXPAND | wxALL, 5);

  // Add left and right panels to the main sizer
  MainSizer->Add(LeftSizer, 1, wxEXPAND | wxALL, 5);
  MainSizer->Add(RightSizer, 0, wxEXPAND | wxALL, 5);

  SetSizerAndFit(MainSizer);
  SetSizeHints(this->GetSize());

  Bind(wxEVT_INIT_DIALOG, &LauncherWindow::onInitDialog, this);
}

void LauncherWindow::onInitDialog(wxInitDialogEvent &Event) {
  // This is where we populate existing params

  // Game
  GameLocationTextbox->SetValue(InitParams.Game.Dir.wstring());
  for (const auto &GameType : BethesdaGame::getGameTypes()) {
    if (GameType == InitParams.Game.Type) {
      GameTypeRadios[GameType]->SetValue(true);
    }
  }

  // Mod Manager
  for (const auto &MMType : ModManagerDirectory::getModManagerTypes()) {
    if (MMType == InitParams.ModManager.Type) {
      ModManagerRadios[MMType]->SetValue(true);
    }
  }

  // MO2-specific options
  MO2InstanceLocationTextbox->SetValue(InitParams.ModManager.MO2InstanceDir.wstring());

  // Manually trigger the onMO2InstanceLocationChange to populate the listbox
  wxCommandEvent ChangeEvent(wxEVT_TEXT, MO2InstanceLocationTextbox->GetId());
  onMO2InstanceLocationChange(ChangeEvent); // Call the handler directly

  // Select the profile if it exists in the dropdown
  if (MO2ProfileChoice->FindString(InitParams.ModManager.MO2Profile) != wxNOT_FOUND) {
    MO2ProfileChoice->SetStringSelection(InitParams.ModManager.MO2Profile);
  }

  // Output
  OutputLocationTextbox->SetValue(InitParams.Output.Dir.wstring());
  OutputZipCheckbox->SetValue(InitParams.Output.Zip);

  // Processing
  ProcessingPluginPatchingCheckbox->SetValue(InitParams.Processing.PluginPatching);
  ProcessingMultithreadingCheckbox->SetValue(InitParams.Processing.Multithread);
  ProcessingHighMemCheckbox->SetValue(InitParams.Processing.HighMem);
  ProcessingGPUAccelerationCheckbox->SetValue(InitParams.Processing.GPUAcceleration);
  ProcessingMapFromMeshesCheckbox->SetValue(InitParams.Processing.MapFromMeshes);
  ProcessingBSACheckbox->SetValue(InitParams.Processing.BSA);

  // Pre-Patchers
  PrePatcherDisableMLPCheckbox->SetValue(InitParams.PrePatcher.DisableMLP);

  // Shader Patchers
  ShaderPatcherParallaxCheckbox->SetValue(InitParams.ShaderPatcher.Parallax);
  ShaderPatcherComplexMaterialCheckbox->SetValue(InitParams.ShaderPatcher.ComplexMaterial);
  ShaderPatcherTruePBRCheckbox->SetValue(InitParams.ShaderPatcher.TruePBR);

  // Shader Transforms
  ShaderTransformParallaxToCMCheckbox->SetValue(InitParams.ShaderTransforms.ParallaxToCM);

  // Post-Patchers
  PostPatcherOptimizeMeshesCheckbox->SetValue(InitParams.PostPatcher.OptimizeMeshes);

  // Call the base class's event handler if needed
  Event.Skip();
}

auto LauncherWindow::getParams() -> ParallaxGenConfig::PGParams {
  ParallaxGenConfig::PGParams Params;

  // Game
  for (const auto &GameType : BethesdaGame::getGameTypes()) {
    if (GameTypeRadios[GameType]->GetValue()) {
      Params.Game.Type = GameType;
      break;
    }
  }
  Params.Game.Dir = GameLocationTextbox->GetValue().ToStdWstring();

  // Mod Manager
  for (const auto &MMType : ModManagerDirectory::getModManagerTypes()) {
    if (ModManagerRadios[MMType]->GetValue()) {
      Params.ModManager.Type = MMType;
      break;
    }
  }
  Params.ModManager.MO2InstanceDir = MO2InstanceLocationTextbox->GetValue().ToStdWstring();
  Params.ModManager.MO2Profile = MO2ProfileChoice->GetStringSelection().ToStdWstring();

  // Output
  Params.Output.Dir = OutputLocationTextbox->GetValue().ToStdWstring();
  Params.Output.Zip = OutputZipCheckbox->GetValue();

  // Processing
  Params.Processing.PluginPatching = ProcessingPluginPatchingCheckbox->GetValue();
  Params.Processing.Multithread = ProcessingMultithreadingCheckbox->GetValue();
  Params.Processing.HighMem = ProcessingHighMemCheckbox->GetValue();
  Params.Processing.GPUAcceleration = ProcessingGPUAccelerationCheckbox->GetValue();
  Params.Processing.MapFromMeshes = ProcessingMapFromMeshesCheckbox->GetValue();
  Params.Processing.BSA = ProcessingBSACheckbox->GetValue();

  // Pre-Patchers
  Params.PrePatcher.DisableMLP = PrePatcherDisableMLPCheckbox->GetValue();

  // Shader Patchers
  Params.ShaderPatcher.Parallax = ShaderPatcherParallaxCheckbox->GetValue();
  Params.ShaderPatcher.ComplexMaterial = ShaderPatcherComplexMaterialCheckbox->GetValue();
  Params.ShaderPatcher.TruePBR = ShaderPatcherTruePBRCheckbox->GetValue();

  // Shader Transforms
  Params.ShaderTransforms.ParallaxToCM = ShaderTransformParallaxToCMCheckbox->GetValue();

  // Post-Patchers
  Params.PostPatcher.OptimizeMeshes = PostPatcherOptimizeMeshesCheckbox->GetValue();

  return Params;
}

void LauncherWindow::onToggleAdvanced([[maybe_unused]] wxCommandEvent &Event) {
  AdvancedVisible = !AdvancedVisible;
  AdvancedOptionsSizer->Show(AdvancedVisible);
  AdvancedButton->SetLabel(AdvancedVisible ? "Hide Advanced" : "Show Advanced");
  Layout(); // Refresh layout to apply the visibility change
  Fit();
}

void LauncherWindow::updateModManagerOptions(wxCommandEvent &Event) {
  // Show MO2 options only if the MO2 radio button is selected
  bool IsMO2Selected = (Event.GetEventObject() == ModManagerRadios[ModManagerDirectory::ModManagerType::ModOrganizer2]);
  MO2OptionsSizer->Show(IsMO2Selected);
  Layout(); // Refresh layout to apply visibility changes
  Fit();
}

void LauncherWindow::onBrowseGameLocation([[maybe_unused]] wxCommandEvent &Event) {
  wxDirDialog Dialog(this, "Select Game Location", GameLocationTextbox->GetValue());
  if (Dialog.ShowModal() == wxID_OK) {
    GameLocationTextbox->SetValue(Dialog.GetPath());
  }
}

void LauncherWindow::onBrowseMO2InstanceLocation([[maybe_unused]] wxCommandEvent &Event) {
  wxDirDialog Dialog(this, "Select MO2 Instance Location", MO2InstanceLocationTextbox->GetValue());
  if (Dialog.ShowModal() == wxID_OK) {
    MO2InstanceLocationTextbox->SetValue(Dialog.GetPath());
  }

  // Trigger the change event to update the profiles
  wxCommandEvent ChangeEvent(wxEVT_TEXT, MO2InstanceLocationTextbox->GetId());
  onMO2InstanceLocationChange(ChangeEvent); // Call the handler directly
}

void LauncherWindow::onMO2InstanceLocationChange([[maybe_unused]] wxCommandEvent &Event) {
  // Clear existing items
  MO2ProfileChoice->Clear();

  // check if the "profiles" folder exists
  filesystem::path ProfilesDir = MO2InstanceLocationTextbox->GetValue().ToStdWstring() + L"/profiles";
  if (!filesystem::exists(ProfilesDir)) {
    // set instance directory text to red
    MO2InstanceLocationTextbox->SetForegroundColour(*wxRED);
    return;
  }

  // set instance directory text to black
  MO2InstanceLocationTextbox->SetForegroundColour(*wxBLACK);

  // Find all directories within "profiles"
  for (const auto &Entry : filesystem::directory_iterator(ProfilesDir)) {
    if (Entry.is_directory()) {
      MO2ProfileChoice->Append(Entry.path().filename().wstring());
    }
  }

  // Optionally, select the first item
  if (MO2ProfileChoice->GetCount() > 0) {
    MO2ProfileChoice->SetSelection(0);
  }
}

void LauncherWindow::onBrowseOutputLocation([[maybe_unused]] wxCommandEvent &Event) {
  wxDirDialog Dialog(this, "Select Output Location", OutputLocationTextbox->GetValue());
  if (Dialog.ShowModal() == wxID_OK) {
    OutputLocationTextbox->SetValue(Dialog.GetPath());
  }
}

void LauncherWindow::onOkButtonPressed([[maybe_unused]] wxCommandEvent &Event) {
  vector<string> Errors;
  const auto Params = getParams();

  // Validate the parameters
  if (!ParallaxGenConfig::validateParams(Params, Errors)) {
    wxMessageDialog ErrorDialog(this, boost::algorithm::join(Errors, "\n"), "Errors", wxOK | wxICON_ERROR);
    ErrorDialog.ShowModal();
    return;
  }

  // All validation passed, proceed with OK actions
  EndModal(wxID_OK);
}

void LauncherWindow::onClose( // NOLINT(readability-convert-member-functions-to-static)
    [[maybe_unused]] wxCloseEvent &Event) {
  ParallaxGenUI::UIExitTriggered = true;
  wxTheApp->Exit();
}

// class ModSortDialog
ModSortDialog::ModSortDialog(const std::vector<std::wstring> &Mods, const std::vector<std::wstring> &Shaders,
                             const std::vector<bool> &IsNew,
                             const std::unordered_map<std::wstring, std::unordered_set<std::wstring>> &Conflicts)
    : wxDialog(nullptr, wxID_ANY, "Set Mod Priority", wxDefaultPosition, wxSize(300, 400),
               wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP | wxRESIZE_BORDER),
      ScrollTimer(this), ConflictsMap(Conflicts), SortAscending(true) {
  Bind(wxEVT_TIMER, &ModSortDialog::onTimer, this, ScrollTimer.GetId());

  auto *MainSizer = new wxBoxSizer(wxVERTICAL); // NOLINT(cppcoreguidelines-owning-memory)
  // Create the ListCtrl
  ListCtrl = // NOLINT(cppcoreguidelines-owning-memory)
      new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(250, 300), wxLC_REPORT);
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
  MainSizer->Add(MessageText, 0, wxALL, 10);

  // Adjust dialog width to match the total width of columns and padding
  SetSizeHints(TotalWidth, 400, TotalWidth, wxDefaultCoord); // Adjust minimum width and height
  SetSize(TotalWidth, 600);                                  // Set dialog size

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
    // Start the timer to handle scrolling
    if (!ScrollTimer.IsRunning()) {
      ScrollTimer.Start(250); // Start the timer with a 50ms interval
    }

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

void ModSortDialog::onTimer([[maybe_unused]] wxTimerEvent &Event) {
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
      ShaderStrs.insert(ShaderStrs.begin(), ParallaxGenUtil::strToWstr(NIFUtil::getStrFromShader(Shader)));
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
