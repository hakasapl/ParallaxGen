#pragma once

#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <unordered_set>

namespace ParallaxGenUtil {
// terminals usually auto exit when program ends, this function waits for user
// input before exiting
void exitWithUserInput(const int &ExitCode = 0);

// converts a string to a wstring
auto stringToWstring(const std::string &Str) -> std::wstring;
auto stringVecToWstringVec(const std::vector<std::string> &StrVec) -> std::vector<std::wstring>;

// converts a wide string to a utf-8 narrow string
auto wstringToString(const std::wstring &Str) -> std::string;

// Get the file bytes of a file
auto getFileBytes(const std::filesystem::path &FilePath) -> std::vector<std::byte>;

// Replace last of helpers for string and path
auto replaceLastOf(const std::filesystem::path &Path, const std::wstring &ToReplace,
                   const std::wstring &ReplaceWith) -> std::filesystem::path;
auto replaceLastOf(const std::string &Path, const std::string &ToReplace,
                   const std::string &ReplaceWith) -> std::string;

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
