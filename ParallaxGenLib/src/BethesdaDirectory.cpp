#include "BethesdaDirectory.hpp"

#include "BethesdaGame.hpp"
#include "ModManagerDirectory.hpp"
#include "ParallaxGenUtil.hpp"

#include <bsa/tes4.hpp>

#include <spdlog/spdlog.h>

#include <binary_io/memory_stream.hpp>
#include <binary_io/any_stream.hpp>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/crc.hpp>

#include <shlwapi.h>
#include <winnt.h>

#include <algorithm>
#include <exception>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_set>
#include <utility>

#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>



using namespace std;
using namespace ParallaxGenUtil;

BethesdaDirectory::BethesdaDirectory(BethesdaGame &BG, filesystem::path GeneratedPath, ModManagerDirectory *MMD, const bool &Logging) : GeneratedDir(std::move(GeneratedPath)), Logging(Logging), BG(BG), MMD(MMD) {
  // Assign instance vars
  DataDir = filesystem::path(this->BG.getGameDataPath());

  if (this->Logging) {
    // Log starting message
    spdlog::info(L"Opening Data Folder \"{}\"", DataDir.wstring());
  }
}

//
// Constant Definitions
//
auto BethesdaDirectory::getINIBSAFields() -> vector<string> {
  // these fields will be searched in ini files for manually specified BSA
  // loading
  const static vector<string> INIBSAFields = {"sResourceArchiveList", "sResourceArchiveList2", "sResourceArchiveListBeta"};

  return INIBSAFields;
}

auto BethesdaDirectory::getExtensionBlocklist() -> vector<wstring> {
  // any file that ends with these strings will be ignored
  // allowed BSAs etc. to be hidden from the file map since this object is an
  // abstraction of the data directory that no longer factors BSAs for
  // downstream users
  const static vector<wstring> ExtensionBlocklist = {L".bsa", L".esp", L".esl", L".esm"};

  return ExtensionBlocklist;
}

auto BethesdaDirectory::checkGlob(const wstring &Str, const vector<wstring> &GlobList) -> bool {
  // convert wstring vector to LPCWSTR vector
  vector<LPCWSTR> GlobListCstr = convertWStringToLPCWSTRVector(GlobList);

  // convert wstring to LPCWSTR
  LPCWSTR StrCstr = Str.c_str();

  // check if string matches any glob
  return std::ranges::any_of(GlobListCstr, [&](LPCWSTR Glob) { return PathMatchSpecW(StrCstr, Glob); });
}

void BethesdaDirectory::populateFileMap(bool IncludeBSAs) {
  // clear map before populating
  FileMap.clear();

  if (IncludeBSAs) {
    // add BSA files to file map
    addBSAFilesToMap();
  }

  // add loose files to file map
  addLooseFilesToMap();
}

auto BethesdaDirectory::getFileMap() const -> const map<filesystem::path, BethesdaDirectory::BethesdaFile>& { return FileMap; }

