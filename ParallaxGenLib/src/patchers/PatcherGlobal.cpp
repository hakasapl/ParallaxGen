#include "patchers/PatcherGlobal.hpp"

using namespace std;

PatcherGlobal::PatcherGlobal(std::filesystem::path nifPath, nifly::NifFile* nif, std::string patcherName)
    : PatcherMesh(std::move(nifPath), nif, std::move(patcherName))
{
}
