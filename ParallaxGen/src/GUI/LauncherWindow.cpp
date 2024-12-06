#include "GUI/LauncherWindow.hpp"

#include <boost/algorithm/string/join.hpp>
#include <wx/arrstr.h>
#include <wx/listbase.h>
#include <wx/statline.h>

#include "ModManagerDirectory.hpp"
#include "NIFUtil.hpp"

using namespace std;

// class LauncherWindow
LauncherWindow::LauncherWindow(ParallaxGenConfig &PGC)
    : wxDialog(nullptr, wxID_ANY, "ParallaxGen Options", wxDefaultPosition, wxSize(600, 800),
               wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP),
      PGC(PGC), TextureMapTypeCombo(nullptr) {
  // Calculate the scrollbar width (if visible)
  static const int ScrollbarWidth = wxSystemSettings::GetMetric(wxSYS_VSCROLL_X);

  // Main sizer
  auto *MainSizer = new wxBoxSizer(wxVERTICAL); // NOLINT(cppcoreguidelines-owning-memory)

  // Create a horizontal sizer for left and right columns
  auto *ColumnsSizer = new wxBoxSizer(wxHORIZONTAL); // NOLINT(cppcoreguidelines-owning-memory)

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
  GameLocationTextbox->Bind(wxEVT_TEXT, &LauncherWindow::onGameLocationChange, this);
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
    Radio->Bind(wxEVT_RADIOBUTTON, &LauncherWindow::onGameTypeChange, this);
    IsFirst = false;
    GameTypeRadios[GameType] = Radio;
    GameSizer->Add(Radio, 0, wxALL, 5);
  }

  LeftSizer->Add(GameSizer, 0, wxEXPAND | wxALL, 5);

  //
  // Mod Manager
  //
  auto *ModManagerSizer = new wxStaticBoxSizer(
      wxVERTICAL, this, "Conflict Resolution Mod Manager"); // NOLINT(cppcoreguidelines-owning-memory)

  IsFirst = true;
  for (const auto &MMType : ModManagerDirectory::getModManagerTypes()) {
    auto *Radio = new wxRadioButton(this, wxID_ANY, ModManagerDirectory::getStrFromModManagerType(MMType),
                                    wxDefaultPosition, wxDefaultSize, IsFirst ? wxRB_GROUP : 0);
    IsFirst = false;
    ModManagerRadios[MMType] = Radio;
    ModManagerSizer->Add(Radio, 0, wxALL, 5);
    Radio->Bind(wxEVT_RADIOBUTTON, &LauncherWindow::onModManagerChange, this);
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
  MO2ProfileChoice->Bind(wxEVT_CHOICE, &LauncherWindow::onMO2ProfileChange, this);

  // Checkbox to use MO2 order
  MO2UseOrderCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "Use MO2 Loose File Order (Recommended)");
  MO2UseOrderCheckbox->SetToolTip("Use the order set in MO2's left pane instead of manually defining an order");
  MO2UseOrderCheckbox->Bind(wxEVT_CHECKBOX, &LauncherWindow::onMO2UseOrderChange, this);

  // Add the label and dropdown to MO2 options sizer
  MO2OptionsSizer->Add(MO2InstanceLocationLabel, 0, wxLEFT | wxRIGHT | wxTOP, 5);
  MO2OptionsSizer->Add(MO2InstanceLocationSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 0);
  MO2OptionsSizer->Add(MO2ProfileLabel, 0, wxLEFT | wxRIGHT | wxTOP, 5);
  MO2OptionsSizer->Add(MO2ProfileChoice, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
  MO2OptionsSizer->Add(MO2UseOrderCheckbox, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);

  // Add MO2 options to leftSizer but hide it initially
  ModManagerSizer->Add(MO2OptionsSizer, 0, wxEXPAND | wxALL, 5);

  //
  // Output
  //
  auto *OutputSizer = new wxStaticBoxSizer(wxVERTICAL, this, "Output");

  auto *OutputLocationLabel = new wxStaticText(this, wxID_ANY, "Location"); // NOLINT(cppcoreguidelines-owning-memory)
  OutputLocationTextbox = new wxTextCtrl(this, wxID_ANY);                   // NOLINT(cppcoreguidelines-owning-memory)
  OutputLocationTextbox->SetToolTip(
      "Path to the output folder - This folder should be used EXCLUSIVELY for ParallaxGen. Don't set it to your data "
      "directory or any other folder that contains mods.");
  OutputLocationTextbox->Bind(wxEVT_TEXT, &LauncherWindow::onOutputLocationChange, this);

  auto *OutputLocationBrowseButton = new wxButton(this, wxID_ANY, "Browse");
  OutputLocationBrowseButton->Bind(wxEVT_BUTTON, &LauncherWindow::onBrowseOutputLocation, this);

  auto *OutputLocationSizer = new wxBoxSizer(wxHORIZONTAL); // NOLINT(cppcoreguidelines-owning-memory)
  OutputLocationSizer->Add(OutputLocationTextbox, 1, wxEXPAND | wxALL, 5);
  OutputLocationSizer->Add(OutputLocationBrowseButton, 0, wxALL, 5);

  OutputSizer->Add(OutputLocationLabel, 0, wxLEFT | wxRIGHT | wxTOP, 5);
  OutputSizer->Add(OutputLocationSizer, 0, wxEXPAND);

  OutputZipCheckbox = new wxCheckBox(this, wxID_ANY, "Zip Output"); // NOLINT(cppcoreguidelines-owning-memory)
  OutputZipCheckbox->SetToolTip("Zip the output folder after processing");

  OutputSizer->Add(OutputZipCheckbox, 0, wxALL, 5);
  LeftSizer->Add(OutputSizer, 0, wxEXPAND | wxALL, 5);

  //
  // Advanced
  //
  auto *AdvancedSizer = new wxStaticBoxSizer(wxVERTICAL, this, "Advanced");
  AdvancedOptionsCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "Show Advanced Options");
  AdvancedOptionsCheckbox->Bind(wxEVT_CHECKBOX, &LauncherWindow::onAdvancedOptionsChange, this);
  AdvancedSizer->Add(AdvancedOptionsCheckbox, 0, wxALL, 5);

  LeftSizer->Add(AdvancedSizer, 0, wxEXPAND | wxALL, 5);

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
  PrePatcherDisableMLPCheckbox->Bind(wxEVT_CHECKBOX, &LauncherWindow::onPrePatcherDisableMLPChange, this);
  PrePatcherSizer->Add(PrePatcherDisableMLPCheckbox, 0, wxALL, 5);

  RightSizer->Add(PrePatcherSizer, 0, wxEXPAND | wxALL, 5);

  //
  // Shader Patchers
  //
  auto *ShaderPatcherSizer = new wxStaticBoxSizer(wxVERTICAL, this, "Shader Patchers");

  ShaderPatcherParallaxCheckbox = new wxCheckBox(this, wxID_ANY, "Parallax"); // NOLINT(cppcoreguidelines-owning-memory)
  ShaderPatcherParallaxCheckbox->Bind(wxEVT_CHECKBOX, &LauncherWindow::onShaderPatcherParallaxChange, this);
  ShaderPatcherSizer->Add(ShaderPatcherParallaxCheckbox, 0, wxALL, 5);

  ShaderPatcherComplexMaterialCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "Complex Material");
  ShaderPatcherComplexMaterialCheckbox->Bind(wxEVT_CHECKBOX, &LauncherWindow::onShaderPatcherComplexMaterialChange,
                                             this);
  ShaderPatcherSizer->Add(ShaderPatcherComplexMaterialCheckbox, 0, wxALL, 5);

  ShaderPatcherComplexMaterialOptionsSizer = // NOLINT(cppcoreguidelines-owning-memory)
      new wxStaticBoxSizer(wxVERTICAL, this, "Complex Material Options");
  auto *ShaderPatcherComplexMaterialDynCubemapBlocklistLabel = new wxStaticText(this, wxID_ANY, "DynCubemap Blocklist");
  ShaderPatcherComplexMaterialOptionsSizer->Add(ShaderPatcherComplexMaterialDynCubemapBlocklistLabel, 0,
                                                wxLEFT | wxRIGHT | wxTOP, 5);

  ShaderPatcherComplexMaterialDynCubemapBlocklist = // NOLINT(cppcoreguidelines-owning-memory)
      new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_EDIT_LABELS | wxLC_NO_HEADER);
  ShaderPatcherComplexMaterialDynCubemapBlocklist->AppendColumn("DynCubemap Blocklist", wxLIST_FORMAT_LEFT);
  ShaderPatcherComplexMaterialDynCubemapBlocklist->SetColumnWidth(
      0, ShaderPatcherComplexMaterialDynCubemapBlocklist->GetClientSize().GetWidth() - ScrollbarWidth);
  ShaderPatcherComplexMaterialDynCubemapBlocklist->Bind(
      wxEVT_LIST_ITEM_ACTIVATED, &LauncherWindow::onShaderPatcherComplexMaterialDynCubemapBlocklistChange, this);
  ShaderPatcherComplexMaterialDynCubemapBlocklist->Bind(wxEVT_LIST_ITEM_ACTIVATED, &LauncherWindow::onListItemActivated,
                                                        this);
  ShaderPatcherComplexMaterialOptionsSizer->Add(ShaderPatcherComplexMaterialDynCubemapBlocklist, 0, wxEXPAND | wxALL,
                                                5);

  ShaderPatcherSizer->Add(ShaderPatcherComplexMaterialOptionsSizer, 0, wxEXPAND | wxALL, 5);

  ShaderPatcherTruePBRCheckbox = new wxCheckBox(this, wxID_ANY, "True PBR"); // NOLINT(cppcoreguidelines-owning-memory)
  ShaderPatcherTruePBRCheckbox->Bind(wxEVT_CHECKBOX, &LauncherWindow::onShaderPatcherTruePBRChange, this);
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
  ShaderTransformParallaxToCMCheckbox->Bind(wxEVT_CHECKBOX, &LauncherWindow::onShaderTransformParallaxToCMChange, this);
  ShaderTransformSizer->Add(ShaderTransformParallaxToCMCheckbox, 0, wxALL, 5);

  RightSizer->Add(ShaderTransformSizer, 0, wxEXPAND | wxALL, 5);

  //
  // Post-Patchers
  //
  auto *PostPatcherSizer = new wxStaticBoxSizer(wxVERTICAL, this, "Post-Patchers");

  PostPatcherOptimizeMeshesCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "Optimize Meshes (Experimental)");
  PostPatcherOptimizeMeshesCheckbox->SetToolTip("Experimental - sometimes results in invisible meshes");
  PostPatcherOptimizeMeshesCheckbox->Bind(wxEVT_CHECKBOX, &LauncherWindow::onPostPatcherOptimizeMeshesChange, this);
  PostPatcherSizer->Add(PostPatcherOptimizeMeshesCheckbox, 0, wxALL, 5);

  RightSizer->Add(PostPatcherSizer, 0, wxEXPAND | wxALL, 5);

  // Restore defaults button
  auto *RestoreDefaultsButton =
      new wxButton(this, wxID_ANY, "Restore Defaults"); // NOLINT(cppcoreguidelines-owning-memory)
  wxFont RestoreDefaultsButtonFont = RestoreDefaultsButton->GetFont();
  RestoreDefaultsButtonFont.SetPointSize(12); // Set font size to 12
  RestoreDefaultsButton->SetFont(RestoreDefaultsButtonFont);
  RestoreDefaultsButton->Bind(wxEVT_BUTTON, &LauncherWindow::onRestoreDefaultsButtonPressed, this);

  RightSizer->Add(RestoreDefaultsButton, 0, wxEXPAND | wxALL, 5);

  // Load config button
  LoadConfigButton = new wxButton(this, wxID_ANY, "Load Config"); // NOLINT(cppcoreguidelines-owning-memory)
  wxFont LoadConfigButtonFont = LoadConfigButton->GetFont();
  LoadConfigButtonFont.SetPointSize(12); // Set font size to 12
  LoadConfigButton->SetFont(LoadConfigButtonFont);
  LoadConfigButton->Bind(wxEVT_BUTTON, &LauncherWindow::onLoadConfigButtonPressed, this);

  RightSizer->Add(LoadConfigButton, 0, wxEXPAND | wxALL, 5);

  // Save config button
  SaveConfigButton = new wxButton(this, wxID_ANY, "Save Config"); // NOLINT(cppcoreguidelines-owning-memory)
  wxFont SaveConfigButtonFont = SaveConfigButton->GetFont();
  SaveConfigButtonFont.SetPointSize(12); // Set font size to 12
  SaveConfigButton->SetFont(SaveConfigButtonFont);
  SaveConfigButton->Bind(wxEVT_BUTTON, &LauncherWindow::onSaveConfigButtonPressed, this);

  RightSizer->Add(SaveConfigButton, 0, wxEXPAND | wxALL, 5);

  // Add a horizontal line
  auto *SeparatorLine = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL); // NOLINT
  RightSizer->Add(SeparatorLine, 0, wxEXPAND | wxALL, 5);

  // Start Patching button on the right side
  OKButton = new wxButton(this, wxID_OK, "Start Patching"); // NOLINT(cppcoreguidelines-owning-memory)

  // Set font size for the "start patching" button
  wxFont OKButtonFont = OKButton->GetFont();
  OKButtonFont.SetPointSize(12); // Set font size to 12
  OKButton->SetFont(OKButtonFont);
  OKButton->Bind(wxEVT_BUTTON, &LauncherWindow::onOkButtonPressed, this);
  Bind(wxEVT_CLOSE_WINDOW, &LauncherWindow::onClose, this);

  RightSizer->Add(OKButton, 0, wxEXPAND | wxALL, 5);

  //
  // Advanced
  //

  // Processing Options (hidden by default)
  AdvancedOptionsSizer = // NOLINT(cppcoreguidelines-owning-memory, cppcoreguidelines-prefer-member-initializer)
      new wxBoxSizer(wxVERTICAL);

  //
  // Processing
  //
  ProcessingOptionsSizer = // NOLINT(cppcoreguidelines-owning-memory)
      new wxStaticBoxSizer(wxVERTICAL, this, "Processing Options");

  ProcessingPluginPatchingCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "Plugin Patching");
  ProcessingPluginPatchingCheckbox->SetToolTip(
      "Creates a 'ParallaxGen.esp' plugin in the output that patches TXST records according to how NIFs were patched");
  ProcessingPluginPatchingCheckbox->Bind(wxEVT_CHECKBOX, &LauncherWindow::onProcessingPluginPatchingChange, this);
  ProcessingOptionsSizer->Add(ProcessingPluginPatchingCheckbox, 0, wxALL, 5);

  ProcessingPluginPatchingOptions = // NOLINT(cppcoreguidelines-owning-memory)
      new wxStaticBoxSizer(wxVERTICAL, this, "Plugin Patching Options");
  ProcessingPluginPatchingOptionsESMifyCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "ESMify Plugin");
  ProcessingPluginPatchingOptionsESMifyCheckbox->SetToolTip(
      "ESM flags the output plugin (don't check this if you don't know what you're doing)");
  ProcessingPluginPatchingOptionsESMifyCheckbox->Bind(
      wxEVT_CHECKBOX, &LauncherWindow::onProcessingPluginPatchingOptionsESMifyChange, this);
  ProcessingPluginPatchingOptions->Add(ProcessingPluginPatchingOptionsESMifyCheckbox, 0, wxALL, 5);

  ProcessingOptionsSizer->Add(ProcessingPluginPatchingOptions, 0, wxEXPAND | wxALL, 5);

  ProcessingMultithreadingCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "Multithreading");
  ProcessingMultithreadingCheckbox->SetToolTip("Speeds up runtime at the cost of using more resources");
  ProcessingMultithreadingCheckbox->Bind(wxEVT_CHECKBOX, &LauncherWindow::onProcessingMultithreadingChange, this);
  ProcessingOptionsSizer->Add(ProcessingMultithreadingCheckbox, 0, wxALL, 5);

  ProcessingHighMemCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "High Memory Usage");
  ProcessingHighMemCheckbox->SetToolTip(
      "Uses more memory to speed up processing. You need to have enough RAM to be able to load ALL your NIFs at once.");
  ProcessingHighMemCheckbox->Bind(wxEVT_CHECKBOX, &LauncherWindow::onProcessingHighMemChange, this);
  ProcessingOptionsSizer->Add(ProcessingHighMemCheckbox, 0, wxALL, 5);

  ProcessingGPUAccelerationCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "GPU Acceleration");
  ProcessingGPUAccelerationCheckbox->SetToolTip("Uses the GPU to speed up processing some DDS related tasks");
  ProcessingGPUAccelerationCheckbox->Bind(wxEVT_CHECKBOX, &LauncherWindow::onProcessingGPUAccelerationChange, this);
  ProcessingOptionsSizer->Add(ProcessingGPUAccelerationCheckbox, 0, wxALL, 5);

  ProcessingMapFromMeshesCheckbox = // NOLINT(cppcoreguidelines-owning-memory)
      new wxCheckBox(this, wxID_ANY, "Map Textures From Meshes");
  ProcessingMapFromMeshesCheckbox->SetToolTip("Attempts to map textures from meshes instead of relying entirely on the "
                                              "DDS suffixes (slower, but more accurate)");
  ProcessingMapFromMeshesCheckbox->Bind(wxEVT_CHECKBOX, &LauncherWindow::onProcessingMapFromMeshesChange, this);
  ProcessingOptionsSizer->Add(ProcessingMapFromMeshesCheckbox, 0, wxALL, 5);

  ProcessingBSACheckbox = new wxCheckBox(this, wxID_ANY, "Read BSAs"); // NOLINT(cppcoreguidelines-owning-memory)
  ProcessingBSACheckbox->SetToolTip("Read meshes/textures from BSAs in addition to loose files");
  ProcessingBSACheckbox->Bind(wxEVT_CHECKBOX, &LauncherWindow::onProcessingBSAChange, this);
  ProcessingOptionsSizer->Add(ProcessingBSACheckbox, 0, wxALL, 5);

  LeftSizer->Add(ProcessingOptionsSizer, 1, wxEXPAND | wxALL, 5);

  //
  // Mesh Rules
  //
  auto *MeshRulesSizer =
      new wxStaticBoxSizer(wxVERTICAL, this, "Mesh Rules"); // NOLINT(cppcoreguidelines-owning-memory)

  auto *MeshRulesAllowListLabel = new wxStaticText(this, wxID_ANY, "Mesh Allow List");
  MeshRulesSizer->Add(MeshRulesAllowListLabel, 0, wxLEFT | wxRIGHT | wxTOP, 5);

  MeshRulesAllowList = // NOLINT(cppcoreguidelines-owning-memory)
      new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_EDIT_LABELS | wxLC_NO_HEADER);
  MeshRulesAllowList->AppendColumn("Mesh Allow List", wxLIST_FORMAT_LEFT);
  MeshRulesAllowList->SetColumnWidth(0, MeshRulesAllowList->GetClientSize().GetWidth() - ScrollbarWidth);
  MeshRulesAllowList->SetToolTip(
      "If anything is in this list, only meshes matching items in this list will be patched. Wildcards are supported.");
  MeshRulesAllowList->Bind(wxEVT_LIST_END_LABEL_EDIT, &LauncherWindow::onMeshRulesAllowListChange, this);
  MeshRulesAllowList->Bind(wxEVT_LIST_ITEM_ACTIVATED, &LauncherWindow::onListItemActivated, this);
  MeshRulesSizer->Add(MeshRulesAllowList, 0, wxEXPAND | wxALL, 5);

  auto *MeshRulesBlockListLabel = new wxStaticText(this, wxID_ANY, "Mesh Block List");
  MeshRulesSizer->Add(MeshRulesBlockListLabel, 0, wxLEFT | wxRIGHT | wxTOP, 5);

  MeshRulesBlockList = // NOLINT(cppcoreguidelines-owning-memory)
      new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_EDIT_LABELS | wxLC_NO_HEADER);
  MeshRulesBlockList->AppendColumn("Mesh Block List", wxLIST_FORMAT_LEFT);
  MeshRulesBlockList->SetColumnWidth(0, MeshRulesBlockList->GetClientSize().GetWidth() - ScrollbarWidth);
  MeshRulesBlockList->SetToolTip(
      "Any matches in this list will not be patched. This is checked after allowlist. Wildcards are supported.");
  MeshRulesBlockList->Bind(wxEVT_LIST_END_LABEL_EDIT, &LauncherWindow::onMeshRulesBlockListChange, this);
  MeshRulesBlockList->Bind(wxEVT_LIST_ITEM_ACTIVATED, &LauncherWindow::onListItemActivated, this);
  MeshRulesSizer->Add(MeshRulesBlockList, 0, wxEXPAND | wxALL, 5);

  AdvancedOptionsSizer->Add(MeshRulesSizer, 0, wxEXPAND | wxALL, 5);

  //
  // Texture Rules
  //
  auto *TextureRulesSizer =
      new wxStaticBoxSizer(wxVERTICAL, this, "Texture Rules"); // NOLINT(cppcoreguidelines-owning-memory)

  auto *TextureRulesMapsLabel = new wxStaticText(this, wxID_ANY, "Manual Texture Maps");
  TextureRulesSizer->Add(TextureRulesMapsLabel, 0, wxLEFT | wxRIGHT | wxTOP, 5);

  TextureRulesMaps = // NOLINT(cppcoreguidelines-owning-memory)
      new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_EDIT_LABELS | wxLC_NO_HEADER);
  TextureRulesMaps->AppendColumn("Texture Maps", wxLIST_FORMAT_LEFT);
  TextureRulesMaps->AppendColumn("Type", wxLIST_FORMAT_LEFT);
  TextureRulesMaps->SetColumnWidth(0, (TextureRulesMaps->GetClientSize().GetWidth() - ScrollbarWidth) / 2);
  TextureRulesMaps->SetColumnWidth(1, (TextureRulesMaps->GetClientSize().GetWidth() - ScrollbarWidth) / 2);
  TextureRulesMaps->SetToolTip(
      "Use this to manually specify what type of texture a texture is. This is useful for textures that don't follow "
      "the standard naming conventions. Set to unknown for it to be skipped.");
  TextureRulesMaps->Bind(wxEVT_LIST_END_LABEL_EDIT, &LauncherWindow::onTextureRulesMapsChange, this);
  TextureRulesMaps->Bind(wxEVT_LEFT_DCLICK, &LauncherWindow::onTextureRulesMapsChangeStart, this);
  TextureRulesSizer->Add(TextureRulesMaps, 0, wxEXPAND | wxALL, 5);

  auto *TextureRulesVanillaBSAListLabel = new wxStaticText(this, wxID_ANY, "Vanilla BSA List");
  TextureRulesSizer->Add(TextureRulesVanillaBSAListLabel, 0, wxLEFT | wxRIGHT | wxTOP, 5);

  TextureRulesVanillaBSAList = // NOLINT(cppcoreguidelines-owning-memory)
      new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_EDIT_LABELS | wxLC_NO_HEADER);
  TextureRulesVanillaBSAList->AppendColumn("Vanilla BSA List", wxLIST_FORMAT_LEFT);
  TextureRulesVanillaBSAList->SetColumnWidth(0,
                                             TextureRulesVanillaBSAList->GetClientSize().GetWidth() - ScrollbarWidth);
  TextureRulesVanillaBSAList->SetToolTip(
      "Define vanilla or vanilla-like BSAs. Files from these BSAs will only be considered vanilla types.");
  TextureRulesVanillaBSAList->Bind(wxEVT_LIST_END_LABEL_EDIT, &LauncherWindow::onTextureRulesVanillaBSAListChange,
                                   this);
  TextureRulesVanillaBSAList->Bind(wxEVT_LIST_ITEM_ACTIVATED, &LauncherWindow::onListItemActivated, this);
  TextureRulesSizer->Add(TextureRulesVanillaBSAList, 0, wxEXPAND | wxALL, 5);

  AdvancedOptionsSizer->Add(TextureRulesSizer, 0, wxEXPAND | wxALL, 5);

  //
  // Finalize
  //
  AdvancedOptionsSizer->Show(false);                     // Initially hidden
  ShaderPatcherComplexMaterialOptionsSizer->Show(false); // Initially hidden
  ProcessingOptionsSizer->Show(false);                   // Initially hidden
  ProcessingPluginPatchingOptions->Show(false);          // Initially hidden

  ColumnsSizer->Add(AdvancedOptionsSizer, 0, wxEXPAND | wxALL, 0);
  ColumnsSizer->Add(LeftSizer, 1, wxEXPAND | wxALL, 0);
  ColumnsSizer->Add(RightSizer, 0, wxEXPAND | wxALL, 0);

  MainSizer->Add(ColumnsSizer, 1, wxEXPAND | wxALL, 5);

  SetSizerAndFit(MainSizer);
  SetSizeHints(this->GetSize());

  Bind(wxEVT_INIT_DIALOG, &LauncherWindow::onInitDialog, this);
}

