#include "ParallaxGenDirectory.hpp"

#include <DirectXTex.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <spdlog/spdlog.h>

#include "NIFUtil.hpp"
#include "ParallaxGenUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;

ParallaxGenDirectory::ParallaxGenDirectory(BethesdaGame BG) : BethesdaDirectory(BG, true) {}

auto ParallaxGenDirectory::getTruePBRConfigFilenameFields() -> vector<string> {
  static const vector<string> PGConfigFilenameFields = {"match_normal", "match_diffuse", "rename"};
  return PGConfigFilenameFields;
}

auto ParallaxGenDirectory::getConfigPath() -> wstring {
  static const wstring PGConfPath = L"parallaxgen";
  return PGConfPath;
}

auto ParallaxGenDirectory::getDefaultCubemapPath() -> filesystem::path {
  static const filesystem::path DefCubemapPath = "textures\\cubemaps\\dynamic1pxcubemap_black.dds";
  return DefCubemapPath;
}

void ParallaxGenDirectory::findHeightMaps(const vector<wstring> &Allowlist, const vector<wstring> &Blocklist,
                                          const vector<wstring> &ArchiveBlocklist) {
  // find height maps
  spdlog::info("Finding parallax height maps");
  // Find heightmaps
  HeightMaps = findFiles(true, Allowlist, Blocklist, ArchiveBlocklist, true, false);
  sort(HeightMaps.begin(), HeightMaps.end());
  spdlog::info("Found {} height maps", HeightMaps.size());
}

void ParallaxGenDirectory::findComplexMaterialMaps(const vector<wstring> &Allowlist, const vector<wstring> &Blocklist,
                                                   const vector<wstring> &ArchiveBlocklist) {
  spdlog::info("Finding complex material maps");
  // find complex material maps
  // TODO find a way to have downstream stuff edit this directly instead of replacement
  ComplexMaterialMaps = findFiles(true, Allowlist, Blocklist, ArchiveBlocklist, false, false);
  sort(ComplexMaterialMaps.begin(), ComplexMaterialMaps.end());
  // No conclusion because this is affected by D3D later
}

void ParallaxGenDirectory::findMeshes(const vector<wstring> &Allowlist, const vector<wstring> &Blocklist,
                                      const vector<wstring> &ArchiveBlocklist) {
  // find Meshes
  spdlog::info("Finding Meshes");
  Meshes = findFiles(true, Allowlist, Blocklist, ArchiveBlocklist, true, true);
  spdlog::info("Found {} Meshes", Meshes.size());
}

void ParallaxGenDirectory::findTruePBRConfigs(const vector<wstring> &Allowlist, const vector<wstring> &Blocklist,
                                              const vector<wstring> &ArchiveBlocklist) {
  // TODO more logging here
  // Find True PBR Configs
  spdlog::info("Finding TruePBR Configs");
  auto ConfigFiles = findFiles(true, Allowlist, Blocklist, ArchiveBlocklist, true, false);

  // loop through and parse Configs
  size_t ConfigOrder = 0;
  for (auto &Config : ConfigFiles) {
    // check if Config is valid
    auto ConfigFileBytes = getFile(Config);
    string ConfigFileStr(reinterpret_cast<const char *>(ConfigFileBytes.data()), ConfigFileBytes.size());

    try {
      nlohmann::json J = nlohmann::json::parse(ConfigFileStr);
      // loop through each Element
      for (auto &Element : J) {
        // Preprocessing steps here
        if (Element.contains("texture")) {
          Element["match_diffuse"] = Element["texture"];
        }

        Element["json"] = Config.string();

        // loop through filename Fields
        for (const auto &Field : getTruePBRConfigFilenameFields()) {
          if (Element.contains(Field) && !boost::istarts_with(Element[Field].get<string>(), "\\")) {
            Element[Field] = Element[Field].get<string>().insert(0, 1, '\\');
          }
        }

        TruePBRConfigs[ConfigOrder++] = Element;
      }
    } catch (nlohmann::json::parse_error &E) {
      spdlog::error(L"Unable to parse TruePBR Config file {}: {}", Config.wstring(), stringToWstring(E.what()));
      continue;
    }
  }

  spdlog::info("Found {} TruePBR entries", TruePBRConfigs.size());
}

