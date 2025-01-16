#pragma once

#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <unordered_set>

namespace ParallaxGenUtil {

// narrow and wide string conversion functions
auto UTF8toUTF16(const std::string &Str) -> std::wstring;
auto UTF16toUTF8(const std::wstring &Str) -> std::string;
auto Windows1252toUTF16(const std::string &Str) -> std::wstring;
auto UTF16toWindows1252(const std::wstring &Str) -> std::string;
auto ASCIItoUTF16(const std::string &Str) -> std::wstring;
auto UTF16toASCII(const std::wstring &Str) -> std::string;

auto utf8VectorToUTF16(const std::vector<std::string>& vec) -> std::vector<std::wstring>;
auto utf16VectorToUTF8(const std::vector<std::wstring>& vec) -> std::vector<std::string>;
auto windows1252VectorToUTF16(const std::vector<std::string>& vec) -> std::vector<std::wstring>;
auto utf16VectorToWindows1252(const std::vector<std::wstring>& vec) -> std::vector<std::string>;
auto asciiVectorToUTF16(const std::vector<std::string>& vec) -> std::vector<std::wstring>;
auto utf16VectorToASCII(const std::vector<std::wstring>& vec) -> std::vector<std::string>;

auto ContainsOnlyAscii(const std::string &Str) -> bool;
auto ContainsOnlyAscii(const std::wstring &Str) -> bool;

auto ToLowerASCII(const std::wstring &Str) -> std::wstring;

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

auto getThreadID() -> std::string;

} // namespace ParallaxGenUtil
