#include "patchers/base/PatcherTexture.hpp"
#include <DirectXTex.h>

using namespace std;

PatcherTexture::PatcherTexture(filesystem::path ddsPath, DirectX::ScratchImage* dds, string patcherName)
    : Patcher(std::move(patcherName))
    , m_ddsPath(std::move(ddsPath))
    , m_dds(dds)
{
}

auto PatcherTexture::getDDSPath() const -> filesystem::path { return m_ddsPath; }
auto PatcherTexture::getDDS() const -> DirectX::ScratchImage* { return m_dds; }
