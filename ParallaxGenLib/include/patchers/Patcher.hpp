#pragma once

#include <filesystem>

#include "NifFile.hpp"

#include "ParallaxGenD3D.hpp"
#include "ParallaxGenDirectory.hpp"

/**
 * @class Patcher
 * @brief Base class for all patchers
 */
class Patcher {
private:
  // Instance vars
  std::filesystem::path NIFPath; /** Stores the path to the NIF file currently being patched */
  nifly::NifFile *NIF;           /** Stores the NIF object itself */
  std::string PatcherName;       /** Name of the patcher (used in log and UI elements) */

  // Static Tools
  static ParallaxGenDirectory *PGD; /** All patchers have access to PGD */
  static ParallaxGenD3D *PGD3D;     /** All patchers have access to PGD3D */

  // Each patcher needs to also implement these static methods:
  // static auto getFactory()

protected:
  /**
   * @brief Get the NIF path for the current patcher (used only within child patchers)
   *
   * @return std::filesystem::path Path to NIF
   */
  [[nodiscard]] auto getNIFPath() const -> std::filesystem::path;

  /**
   * @brief Get the NIF object for the current patcher (used only within child patchers)
   *
   * @return nifly::NifFile* pointer to NIF object
   */
  [[nodiscard]] auto getNIF() const -> nifly::NifFile *;

  /**
   * @brief Get the PGD object (used only within child patchers)
   *
   * @return ParallaxGenDirectory* PGD pointer
   */
  [[nodiscard]] static auto getPGD() -> ParallaxGenDirectory *;

  /**
   * @brief Get the PGD3D object (used only within child patchers)
   *
   * @return ParallaxGenD3D* PGD3D pointer
   */
  [[nodiscard]] static auto getPGD3D() -> ParallaxGenD3D *;

public:
  /**
   * @brief Load the statics for all patchers
   *
   * @param PGD initialized PGD object
   * @param PGD3D initialized PGD3D object
   */
  static void loadStatics(ParallaxGenDirectory &PGD, ParallaxGenD3D &PGD3D);

  /**
   * @brief Construct a new Patcher object
   *
   * @param NIFPath Path to NIF being patched
   * @param NIF NIF object
   * @param PatcherName Name of patcher
   */
  Patcher(std::filesystem::path NIFPath, nifly::NifFile *NIF, std::string PatcherName);

  /**
   * @brief Get the Patcher Name object
   *
   * @return std::string Patcher name
   */
  [[nodiscard]] auto getPatcherName() const -> std::string;
};
