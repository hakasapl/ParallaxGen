#include "patchers/Patcher.hpp"

using namespace std;

ParallaxGenDirectory* Patcher::s_pgd = nullptr;
ParallaxGenD3D* Patcher::s_pgd3d = nullptr;

auto Patcher::loadStatics(ParallaxGenDirectory& pgd, ParallaxGenD3D& pgd3d) -> void
{
    Patcher::s_pgd = &pgd;
    Patcher::s_pgd3d = &pgd3d;
}

Patcher::Patcher(filesystem::path nifPath, nifly::NifFile* nif, string patcherName)
    : m_nifPath(std::move(nifPath))
    , m_nif(nif)
    , m_patcherName(std::move(patcherName))
{
}

auto Patcher::getNIFPath() const -> filesystem::path { return m_nifPath; }
auto Patcher::getNIF() const -> nifly::NifFile* { return m_nif; }
auto Patcher::getPatcherName() const -> string { return m_patcherName; }

auto Patcher::getPGD() -> ParallaxGenDirectory* { return s_pgd; }
auto Patcher::getPGD3D() -> ParallaxGenD3D* { return s_pgd3d; }
