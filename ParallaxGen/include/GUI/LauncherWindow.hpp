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

#include "BethesdaGame.hpp"
#include "ModManagerDirectory.hpp"
#include "ParallaxGenConfig.hpp"

/**
 * @brief wxDialog that allows the user to configure the ParallaxGen parameters.
 */
class LauncherWindow : public wxDialog {
public:
  static inline bool UIExitTriggered = false;

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
  wxCheckBox *MO2UseOrderCheckbox;

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

  /**
   * @brief Event handler responsible for changing game location when game type is changed
   *
   * @param Event wxWidgets event object
   */
  void onGameTypeChanged(wxCommandEvent &Event);

  /**
   * @brief Event handler responsible for enabling/disabling elements based on selection
   *
   * @param Event wxWidgets event object
   */
  void onUpdateDeps(wxCommandEvent &Event);

  /**
   * @brief Updates disabled elements based on the current state of the UI
   */
  void updateDisabledElements();

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
