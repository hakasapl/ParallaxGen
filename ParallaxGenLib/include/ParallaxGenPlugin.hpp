#pragma once

#include <vector>

#include "BethesdaGame.hpp"
#include "XEditLibCpp.hpp"

#define PLUGIN_LOADER_CHECK_INTERVAL 100

class ParallaxGenPlugin : public XEditLibCpp {
private:
  std::vector<std::wstring> ActivePlugins;

public:
  ParallaxGenPlugin(const BethesdaGame &Game);

  void init();
};