auto BethesdaDirectory::getFile(const filesystem::path &RelPath, const bool &CacheFile) -> vector<std::byte> {
  // find bsa/loose file to open
  const BethesdaFile File = getFileFromMap(RelPath);
  if (File.Path.empty()) {
    if (Logging) {
      spdlog::error(L"File not found in file map: {}", RelPath.wstring());
    }
    throw runtime_error("File not found in file map");
  }

  auto LowerRelPath = getPathLower(RelPath);
  if (!CacheFile) {
    const lock_guard<mutex> Lock(FileCacheMutex);

    if (FileCache.find(LowerRelPath) != FileCache.end()) {
      if (Logging) {
        spdlog::trace(L"Reading file from cache: {}", RelPath.wstring());
      }

      return FileCache[LowerRelPath];
    }
  }

  vector<std::byte> OutFileBytes;
  const shared_ptr<BSAFile> BSAStruct = File.BSAFile;
  if (BSAStruct == nullptr) {
    if (Logging) {
      spdlog::trace(L"Reading loose file from BethesdaDirectory: {}", RelPath.wstring());
    }

    filesystem::path FilePath;
    if (File.Generated) {
      FilePath = GeneratedDir / RelPath;
    } else {
      FilePath = DataDir / RelPath;
    }

    OutFileBytes = getFileBytes(FilePath);
  } else {
    const filesystem::path BSAPath = BSAStruct->Path;

    if (Logging) {
      spdlog::trace(L"Reading BSA file from {}: {}", BSAPath.wstring(), RelPath.wstring());
    }

    // this is a bsa archive file
    const bsa::tes4::version BSAVersion = BSAStruct->Version;
    const bsa::tes4::archive BSAObj = BSAStruct->Archive;

    string ParentPath = wstrToStr(RelPath.parent_path().wstring());
    string Filename = wstrToStr(RelPath.filename().wstring());

    const auto File = BSAObj[ParentPath][Filename];
    if (File) {
      binary_io::any_ostream AOS{std::in_place_type<binary_io::memory_ostream>};
      // read file from output stream
      try {
        File->write(AOS, BSAVersion);
      } catch (const std::exception &E) {
        if (Logging) {
          spdlog::error(L"Failed to read file {}: {}", RelPath.wstring(), strToWstr(E.what()));
        }
      }

      auto &S = AOS.get<binary_io::memory_ostream>();
      OutFileBytes = S.rdbuf();
    } else {
      if (Logging) {
        spdlog::error(L"File not found in BSA archive: {}", RelPath.wstring());
      }
      throw runtime_error("File not found in BSA archive");
    }
  }

  if (OutFileBytes.empty()) {
    return {};
  }

  // cache file if flag is set
  if (CacheFile) {
    const lock_guard<mutex> Lock(FileCacheMutex);
    FileCache[LowerRelPath] = OutFileBytes;
  }

  return OutFileBytes;
}

auto BethesdaDirectory::getMod(const filesystem::path &RelPath) -> wstring {
  if (FileMap.empty()) {
    throw runtime_error("File map was not populated");
  }

  const BethesdaFile File = getFileFromMap(RelPath);
  return File.Mod;
}

void BethesdaDirectory::addGeneratedFile(const filesystem::path &RelPath, const wstring &Mod) {
  updateFileMap(RelPath, nullptr, Mod, true);
}

auto BethesdaDirectory::clearCache() -> void {
  const lock_guard<mutex> Lock(FileCacheMutex);
  FileCache.clear();
}

auto BethesdaDirectory::isLooseFile(const filesystem::path &RelPath) const -> bool {
  if (FileMap.empty()) {
    throw runtime_error("File map was not populated");
  }
  const BethesdaFile File = getFileFromMap(RelPath);
  return !File.Path.empty() && File.BSAFile == nullptr;
}

auto BethesdaDirectory::isBSAFile(const filesystem::path &RelPath) const -> bool {
  if (FileMap.empty()) {
    throw runtime_error("File map was not populated");
  }

  const BethesdaFile File = getFileFromMap(RelPath);
  return !File.Path.empty() && File.BSAFile != nullptr;
}

auto BethesdaDirectory::isFile(const filesystem::path &RelPath) const -> bool {
  if (FileMap.empty()) {
    throw runtime_error("File map was not populated");
  }

  const BethesdaFile File = getFileFromMap(RelPath);
  return !File.Path.empty();
}

auto BethesdaDirectory::isGenerated(const filesystem::path &RelPath) const -> bool {
  if (FileMap.empty()) {
    throw runtime_error("File map was not populated");
  }

  const BethesdaFile File = getFileFromMap(RelPath);
  return !File.Path.empty() && File.Generated;
}

auto BethesdaDirectory::isPrefix(const filesystem::path &RelPath) const -> bool {
  if (FileMap.empty()) {
    throw runtime_error("File map was not populated");
  }

  auto It = FileMap.lower_bound(RelPath);
  if (It == FileMap.end()) {
    return false;
  }

  return boost::istarts_with(It->first.wstring(), RelPath.wstring()) ||
         (It != FileMap.begin() && boost::istarts_with(prev(It)->first.wstring(), RelPath.wstring()));
}

auto BethesdaDirectory::getLooseFileFullPath(const filesystem::path &RelPath) const -> filesystem::path {
  if (FileMap.empty()) {
    throw runtime_error("File map was not populated");
  }

  const BethesdaFile File = getFileFromMap(RelPath);

  if (File.Generated) {
    return GeneratedDir / RelPath;
  }
  
  return DataDir / RelPath;
}

