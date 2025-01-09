#include "Logger.hpp"

#include <spdlog/spdlog.h>

#include "ParallaxGenUtil.hpp"

using namespace std;

// Static thread-local variable
thread_local vector<wstring> Logger::PrefixStack;

// Helper function to build the full prefix string
auto Logger::buildPrefixWString() -> wstring {
  wstringstream FullPrefix;
  for (const auto &Block : Logger::PrefixStack) {
    FullPrefix << L"[" << Block << L"] ";
  }
  return FullPrefix.str();
}

auto Logger::buildPrefixString() -> string {
  return ParallaxGenUtil::UTF16toUTF8(buildPrefixWString());
}

// ScopedPrefix class implementation
Logger::Prefix::Prefix(const wstring &Prefix) {
  // Add the new prefix block to the stack
  PrefixStack.push_back(Prefix);
}

Logger::Prefix::Prefix(const string &Prefix) {
  // Add the new prefix block to the stack
  PrefixStack.push_back(ParallaxGenUtil::UTF8toUTF16(Prefix));
}

Logger::Prefix::~Prefix() {
  // Remove the last prefix block
  if (!PrefixStack.empty()) {
    PrefixStack.pop_back();
  }
}
