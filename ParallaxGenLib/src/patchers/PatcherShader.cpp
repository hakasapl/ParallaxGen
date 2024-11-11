#include "Patchers/PatcherShader.hpp"

#include "NIFUtil.hpp"
#include "patchers/Patcher.hpp"
#include <string>
#include <utility>

using namespace std;

// Constructor
PatcherShader::PatcherShader(filesystem::path NIFPath, nifly::NifFile *NIF, string PatcherName)
    : Patcher(std::move(NIFPath), NIF, std::move(PatcherName)) {}
