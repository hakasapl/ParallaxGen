#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "BethesdaGame.hpp"
#include "ParallaxGenTask.hpp"
#include "XEditLibCpp.hpp"

#define PLUGIN_LOADER_CHECK_INTERVAL 100
#define HASH_GOLDENRATIO 0x9e3779b9
#define HASH_LEFTWISE 6
#define HASH_RIGHTWISE 2

#define FORMID_BASE 16

class ParallaxGenPlugin : public XEditLibCpp {
private:
  std::vector<std::wstring> ActivePlugins;

  // A helper to combine hash values
  template <typename T> static void hashCombine(std::size_t &Seed, const T &Value) {
    std::hash<T> Hasher;
    Seed ^= Hasher(Value) + HASH_GOLDENRATIO + (Seed << HASH_LEFTWISE) + (Seed >> HASH_RIGHTWISE);
  }

  struct TXSTRefID {
    std::wstring NIFPath;
    std::wstring Name3D;
    int Index3D;

    // Equality operator (needed for std::unordered_map)
    auto operator==(const TXSTRefID &Other) const -> bool {
      return NIFPath == Other.NIFPath && Name3D == Other.Name3D && Index3D == Other.Index3D;
    }
  };

  struct TXSTRefIDHash {
    auto operator()(const TXSTRefID &Object) const -> size_t {
      std::size_t Seed = 0;

      // Hash each element of the tuple and combine them
      hashCombine(Seed, Object.NIFPath);   // Hash the first wstring
      hashCombine(Seed, Object.Name3D); // Hash the second wstring
      hashCombine(Seed, Object.Index3D);   // Hash the int

      return Seed;
    }
  };

  std::unordered_map<TXSTRefID, std::unordered_set<unsigned int>, TXSTRefIDHash> TXSTRefsMap;

public:
  ParallaxGenPlugin(const BethesdaGame &Game);

  auto init() -> ParallaxGenTask::PGResult;

private:
  void addAlternateTexturesToMap(const unsigned int &Id, const std::wstring &MDLKey, const std::wstring &AltTXSTKey);
};