auto BethesdaDirectory::getDataPath() const -> filesystem::path { return DataDir; }

auto BethesdaDirectory::findFiles(const bool &Lower, const vector<wstring> &GlobListAllow,
                                  const vector<wstring> &GlobListDeny, const vector<wstring> &ArchiveListDeny,
                                  const bool &LogFindings, const bool &AllowWString) const -> vector<filesystem::path> {
  if (FileMap.empty()) {
    throw runtime_error("File map was not populated");
  }

  // find all keys in FileMap that match pattern
  vector<filesystem::path> FoundFiles;

  // Create LPCWSTR vectors from wstrings
  const vector<LPCWSTR> GlobListAllowCstr = convertWStringToLPCWSTRVector(GlobListAllow);
  const vector<LPCWSTR> GlobListDenyCstr = convertWStringToLPCWSTRVector(GlobListDeny);
  const vector<LPCWSTR> ArchiveListDenyCstr = convertWStringToLPCWSTRVector(ArchiveListDeny);

  LPCWSTR LastWinningGlobAllow = L"";
  LPCWSTR LastWinningGlobDeny = L"";
  LPCWSTR LastWinningGlobArchiveDeny = L"";

  // loop through filemap and match keys
  for (const auto &[key, value] : FileMap) {
    const filesystem::path CurFilePath = value.Path;

    // Check globs
    const wstring KeyWStr = key.wstring();
    LPCWSTR KeyCstr = KeyWStr.c_str();

    // Check allowlist
    if (!GlobListAllowCstr.empty() && !checkGlob(KeyCstr, LastWinningGlobAllow, GlobListAllowCstr)) {
      continue;
    }

    // Check denylist
    if (!GlobListDenyCstr.empty() && checkGlob(KeyCstr, LastWinningGlobDeny, GlobListDenyCstr)) {
      continue;
    }

    // Verify BSA blocklist
    if (value.BSAFile != nullptr) {
      // Get BSA file
      const wstring BSAFileWstr = value.BSAFile->Path.filename().wstring();
      LPCWSTR BSAFile = BSAFileWstr.c_str();

      if (checkGlob(BSAFile, LastWinningGlobArchiveDeny, ArchiveListDenyCstr)) {
        continue;
      }
    }

    // Check encoding
    if (!AllowWString && !isPathAscii(key)) {
      if (Logging) {
        spdlog::warn(L"Skipping file with non-ASCII characters: {}", key.wstring());
      }
      continue;
    }

    // If not allowed, skip
    if (Logging) {
      spdlog::trace(L"Matched file by glob: {}", CurFilePath.wstring());
    }

    if (LogFindings) {
      spdlog::debug(L"Found File: {}", CurFilePath.wstring());
    }

    if (Lower) {
      FoundFiles.push_back(key);
    } else {
      FoundFiles.push_back(CurFilePath);
    }
  }

  return FoundFiles;
}

void BethesdaDirectory::addBSAFilesToMap() {
  if (Logging) {
    spdlog::info("Adding BSA files to file map.");
  }

  // Get list of BSA files
  const vector<wstring> BSAFiles = getBSALoadOrder();

  // Loop through each BSA file
  for (const auto &BSAName : BSAFiles) {
    // add bsa to file map
    try {
      addBSAToFileMap(BSAName);
    } catch (const std::exception &E) {
      if (Logging) {
        spdlog::error(L"Failed to add BSA file {} to map (Skipping): {}", BSAName, strToWstr(E.what()));
      }
      continue;
    }
  }
}

void BethesdaDirectory::addLooseFilesToMap() {
  if (Logging) {
    spdlog::info("Adding loose files to file map.");
  }

  for (const auto &Entry :
       filesystem::recursive_directory_iterator(DataDir, filesystem::directory_options::skip_permission_denied)) {
    try {
      if (Entry.is_regular_file()) {
        const filesystem::path &FilePath = Entry.path();
        const filesystem::path RelativePath = FilePath.lexically_relative(DataDir);

        // check type of file, skip BSAs and ESPs
        if (!isFileAllowed(FilePath)) {
          continue;
        }

        if (Logging) {
          spdlog::trace(L"Adding loose file to map: {}", RelativePath.wstring());
        }

        auto CurMod = MMD->getMod(RelativePath);
        updateFileMap(RelativePath, nullptr, CurMod);
      }
    } catch (const std::exception &E) {
      if (Logging) {
        spdlog::error(L"Failed to load file from iterator (Skipping): {}", strToWstr(E.what()));
      }
      continue;
    }
  }
}