void LauncherWindow::onInitDialog(wxInitDialogEvent &Event) {
  loadConfig();

  // Trigger the updateDeps event to update the dependencies
  updateDisabledElements();

  // Call the base class's event handler if needed
  Event.Skip();
}

void LauncherWindow::loadConfig() {
  // This is where we populate existing params
  const auto InitParams = PGC.getParams();

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

      // Show MO2 options only if MO2 is selected
      if (MMType == ModManagerDirectory::ModManagerType::ModOrganizer2) {
        MO2OptionsSizer->Show(true);
      } else {
        MO2OptionsSizer->Show(false);
      }
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
  MO2UseOrderCheckbox->SetValue(InitParams.ModManager.MO2UseOrder);

  // Output
  OutputLocationTextbox->SetValue(InitParams.Output.Dir.wstring());
  OutputZipCheckbox->SetValue(InitParams.Output.Zip);

  // Advanced
  AdvancedOptionsCheckbox->SetValue(InitParams.Advanced);
  updateAdvanced();

  // Processing
  ProcessingPluginPatchingCheckbox->SetValue(InitParams.Processing.PluginPatching);
  ProcessingPluginPatchingOptionsESMifyCheckbox->SetValue(InitParams.Processing.PluginESMify);
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
  ShaderPatcherComplexMaterialDynCubemapBlocklist->DeleteAllItems();
  for (const auto &DynCubemap : InitParams.ShaderPatcher.ShaderPatcherComplexMaterial.ListsDyncubemapBlocklist) {
    ShaderPatcherComplexMaterialDynCubemapBlocklist->InsertItem(
        ShaderPatcherComplexMaterialDynCubemapBlocklist->GetItemCount(), DynCubemap);
  }
  ShaderPatcherComplexMaterialDynCubemapBlocklist->InsertItem(
      ShaderPatcherComplexMaterialDynCubemapBlocklist->GetItemCount(), ""); // Add empty line
  ShaderPatcherComplexMaterialDynCubemapBlocklist->SetToolTip(
      "Textures or meshes matching elements of this list will not have dynamic cubemap applied with complex material. "
      "Wildcards are supported.");

  ShaderPatcherTruePBRCheckbox->SetValue(InitParams.ShaderPatcher.TruePBR);

  // Shader Transforms
  ShaderTransformParallaxToCMCheckbox->SetValue(InitParams.ShaderTransforms.ParallaxToCM);

  // Post-Patchers
  PostPatcherOptimizeMeshesCheckbox->SetValue(InitParams.PostPatcher.OptimizeMeshes);

  // Mesh Rules
  MeshRulesAllowList->DeleteAllItems();
  for (const auto &MeshRule : InitParams.MeshRules.AllowList) {
    MeshRulesAllowList->InsertItem(MeshRulesAllowList->GetItemCount(), MeshRule);
  }
  MeshRulesAllowList->InsertItem(MeshRulesAllowList->GetItemCount(), ""); // Add empty line

  MeshRulesBlockList->DeleteAllItems();
  for (const auto &MeshRule : InitParams.MeshRules.BlockList) {
    MeshRulesBlockList->InsertItem(MeshRulesBlockList->GetItemCount(), MeshRule);
  }
  MeshRulesBlockList->InsertItem(MeshRulesBlockList->GetItemCount(), ""); // Add empty line

  // Texture Rules
  TextureRulesMaps->DeleteAllItems();
  for (const auto &TextureRule : InitParams.TextureRules.TextureMaps) {
    const auto NewIndex = TextureRulesMaps->InsertItem(TextureRulesMaps->GetItemCount(), TextureRule.first);
    TextureRulesMaps->SetItem(NewIndex, 1, NIFUtil::getStrFromTexType(TextureRule.second));
  }
  TextureRulesMaps->InsertItem(TextureRulesMaps->GetItemCount(), ""); // Add empty line

  TextureRulesVanillaBSAList->DeleteAllItems();
  for (const auto &VanillaBSA : InitParams.TextureRules.VanillaBSAList) {
    TextureRulesVanillaBSAList->InsertItem(TextureRulesVanillaBSAList->GetItemCount(), VanillaBSA);
  }
  TextureRulesVanillaBSAList->InsertItem(TextureRulesVanillaBSAList->GetItemCount(), ""); // Add empty line
}

