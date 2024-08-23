#pragma once

#include <boost/algorithm/string.hpp>
#include <bsa/tes4.hpp>
#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "BethesdaGame.hpp"

#define ASCII_UPPER_BOUND 127

class BethesdaDirectory {
private:
  /**
   * @struct BSAFile
   * @brief Stores data about an individual BSA file
   *
   * path stores the path to the BSA archive, preserving case from the original
   * path version stores the version of the BSA archive archive stores the BSA
   * archive object, which is where files can be accessed
   */
  struct BSAFile {
    std::filesystem::path Path;
    bsa::tes4::version Version;
    bsa::tes4::archive Archive;
  };

  /**
   * @struct BethesdaFile
   * @brief Structure which holds information about a specific file in the file
   * map
   *
   * path stores the path to the file, preserving case from the original path
   * bsa_file stores a shared pointer to a BSA file struct, or nullptr if the
   * file is a loose file
   */
  struct BethesdaFile {
    std::filesystem::path Path;
    std::shared_ptr<BSAFile> BSAFile;
  };

  // Class member variables
  std::filesystem::path DataDir;                         /**< Stores the path to the game data directory */
  std::map<std::filesystem::path, BethesdaFile> FileMap; /** < Stores the file map for every file found in the load
                                                            order. Key is a lowercase path, value is a BethesdaFile */
  bool Logging;                                          /** < Bool for whether logging is enabled or not */
  BethesdaGame BG;                                       /** < BethesdaGame which stores a BethesdaGame object
                                                            corresponding to this load order */

  /**
   * @brief Returns a vector of strings that represent the fields in the INI
   * file that store information about BSA file loading
   *
   * @return std::vector<std::string>
   */
  static auto getINIBSAFields() -> std::vector<std::string>;
  /**
   * @brief Gets a list of extensions to ignore when populating the file map
   *
   * @return std::vector<std::wstring>
   */
  static auto getExtensionBlocklist() -> std::vector<std::wstring>;

public:
  /**
   * @brief Construct a new Bethesda Directory object
   *
   * @param BG BethesdaGame object corresponding to load order
   * @param Logging Whether to enable CLI logging
   */
  BethesdaDirectory(BethesdaGame &BG, const bool &Logging);

  /**
   * @brief Populate file map with all files in the load order
   */
  void populateFileMap(bool IncludeBSAs = true);

  /**
   * @brief Get the file map vector
   *
   * @return std::map<std::filesystem::path, BethesdaFile>
   */
  [[nodiscard]] auto getFileMap() const -> std::map<std::filesystem::path, BethesdaFile>;

  /**
   * @brief Get the data directory path
   *
   * @return std::filesystem::path absolute path to the data directory
   */
  [[nodiscard]] auto getDataPath() const -> std::filesystem::path;

  /**
   * @brief Get bytes from a file in the load order
   *
   * @param RelPath path to the file relative to the data directory
   * @return std::vector<std::byte> vector of bytes of the file
   */
  [[nodiscard]] auto getFile(const std::filesystem::path &RelPath) const -> std::vector<std::byte>;

  /**
   * @brief Check if a file in the load order is a loose file
   *
   * @param RelPath path to the file relative to the data directory
   * @return true if file is a loose file
   * @return false if file is not a loose file or doesn't exist
   */
  [[nodiscard]] auto isLooseFile(const std::filesystem::path &RelPath) const -> bool;

  /**
   * @brief Check if a file in the load order is a BSA file
   *
   * @param RelPath path to the file relative to the data directory
   * @return true if file is a BSA file
   * @return false if file is not a BSA file or doesn't exist
   */
  [[nodiscard]] auto isBSAFile(const std::filesystem::path &RelPath) const -> bool;

  /**
   * @brief Check if a file exists in the load order
   *
   * @param RelPath path to the file relative to the data directory
   * @return true if file exists in the load order
   * @return false if file does not exist in the load order
   */
  [[nodiscard]] auto isFile(const std::filesystem::path &RelPath) const -> bool;

  /**
   * @brief Get the full path of a file in the load order
   *
   * @param RelPath path to the file relative to the data directory
   * @return std::filesystem::path absolute path to the file
   */
  [[nodiscard]] auto getFullPath(const std::filesystem::path &RelPath) const -> std::filesystem::path;

  /**
   * @brief Find files in the load order
   *
   * @param Lower If true, returns lowercase paths
   * @param GlobListAllow List of globs to allow, ignored if empty
   * @param GlobListDeny List of globs to deny, ignored if empty
   * @param ArchiveListDeny List of BSA files to ignore files from, ignored if
   * empty
   * @return std::vector<std::filesystem::path> vector of files that match the
   * criteria
   */
  [[nodiscard]] auto findFiles(const bool &Lower = false, const std::vector<std::wstring> &GlobListAllow = {},
                               const std::vector<std::wstring> &GlobListDeny = {},
                               const std::vector<std::wstring> &ArchiveListDeny = {}, const bool &LogFindings = false,
                               const bool &AllowWString = false) const -> std::vector<std::filesystem::path>;

  /**
   * @brief Get the load order of BSAs
   *
   * @return std::vector<std::wstring> Names of BSA files ordered by load order.
   * First element is loaded first.
   */
  [[nodiscard]] auto getBSALoadOrder() const -> std::vector<std::wstring>;