void BethesdaDirectory::addBSAToFileMap(const wstring &BSAName) {
  if (Logging) {
    // log message
    spdlog::debug(L"Adding files from {} to file map.", BSAName);
  }

  bsa::tes4::archive BSAObj;
  const filesystem::path BSAPath = DataDir / BSAName;

  // skip BSA if it doesn't exist (can happen if it's in the ini but not in the
  // data folder)
  if (!filesystem::exists(BSAPath)) {
    if (Logging) {
      spdlog::warn(L"Skipping BSA {} because it doesn't exist", BSAPath.wstring());
    }
    return;
  }

  const bsa::tes4::version BSAVersion = BSAObj.read(BSAPath);
  const BSAFile BSAStruct = {BSAPath, BSAVersion, BSAObj};

  const shared_ptr<BSAFile> BSAStructPtr = make_shared<BSAFile>(BSAStruct);

  // loop iterator
  for (auto &FileEntry : BSAObj) {
    // get file entry from pointer
    try {
      // get folder name within the BSA vfs
      const filesystem::path FolderName = FileEntry.first.name();

      // .second stores the files in the folder
      const auto FileName = FileEntry.second;

      // loop through files in folder
      for (const auto &Entry : FileName) {
        // get name of file
        const string_view CurEntry = Entry.first.name();
        const filesystem::path CurPath = FolderName / CurEntry;

        // chekc if we should ignore this file
        if (!isFileAllowed(CurPath)) {
          continue;
        }

        if (Logging) {
          spdlog::trace(L"Adding file from BSA {} to file map: {}", BSAName, CurPath.wstring());
        }

        // add to filemap
        auto BSAMod = MMD->getMod(BSAName);
        updateFileMap(CurPath, BSAStructPtr, BSAMod);
      }
    } catch (const std::exception &E) {
      if (Logging) {
        spdlog::error(L"Failed to get file pointer from BSA, skipping {}: {}", BSAName, strToWstr(E.what()));
      }
      continue;
    }
  }
}

auto BethesdaDirectory::getBSALoadOrder() const -> vector<wstring> {
  // get bsa files not loaded from esp (also initializes output vector)
  vector<wstring> OutBSAOrder = getBSAFilesFromINIs();

  // get esp priority list
  const vector<wstring> LoadOrder = BG.getActivePlugins(true);

  // list BSA files in data directory
  const vector<wstring> AllBSAFiles = getBSAFilesInDirectory();

  // loop through each esp in the priority list
  for (const auto &Plugin : LoadOrder) {
    // add any BSAs to list
    const vector<wstring> CurFoundBSAs = findBSAFilesFromPluginName(AllBSAFiles, Plugin);
    concatenateVectorsWithoutDuplicates(OutBSAOrder, CurFoundBSAs);
  }

  if (Logging) {
    // log output
    wstring BSAListStr = boost::algorithm::join(OutBSAOrder, ",");
    spdlog::trace(L"BSA Load Order: {}", BSAListStr);

    for (const auto &BSA : AllBSAFiles) {
      if (!isInVector(OutBSAOrder, BSA)) {
        spdlog::warn(L"BSA file {} not loaded by any active plugin or INI.", BSA);
      }
    }
  }

  return OutBSAOrder;
}

auto BethesdaDirectory::getPathLower(const filesystem::path &Path) -> filesystem::path {
  return {boost::to_lower_copy(Path.wstring())};
}

auto BethesdaDirectory::pathEqualityIgnoreCase(const filesystem::path &Path1, const filesystem::path &Path2) -> bool {
  return getPathLower(Path1) == getPathLower(Path2);
}