// Component event handlers

void LauncherWindow::onGameLocationChange([[maybe_unused]] wxCommandEvent &Event) { updateDisabledElements(); }

void LauncherWindow::onGameTypeChange([[maybe_unused]] wxCommandEvent &Event) {
  const auto InitParams = PGC.getParams();

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

  updateDisabledElements();
}

void LauncherWindow::onModManagerChange([[maybe_unused]] wxCommandEvent &Event) {
  // Show MO2 options only if the MO2 radio button is selected
  bool IsMO2Selected = (Event.GetEventObject() == ModManagerRadios[ModManagerDirectory::ModManagerType::ModOrganizer2]);
  MO2OptionsSizer->Show(IsMO2Selected);
  Layout(); // Refresh layout to apply visibility changes
  Fit();

  updateDisabledElements();
}

void LauncherWindow::onMO2ProfileChange([[maybe_unused]] wxCommandEvent &Event) {
  // Update the MO2 profile
  updateDisabledElements();
}

void LauncherWindow::onMO2UseOrderChange([[maybe_unused]] wxCommandEvent &Event) {
  // Update the MO2 use order
  updateDisabledElements();
}

void LauncherWindow::onOutputLocationChange([[maybe_unused]] wxCommandEvent &Event) { updateDisabledElements(); }

