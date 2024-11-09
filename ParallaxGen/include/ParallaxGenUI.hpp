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

/**
 * @brief wxDialog that allows the user to configure the ParallaxGen parameters.
 */
class LauncherWindow : public wxDialog {
public:
  /**
   * @brief Construct a new Launcher Window object
   *
   * @param Params initial set of parameters to have the dialog display
   */
  LauncherWindow(const ParallaxGenConfig::PGParams &Params);

  /**
   * @brief Get the Params object (meant to be called after the user presses okay)
   *
   * @return ParallaxGenConfig::PGParams Gets the current parameters set by the user on the dialog
   */
  [[nodiscard]] auto getParams() -> ParallaxGenConfig::PGParams;

private:
  const ParallaxGenConfig::PGParams &InitParams; /** Holds the initial parameters passed in the constructor */

  /**
   * @brief Runs immediately after the wxDialog gets constructed, intended to set the UI elements to the initial
   * parameters
   *
   * @param Event wxWidgets event object
   */
  void onInitDialog(wxInitDialogEvent &Event);

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
  wxStaticBoxSizer *MO2OptionsSizer; /** Stores the MO2-specific options since these are only sometimes shown */

  /**
   * @brief Event handler that triggers every time the mod manager radio buttons are changed
   *
   * @param Event wxWidgets event object
   */
  void updateModManagerOptions(wxCommandEvent &Event);

  /**
   * @brief Event handler responsible for showing the brose dialog when the user clicks on the browse button - for game
   * location
   *
   * @param Event wxWidgets event object
   */
  void onBrowseGameLocation(wxCommandEvent &Event);

  /**
   * @brief Event handler responsible for showing the brose dialog when the user clicks on the browse button - for MO2
   * instance location
   *
   * @param Event wxWidgets event object
   */
  void onBrowseMO2InstanceLocation(wxCommandEvent &Event);

  /**
   * @brief Event handler responsible for updating the mo2 profiles dropdown whenever the instance location changes
   *
   * @param Event wxWidgets event object
   */
  void onMO2InstanceLocationChange(wxCommandEvent &Event);

  /**
   * @brief Event handler responsible for showing the brose dialog when the user clicks on the browse button - for
   * output location
   *
   * @param Event wxWidgets event object
   */
  void onBrowseOutputLocation(wxCommandEvent &Event);

  wxBoxSizer *AdvancedOptionsSizer; /** Container that stores advanced options */
  wxButton *AdvancedButton;         /** Button to show/hide advanced options */
  bool AdvancedVisible = false;     /** Stores whether advanced options are shown or not */

  /**
   * @brief Event handler responsible for showing/hiding the advanced options when the button is pressed
   *
   * @param Event wxWidgets event object
   */
  void onToggleAdvanced(wxCommandEvent &Event);

  //
  // Validation
  //
  wxButton *OKButton; /** Stores the OKButton as a member var in case it needs to be disabled/enabled */

  /**
   * @brief Event handler that triggers when the user presses "Start Patching" - performs validation
   *
   * @param Event wxWidgets event object
   */
  void onOkButtonPressed(wxCommandEvent &Event);

  /**
   * @brief Event handler that triggers when the user presses the X on the dialog window, which closes the application
   *
   * @param Event wxWidgets event object
   */
  void onClose(wxCloseEvent &Event);
};

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

class ParallaxGenUI {
public:
  static bool UIExitTriggered; /** If this is true than the UI triggered an exit, which means the CLI should NOT wait
                                  for user to press ENTER */

  /**
   * @brief Initialize the wxWidgets UI framwork
   */
  static void init();

  /**
   * @brief Shows the launcher dialog to the user (Hangs thread until user presses okay)
   *
   * @param OldParams Params to show in the UI
   * @return ParallaxGenConfig::PGParams Params set by the user
   */
  static auto showLauncher(const ParallaxGenConfig::PGParams &OldParams) -> ParallaxGenConfig::PGParams;

  /**
   * @brief Shows the mod selection dialog to the user (Hangs thread until user presses okay)
   *
   * @param Conflicts Mod conflict map
   * @param ExistingMods Mods that already exist in the order
   * @return std::vector<std::wstring> Vector of mods sorted by the user
   */
  static auto selectModOrder(
      const std::unordered_map<std::wstring,
                               std::tuple<std::set<NIFUtil::ShapeShader>, std::unordered_set<std::wstring>>> &Conflicts,
      const std::vector<std::wstring> &ExistingMods) -> std::vector<std::wstring>;
};