  /**
   * @brief Get the plugin load order
   *
   * @param TrimExtension If true, trims the .esp/.esm extension from the
   * plugin names
   * @return std::vector<std::wstring> Names of plugins ordered by load order.
   * First element is loaded first.
   */
  [[nodiscard]] auto getPluginLoadOrder(const bool &TrimExtension = false) const -> std::vector<std::wstring>;

  // Helpers

  /**
   * @brief Checks if fs::path object has only ascii characters
   *
   * @param Path Path to check
   * @return true When path only has ascii chars
   * @return false When path has other than ascii chars
   */
  [[nodiscard]] static auto isPathAscii(const std::filesystem::path &Path) -> bool;

  /**
   * @brief Get the lowercase path of a path
   *
   * @param Path Path to be made lowercase
   * @return std::filesystem::path lowercase path of the input
   */
  static auto getPathLower(const std::filesystem::path &Path) -> std::filesystem::path;

  /**
   * @brief Check if two paths are equal, ignoring case
   *
   * @param Path1 1st path to check
   * @param Path2 2nd path to check
   * @return true if paths equal ignoring case
   * @return false if paths don't equal ignoring case
   */
  static auto pathEqualityIgnoreCase(const std::filesystem::path &Path1, const std::filesystem::path &Path2) -> bool;

  /**
   * @brief Check if any component on a path is in a list of components
   *
   * @param Path path to check
   * @param Components components to check against
   * @return true if any component is in the path
   * @return false if no component is in the path
   */
  static auto checkIfAnyComponentIs(const std::filesystem::path &Path,
                                    const std::vector<std::wstring> &Components) -> bool;

  /**
   * @brief Check if any glob in list matches string
   *
   * @param Str String to check
   * @param GlobList Globs to check
   * @return true if any match
   * @return false if none match
   */
  static auto checkGlob(const std::wstring &Str, const std::vector<std::wstring> &GlobList) -> bool;

private:
  /**
   * @brief Looks through each BSA and adds files to the file map
   */
  void addBSAFilesToMap();

  /**
   * @brief Looks through all loose files in the load order and adds to the file
   * map
   */
  void addLooseFilesToMap();

  /**
   * @brief Add files in a BSA to the file map
   *
   * @param BSAName BSA name to read files from
   */
  void addBSAToFileMap(const std::wstring &BSAName);

  /**
   * @brief Check if a file being added to the file map should be added
   *
   * @param FilePath File being checked
   * @return true if file should be added
   * @return false if file should not be added
   */
  static auto isFileAllowed(const std::filesystem::path &FilePath) -> bool;

  /**
   * @brief Get BSA files defined in INI files
   *
   * @return std::vector<std::wstring> list of BSAs in the order the INI has
   * them
   */
  [[nodiscard]] auto getBSAFilesFromINIs() const -> std::vector<std::wstring>;

  /**
   * @brief Get BSA files in the data directory
   *
   * @return std::vector<std::wstring> list of BSAs in the data directory
   */
  [[nodiscard]] auto getBSAFilesInDirectory() const -> std::vector<std::wstring>;

  /**
   * @brief gets BSA files that are loaded with a plugin
   *
   * @param BSAFileList List of BSA files to check
   * @param PluginPrefix Plugin to check without extension
   * @return std::vector<std::wstring> list of BSAs loaded with plugin
   */
  [[nodiscard]] auto findBSAFilesFromPluginName(const std::vector<std::wstring> &BSAFileList,
                                                const std::wstring &PluginPrefix) const -> std::vector<std::wstring>;

  /**
   * @brief Get a file object from the file map
   *
   * @param FilePath Path to get the object for
   * @return BethesdaFile object of file in load order
   */
  [[nodiscard]] auto getFileFromMap(const std::filesystem::path &FilePath) const -> BethesdaFile;

  /**
   * @brief Update the file map with
   *
   * @param FilePath path to update or add
   * @param BSAFile BSA file or nullptr if it doesn't exist
   */
  void updateFileMap(const std::filesystem::path &FilePath, std::shared_ptr<BSAFile> BSAFile);

  /**
   * @brief Convert a list of wstrings to a LPCWSTRs
   *
   * @param Original original list of wstrings to convert
   * @return std::vector<LPCWSTR> list of LPCWSTRs
   */
  [[nodiscard]] static auto
  convertWStringToLPCWSTRVector(const std::vector<std::wstring> &Original) -> std::vector<LPCWSTR>;

  /**
   * @brief Checks whether a glob matches a provided list of globs
   *
   * @param Str String to check
   * @param WinningGlob Last glob that one (for performance)
   * @param GlobList List of globs to check
   * @return true Any glob is the list mated
   * @return false No globs in the list matched
   */
  static auto checkGlob(const LPCWSTR &Str, LPCWSTR &WinningGlob, const std::vector<LPCWSTR> &GlobList) -> bool;

  static auto readINIValue(const std::filesystem::path &INIPath, const std::wstring &Section, const std::wstring &Key,
                           const bool &Logging, const bool &FirstINIRead) -> std::wstring;
};
