#pragma once

#include <wx/wx.h>

#include <string>
#include <vector>

class ParallaxGenUI {
public:
  static void init();
  static void updateUI();
  static auto promptForSelection(const std::string &Message, const std::vector<std::wstring> &Options) -> size_t;
};
