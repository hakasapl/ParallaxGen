#pragma once

#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <unordered_set>

namespace ParallaxGenUtil {

// narrow and wide string conversion functions
auto strToWstr(const std::string &Str) -> std::wstring;
auto wstrToStr(const std::wstring &Str) -> std::string;

// Get the file bytes of a file
auto getFileBytes(const std::filesystem::path &FilePath) -> std::vector<std::byte>;

// Template Functions
template <typename T> auto isInVector(const std::vector<T> &Vec, const T &Test) -> bool {
  return std::find(Vec.begin(), Vec.end(), Test) != Vec.end();
}

// concatenates two vectors without duplicates
template <typename T> void concatenateVectorsWithoutDuplicates(std::vector<T> &Vec1, const std::vector<T> &Vec2) {
  std::unordered_set<T> Set(Vec1.begin(), Vec1.end());

  for (const auto &Element : Vec2) {
    if (Set.find(Element) == Set.end()) {
      Vec1.push_back(Element);
      Set.insert(Element);
    }
  }
};

// adds an element to a vector if it is not already present
template <typename T> void addUniqueElement(std::vector<T> &Vec, const T &Element) {
  if (std::find(Vec.begin(), Vec.end(), Element) == Vec.end()) {
    Vec.push_back(Element);
  }
}

} // namespace ParallaxGenUtil
