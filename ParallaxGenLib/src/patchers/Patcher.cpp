#include "patchers/Patcher.hpp"

using namespace std;

ParallaxGenDirectory *Patcher::PGD = nullptr;
ParallaxGenD3D *Patcher::PGD3D = nullptr;

auto Patcher::loadStatics(ParallaxGenDirectory &PGD, ParallaxGenD3D &PGD3D) -> void {
  Patcher::PGD = &PGD;
  Patcher::PGD3D = &PGD3D;
}

Patcher::Patcher(filesystem::path NIFPath, nifly::NifFile *NIF, string PatcherName)
    : NIFPath(std::move(NIFPath)), NIF(NIF), PatcherName(std::move(PatcherName)) {}

auto Patcher::getNIFPath() const -> filesystem::path { return NIFPath; }
auto Patcher::getNIF() const -> nifly::NifFile * { return NIF; }
auto Patcher::getPatcherName() const -> string { return PatcherName; }

auto Patcher::getPGD() -> ParallaxGenDirectory * { return PGD; }
auto Patcher::getPGD3D() -> ParallaxGenD3D * { return PGD3D; }
