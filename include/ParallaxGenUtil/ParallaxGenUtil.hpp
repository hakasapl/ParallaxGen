#pragma once

#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <random>
#include <unordered_set>

namespace ParallaxGenUtil {
// get a single value from ini file
// TODO move this to BethesdaDirectory
std::wstring readINIValue(const std::filesystem::path &ini_path,
                          const std::wstring &section, const std::wstring &key,
                          const bool logging, const bool first_ini_read);

// terminals usually auto exit when program ends, this function waits for user
// input before exiting
void exitWithUserInput(const int exit_code);

// converts a string to a wstring
std::wstring convertToWstring(const std::string str);

// converts a wide string to a utf-8 narrow string
std::string wstring_to_utf8(const std::wstring &str);

// Get the file bytes of a file
std::vector<std::byte> getFileBytes(const std::filesystem::path &file_path);

// Replace last of helpers for string and path
std::filesystem::path replaceLastOf(const std::filesystem::path &path,
                                    const std::wstring &to_replace,
                                    const std::wstring &replace_with);
std::string replaceLastOf(const std::string &path,
                          const std::string &to_replace,
                          const std::string &replace_with);

template <typename T>
bool isInVector(const std::vector<T> &vec, const T &test) {
  return std::find(vec.begin(), vec.end(), test) != vec.end();
}

// concatenates two vectors without duplicates
template <typename T>
void concatenateVectorsWithoutDuplicates(std::vector<T> &vec1,
                                         const std::vector<T> &vec2) {
  std::unordered_set<T> set(vec1.begin(), vec1.end());

  for (const auto &element : vec2) {
    if (set.find(element) == set.end()) {
      vec1.push_back(element);
      set.insert(element);
    }
  }
};

// adds an element to a vector if it is not already present
template <typename T>
void addUniqueElement(std::vector<T> &vec, const T &element) {
  if (std::find(vec.begin(), vec.end(), element) == vec.end()) {
    vec.push_back(element);
  }
}
} // namespace ParallaxGenUtil
