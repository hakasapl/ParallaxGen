#include "ModManagerDirectory.hpp"
#include "BethesdaDirectory.hpp"
#include "ParallaxGenUtil.hpp"

#include <boost/crc.hpp>

#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

ModManagerDirectory::ModManagerDirectory(std::filesystem::path const &StagingDir) : StagingDir(StagingDir) {}

auto ModManagerDirectory::FindTextureFilesInDir() -> void {
  std::filesystem::recursive_directory_iterator ModIterator(StagingDir);
  for (auto &CurItem : ModIterator) {
    std::filesystem::path Path = CurItem.path();
    if (std::filesystem::is_regular_file(Path) &&
        (BethesdaDirectory::pathEqualityIgnoreCase(Path.extension(), std::filesystem::path{L".dds"}))) {

      // note: do not precompute crc32 since it is very expensive
      Textures.push_back({Path, std::filesystem::file_size(Path)});
    }
  }
}

auto ModManagerDirectory::FindModPath(std::filesystem::path const &TextureToCompare) const -> std::filesystem::path {
  return FindModPath({TextureToCompare, std::filesystem::file_size(TextureToCompare)});
}

auto ModManagerDirectory::FindModPath(TextureEntry const &TextureToCompare) const -> std::filesystem::path {
  for (auto const &Texture : Textures) {
    if (BethesdaDirectory::pathEqualityIgnoreCase(Texture.Path.filename(), TextureToCompare.Path.filename()) &&
        (Texture.FileSize == TextureToCompare.FileSize)) {

      boost::crc_32_type CRCTexture;
      boost::crc_32_type CRCTextureToCompare;

      auto TextureBytes = ParallaxGenUtil::getFileBytes(Texture.Path);
      auto TextureToCompareBytes = ParallaxGenUtil::getFileBytes(TextureToCompare.Path);

      CRCTexture.process_bytes(TextureBytes.data(), TextureBytes.size());
      CRCTextureToCompare.process_bytes(TextureToCompareBytes.data(), TextureToCompareBytes.size());
      if (CRCTexture.checksum() == CRCTextureToCompare.checksum())
        return Texture.Path;
    }
  }
  return {};
}

auto ModManagerDirectory::GetModFromTexture(std::filesystem::path TexturePath) -> std::wstring {

  std::filesystem::path ParentPath = TexturePath.parent_path();

  while (!BethesdaDirectory::pathEqualityIgnoreCase(ParentPath.filename(), {L"textures"})) {
    // no parent anymore
    if (!ParentPath.has_relative_path())
      break;

    ParentPath = ParentPath.parent_path();
  }

  return ParentPath.parent_path().filename().wstring();
}
