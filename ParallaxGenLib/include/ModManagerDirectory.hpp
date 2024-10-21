#pragma once

#include <boost/crc.hpp>
#include <boost/functional/hash.hpp>

#include <filesystem>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

class ModManagerDirectory {
public:
  struct TextureEntry {
    std::filesystem::path Path{};
    uintmax_t FileSize{};

    auto operator==(const TextureEntry &Other) const -> bool {
      return Path == Other.Path && FileSize == Other.FileSize;
    }
  };

  struct TextureEntryHasher {
    auto operator()(const TextureEntry &Texture) const -> size_t {
      std::size_t Hash = std::hash<std::filesystem::path>()(Texture.Path);
      boost::hash_combine(Hash, Texture.FileSize);
      return Hash;
    }
  };

private:
  auto FindModPath(TextureEntry const &TextureToCompare) const -> std::filesystem::path;
  std::filesystem::path StagingDir;
  // vector is slightly faster than unordered_set, probably due to hash function of Path
  std::vector<TextureEntry> Textures;

public:
  ModManagerDirectory(std::filesystem::path const &StagingDir);

  /// @brief find all textures in the mod staging folder and store them internally for further processing
  auto FindTextureFilesInDir() -> void;

  /// @brief Find the texture in the mod directories
  /// @param TextureToCompare full path of the texture
  /// @return full path of the mod the texture in mod directories
  auto FindModPath(std::filesystem::path const &TextureToCompare) const -> std::filesystem::path;

  /// @brief deduct the mod name from the texture path
  /// @param full path of the texture
  /// @return mod name
  static auto GetModFromTexture(std::filesystem::path) -> std::wstring;
};
