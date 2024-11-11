#pragma once

#include <filesystem>

#include "NifFile.hpp"

#include "ParallaxGenD3D.hpp"
#include "ParallaxGenDirectory.hpp"

class Patcher {
private:
  // Instance vars
  std::filesystem::path NIFPath;
  nifly::NifFile *NIF;
  std::string PatcherName;

  // Static Tools
  static ParallaxGenDirectory *PGD;
  static ParallaxGenD3D *PGD3D;

protected:
  [[nodiscard]] auto getNIFPath() const -> std::filesystem::path;
  [[nodiscard]] auto getNIF() const -> nifly::NifFile *;

  [[nodiscard]] static auto getPGD() -> ParallaxGenDirectory *;
  [[nodiscard]] static auto getPGD3D() -> ParallaxGenD3D *;

public:
  static void loadStatics(ParallaxGenDirectory &PGD, ParallaxGenD3D &PGD3D);

  Patcher(std::filesystem::path NIFPath, nifly::NifFile *NIF, std::string PatcherName);

  [[nodiscard]] auto getPatcherName() const -> std::string;
};