void LauncherWindow::onOutputZipChange([[maybe_unused]] wxCommandEvent &Event) { updateDisabledElements(); }

void LauncherWindow::onAdvancedOptionsChange([[maybe_unused]] wxCommandEvent &Event) {
  updateAdvanced();

  updateDisabledElements();
}

void LauncherWindow::updateAdvanced() {
  bool AdvancedVisible = AdvancedOptionsCheckbox->GetValue();

  AdvancedOptionsSizer->Show(AdvancedVisible);
  ProcessingOptionsSizer->Show(AdvancedVisible);

  if (ShaderPatcherComplexMaterialCheckbox->GetValue()) {
    ShaderPatcherComplexMaterialOptionsSizer->Show(AdvancedVisible);
  }

  Layout(); // Refresh layout to apply visibility changes
  Fit();
}

void LauncherWindow::onProcessingPluginPatchingChange([[maybe_unused]] wxCommandEvent &Event) {
  if (ProcessingPluginPatchingCheckbox->GetValue()) {
    ProcessingPluginPatchingOptions->Show(ProcessingPluginPatchingCheckbox->GetValue());
    Layout(); // Refresh layout to apply visibility changes
    Fit();
  } else {
    ProcessingPluginPatchingOptions->Show(false);
    Layout(); // Refresh layout to apply visibility changes
    Fit();
  }

  updateDisabledElements();
}

