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
#include <vector>

#include "BethesdaGame.hpp"
#include "ModManagerDirectory.hpp"
#include "ParallaxGenConfig.hpp"

#include "NIFUtil.hpp"

class LauncherWindow : public wxDialog {
public:
  LauncherWindow(const ParallaxGenConfig::PGParams &Params);

  [[nodiscard]] auto getParams() -> ParallaxGenConfig::PGParams;

private:
  const ParallaxGenConfig::PGParams &InitParams;
  void onInitDialog(wxInitDialogEvent& Event);

  //
  // UI Param Elements
  //

  // Game
  wxTextCtrl *GameLocationTextbox;
  std::unordered_map<BethesdaGame::GameType, wxRadioButton *> GameTypeRadios;

  // Mod Manager
  std::unordered_map<ModManagerDirectory::ModManagerType, wxRadioButton *> ModManagerRadios;
  wxTextCtrl *MO2InstanceLocationTextbox;
  wxChoice *MO2ProfileChoice;

  // Output
  wxTextCtrl *OutputLocationTextbox;
  wxCheckBox *OutputZipCheckbox;

  // Processing
  wxCheckBox *ProcessingPluginPatchingCheckbox;
  wxCheckBox *ProcessingMultithreadingCheckbox;
  wxCheckBox *ProcessingHighMemCheckbox;
  wxCheckBox *ProcessingGPUAccelerationCheckbox;
  wxCheckBox *ProcessingMapFromMeshesCheckbox;
  wxCheckBox *ProcessingBSACheckbox;

  // Pre-Patchers
  wxCheckBox *PrePatcherDisableMLPCheckbox;

  // Shader Patchers
  wxCheckBox *ShaderPatcherParallaxCheckbox;
  wxCheckBox *ShaderPatcherComplexMaterialCheckbox;
  wxCheckBox *ShaderPatcherTruePBRCheckbox;

  // Shader Transforms
  wxCheckBox *ShaderTransformParallaxToCMCheckbox;

  // Post-Patchers
  wxCheckBox *PostPatcherOptimizeMeshesCheckbox;

  //
  // UI Controls
  //
  wxStaticBoxSizer *MO2OptionsSizer;
  void updateModManagerOptions(wxCommandEvent &Event);

  void onBrowseGameLocation(wxCommandEvent& Event);
  void onBrowseMO2InstanceLocation(wxCommandEvent& Event);
  void onMO2InstanceLocationChange(wxCommandEvent& Event);
  void onBrowseOutputLocation(wxCommandEvent& Event);

  wxBoxSizer *AdvancedOptionsSizer;
  wxButton *AdvancedButton;
  bool AdvancedVisible = false;
  void onToggleAdvanced(wxCommandEvent &Event);

  //
  // Validation
  //
  wxButton *OKButton;
  void onOkButtonPressed(wxCommandEvent& Event);
  void onClose(wxCloseEvent& Event);
};

class ModSortDialog : public wxDialog {
private:
  wxListCtrl *ListCtrl;
  wxTimer ScrollTimer; // Timer for continuous scrolling
  int ListCtrlHeaderHeight = 0;

  std::unordered_map<std::wstring, std::unordered_set<std::wstring>> ConflictsMap;

  // Stores the original color of things
  std::unordered_map<std::wstring, wxColour> OriginalBackgroundColors;

  // Index of item being dragged
  int DraggedIndex = -1;
  // Index of target line where a dragged item will be dropped
  int TargetLineIndex = -1;

  // Stores the elements currently being dragged
  std::vector<long> DraggedIndices;

  // Overlay used for drop indicators
  wxOverlay Overlay;

  // Flag to determine if the list is sorted in ascending order
  bool SortAscending;

public:
  ModSortDialog(const std::vector<std::wstring> &Mods, const std::vector<std::wstring> &Shaders,
                const std::vector<bool> &IsNew,
                const std::unordered_map<std::wstring, std::unordered_set<std::wstring>> &Conflicts);
  [[nodiscard]] auto getSortedItems() const -> std::vector<std::wstring>;

private:
  void onItemSelected(wxListEvent &Event);
  void onMouseLeftDown(wxMouseEvent &Event);
  void onMouseMotion(wxMouseEvent &Event);
  void onMouseLeftUp(wxMouseEvent &Event);
  void onColumnClick(wxListEvent &Event);
  void onTimer(wxTimerEvent &Event);
  auto getHeaderHeight() -> int;
  auto calculateColumnWidth(int ColIndex) -> int;
  void highlightConflictingItems(const std::wstring &SelectedMod);
  void drawDropIndicator(int TargetIndex);
  void resetIndices();
  void reverseListOrder();
};

class ParallaxGenUI {
public:
  static bool UIExitTriggered;

  static void init();

  static auto showLauncher(const ParallaxGenConfig::PGParams &OldParams) -> ParallaxGenConfig::PGParams;

  static auto selectModOrder(
      const std::unordered_map<std::wstring,
                               std::tuple<std::set<NIFUtil::ShapeShader>, std::unordered_set<std::wstring>>> &Conflicts,
      const std::vector<std::wstring> &ExistingMods) -> std::vector<std::wstring>;
};
