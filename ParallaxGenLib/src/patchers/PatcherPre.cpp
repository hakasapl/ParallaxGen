#include "patchers/PatcherPre.hpp"

using namespace std;

PatcherPre::PatcherPre(std::filesystem::path nifPath, nifly::NifFile* nif, std::string patcherName)
    : PatcherMesh(std::move(nifPath), nif, std::move(patcherName))
{
}