void LauncherWindow::onProcessingPluginPatchingOptionsESMifyChange([[maybe_unused]] wxCommandEvent &Event) {
  updateDisabledElements();
}

void LauncherWindow::onProcessingMultithreadingChange([[maybe_unused]] wxCommandEvent &Event) {
  updateDisabledElements();
}

void LauncherWindow::onProcessingHighMemChange([[maybe_unused]] wxCommandEvent &Event) { updateDisabledElements(); }

void LauncherWindow::onProcessingGPUAccelerationChange([[maybe_unused]] wxCommandEvent &Event) {
  updateDisabledElements();
}

void LauncherWindow::onProcessingMapFromMeshesChange([[maybe_unused]] wxCommandEvent &Event) {
  updateDisabledElements();
}

void LauncherWindow::onProcessingBSAChange([[maybe_unused]] wxCommandEvent &Event) { updateDisabledElements(); }

void LauncherWindow::onPrePatcherDisableMLPChange([[maybe_unused]] wxCommandEvent &Event) { updateDisabledElements(); }

void LauncherWindow::onShaderPatcherParallaxChange([[maybe_unused]] wxCommandEvent &Event) { updateDisabledElements(); }

void LauncherWindow::onShaderPatcherComplexMaterialChange([[maybe_unused]] wxCommandEvent &Event) {
  if (ShaderPatcherComplexMaterialCheckbox->GetValue() && AdvancedOptionsCheckbox->GetValue()) {
    ShaderPatcherComplexMaterialOptionsSizer->Show(ShaderPatcherComplexMaterialCheckbox->GetValue());
    Layout(); // Refresh layout to apply visibility changes
    Fit();
  } else {
    ShaderPatcherComplexMaterialOptionsSizer->Show(false);
    Layout(); // Refresh layout to apply visibility changes
    Fit();
  }

  updateDisabledElements();
}

