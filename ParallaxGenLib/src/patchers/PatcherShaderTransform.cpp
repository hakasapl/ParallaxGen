#include <utility>

#include "patchers/PatcherShaderTransform.hpp"

PatcherShaderTransform::PatcherShaderTransform(std::filesystem::path NIFPath, nifly::NifFile *NIF,
                                               std::string PatcherName)
    : Patcher(std::move(NIFPath), NIF, std::move(PatcherName)) {}
