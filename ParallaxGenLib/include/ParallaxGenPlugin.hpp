#pragma once

#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <windows.h>

#include "BethesdaGame.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenDirectory.hpp"
#include "patchers/PatcherUtil.hpp"

#define LOG_POLL_INTERVAL 1000

class ParallaxGenPlugin {
private:
  static std::mutex LibMutex;
  static void libLogMessageIfExists();
  static void libThrowExceptionIfExists();
  static void libInitialize(const int &GameType, const std::wstring &DataPath, const std::wstring &OutputPlugin,
                            const std::vector<std::wstring> &LoadOrder = {});
  static void libPopulateObjs();
  static void libFinalize(const std::filesystem::path &OutputPath, const bool &ESMify);

  /// @brief get the TXST objects that are used by a shape
  /// @param[in] NIFName filename of the nif
  /// @param[in] Name3D "3D name" - name of the shape
  /// @param[in] Index3D "3D index" - index of the shape in the nif
  /// @return TXST objects, pairs of (texture set index,alternate textures ids)
  static auto libGetMatchingTXSTObjs(const std::wstring &NIFName, const std::wstring &Name3D,
                                     const int &Index3D) -> std::vector<std::tuple<int, int>>;

  /// @brief get the assigned textures of all slots in a texture set
  /// @param[in] TXSTIndex index of the texture set
  /// @return array of texture files for all texture slots
  static auto libGetTXSTSlots(const int &TXSTIndex) -> std::array<std::wstring, NUM_TEXTURE_SLOTS>;

  static void libCreateTXSTPatch(const int &TXSTIndex, const std::array<std::wstring, NUM_TEXTURE_SLOTS> &Slots);

  static auto libCreateNewTXSTPatch(const int &AltTexIndex,
                                    const std::array<std::wstring, NUM_TEXTURE_SLOTS> &Slots) -> int;

  /// @brief assign texture set to an "alternate texture", i.e. the "new texture" entry
  /// @param[in] AltTexIndex global index of the alternate texture
  /// @param[in] TXSTIndex global index of the texture set
  static void libSetModelAltTex(const int &AltTexIndex, const int &TXSTIndex);

  /// @brief set the "3D index" of an alternate texture, i.e. the shape index
  /// @param[in] AltTexIndex global index of the alternate texture
  /// @param[in] Index3D index to set
  static void libSet3DIndex(const int &AltTexIndex, const int &Index3D);

  /// @brief get the form id and mod name of the given texture set
  /// @param[in] TXSTIndex  the texture set
  /// @return pair of form id and mod name
  static auto libGetTXSTFormID(const int &TXSTIndex) -> std::tuple<unsigned int, std::wstring>;

  static std::mutex TXSTModMapMutex;

  /// @brief map of texture sets that are already modded, value is a map of shader types to new texture set
  static std::unordered_map<int, std::unordered_map<NIFUtil::ShapeShader, int>> TXSTModMap;

  static std::mutex TXSTWarningMapMutex;

  /// @brief store all txsts where a warning has already been issued
  static std::unordered_map<int, NIFUtil::ShapeShader> TXSTWarningMap;

  static bool LoggingEnabled;
  static void logMessages();

  static ParallaxGenDirectory *PGD;

  static std::mutex ProcessShapeMutex;

public:
  static void loadStatics(ParallaxGenDirectory *PGD);

  static void initialize(const BethesdaGame &Game);

  static void populateObjs();

  /// @brief Patch texture sets for a given shape
  /// @param[in] AppliedShader shader that was applied to the shape by the patchers
  /// @param[in] NIFPath path of the nif file for the shape to process
  /// @param[in] Name3D name of the shape in the NIF
  /// @param[in] Index3DOld zero-based index of the shape in the nif before patching the shapes
  /// @param[in] Index3DNew zero-based index of the shape in the nif after patching the shapes in the nif
  /// @param[in] Patchers patchers for the given nif
  /// @param[out] NewSlots textures that were assigned to the texture set slots
  static void processShape(const NIFUtil::ShapeShader &AppliedShader, const std::wstring &NIFPath,
                           const std::wstring &Name3D, const int &Index3D,
                           const PatcherUtil::PatcherObjectSet &Patchers,
                           const std::unordered_map<std::wstring, int> *ModPriority,
                           std::array<std::wstring, NUM_TEXTURE_SLOTS> &NewSlots);

  static void set3DIndices(const std::wstring &NIFPath,
                           const std::vector<std::tuple<nifly::NiShape *, int, int>> &ShapeTracker);

  static void savePlugin(const std::filesystem::path &OutputDir, bool ESMify);
};