auto BethesdaDirectory::getBSAFilesFromINIs() const -> vector<wstring> {
  // output vector
  vector<wstring> BSAFiles;

  if (Logging) {
    spdlog::debug("Reading manually loaded BSAs from INI files.");
  }

  // find ini paths
  const BethesdaGame::ININame INILocs = BG.getINIPaths();

  const vector<filesystem::path> INIFileOrder = {INILocs.INI, INILocs.INICustom};

  // loop through each field
  bool FirstINIRead = true;
  for (const auto &Field : getINIBSAFields()) {
    // loop through each ini file
    wstring INIVal;
    for (const auto &INIPath : INIFileOrder) {
      wstring CurVal = readINIValue(INIPath, L"Archive", strToWstr(Field), Logging, FirstINIRead);

      if (Logging) {
        spdlog::trace(L"Found ini key pair from INI {}: {}: {}", INIPath.wstring(), strToWstr(Field), CurVal);
      }

      if (CurVal.empty()) {
        continue;
      }

      INIVal = CurVal;
    }

    FirstINIRead = false;

    if (INIVal.empty()) {
      continue;
    }

    if (Logging) {
      spdlog::trace(L"Found BSA files from INI field {}: {}", strToWstr(Field), INIVal);
    }

    // split into components
    vector<wstring> INIComponents;
    boost::split(INIComponents, INIVal, boost::is_any_of(","));
    for (auto &BSA : INIComponents) {
      // remove leading/trailing whitespace
      boost::trim(BSA);

      // add to output
      addUniqueElement(BSAFiles, BSA);
    }
  }

  return BSAFiles;
}

auto BethesdaDirectory::getBSAFilesInDirectory() const -> vector<wstring> {
  if (Logging) {
    spdlog::debug("Finding existing BSA files in data directory.");
  }

  vector<wstring> BSAFiles;

  for (const auto &Entry : filesystem::directory_iterator(this->DataDir)) {
    if (Entry.is_regular_file()) {
      const auto FileExtension = Entry.path().extension().string();
      // only interested in BSA files
      if (!boost::iequals(FileExtension, ".bsa")) {
        continue;
      }

      // add to output
      BSAFiles.push_back(Entry.path().filename().wstring());
    }
  }

  return BSAFiles;
}

auto BethesdaDirectory::findBSAFilesFromPluginName(const vector<wstring> &BSAFileList,
                                                   const wstring &PluginPrefix) const -> vector<wstring> {
  if (Logging) {
    spdlog::trace(L"Finding BSA files that correspond to plugin {}", PluginPrefix);
  }

  vector<wstring> BSAFilesFound;
  const wstring PluginPrefixLower = boost::to_lower_copy(PluginPrefix);

  for (wstring BSA : BSAFileList) {
    const wstring BSALower = boost::to_lower_copy(BSA);
    if (BSALower.starts_with(PluginPrefixLower)) {
      if (BSALower == PluginPrefixLower + L".bsa") {
        // load bsa with the plugin name before any others
        BSAFilesFound.insert(BSAFilesFound.begin(), BSA);
        continue;
      }

      // skip any BSAs that may start with the prefix but belong to a different
      // plugin
      wstring AfterPrefix = BSA.substr(PluginPrefix.length());

      // todo: Is this actually how the game handles BSA files? Example:
      // 3DNPC0.bsa, 3DNPC1.bsa, 3DNPC2.bsa are loaded, todo: but 3DNPC -
      // Textures.bsa is also loaded, whats the logic there?
      if (AfterPrefix.starts_with(L" ") && !AfterPrefix.starts_with(L" -")) {
        continue;
      }

      if (!AfterPrefix.starts_with(L" ") && (isdigit(AfterPrefix[0]) == 0)) {
        continue;
      }

      if (Logging) {
        spdlog::trace(L"Found BSA file that corresponds to plugin {}: {}", PluginPrefix, BSA);
      }

      BSAFilesFound.push_back(BSA);
    }
  }

  return BSAFilesFound;
}

auto BethesdaDirectory::isFileAllowed(const filesystem::path &FilePath) -> bool {
  wstring FileExtension = FilePath.extension().wstring();
  boost::algorithm::to_lower(FileExtension);

  return !(isInVector(getExtensionBlocklist(), FileExtension));
}

// helpers

auto BethesdaDirectory::isPathAscii(const filesystem::path &Path) -> bool {
  return ranges::all_of(Path.wstring(), [](wchar_t WC) { return WC <= ASCII_UPPER_BOUND; });
}

auto BethesdaDirectory::getFileFromMap(const filesystem::path &FilePath) const -> BethesdaDirectory::BethesdaFile {
  const filesystem::path LowerPath = getPathLower(FilePath);

  if (FileMap.find(LowerPath) == FileMap.end()) {
    return BethesdaFile{filesystem::path(), nullptr};
  }

  return FileMap.at(LowerPath);
}