void ParallaxGenDirectory::findPGConfigs() {
  // find PGConfigs
  spdlog::info("Finding ParallaxGen configs in load order");
  PGConfigs = findFiles(true, {getConfigPath() + L"\\**.json"});
  spdlog::info("Found {} ParallaxGen configs in load order", PGConfigs.size());
}

void ParallaxGenDirectory::buildBaseVectors(const vector<vector<string>> &Suffixes) {
  buildBaseVector(HeightMapsBases, HeightMaps, Suffixes[static_cast<int>(NIFUtil::TextureSlots::Parallax)]);
  buildBaseVector(ComplexMaterialMapsBases, ComplexMaterialMaps,
                  Suffixes[static_cast<int>(NIFUtil::TextureSlots::EnvMask)]);
}

void ParallaxGenDirectory::buildBaseVector(vector<string> &BaseVector, vector<filesystem::path> &Files,
                                           const vector<string> &Suffixes) {
  // Create base vector
  for (const auto &Elem : Files) {
    string ElemStr = Elem.string();

    auto ElemBase = NIFUtil::getTexBase(ElemStr, Suffixes);
    BaseVector.push_back(ElemBase);
  }
}

void ParallaxGenDirectory::addHeightMap(const filesystem::path &Path, const string &Base) {
  filesystem::path PathLower = getPathLower(Path);

  // add to vector
  HeightMaps.push_back(PathLower);
  sort(HeightMaps.begin(), HeightMaps.end());

  // add to base vector
  HeightMapsBases.push_back(Base);
  sort(HeightMapsBases.begin(), HeightMapsBases.end());
}

void ParallaxGenDirectory::addComplexMaterialMap(const filesystem::path &Path, const string &Base) {
  filesystem::path PathLower = getPathLower(Path);

  // add to vector
  ComplexMaterialMaps.push_back(PathLower);
  sort(ComplexMaterialMaps.begin(), ComplexMaterialMaps.end());

  // add to base vector
  ComplexMaterialMapsBases.push_back(Base);
  sort(ComplexMaterialMapsBases.begin(), ComplexMaterialMapsBases.end());
}

void ParallaxGenDirectory::addMesh(const filesystem::path &Path) {
  filesystem::path PathLower = getPathLower(Path);

  // add to vector
  addUniqueElement(Meshes, PathLower);
}

void ParallaxGenDirectory::setHeightMaps(const vector<filesystem::path> &Paths) { HeightMaps = Paths; }

void ParallaxGenDirectory::setComplexMaterialMaps(const vector<filesystem::path> &Paths) {
  ComplexMaterialMaps = Paths;
}

void ParallaxGenDirectory::setMeshes(const vector<filesystem::path> &Paths) { Meshes = Paths; }

auto ParallaxGenDirectory::isHeightMap(const filesystem::path &Path) const -> bool {
  return find(HeightMaps.begin(), HeightMaps.end(), getPathLower(Path)) != HeightMaps.end();
}

auto ParallaxGenDirectory::isComplexMaterialMap(const filesystem::path &Path) const -> bool {
  return find(ComplexMaterialMaps.begin(), ComplexMaterialMaps.end(), getPathLower(Path)) != ComplexMaterialMaps.end();
}

auto ParallaxGenDirectory::isMesh(const filesystem::path &Path) const -> bool {
  return find(Meshes.begin(), Meshes.end(), getPathLower(Path)) != Meshes.end();
}

auto ParallaxGenDirectory::defCubemapExists() -> bool { return isFile(getDefaultCubemapPath()); }

auto ParallaxGenDirectory::getHeightMaps() const -> const vector<filesystem::path> & { return HeightMaps; }
auto ParallaxGenDirectory::getHeightMapsBases() const -> const vector<string> & { return HeightMapsBases; }

auto ParallaxGenDirectory::getComplexMaterialMaps() const -> const vector<filesystem::path> & {
  return ComplexMaterialMaps;
}
auto ParallaxGenDirectory::getComplexMaterialMapsBases() const -> const vector<string> & {
  return ComplexMaterialMapsBases;
}

auto ParallaxGenDirectory::getMeshes() const -> const vector<filesystem::path> & { return Meshes; }

auto ParallaxGenDirectory::getTruePBRConfigs() const -> const map<size_t, nlohmann::json> & { return TruePBRConfigs; }

auto ParallaxGenDirectory::getPGConfigs() const -> const vector<filesystem::path> & { return PGConfigs; }
