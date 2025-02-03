#include "patchers/base/PatcherTextureGlobal.hpp"
#include <DirectXTex.h>

using namespace std;

PatcherTextureGlobal::PatcherTextureGlobal(
    std::filesystem::path texPath, DirectX::ScratchImage* tex, std::string patcherName)
    : PatcherTexture(std::move(texPath), tex, std::move(patcherName))
{
}