void BethesdaDirectory::updateFileMap(const filesystem::path &FilePath,
                                      shared_ptr<BethesdaDirectory::BSAFile> BSAFile, const wstring &Mod, const bool &Generated) {
  const filesystem::path LowerPath = getPathLower(FilePath);

  const BethesdaFile NewBFile = {FilePath, std::move(BSAFile), Mod, Generated};

  FileMap[LowerPath] = NewBFile;
}

auto BethesdaDirectory::isFileInBSA(const filesystem::path &File, const std::unordered_set<std::wstring> &BSAFiles) -> bool {
  if (isBSAFile(File)) {
    BethesdaFile const BethFile = getFileFromMap(File);
    std::filesystem::path const BsaFilepath = BethFile.BSAFile->Path.filename();
    const std::wstring BsaFilename = BsaFilepath.wstring();

    if (std::any_of(BSAFiles.begin(), BSAFiles.end(),
                    [&BsaFilename](std::wstring const &File) { return boost::iequals(File, BsaFilename); })) {
      return true;
    }
  }
  return false;
}

auto BethesdaDirectory::checkIfAnyComponentIs(const filesystem::path &Path, const vector<wstring> &Components) -> bool {
  for (const auto &Component : Path) {
    for (const auto &Comp : Components) {
      if (boost::iequals(Component.wstring(), Comp)) {
        return true;
      }
    }
  }

  return false;
}

auto BethesdaDirectory::convertWStringToLPCWSTRVector(const vector<wstring> &Original) -> vector<LPCWSTR> {
  vector<LPCWSTR> Output(Original.size());
  for (size_t I = 0; I < Original.size(); I++) {
    Output[I] = Original[I].c_str();
  }

  return Output;
}

auto BethesdaDirectory::checkGlob(const LPCWSTR &Str, LPCWSTR &WinningGlob, const vector<LPCWSTR> &GlobList) -> bool {
  if (!boost::equals(WinningGlob, L"") && (PathMatchSpecW(Str, WinningGlob) != 0)) {
    // no winning glob, check all globs
    return true;
  }

  for (const auto &Glob : GlobList) {
    if (Glob != WinningGlob && (PathMatchSpecW(Str, Glob) != 0)) {
      WinningGlob = Glob;
      return true;
    }
  }

  return false;
}

auto BethesdaDirectory::readINIValue(const filesystem::path &INIPath, const wstring &Section, const wstring &Key,
                                     const bool &Logging, const bool &FirstINIRead) -> wstring {
  if (!filesystem::exists(INIPath)) {
    if (Logging && FirstINIRead) {
      spdlog::warn(L"INI file does not exist (ignoring): {}", INIPath.wstring());
    }
    return L"";
  }

  wifstream F(INIPath);
  if (!F.is_open()) {
    if (Logging && FirstINIRead) {
      spdlog::warn(L"Unable to open INI (ignoring): {}", INIPath.wstring());
    }
    return L"";
  }

  wstring CurLine;
  wstring CurSection;
  bool FoundSection = false;

  while (getline(F, CurLine)) {
    boost::trim(CurLine);

    // ignore comments
    if (CurLine.empty() || CurLine[0] == ';' || CurLine[0] == '#') {
      continue;
    }

    // Check if it's a section
    if (CurLine.front() == '[' && CurLine.back() == ']') {
      CurSection = CurLine.substr(1, CurLine.size() - 2);
      continue;
    }

    // Check if it's the correct section
    if (boost::iequals(CurSection, Section)) {
      FoundSection = true;
    }

    if (!boost::iequals(CurSection, Section)) {
      // exit if already checked section
      if (FoundSection) {
        break;
      }
      continue;
    }

    // check key
    const size_t Pos = CurLine.find('=');
    if (Pos != std::string::npos) {
      // found key value pair
      wstring CurKey = CurLine.substr(0, Pos);
      boost::trim(CurKey);
      if (boost::iequals(CurKey, Key)) {
        wstring CurValue = CurLine.substr(Pos + 1);
        boost::trim(CurValue);
        return CurValue;
      }
    }
  }

  return L"";
}