void LauncherWindow::onShaderPatcherComplexMaterialDynCubemapBlocklistChange([[maybe_unused]] wxListEvent &Event) {
  onListEdit(Event);

  this->CallAfter([this]() { updateDisabledElements(); });
}

void LauncherWindow::onShaderPatcherTruePBRChange([[maybe_unused]] wxCommandEvent &Event) {
  // TODO
  updateDisabledElements();
}

void LauncherWindow::onShaderTransformParallaxToCMChange([[maybe_unused]] wxCommandEvent &Event) {
  updateDisabledElements();
}

void LauncherWindow::onPostPatcherOptimizeMeshesChange([[maybe_unused]] wxCommandEvent &Event) {
  updateDisabledElements();
}

void LauncherWindow::onMeshRulesAllowListChange(wxListEvent &Event) {
  onListEdit(Event);

  this->CallAfter([this]() { updateDisabledElements(); });
}

void LauncherWindow::onMeshRulesBlockListChange(wxListEvent &Event) {
  onListEdit(Event);

  this->CallAfter([this]() { updateDisabledElements(); });
}

void LauncherWindow::onTextureRulesMapsChange([[maybe_unused]] wxListEvent &Event) {
  onListEdit(Event);

  this->CallAfter([this]() { updateDisabledElements(); });
}

