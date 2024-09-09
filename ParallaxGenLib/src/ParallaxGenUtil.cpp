#include "ParallaxGenUtil.hpp"

#include <DirectXTex.h>
#include <spdlog/spdlog.h>

#include <boost/algorithm/string.hpp>
#include <fstream>
#include <iostream>

using namespace std;
namespace ParallaxGenUtil {

auto stringToWstring(const string &Str) -> wstring {
  size_t Length = Str.length() + 1; // Including null terminator
  std::vector<wchar_t> WBuffer(Length);
  size_t ConvertedChars = 0;

  errno_t Err = mbstowcs_s(&ConvertedChars, WBuffer.data(), WBuffer.size(), Str.c_str(), Length - 1);
  if (Err != 0) {
    throw std::runtime_error("Conversion failed");
  }

  return std::wstring(WBuffer.data());
}

auto stringVecToWstringVec(const vector<string> &StrVec) -> vector<wstring> {
  vector<wstring> WStrVec;
  WStrVec.reserve(StrVec.size());
  for (const auto &Str : StrVec) {
    WStrVec.push_back(stringToWstring(Str));
  }
  return WStrVec;
}

auto wstringToString(const wstring &Str) -> string {
  if (Str.empty()) {
    return {};
  }

  int SizeNeeded = WideCharToMultiByte(CP_UTF8, 0, Str.data(), (int)Str.size(), nullptr, 0, nullptr, nullptr);
  string StrTo(SizeNeeded, 0);
  WideCharToMultiByte(CP_UTF8, 0, Str.data(), (int)Str.size(), StrTo.data(), SizeNeeded, nullptr, nullptr);
  return StrTo;
}

auto getFileBytes(const filesystem::path &FilePath) -> vector<std::byte> {
  ifstream InputFile(FilePath, ios::binary | ios::ate);
  if (!InputFile.is_open()) {
    // Unable to open file
    return {};
  }

  auto Length = InputFile.tellg();
  if (Length == -1) {
    // Unable to find length
    InputFile.close();
    return {};
  }

  InputFile.seekg(0, ios::beg);

  // Make a buffer of the exact size of the file and read the data into it.
  vector<std::byte> Buffer(Length);
  InputFile.read(reinterpret_cast<char *>(Buffer.data()), Length); // NOLINT

  InputFile.close();

  return Buffer;
}

auto replaceLastOf(const filesystem::path &Path, const wstring &ToReplace,
                   const wstring &ReplaceWith) -> filesystem::path {
  wstring PathStr = Path.wstring();
  size_t Pos = PathStr.rfind(ToReplace);
  if (Pos == wstring::npos) {
    return Path;
  }

  PathStr.replace(Pos, ToReplace.size(), ReplaceWith);
  return PathStr;
}

auto replaceLastOf(const string &Path, const string &ToReplace, const string &ReplaceWith) -> string {
  size_t Pos = Path.rfind(ToReplace);
  if (Pos == wstring::npos) {
    return Path;
  }

  string OutPath = Path;
  OutPath.replace(Pos, ToReplace.size(), ReplaceWith);
  return OutPath;
}
} // namespace ParallaxGenUtil
