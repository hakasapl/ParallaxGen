#include "patchers/base/Patcher.hpp"

using namespace std;

ParallaxGenDirectory* Patcher::s_pgd = nullptr;
ParallaxGenD3D* Patcher::s_pgd3d = nullptr;

auto Patcher::loadStatics(ParallaxGenDirectory& pgd, ParallaxGenD3D& pgd3d) -> void
{
    Patcher::s_pgd = &pgd;
    Patcher::s_pgd3d = &pgd3d;
}

Patcher::Patcher(string patcherName)
    : m_patcherName(std::move(patcherName))
{
}

auto Patcher::getPatcherName() const -> string { return m_patcherName; }

auto Patcher::getPGD() -> ParallaxGenDirectory* { return s_pgd; }
auto Patcher::getPGD3D() -> ParallaxGenD3D* { return s_pgd3d; }
