#pragma once

#include <mutex>
#include <unordered_set>

#include "ParallaxGenConfig.hpp"
#include "ParallaxGenDirectory.hpp"

class ParallaxGenWarnings {
private:
  // Dependency objects
  static ParallaxGenDirectory *PGD; /** Pointer to initialized ParallaxGenDirectory object */
  static ParallaxGenConfig *PGC;    /** Pointer to initialized ParallaxGenConfig object */

  /**
   * @brief Struct that stores hash function for pair of wstrings (used for trackers)
   */
  struct PairHash {
    auto operator()(const std::pair<std::wstring, std::wstring> &P) const -> size_t {
      std::hash<std::wstring> HashWstr;
      return HashWstr(P.first) ^ (HashWstr(P.second) << 1);
    }
  };

  // Trackers for warnings
  static std::unordered_set<std::pair<std::wstring, std::wstring>, PairHash>
      MismatchWarnTracker;                    /** Keeps tabs on WARN mismatch messages to avoid duplicates */
  static std::mutex MismatchWarnTrackerMutex; /** Mutex for MismatchWarnTracker */
  static std::unordered_set<std::pair<std::wstring, std::wstring>, PairHash>
      MismatchWarnDebugTracker;                    /** Keeps tabs on DEBUG mismatch messages to avoid duplicates */
  static std::mutex MismatchWarnDebugTrackerMutex; /** Mutex for MismatchWarnDebugTracker */

  static std::unordered_set<std::pair<std::wstring, std::wstring>, PairHash>
      MeshWarnTracker;                    /** Keeps tabs on WARN mesh mismatch messages to avoid duplicates */
  static std::mutex MeshWarnTrackerMutex; /** Mutex for MeshWarnTracker */
  static std::unordered_set<std::pair<std::wstring, std::wstring>, PairHash>
      MeshWarnDebugTracker;                    /** Keeps tabs on DEBUG mesh mismatch messages to avoid duplicates */
  static std::mutex MeshWarnDebugTrackerMutex; /** Mutex for MeshWarnDebugTracker */

public:
  /**
   * @brief Loads required static objects and init trackers
   *
   * @param PGD pointer to initialized ParallaxGenDirectory instance
   * @param PGC pointer to initialized ParallaxGenConfig instance
   */
  static void init(ParallaxGenDirectory *PGD, ParallaxGenConfig *PGC);

  /**
   * @brief Posts warning about a mismatch between diffuse/normal and a matched path. Will not repost the same warning.
   *
   * @param MatchedPath Path that was matched (for example _m or _p file)
   * @param BaseTex diffuse/normal that it was matched from
   */
  static void mismatchWarn(const std::wstring &MatchedPath, const std::wstring &BaseTex);

  /**
   * @brief Posts warning about a mismatch between a mesh and a matched path. Will not repost the same warning.
   *
   * @param MatchedPath Path that was matched (for example _m or _p file)
   * @param NIFPath NIF that it was patched on
   */
  static void meshWarn(const std::wstring &MatchedPath, const std::wstring &NIFPath);
};
