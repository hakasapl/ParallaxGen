#include <utility>

#include "NIFUtil.hpp"
#include "ParallaxGenUtil.hpp"
#include "patchers/PatcherShaderTransform.hpp"

#include <spdlog/spdlog.h>

PatcherShaderTransform::PatcherShaderTransform(std::filesystem::path NIFPath, nifly::NifFile *NIF,
                                               std::string PatcherName, const NIFUtil::ShapeShader &From,
                                               const NIFUtil::ShapeShader &To)
    : Patcher(std::move(NIFPath), NIF, std::move(PatcherName)), FromShader(From), ToShader(To) {}

std::mutex PatcherShaderTransform::ErrorTrackerMutex;
std::unordered_set<std::tuple<std::filesystem::path, NIFUtil::ShapeShader, NIFUtil::ShapeShader>,
                   PatcherShaderTransform::ErrorTrackerHasher>
    PatcherShaderTransform::ErrorTracker;

void PatcherShaderTransform::postError(const std::filesystem::path &File) {
  std::lock_guard<std::mutex> Lock(ErrorTrackerMutex);

  if (ErrorTracker.insert({File, FromShader, ToShader}).second) {
    spdlog::error(L"Failed to transform from {} to {} for {}",
                  ParallaxGenUtil::ASCIItoUTF16(NIFUtil::getStrFromShader(FromShader)),
                  ParallaxGenUtil::ASCIItoUTF16(NIFUtil::getStrFromShader(ToShader)), File.wstring());
  }
}

auto PatcherShaderTransform::alreadyTried(const std::filesystem::path &File) -> bool {
  std::lock_guard<std::mutex> Lock(ErrorTrackerMutex);

  return ErrorTracker.contains({File, FromShader, ToShader});
}
