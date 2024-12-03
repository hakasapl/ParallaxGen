#include "GUI/LauncherWindow.hpp"

#include <boost/algorithm/string/join.hpp>

#include "ModManagerDirectory.hpp"

using namespace std;

// class LauncherWindow
LauncherWindow::LauncherWindow(const ParallaxGenConfig::PGParams &Params)
    : wxDialog(nullptr, wxID_ANY, "ParallaxGen Options", wxDefaultPosition, wxSize(600, 800),
               wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP),
      InitParams(Params) {
  // Main sizer
  auto *MainSizer = new wxBoxSizer(wxHORIZONTAL); // NOLINT(cppcoreguidelines-owning-memory)

  // Left/Right sizers
  auto *LeftSizer = new wxBoxSizer(wxVERTICAL); // NOLINT(cppcoreguidelines-owning-memory)
  LeftSizer->SetMinSize(wxSize(600, -1));
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
  GameLocationTextbox = new wxTextCtrl(this, wxID_ANY);                   // NOLINT(cppcoreguidelines-owning-memory)
  GameLocationTextbox->SetToolTip("Path to the game folder (NOT the data folder)");
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
    Radio->Bind(wxEVT_RADIOBUTTON, &LauncherWindow::onGameTypeChanged, this);
    IsFirst = false;
    GameTypeRadios[GameType] = Radio;
    GameSizer->Add(Radio, 0, wxALL, 5);
  }

  LeftSizer->Add(GameSizer, 0, wxEXPAND | wxALL, 5);

  //
  // Mod Manager
  //
  auto *ModManagerSizer =
      new wxStaticBoxSizer(wxVERTICAL, this, "Conflict Resolution Mod Manager"); // NOLINT(cppcoreguidelines-owning-memory)

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
  MO2InstanceLocationTextbox->SetToolTip("Path to the MO2 instance folder (Folder Icon > Open Instnace folder in MO2)");
  MO2InstanceLocationTextbox->Bind(wxEVT_TEXT, &LauncherWindow::onMO2InstanceLocationChange, this);

  auto *MO2InstanceBrowseButton = new wxButton(this, wxID_ANY, "Browse");
  MO2InstanceBrowseButton->Bind(wxEVT_BUTTON, &LauncherWindow::onBrowseMO2InstanceLocation, this);

  MO2InstanceLocationSizer->Add(MO2InstanceLocationTextbox, 1, wxEXPAND | wxALL, 5);
  MO2InstanceLocationSizer->Add(MO2InstanceBrowseButton, 0, wxALL, 5);

  // Dropdown for MO2 profile selection
  auto *MO2ProfileLabel = new wxStaticText(this, wxID_ANY, "Profile"); // NOLINT(cppcoreguidelines-owning-memory)
  MO2ProfileChoice = new wxChoice(this, wxID_ANY);                     // NOLINT(cppcoreguidelines-owning-memory)
  MO2ProfileChoice->SetToolTip("MO2 profile to read from");

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
  OutputLocationTextbox = new wxTextCtrl(this, wxID_ANY);                   // NOLINT(cppcoreguidelines-owning-memory)
  OutputLocationTextbox->SetToolTip(
      "Path to the output folder - This folder should be used EXCLUSIVELY for ParallaxGen. Don't set it to your data "
      "directory or any other folder that contains mods.");
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
  ProcessingPluginPatchingCheckbox->SetToolTip(
      "Creates a 'ParallaxGen.esp' plugin in the output that patches TXST records according to how NIFs were patched");
  ProcessingOptionsSizer->Add(ProcessingPluginPatchingCheckbox, 0, wxALL, 5);

  ProcessingMultithreadingCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "Multithreading");
  ProcessingMultithreadingCheckbox->SetToolTip("Speeds up runtime at the cost of using more resources");
  ProcessingOptionsSizer->Add(ProcessingMultithreadingCheckbox, 0, wxALL, 5);

  ProcessingHighMemCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "High Memory Usage");
  ProcessingHighMemCheckbox->SetToolTip(
      "Uses more memory to speed up processing. You need to have enough RAM to be able to load ALL your NIFs at once.");
  ProcessingOptionsSizer->Add(ProcessingHighMemCheckbox, 0, wxALL, 5);

  ProcessingGPUAccelerationCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "GPU Acceleration");
  ProcessingGPUAccelerationCheckbox->SetToolTip("Uses the GPU to speed up processing some DDS related tasks");
  ProcessingOptionsSizer->Add(ProcessingGPUAccelerationCheckbox, 0, wxALL, 5);

  ProcessingMapFromMeshesCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "Map Textures From Meshes");
  ProcessingMapFromMeshesCheckbox->SetToolTip("Attempts to map textures from meshes instead of relying entirely on the "
                                              "DDS suffixes (slower, but more accurate)");
  ProcessingOptionsSizer->Add(ProcessingMapFromMeshesCheckbox, 0, wxALL, 5);

  ProcessingBSACheckbox = new wxCheckBox(this, wxID_ANY, "Read BSAs"); // NOLINT(cppcoreguidelines-owning-memory)
  ProcessingBSACheckbox->SetToolTip("Read meshes/textures from BSAs in addition to loose files");
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
  PrePatcherDisableMLPCheckbox->SetToolTip("Disables Multi-Layer Parallax in all meshes (Usually not recommended)");
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
  ShaderTransformParallaxToCMCheckbox->SetToolTip(
      "Upgrages any parallax textures and meshes to complex material by moving the height map to the alpha channel of "
      "the environment mask (highly recommended)");
  ShaderTransformParallaxToCMCheckbox->Bind(wxEVT_CHECKBOX, &LauncherWindow::onUpdateDeps, this);
  ShaderTransformSizer->Add(ShaderTransformParallaxToCMCheckbox, 0, wxALL, 5);

  RightSizer->Add(ShaderTransformSizer, 0, wxEXPAND | wxALL, 5);

  //
  // Post-Patchers
  //
  auto *PostPatcherSizer = new wxStaticBoxSizer(wxVERTICAL, this, "Post-Patchers");

  PostPatcherOptimizeMeshesCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "Optimize Meshes (Experimental)");
  PostPatcherOptimizeMeshesCheckbox->SetToolTip("Experimental - sometimes results in invisible meshes");
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

  // Trigger the updateDeps event to update the dependencies
  updateDisabledElements();

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

  // Get profiles
  const auto Profiles = ModManagerDirectory::getMO2ProfilesFromInstanceDir(MO2InstanceLocationTextbox->GetValue().ToStdWstring());

  // check if the "profiles" folder exists
  if (Profiles.empty()) {
    // set instance directory text to red
    MO2InstanceLocationTextbox->SetForegroundColour(*wxRED);
    return;
  }

  // set instance directory text to black
  MO2InstanceLocationTextbox->SetForegroundColour(*wxBLACK);

  // Find all directories within "profiles"
  for (const auto &Profile : Profiles) {
    MO2ProfileChoice->Append(Profile);
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

void LauncherWindow::onGameTypeChanged([[maybe_unused]] wxCommandEvent &Event) {
  // update the game location textbox from bethesdagame
  for (const auto &GameType : BethesdaGame::getGameTypes()) {
    if (GameTypeRadios[GameType]->GetValue()) {
      if (InitParams.Game.Type == GameType) {
        GameLocationTextbox->SetValue(InitParams.Game.Dir.wstring());
        return;
      }

      GameLocationTextbox->SetValue(BethesdaGame::findGamePathFromSteam(GameType).wstring());
      return;
    }
  }
}

void LauncherWindow::onUpdateDeps([[maybe_unused]] wxCommandEvent &Event) {
  updateDisabledElements();
}

void LauncherWindow::updateDisabledElements() {
  const auto CurParams = getParams();

  if (CurParams.ShaderTransforms.ParallaxToCM) {
    // disable and check vanilla parallax patcher
    ShaderPatcherParallaxCheckbox->SetValue(true);
    ShaderPatcherParallaxCheckbox->Enable(false);

    // disable and check CM patcher
    ShaderPatcherComplexMaterialCheckbox->SetValue(true);
    ShaderPatcherComplexMaterialCheckbox->Enable(false);

    // disable and check GPU acceleration
    ProcessingGPUAccelerationCheckbox->SetValue(true);
    ProcessingGPUAccelerationCheckbox->Enable(false);
  } else {
    ShaderPatcherParallaxCheckbox->Enable(true);
    ShaderPatcherComplexMaterialCheckbox->Enable(true);
    ProcessingGPUAccelerationCheckbox->Enable(true);
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
  UIExitTriggered = true;
  wxTheApp->Exit();
}
