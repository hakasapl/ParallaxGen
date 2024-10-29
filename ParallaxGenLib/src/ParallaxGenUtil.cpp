#include "ParallaxGenUtil.hpp"

#include <DirectXTex.h>
#include <spdlog/spdlog.h>

#include <boost/algorithm/string.hpp>
#include <fstream>
#include <iostream>
#include <wingdi.h>
#include <winnt.h>

using namespace std;
namespace ParallaxGenUtil {

auto strToWstr(const string &Str) -> wstring {
  // Just return empty string if empty
  if (Str.empty()) {
    return {};
  }

  // Convert string > wstring
  const int SizeNeeded = MultiByteToWideChar(CP_UTF8, 0, Str.c_str(), (int)Str.length(), nullptr, 0);
  std::wstring WStr(SizeNeeded, 0);
  MultiByteToWideChar(CP_UTF8, 0, Str.data(), (int)Str.length(), WStr.data(), SizeNeeded);

  return WStr;
}

auto wstrToStr(const wstring &WStr) -> string {
  // Just return empty string if empty
  if (WStr.empty()) {
    return {};
  }

  // Convert wstring > string
  const int SizeNeeded = WideCharToMultiByte(CP_UTF8, 0, WStr.data(), (int)WStr.size(), nullptr, 0, nullptr, nullptr);
  string Str(SizeNeeded, 0);
  WideCharToMultiByte(CP_UTF8, 0, WStr.data(), (int)WStr.size(), Str.data(), SizeNeeded, nullptr, nullptr);

  return Str;
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

auto getThreadID() -> string {
  // Get the current thread ID
    std::ostringstream OSS;
    OSS << std::this_thread::get_id();
    return OSS.str();
}

} // namespace ParallaxGenUtil
