#pragma once

#include <string>
#include <unordered_set>

#include "NIFUtil.hpp"
#include "patchers/Patcher.hpp"
#include "patchers/PatcherShader.hpp"

/**
 * @class PatcherShaderTransform
 * @brief Base class for shader transform patchers
 */
class PatcherShaderTransform : public Patcher {
private:
  struct ErrorTrackerHasher {
    auto operator()(const std::tuple<std::filesystem::path, NIFUtil::ShapeShader, NIFUtil::ShapeShader> &Key) const
        -> std::size_t {
      return std::hash<std::filesystem::path>{}(std::get<0>(Key)) ^
             std::hash<NIFUtil::ShapeShader>{}(std::get<1>(Key)) ^ std::hash<NIFUtil::ShapeShader>{}(std::get<2>(Key));
    }
  };

  static std::unordered_set<std::tuple<std::filesystem::path, NIFUtil::ShapeShader, NIFUtil::ShapeShader>,
                            ErrorTrackerHasher>
      ErrorTracker;                /** Tracks transforms that failed for error messages */
  NIFUtil::ShapeShader FromShader; /** Shader to transform from */
  NIFUtil::ShapeShader ToShader;   /** Shader to transform to */

protected:
  void postError(const std::filesystem::path &File);            /** Post error message for transform */
  auto alreadyTried(const std::filesystem::path &File) -> bool; /** Check if transform has already been tried */

public:
  // Custom type definitions
  using PatcherShaderTransformFactory =
      std::function<std::unique_ptr<PatcherShaderTransform>(std::filesystem::path, nifly::NifFile *)>;
  using PatcherShaderTransformObject = std::unique_ptr<PatcherShaderTransform>;

  // Constructors
  PatcherShaderTransform(std::filesystem::path NIFPath, nifly::NifFile *NIF, std::string PatcherName,
                         const NIFUtil::ShapeShader &From, const NIFUtil::ShapeShader &To);
  virtual ~PatcherShaderTransform() = default;
  PatcherShaderTransform(const PatcherShaderTransform &Other) = default;
  auto operator=(const PatcherShaderTransform &Other) -> PatcherShaderTransform & = default;
  PatcherShaderTransform(PatcherShaderTransform &&Other) noexcept = default;
  auto operator=(PatcherShaderTransform &&Other) noexcept -> PatcherShaderTransform & = default;

  /**
   * @brief Transform shader match to new shader match
   *
   * @param FromMatch shader match to transform
   * @return PatcherShader::PatcherMatch transformed match
   */
  virtual auto transform(const PatcherShader::PatcherMatch &FromMatch, PatcherShader::PatcherMatch &Result) -> bool = 0;
};
