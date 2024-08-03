#include "ParallaxGenDirectory.hpp"

#include <DirectXTex.h>
#include <boost/algorithm/string.hpp>

#include <spdlog/spdlog.h>

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
  HeightMaps = findFiles(true, Allowlist, Blocklist, ArchiveBlocklist);
  spdlog::info("Found {} height maps", HeightMaps.size());
}

void ParallaxGenDirectory::findComplexMaterialMaps(const vector<wstring> &Allowlist, const vector<wstring> &Blocklist,
                                                   const vector<wstring> &ArchiveBlocklist) {
  spdlog::info("Finding complex material maps");
  // find complex material maps
  vector<filesystem::path> EnvMaps = findFiles(true, Allowlist, Blocklist, ArchiveBlocklist);

  // loop through env maps
  for (const auto &EnvMap : EnvMaps) {
    // check if env map is actually a complex material map
    const vector<std::byte> EnvMapData = getFile(EnvMap);

    // load image into directxtex
    DirectX::ScratchImage Image;
    HRESULT HR =
        DirectX::LoadFromDDSMemory(EnvMapData.data(), EnvMapData.size(), DirectX::DDS_FLAGS_NONE, nullptr, Image);
    if (FAILED(HR)) {
      spdlog::warn(L"Failed to load DDS from memory: {} - skipping", EnvMap.wstring());
      continue;
    }

    // check if image is a complex material map
    if (!Image.IsAlphaAllOpaque()) {
      // if alpha channel is used, there is parallax data
      // this won't work on complex matterial maps that don't make use of
      // complex parallax I'm not sure there's a way to check for those other
      // cases
      spdlog::trace(L"Adding {} as a complex material map", EnvMap.wstring());
      ComplexMaterialMaps.push_back(EnvMap);
    }
  }

  spdlog::info("Found {} complex material maps", ComplexMaterialMaps.size());
}

void ParallaxGenDirectory::findMeshes(const vector<wstring> &Allowlist, const vector<wstring> &Blocklist,
                                      const vector<wstring> &ArchiveBlocklist) {
  // find Meshes
  spdlog::info("Finding Meshes");
  Meshes = findFiles(true, Allowlist, Blocklist, ArchiveBlocklist);
  spdlog::info("Found {} Meshes", Meshes.size());
}

void ParallaxGenDirectory::findTruePBRConfigs(const vector<wstring> &Allowlist, const vector<wstring> &Blocklist,
                                              const vector<wstring> &ArchiveBlocklist) {
  // TODO more logging here
  // Find True PBR Configs
  spdlog::info("Finding TruePBR Configs");
  auto ConfigFiles = findFiles(true, Allowlist, Blocklist, ArchiveBlocklist);

  // loop through and parse Configs
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

        // loop through filename Fields
        for (const auto &Field : getTruePBRConfigFilenameFields()) {
          if (Element.contains(Field) && !boost::istarts_with(Element[Field].get<string>(), "\\")) {
            Element[Field] = Element[Field].get<string>().insert(0, 1, '\\');
          }
        }

        TruePBRConfigs.push_back(Element);
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

void ParallaxGenDirectory::addHeightMap(const filesystem::path &Path) {
  filesystem::path PathLower = getPathLower(Path);

  // add to vector
  addUniqueElement(HeightMaps, PathLower);
}

void ParallaxGenDirectory::addComplexMaterialMap(const filesystem::path &Path) {
  filesystem::path PathLower = getPathLower(Path);

  // add to vector
  addUniqueElement(ComplexMaterialMaps, PathLower);
}

void ParallaxGenDirectory::addMesh(const filesystem::path &Path) {
  filesystem::path PathLower = getPathLower(Path);

  // add to vector
  addUniqueElement(Meshes, PathLower);
}

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

auto ParallaxGenDirectory::getHeightMaps() const -> vector<filesystem::path> { return HeightMaps; }

auto ParallaxGenDirectory::getComplexMaterialMaps() const -> vector<filesystem::path> { return ComplexMaterialMaps; }

auto ParallaxGenDirectory::getMeshes() const -> vector<filesystem::path> { return Meshes; }

auto ParallaxGenDirectory::getTruePBRConfigs() const -> vector<nlohmann::json> { return TruePBRConfigs; }

auto ParallaxGenDirectory::getPGConfigs() const -> vector<filesystem::path> { return PGConfigs; }

auto ParallaxGenDirectory::getHeightMapFromBase(const string &Base) const -> string {
  return matchBase(Base, HeightMaps).string();
}

auto ParallaxGenDirectory::getComplexMaterialMapFromBase(const string &Base) const -> string {
  return matchBase(Base, ComplexMaterialMaps).string();
}

auto ParallaxGenDirectory::matchBase(const string &Base,
                                     const vector<filesystem::path> &SearchList) -> filesystem::path {
  for (const auto &Search : SearchList) {
    auto SearchStr = Search.wstring();
    if (boost::istarts_with(SearchStr, Base)) {
      size_t Pos = SearchStr.find_last_of(L'_');
      auto MapBase = SearchStr.substr(0, Pos);
      if (Pos != wstring::npos && boost::iequals(MapBase, Base)) {
        return Search;
      }
    }
  }

  return {};
}

auto ParallaxGenDirectory::getBase(const filesystem::path &Path) -> string {
  auto FileStem = Path.stem().string();
  size_t Pos = FileStem.find_last_of('_');
  if (Pos == string::npos) {
    return (Path.parent_path() / Path.stem()).string();
  }

  return (Path.parent_path() / FileStem.substr(0, Pos)).string();
}