void LauncherWindow::onTextureRulesMapsChangeStart([[maybe_unused]] wxMouseEvent &Event) {
  static const auto PossibleTexTypes = NIFUtil::getTexTypesStr();
  // convert to wxArrayStr
  wxArrayString WXPossibleTexTypes;
  for (const auto &TexType : PossibleTexTypes) {
    WXPossibleTexTypes.Add(TexType);
  }

  wxPoint Pos = Event.GetPosition();
  int Flags = 0;
  long Item = TextureRulesMaps->HitTest(Pos, Flags);

  if (Item != wxNOT_FOUND && ((Flags & wxLIST_HITTEST_ONITEM) != 0)) {
    int Column = getColumnAtPosition(Pos, Item);
    if (Column == 0) {
      // Start editing the first column
      TextureRulesMaps->EditLabel(Item);
    } else if (Column == 1) {
      // Create dropdown for the second column
      wxRect Rect;
      TextureRulesMaps->GetSubItemRect(Item, Column, Rect);

      TextureMapTypeCombo = // NOLINT(cppcoreguidelines-owning-memory)
          new wxComboBox(TextureRulesMaps, wxID_ANY, "", Rect.GetTopLeft(), Rect.GetSize(), WXPossibleTexTypes,
                         wxCB_DROPDOWN | wxCB_READONLY);
      TextureMapTypeCombo->SetFocus();
      TextureMapTypeCombo->Popup();
      TextureMapTypeCombo->Bind(wxEVT_COMBOBOX, [=, this](wxCommandEvent &) {
        TextureRulesMaps->SetItem(Item, Column, TextureMapTypeCombo->GetValue());
        TextureMapTypeCombo->Show(false);

        // Check if it's the last row and add a new blank row
        if (Item == TextureRulesMaps->GetItemCount() - 1) {
          TextureRulesMaps->InsertItem(TextureRulesMaps->GetItemCount(), "");
        }
      });

      TextureMapTypeCombo->Bind(wxEVT_KILL_FOCUS, [=, this](wxFocusEvent &) { TextureMapTypeCombo->Show(false); });
    }
  } else {
    Event.Skip();
  }
}

auto LauncherWindow::getColumnAtPosition(const wxPoint &Pos, long Item) -> int {
  wxRect Rect;
  for (int Col = 0; Col < TextureRulesMaps->GetColumnCount(); ++Col) {
    TextureRulesMaps->GetSubItemRect(Item, Col, Rect);
    if (Rect.Contains(Pos)) {
      return Col;
    }
  }
  return -1; // No column found
}

