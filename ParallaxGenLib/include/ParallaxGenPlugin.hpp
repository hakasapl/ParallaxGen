#pragma once

#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <windows.h>

#include "BethesdaGame.hpp"
#include "NIFUtil.hpp"

class ParallaxGenPlugin {
private:
  static std::mutex LibMutex;
  static void libThrowExceptionIfExists();
  static void libInitialize(const int &GameType, const std::wstring &DataPath, const std::wstring &OutputPlugin);
  static void libPopulateObjs();
  static void libFinalize(const std::filesystem::path &OutputPath);
  static auto libGetMatchingTXSTObjs(const std::wstring &NIFName, const std::wstring &Name3D, const int &Index3D) -> std::vector<std::tuple<int, int>>;
  static auto libGetTXSTSlots(const int &TXSTIndex) -> std::array<std::wstring, NUM_TEXTURE_SLOTS>;
  static void libCreateTXSTPatch(const int &TXSTIndex, const std::array<std::wstring, NUM_TEXTURE_SLOTS> &Slots);
  static auto libCreateNewTXSTPatch(const int &AltTexIndex, const std::array<std::wstring, NUM_TEXTURE_SLOTS> &Slots) -> int;
  static void libSetModelAltTex(const int &AltTexIndex, const int &TXSTIndex);
  static void libSet3DIndex(const int &AltTexIndex, const int &Index3D);
  static auto libGetTXSTFormID(const int &TXSTIndex) -> std::tuple<unsigned int, std::wstring>;

  static std::mutex TXSTModMapMutex;
  static std::unordered_map<int, std::unordered_map<NIFUtil::ShapeShader, int>> TXSTModMap;

  static std::mutex TXSTWarningMapMutex;
  static std::unordered_map<int, NIFUtil::ShapeShader> TXSTWarningMap;

public:
  static void initialize(const BethesdaGame &Game);

  static void populateObjs();

  static void processShape(const NIFUtil::ShapeShader &AppliedShader, const std::wstring &NIFPath, const std::wstring &Name3D, const int &Index3DOld, const int &Index3DNew);

  static void savePlugin(const std::filesystem::path &OutputDir);
};
