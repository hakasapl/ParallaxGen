#pragma once

#include <Geometry.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <windows.h>

#include <boost/functional/hash.hpp>

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

  /// @brief get the model record handle from the alternate texture handle
  /// @param[in] AltTexIndex global index of the alternate texture
  /// @return model record handle
  static auto libGetModelRecHandleFromAltTexHandle(const int &AltTexIndex) -> int;

  static void libSetModelRecNIF(const int &ModelRecHandle, const std::wstring &NIFPath);

  static bool LoggingEnabled;
  static void logMessages();

  static ParallaxGenDirectory *PGD;

  static std::mutex ProcessShapeMutex;

  // Custom hash function for std::array<std::string, 9>
  struct ArrayHash {
    auto operator()(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &Arr) const -> std::size_t {
      std::size_t Hash = 0;
      for (const auto &Str : Arr) {
        // Convert string to lowercase and hash
        std::wstring LowerStr = boost::to_lower_copy(Str);
        boost::hash_combine(Hash, boost::hash<std::wstring>()(LowerStr));
      }
      return Hash;
    }
  };

  struct ArrayEqual {
    auto operator()(const std::array<std::wstring, NUM_TEXTURE_SLOTS> &Lhs,
                    const std::array<std::wstring, NUM_TEXTURE_SLOTS> &Rhs) const -> bool {
      for (size_t I = 0; I < NUM_TEXTURE_SLOTS; ++I) {
        if (!boost::iequals(Lhs[I], Rhs[I])) {
          return false;
        }
      }
      return true;
    }
  };
  static std::unordered_map<std::array<std::wstring, NUM_TEXTURE_SLOTS>, int, ArrayHash, ArrayEqual> CreatedTXSTs;
  static std::mutex CreatedTXSTMutex;

  // Runner vars
  static std::unordered_map<std::wstring, int> *ModPriority;

public:
  // TODO all of this should not be static
  static void loadStatics(ParallaxGenDirectory *PGD);
  static void loadModPriorityMap(std::unordered_map<std::wstring, int> *ModPriority);

  static void initialize(const BethesdaGame &Game);

  static void populateObjs();

  struct TXSTResult {
    int ModelRecHandle{};
    int AltTexIndex{};
    int TXSTIndex{};
    NIFUtil::ShapeShader Shader{};
  };

  /// @brief Patch texture sets for a given shape
  /// @param[in] AppliedShader shader that was applied to the shape by the patchers
  /// @param[in] NIFPath path of the nif file for the shape to process
  /// @param[in] Name3D name of the shape in the NIF
  /// @param[in] Index3DOld zero-based index of the shape in the nif before patching the shapes
  /// @param[in] Index3DNew zero-based index of the shape in the nif after patching the shapes in the nif
  /// @param[in] Patchers patchers for the given nif
  /// @param[out] NewSlots textures that were assigned to the texture set slots
  static void processShape(const std::wstring &NIFPath, nifly::NiShape *NIFShape, const std::wstring &Name3D,
                           const int &Index3D, PatcherUtil::PatcherObjectSet &Patchers,
                           std::vector<TXSTResult> &Results, PatcherUtil::ConflictModResults *ConflictMods = nullptr);

  static void assignMesh(const std::wstring &NIFPath, const std::vector<TXSTResult> &Result);

  static void set3DIndices(const std::wstring &NIFPath,
                           const std::vector<std::tuple<nifly::NiShape *, int, int>> &ShapeTracker);

  static void savePlugin(const std::filesystem::path &OutputDir, bool ESMify);
};