void LauncherWindow::onTextureRulesVanillaBSAListChange(wxListEvent &Event) {
  onListEdit(Event);

  this->CallAfter([this]() { updateDisabledElements(); });
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
  Params.ModManager.MO2UseOrder = MO2UseOrderCheckbox->GetValue();

  // Output
  Params.Output.Dir = OutputLocationTextbox->GetValue().ToStdWstring();
  Params.Output.Zip = OutputZipCheckbox->GetValue();

  // Advanced
  Params.Advanced = AdvancedOptionsCheckbox->GetValue();

  // Processing
  Params.Processing.PluginPatching = ProcessingPluginPatchingCheckbox->GetValue();
  Params.Processing.PluginESMify = ProcessingPluginPatchingOptionsESMifyCheckbox->GetValue();
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

  Params.ShaderPatcher.ShaderPatcherComplexMaterial.ListsDyncubemapBlocklist.clear();
  for (int I = 0; I < ShaderPatcherComplexMaterialDynCubemapBlocklist->GetItemCount(); I++) {
    const auto ItemText = ShaderPatcherComplexMaterialDynCubemapBlocklist->GetItemText(I);
    if (!ItemText.IsEmpty()) {
      Params.ShaderPatcher.ShaderPatcherComplexMaterial.ListsDyncubemapBlocklist.push_back(ItemText.ToStdWstring());
    }
  }

  Params.ShaderPatcher.TruePBR = ShaderPatcherTruePBRCheckbox->GetValue();

  // Shader Transforms
  Params.ShaderTransforms.ParallaxToCM = ShaderTransformParallaxToCMCheckbox->GetValue();

  // Post-Patchers
  Params.PostPatcher.OptimizeMeshes = PostPatcherOptimizeMeshesCheckbox->GetValue();

  // Mesh Rules
  Params.MeshRules.AllowList.clear();
  for (int I = 0; I < MeshRulesAllowList->GetItemCount(); I++) {
    const auto ItemText = MeshRulesAllowList->GetItemText(I);
    if (!ItemText.IsEmpty()) {
      Params.MeshRules.AllowList.push_back(ItemText.ToStdWstring());
    }
  }

  Params.MeshRules.BlockList.clear();
  for (int I = 0; I < MeshRulesBlockList->GetItemCount(); I++) {
    const auto ItemText = MeshRulesBlockList->GetItemText(I);
    if (!ItemText.IsEmpty()) {
      Params.MeshRules.BlockList.push_back(ItemText.ToStdWstring());
    }
  }

  // Texture Rules
  Params.TextureRules.TextureMaps.clear();
  for (int I = 0; I < TextureRulesMaps->GetItemCount(); I++) {
    const auto ItemText = TextureRulesMaps->GetItemText(I, 0);
    if (!ItemText.IsEmpty()) {
      const auto TexType = NIFUtil::getTexTypeFromStr(TextureRulesMaps->GetItemText(I, 1).ToStdString());
      Params.TextureRules.TextureMaps.emplace_back(ItemText.ToStdWstring(), TexType);
    }
  }

  Params.TextureRules.VanillaBSAList.clear();
  for (int I = 0; I < TextureRulesVanillaBSAList->GetItemCount(); I++) {
    const auto ItemText = TextureRulesVanillaBSAList->GetItemText(I);
    if (!ItemText.IsEmpty()) {
      Params.TextureRules.VanillaBSAList.push_back(ItemText.ToStdWstring());
    }
  }

  return Params;
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
  const auto Profiles =
      ModManagerDirectory::getMO2ProfilesFromInstanceDir(MO2InstanceLocationTextbox->GetValue().ToStdWstring());

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

void LauncherWindow::onListEdit(wxListEvent &Event) {
  auto *ListCtrl = dynamic_cast<wxListCtrl *>(Event.GetEventObject());
  if (ListCtrl == nullptr) {
    return;
  }

  if (Event.IsEditCancelled()) {
    return;
  }

  const auto EditedIndex = Event.GetIndex();
  const auto &EditedText = Event.GetLabel();

  if (EditedText.IsEmpty() && EditedIndex != ListCtrl->GetItemCount() - 1) {
    // If the edited item is empty and it's not the last item
    this->CallAfter([ListCtrl, EditedIndex]() { ListCtrl->DeleteItem(EditedIndex); });
    return;
  }

  // If the edited item is not empty and it's the last item
  if (!EditedText.IsEmpty() && EditedIndex == ListCtrl->GetItemCount() - 1) {
    // Add a new empty item
    ListCtrl->InsertItem(ListCtrl->GetItemCount(), "");
  }
}

void LauncherWindow::onListItemActivated(wxListEvent &Event) {
  auto *ListCtrl = dynamic_cast<wxListCtrl *>(Event.GetEventObject());
  if (ListCtrl == nullptr) {
    return;
  }

  const auto ItemIndex = Event.GetIndex();

  // Start editing the label of the item
  ListCtrl->EditLabel(ItemIndex);
}

void LauncherWindow::updateDisabledElements() {
  const auto CurParams = getParams();

  // Upgrade parallax to CM rules
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

  // PBR rules
  if (CurParams.ShaderPatcher.TruePBR && !CurParams.ShaderPatcher.Parallax &&
      !CurParams.ShaderPatcher.ComplexMaterial) {
    // disable map from meshes
    ProcessingMapFromMeshesCheckbox->SetValue(false);
    ProcessingMapFromMeshesCheckbox->Enable(false);
  } else {
    ProcessingMapFromMeshesCheckbox->Enable(true);
  }

  // save button
  SaveConfigButton->Enable(CurParams != PGC.getParams());
}

void LauncherWindow::onOkButtonPressed([[maybe_unused]] wxCommandEvent &Event) {
  if (saveConfig()) {
    // All validation passed, proceed with OK actions
    EndModal(wxID_OK);
  }
}

void LauncherWindow::onSaveConfigButtonPressed([[maybe_unused]] wxCommandEvent &Event) {
  if (saveConfig()) {
    // Disable button
    updateDisabledElements();
  }
}

void LauncherWindow::onLoadConfigButtonPressed([[maybe_unused]] wxCommandEvent &Event) {
  int Response = wxMessageBox("Are you sure you want to load the config from the file? This action will overwrite all "
                              "current unsaved settings.",
                              "Confirm Load Config", wxYES_NO | wxICON_WARNING, this);

  if (Response != wxYES) {
    return;
  }

  // Load the config from the file
  loadConfig();

  updateDisabledElements();
}

void LauncherWindow::onRestoreDefaultsButtonPressed([[maybe_unused]] wxCommandEvent &Event) {
  // Show a confirmation dialog
  int Response = wxMessageBox("Are you sure you want to restore the default settings? This action cannot be undone.",
                              "Confirm Restore Defaults", wxYES_NO | wxICON_WARNING, this);

  if (Response != wxYES) {
    return;
  }

  // Reset the config to the default
  PGC.setParams(PGC.getDefaultParams());

  loadConfig();

  updateDisabledElements();
}

auto LauncherWindow::saveConfig() -> bool {
  vector<string> Errors;
  const auto Params = getParams();

  // Validate the parameters
  if (!ParallaxGenConfig::validateParams(Params, Errors)) {
    wxMessageDialog ErrorDialog(this, boost::algorithm::join(Errors, "\n"), "Errors", wxOK | wxICON_ERROR);
    ErrorDialog.ShowModal();
    return false;
  }

  PGC.setParams(Params);
  return true;
}

void LauncherWindow::onClose( // NOLINT(readability-convert-member-functions-to-static)
    [[maybe_unused]] wxCloseEvent &Event) {
  UIExitTriggered = true;
  wxTheApp->Exit();
}
