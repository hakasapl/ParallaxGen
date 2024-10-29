#include "Patchers/PatcherShader.hpp"
#include "NIFUtil.hpp"
#include <string>

using namespace std;

// Static Members
ParallaxGenDirectory *PatcherShader::PGD;
ParallaxGenD3D *PatcherShader::PGD3D;

// Constructor
PatcherShader::PatcherShader(filesystem::path NIFPath, nifly::NifFile *NIF, string PatcherName,
                             const NIFUtil::ShapeShader &ShaderType)
    : NIFPath(std::move(NIFPath)), NIF(NIF), PatcherName(std::move(PatcherName)), ShaderType(ShaderType) {}

// Static Loader
auto PatcherShader::loadStatics(ParallaxGenDirectory &PGD, ParallaxGenD3D &PGD3D) -> void {
  PatcherShader::PGD = &PGD;
  PatcherShader::PGD3D = &PGD3D;
}

// Getters
auto PatcherShader::getNIFPath() const -> filesystem::path { return NIFPath; }
auto PatcherShader::getNIF() const -> nifly::NifFile * { return NIF; }
auto PatcherShader::getPatcherName() const -> string { return PatcherName; }
auto PatcherShader::getShaderType() const -> NIFUtil::ShapeShader { return ShaderType; }

// Helpers
auto PatcherShader::isSameAspectRatio(const filesystem::path &Texture1, const filesystem::path &Texture2) -> bool {
  bool SameAspect = false;
  PGD3D->checkIfAspectRatioMatches(Texture1, Texture2, SameAspect);

  return SameAspect;
}

auto PatcherShader::isPrefixOrFile(const filesystem::path &Path) -> bool { return PGD->isPrefix(Path); }

auto PatcherShader::isFile(const filesystem::path &File) -> bool { return PGD->isFile(File); }

auto PatcherShader::getFile(const filesystem::path &File) -> vector<std::byte> { return PGD->getFile(File); }

auto PatcherShader::getSlotLookupMap(const NIFUtil::TextureSlots &Slot)
    -> const map<wstring, unordered_set<NIFUtil::PGTexture, NIFUtil::PGTextureHasher>> & {
  return PGD->getTextureMapConst(Slot);
}
