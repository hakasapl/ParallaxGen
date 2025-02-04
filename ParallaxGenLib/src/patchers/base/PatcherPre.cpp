#include "patchers/base/PatcherMeshPre.hpp"

using namespace std;

PatcherMeshPre::PatcherMeshPre(std::filesystem::path nifPath, nifly::NifFile* nif, std::string patcherName)
    : PatcherMesh(std::move(nifPath), nif, std::move(patcherName))
{
}
