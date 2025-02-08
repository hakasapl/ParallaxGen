#include "patchers/base/PatcherMeshGlobal.hpp"

using namespace std;

PatcherMeshGlobal::PatcherMeshGlobal(
    std::filesystem::path nifPath, nifly::NifFile* nif, std::string patcherName, const bool& triggerSave)
    : PatcherMesh(std::move(nifPath), nif, std::move(patcherName), triggerSave)
{
}
