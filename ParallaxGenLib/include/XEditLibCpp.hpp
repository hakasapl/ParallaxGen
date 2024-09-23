#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <windows.h>
#include <winuser.h>
#include <wtypes.h>

class XEditLibCpp {
private:
  HMODULE HModule;

public:
  // Constructor
  XEditLibCpp();
  // Destructor
  ~XEditLibCpp();
  // Delete copy constructor and copy assignment operator
  XEditLibCpp(const XEditLibCpp &) = delete;
  auto operator=(const XEditLibCpp &) -> XEditLibCpp & = delete;
  // Delete move constructor and move assignment operator
  XEditLibCpp(XEditLibCpp &&) = delete;
  auto operator=(XEditLibCpp &&) -> XEditLibCpp & = delete;

private:
  //
  // Declare function pointers
  //

  // Meta
  void (*XELibInitXEdit)();                                 // Not exposed
  void (*XELibCloseXEdit)();                                // Not exposed
  bool (*XELibGetResultString)(wchar_t *Str, int Len);      // Not exposed
  bool (*XELibGetResultArray)(unsigned int *Res, int Len);  // Not exposed
  bool (*XELibGetResultBytes)(unsigned char *Res, int Len); // Not exposed
  bool (*XELibGetGlobal)(const wchar_t *Key, int *Len);     // Not exposed
  bool (*XELibGetGlobals)(int *Len);                        // Not exposed
  bool (*XELibSetSortMode)(const unsigned char SortBy, const bool Reverse);
  bool (*XELibRelease)(const unsigned int Id);
  bool (*XELibReleaseNodes)(const unsigned int Id);
  bool (*XELibSwitch)(const unsigned int Id, const unsigned int Id2); // NO DOCS
  bool (*XELibGetDuplicateHandles)(const unsigned int Id, int *Len);
  bool (*XELibResetStore)(); // NO DOCS

  // Message
  void (*XELibGetMessagesLength)(int *Len); // Not exposed
  bool (*XELibGetMessages)(wchar_t *Str, int Len);
  void (*XELibClearMessages)();                            // Not exposed
  void (*XELibGetExceptionMessageLength)(int *Len);        // Not exposed
  bool (*XELibGetExceptionMessage)(wchar_t *Str, int Len); // Not exposed

  // Setup
  bool (*XELibGetGamePath)(const int Mode, int *Len);
  bool (*XELibSetGamePath)(const wchar_t *Path);
  bool (*XELibGetGameLanguage)(const int Mode, int *Len);
  bool (*XELibSetLanguage)(const wchar_t *Lang);
  bool (*XELibSetBackupPath)(const wchar_t *Path);
  bool (*XELibSetGameMode)(const int Mode);
  bool (*XELibGetLoadOrder)(int *Len);
  bool (*XELibGetActivePlugins)(int *Len);
  bool (*XELibLoadPlugins)(const wchar_t *LoadOrder, const bool SmartLoad);
  bool (*XELibLoadPlugin)(const wchar_t *Filename);
  bool (*XELibLoadPluginHeader)(const wchar_t *Filename, unsigned int *Res);
  bool (*XELibBuildReferences)(const unsigned int Id, const bool Synchronous);
  bool (*XELibGetLoaderStatus)(unsigned char *Status);
  bool (*XELibUnloadPlugin)(const unsigned int Id);

  // Files
  bool (*XELibAddFile)(const wchar_t *Filename, unsigned int *Res);
  bool (*XELibFileByIndex)(const int Index, unsigned int *Res);
  bool (*XELibFileByLoadOrder)(const int LoadOrder, unsigned int *Res);
  bool (*XELibFileByName)(const wchar_t *Name, unsigned int *Res);
  bool (*XELibFileByAuthor)(const wchar_t *Author, unsigned int *Res);
  bool (*XELibSaveFile)(const unsigned int Id, const wchar_t *FilePath);
  bool (*XELibGetRecordCount)(const unsigned int Id, int *Count);
  bool (*XELibGetOverrideRecordCount)(const unsigned int Id, int *Count);
  bool (*XELibMD5Hash)(const unsigned int Id, int *Len);
  bool (*XELibCRCHash)(const unsigned int Id, int *Len);
  bool (*XELibSortEditorIDs)(const unsigned int Id, wchar_t *Sig); // NO DOCS
  bool (*XELibSortNames)(const unsigned int Id, wchar_t *Sig);     // NO DOCS
  bool (*XELibGetFileLoadOrder)(const unsigned int Id, int *LoadOrder);

  // Archives
  bool (*XELibExtractContainer)(const wchar_t *Name, const wchar_t *Destination, const bool Replace);
  bool (*XELibExtractFile)(const wchar_t *Name, const wchar_t *Source, const wchar_t *Destination);
  bool (*XELibGetContainerFiles)(const wchar_t *Name, const wchar_t *Path, int *Len);
  bool (*XELibGetFileContainer)(const wchar_t *Path, int *Len);
  bool (*XELibGetLoadedContainers)(int *Len);
  bool (*XELibLoadContainer)(const wchar_t *FilePath);
  bool (*XELibBuildArchive)(const wchar_t *Name, const wchar_t *Folder, const wchar_t *FilePaths, const int ArchiveType,
                       const bool BCompress, const bool BShare, const wchar_t *AF, const wchar_t *FF);
  bool (*XELibGetTextureData)(const wchar_t *ResourceName, int *Width, int *Height);

  // Masters
  bool (*XELibCleanMasters)(unsigned int Id);
  bool (*XELibSortMasters)(unsigned int Id);
  bool (*XELibAddMaster)(unsigned int Id, wchar_t *MasterName);
  bool (*XELibAddMasters)(unsigned int Id, wchar_t *Masters);
  bool (*XELibAddRequiredMasters)(unsigned int Id, unsigned int Id2, bool AsNew);
  bool (*XELibGetMasters)(unsigned int Id, int *Len);
  bool (*XELibGetRequiredBy)(unsigned int Id, int *Len);
  bool (*XELibGetMasterNames)(unsigned int Id, int *Len);

  // Elements
  bool (*XELibHasElement)(const unsigned int Id, const wchar_t *Key, bool *BoolVal);
  bool (*XELibGetElement)(const unsigned int Id, const wchar_t *Key, unsigned int *Res);
  bool (*XELibAddElement)(const unsigned int Id, const wchar_t *Key, unsigned int *Res);
  bool (*XELibAddElementValue)(const unsigned int Id, const wchar_t *Key, const wchar_t *Value, unsigned int *Res);
  bool (*XELibRemoveElement)(const unsigned int Id, const wchar_t *Key);
  bool (*XELibRemoveElementOrParent)(const unsigned int Id);
  bool (*XELibSetElement)(const unsigned int Id, const unsigned int Id2);
  bool (*XELibGetElements)(const unsigned int Id, const wchar_t *Key, const bool Sort, const bool Filter, int *Len);
  bool (*XELibGetDefNames)(const unsigned int Id, int *Len);
  bool (*XELibGetAddList)(const unsigned int Id, int *Len);
  bool (*XELibGetLinksTo)(const unsigned int Id, const wchar_t *Key, unsigned int *Res);
  bool (*XELibSetLinksTo)(const unsigned int Id, const wchar_t *Key, const unsigned int Id2);
  bool (*XELibGetElementIndex)(const unsigned int Id, int *Index);
  bool (*XELibGetContainer)(const unsigned int Id, unsigned int *Res);
  bool (*XELibGetElementFile)(const unsigned int Id, unsigned int *Res);
  bool (*XELibGetElementRecord)(const unsigned int Id, unsigned int *Res);
  bool (*XELibElementCount)(const unsigned int Id, int *Count);
  bool (*XELibElementEquals)(const unsigned int Id, const unsigned int Id2, bool *BoolVal);
  bool (*XELibElementMatches)(const unsigned int Id, const wchar_t *Path, const wchar_t *Value, bool *BoolVal);
  bool (*XELibHasArrayItem)(const unsigned int Id, const wchar_t *Path, const wchar_t *SubPath, const wchar_t *Value,
                       bool *BoolVal);
  bool (*XELibGetArrayItem)(const unsigned int Id, const wchar_t *Path, const wchar_t *SubPath, const wchar_t *Value,
                       unsigned int *Res);
  bool (*XELibAddArrayItem)(const unsigned int Id, const wchar_t *Path, const wchar_t *SubPath, const wchar_t *Value,
                       unsigned int *Res);
  bool (*XELibRemoveArrayItem)(const unsigned int Id, const wchar_t *Path, const wchar_t *SubPath, const wchar_t *Value);
  bool (*XELibMoveArrayItem)(const unsigned int Id, const int Index);
  bool (*XELibCopyElement)(const unsigned int Id, const unsigned int Id2, const bool AsNew, unsigned int *Res);
  bool (*XELibGetSignatureAllowed)(const unsigned int Id, const wchar_t *Sig, bool *BoolVal);
  bool (*XELibGetAllowedSignatures)(const unsigned int Id, int *Len);
  bool (*XELibGetIsModified)(const unsigned int Id, bool *BoolVal);
  bool (*XELibGetIsEditable)(const unsigned int Id, bool *BoolVal);
  bool (*XELibGetIsRemoveable)(const unsigned int Id, bool *BoolVal);
  bool (*XELibGetCanAdd)(const unsigned int Id, bool *BoolVal);
  bool (*XELibSortKey)(const unsigned int Id, int *Len);
  bool (*XELibElementType)(const unsigned int Id, unsigned char *EnumVal);
  bool (*XELibDefType)(const unsigned int Id, unsigned char *EnumVal);
  bool (*XELibSmashType)(const unsigned int Id, unsigned char *EnumVal);
  bool (*XELibValueType)(const unsigned int Id, unsigned char *EnumVal);
  bool (*XELibIsSorted)(unsigned int Id, bool *BoolVal);

  // Element Value
  bool (*XELibName)(const unsigned int Id, int *Len);
  bool (*XELibLongName)(const unsigned int Id, int *Len);
  bool (*XELibDisplayName)(const unsigned int Id, int *Len);
  bool (*XELibPath)(const unsigned int Id, const bool ShortPath, const bool Local, int *Len);
  bool (*XELibSignature)(const unsigned int Id, int *Len);
  bool (*XELibGetValue)(const unsigned int Id, const wchar_t *Path, int *Len);
  bool (*XELibSetValue)(const unsigned int Id, const wchar_t *Path, const wchar_t *Value);
  bool (*XELibGetIntValue)(const unsigned int Id, const wchar_t *Path, int *Value);
  bool (*XELibSetIntValue)(const unsigned int Id, const wchar_t *Path, const int Value);
  bool (*XELibGetUIntValue)(const unsigned int Id, const wchar_t *Path, unsigned int *Value);
  bool (*XELibSetUIntValue)(const unsigned int Id, const wchar_t *Path, const unsigned int Value);
  bool (*XELibGetFloatValue)(const unsigned int Id, const wchar_t *Path, double *Value);
  bool (*XELibSetFloatValue)(const unsigned int Id, const wchar_t *Path, const double Value);
  bool (*XELibGetFlag)(const unsigned int Id, const wchar_t *Path, const wchar_t *Name, bool *Enabled);
  bool (*XELibSetFlag)(const unsigned int Id, const wchar_t *Path, const wchar_t *Name, const bool Enabled);
  bool (*XELibGetAllFlags)(const unsigned int Id, const wchar_t *Path, int *Len);
  bool (*XELibGetEnabledFlags)(const unsigned int Id, const wchar_t *Path, int *Len);
  bool (*XELibSetEnabledFlags)(const unsigned int Id, const wchar_t *Path, const wchar_t *Flags);
  bool (*XELibGetEnumOptions)(const unsigned int Id, const wchar_t *Path, int *Len);
  bool (*XELibSignatureFromName)(const wchar_t *Name, int *Len);
  bool (*XELibNameFromSignature)(const wchar_t *Sig, int *Len);
  bool (*XELibGetSignatureNameMap)(int *Len);

  // Serialization
  bool (*XELibElementToJson)(const unsigned int Id, int *Len);
  bool (*XELibElementFromJson)(const unsigned int Id, const wchar_t *Path, const wchar_t *JSON);

  // Records
  bool (*XELibGetFormID)(const unsigned int Id, unsigned int *FormID, const bool Local);
  bool (*XELibSetFormID)(const unsigned int Id, const unsigned int FormID, const bool Local, const bool FixReferences);
  bool (*XELibGetRecord)(const unsigned int Id, const unsigned int FormID, const bool SearchMasters, unsigned int *Res);
  bool (*XELibGetRecords)(const unsigned int Id, const wchar_t *Search, const bool IncludeOverrides, int *Len);
  bool (*XELibGetREFRs)(const unsigned int Id, const wchar_t *Search, const unsigned int Flags, int *Len);
  bool (*XELibGetOverrides)(const unsigned int Id, int *Len);
  bool (*XELibGetMasterRecord)(const unsigned int Id, unsigned int *Res);
  bool (*XELibGetWinningOverride)(const unsigned int Id, unsigned int *Res);
  bool (*XELibGetInjectionTarget)(const unsigned int Id, unsigned int *Res);
  bool (*XELibFindNextRecord)(const unsigned int Id, const wchar_t *Search, const bool ByEdid, const bool ByName,
                         unsigned int *Res);
  bool (*XELibFindPreviousRecord)(const unsigned int Id, const wchar_t *Search, const bool ByEdid, const bool ByName,
                             unsigned int *Res);
  bool (*XELibFindValidReferences)(const unsigned int Id, const wchar_t *Signature, const wchar_t *Search, const int LimitTo,
                              int *Len);
  bool (*XELibGetReferencedBy)(const unsigned int Id, int *Len);
  bool (*XELibExchangeReferences)(const unsigned int Id, const unsigned int OldFormID, const unsigned int NewFormID);
  bool (*XELibIsMaster)(const unsigned int Id, bool *BoolVal);
  bool (*XELibIsInjected)(const unsigned int Id, bool *BoolVal);
  bool (*XELibIsOverride)(const unsigned int Id, bool *BoolVal);
  bool (*XELibIsWinningOverride)(const unsigned int Id, bool *BoolVal);
  bool (*XELibGetNodes)(const unsigned int Id, unsigned int *Res);
  bool (*XELibGetConflictData)(const unsigned int Id, const unsigned int Id2, unsigned char *ConflictAll,
                          unsigned char *ConflictThis);
  bool (*XELibGetNodeElements)(const unsigned int Id, const unsigned int Id2, int *Len);

  // Errors
  bool (*XELibCheckForErrors)(const unsigned int Id);
  bool (*XELibGetErrorThreadDone)();
  bool (*XELibGetErrors)(int *Len);
  bool (*XELibGetErrorString)(const unsigned int Id, int *Len); // Not Exposed
  bool (*XELibRemoveIdenticalRecords)(const unsigned int Id, const bool RemoveITMs, const bool RemoveITPOs);

  // Filters
  bool (*XELibFilterRecord)(const unsigned int Id);
  bool (*XELibResetFilter)();

  // Helpers
  auto getResultStringSafe(int Len) -> std::wstring;
  auto getResultArraySafe(int Len) -> std::vector<unsigned int>;
  auto getResultBytesSafe(int Len) -> std::vector<unsigned char>;

  auto getExceptionMessageSafe() -> std::string;
  void throwExceptionIfExists();

public:
  //
  // XEditLib Wrappers
  //

  // Meta
  enum class SortBy { None = 0, FormID = 1, EditorID = 2, Name = 3 };

  [[nodiscard]] auto getProgramPath() -> std::filesystem::path;
  [[nodiscard]] auto getVersion() -> std::wstring;
  [[nodiscard]] auto getGameName() -> std::wstring;
  [[nodiscard]] auto getAppName() -> std::wstring;
  [[nodiscard]] auto getLongGameName() -> std::wstring;
  [[nodiscard]] auto getDataPath() -> std::filesystem::path;
  [[nodiscard]] auto getAppDataPath() -> std::filesystem::path;
  [[nodiscard]] auto getMyGamesPath() -> std::filesystem::path;
  [[nodiscard]] auto getGameIniPath() -> std::filesystem::path;
  void setSortMode(const SortBy &SortBy, bool Reverse);
  void release(unsigned int Id);
  void releaseNodes(unsigned int Id);
  auto getDuplicateHandles(unsigned int Id) -> std::vector<unsigned int>;

  // Messages
  [[nodiscard]] auto getMessages() -> std::wstring;

  // Setup
  enum class LoaderStates { IsInactive = 0, IsActive = 1, IsDone = 2, IsError = 3 };

  enum class GameMode { FalloutNewVegas = 0, Fallout3 = 1, Oblivion = 2, Skyrim = 3, SkyrimSE = 4, Fallout4 = 5 };

  [[nodiscard]] auto getGamePath(const GameMode &Mode) -> std::filesystem::path;
  void setGamePath(const std::filesystem::path &Path);
  [[nodiscard]] auto getGameLanguage(const GameMode &Mode) -> std::wstring;
  void setLanguage(const std::wstring &Lang);
  void setBackupPath(const std::filesystem::path &Path);
  void setGameMode(const GameMode &Mode);
  [[nodiscard]] auto getLoadOrder() -> std::vector<std::wstring>;
  [[nodiscard]] auto getActivePlugins() -> std::vector<std::wstring>;
  void loadPlugins(const std::vector<std::wstring> &LoadOrder, bool SmartLoad);
  void loadPlugin(const std::wstring &Filename);
  auto loadPluginHeader(const std::wstring &Filename) -> unsigned int;
  void buildReferences(const unsigned int &Id, const bool &Synchronous);
  [[nodiscard]] auto getLoaderStatus() -> LoaderStates;
  void unloadPlugin(const unsigned int &Id);

  // Files
  [[nodiscard]] auto addFile(const std::filesystem::path &Path) -> unsigned int;
  [[nodiscard]] auto fileByIndex(int Index) -> unsigned int;
  [[nodiscard]] auto fileByLoadOrder(int LoadOrder) -> unsigned int;
  [[nodiscard]] auto fileByName(const std::wstring &Name) -> unsigned int;
  [[nodiscard]] auto fileByAuthor(const std::wstring &Author) -> unsigned int;
  void saveFile(const unsigned int &Id, const std::filesystem::path &FilePath);
  [[nodiscard]] auto getRecordCount(const unsigned int &Id) -> int;
  [[nodiscard]] auto getOverrideRecordCount(const unsigned int &Id) -> int;
  [[nodiscard]] auto md5Hash(const unsigned int &Id) -> std::wstring;
  [[nodiscard]] auto crcHash(const unsigned int &Id) -> std::wstring;
  [[nodiscard]] auto getFileLoadOrder(const unsigned int &Id) -> int;

  // Archives
  enum class ArchiveType { BANone = 0, BATES3 = 1, BAFO3 = 2, BASSE = 3, BAFO4 = 4, BAFO4DDS = 5 };

  void extractContainer(const std::wstring &Name, const std::filesystem::path &Destination, const bool &Replace);
  void extractFile(const std::wstring &Name, const std::filesystem::path &Source,
                   const std::filesystem::path &Destination);
  [[nodiscard]] auto getContainerFiles(const std::wstring &Name,
                                       const std::filesystem::path &Path) -> std::vector<std::wstring>;
  [[nodiscard]] auto getFileContainer(const std::filesystem::path &Path) -> std::wstring;
  [[nodiscard]] auto getLoadedContainers() -> std::vector<std::wstring>;
  void loadContainer(const std::filesystem::path &FilePath);
  void buildArchive(const std::wstring &Name, const std::filesystem::path &Folder,
                    const std::vector<std::filesystem::path> &FilePaths, const ArchiveType &Type, bool BCompress,
                    bool BShare, const std::wstring &AF, const std::wstring &FF);
  [[nodiscard]] auto getTextureData(const std::wstring &ResourceName) -> std::pair<int, int>;

  // Elements
  enum class ElementType : unsigned char {
    ETFile = 0,
    ETMainRecord = 1,
    ETGroupRecord = 2,
    ETSubRecord = 3,
    ETSubRecordStruct = 4,
    ETSubRecordArray = 5,
    ETSubRecordUnion = 6,
    ETArray = 7,
    ETStruct = 8,
    ETValue = 9,
    ETFlag = 10,
    ETStringListTerminator = 11,
    ETUnion = 12,
    ETStructChapter = 13
  };

  enum class DefType : unsigned char {
    DTRecord = 0,
    DTSubRecord = 1,
    DTSubRecordArray = 2,
    DTSubRecordStruct = 3,
    DTSubRecordUnion = 4,
    DTString = 5,
    DTLString = 6,
    DTLenString = 7,
    DTByteArray = 8,
    DTInteger = 9,
    DTIntegerFormater = 10,
    DTIntegerFormaterUnion = 11,
    DTFlag = 12,
    DTFloat = 13,
    DTArray = 14,
    DTStruct = 15,
    DTUnion = 16,
    DTEmpty = 17,
    DTStructChapter = 18
  };

  enum class SmashType : unsigned char {
    STUnknown = 0,
    STRecord = 1,
    STString = 2,
    STInteger = 3,
    STFlag = 4,
    STFloat = 5,
    STStruct = 6,
    STUnsortedArray = 7,
    STUnsortedStructArray = 8,
    STSortedArray = 9,
    STSortedStructArray = 10,
    STByteArray = 11,
    STUnion = 12
  };

  enum class ValueType : unsigned char {
    VTUnknown = 0,
    VTBytes = 1,
    VTNumber = 2,
    VTString = 3,
    VTText = 4,
    VTReference = 5,
    VTFlags = 6,
    VTEnum = 7,
    VTColor = 8,
    VTArray = 9,
    VTStruct = 10
  };

  [[nodiscard]] auto hasElement(const unsigned int &Id, const std::wstring &Key) -> bool;
  [[nodiscard]] auto getElement(const unsigned int &Id, const std::wstring &Key) -> unsigned int;
  [[nodiscard]] auto addElement(const unsigned int &Id, const std::wstring &Key) -> unsigned int;
  [[nodiscard]] auto addElementValue(const unsigned int &Id, const std::wstring &Key,
                                     const std::wstring &Value) -> unsigned int;
  void removeElement(const unsigned int &Id, const std::wstring &Key);
  void removeElementOrParent(const unsigned int &Id);
  void setElement(const unsigned int &Id, const unsigned int &Id2);
  [[nodiscard]] auto getElements(const unsigned int &Id, const std::wstring &Key, const bool &Sort,
                                 const bool &Filter) -> std::vector<unsigned int>;
  [[nodiscard]] auto getDefNames(const unsigned int &Id) -> std::vector<std::wstring>;
  [[nodiscard]] auto getAddList(const unsigned int &Id) -> std::vector<std::wstring>;
  [[nodiscard]] auto getLinksTo(const unsigned int &Id, const std::wstring &Key) -> unsigned int;
  void setLinksTo(const unsigned int &Id, const std::wstring &Key, const unsigned int &Id2);
  [[nodiscard]] auto getElementIndex(const unsigned int &Id) -> int;
  [[nodiscard]] auto getContainer(const unsigned int &Id) -> unsigned int;
  [[nodiscard]] auto getElementFile(const unsigned int &Id) -> unsigned int;
  [[nodiscard]] auto getElementRecord(const unsigned int &Id) -> unsigned int;
  [[nodiscard]] auto elementCount(const unsigned int &Id) -> int;
  [[nodiscard]] auto elementEquals(const unsigned int &Id, const unsigned int &Id2) -> bool;
  [[nodiscard]] auto elementMatches(const unsigned int &Id, const std::wstring &Key,
                                    const std::wstring &Value) -> bool;
  [[nodiscard]] auto hasArrayItem(const unsigned int &Id, const std::wstring &Key, const std::wstring &SubKey,
                                  const std::wstring &Value) -> bool;
  [[nodiscard]] auto getArrayItem(const unsigned int &Id, const std::wstring &Key, const std::wstring &SubKey,
                                  const std::wstring &Value) -> unsigned int;
  [[nodiscard]] auto addArrayItem(const unsigned int &Id, const std::wstring &Key, const std::wstring &SubKey,
                                  const std::wstring &Value) -> unsigned int;
  void removeArrayItem(const unsigned int &Id, const std::wstring &Key, const std::wstring &SubKey,
                       const std::wstring &Value);
  void moveArrayItem(const unsigned int &Id, const int &Index);
  [[nodiscard]] auto copyElement(const unsigned int &Id, const unsigned int &Id2, const bool &AsNew) -> unsigned int;
  [[nodiscard]] auto getSignatureAllowed(const unsigned int &Id, const std::wstring &Sig) -> bool;
  [[nodiscard]] auto getAllowedSignatures(const unsigned int &Id) -> std::vector<std::wstring>;
  [[nodiscard]] auto getIsModified(const unsigned int &Id) -> bool;
  [[nodiscard]] auto getIsEditable(const unsigned int &Id) -> bool;
  [[nodiscard]] auto getIsRemoveable(const unsigned int &Id) -> bool;
  [[nodiscard]] auto getCanAdd(const unsigned int &Id) -> bool;
  [[nodiscard]] auto sortKey(const unsigned int &Id) -> std::wstring;
  [[nodiscard]] auto elementType(const unsigned int &Id) -> ElementType;
  [[nodiscard]] auto defType(const unsigned int &Id) -> DefType;
  [[nodiscard]] auto smashType(const unsigned int &Id) -> SmashType;
  [[nodiscard]] auto valueType(const unsigned int &Id) -> ValueType;
  [[nodiscard]] auto isSorted(const unsigned int &Id) -> bool;

  // Element Value
  [[nodiscard]] auto name(const unsigned int &Id) -> std::wstring;
  [[nodiscard]] auto longName(const unsigned int &Id) -> std::wstring;
  [[nodiscard]] auto displayName(const unsigned int &Id) -> std::wstring;
  [[nodiscard]] auto path(const unsigned int &Id, const bool &ShortPath, const bool &Local) -> std::wstring;
  [[nodiscard]] auto signature(const unsigned int &Id) -> std::wstring;
  [[nodiscard]] auto getValue(const unsigned int &Id, const std::wstring &Key) -> std::wstring;
  void setValue(const unsigned int &Id, const std::wstring &Key, const std::wstring &Value);
  [[nodiscard]] auto getIntValue(const unsigned int &Id, const std::wstring &Key) -> int;
  void setIntValue(const unsigned int &Id, const std::wstring &Key, const int &Value);
  [[nodiscard]] auto getUIntValue(const unsigned int &Id, const std::wstring &Key) -> unsigned int;
  void setUIntValue(const unsigned int &Id, const std::wstring &Key, const unsigned int &Value);
  [[nodiscard]] auto getFloatValue(const unsigned int &Id, const std::wstring &Key) -> double;
  void setFloatValue(const unsigned int &Id, const std::wstring &Key, const double &Value);
  [[nodiscard]] auto getFlag(const unsigned int &Id, const std::wstring &Key, const std::wstring &Flag) -> bool;
  void setFlag(const unsigned int &Id, const std::wstring &Key, const std::wstring &Flag, const bool &Enabled);
  [[nodiscard]] auto getAllFlags(const unsigned int &Id, const std::wstring &Key) -> std::vector<std::wstring>;
  [[nodiscard]] auto getEnabledFlags(const unsigned int &Id, const std::wstring &Key) -> std::vector<std::wstring>;
  void setEnabledFlags(const unsigned int &Id, const std::wstring &Key, const std::wstring &Flags);
  [[nodiscard]] auto getEnumOptions(const unsigned int &Id, const std::wstring &Key) -> std::vector<std::wstring>;
  [[nodiscard]] auto signatureFromName(const std::wstring &Name) -> std::wstring;
  [[nodiscard]] auto nameFromSignature(const std::wstring &Sig) -> std::wstring;
  [[nodiscard]] auto getSignatureNameMap() -> std::vector<std::wstring>;

  // Serialization
  [[nodiscard]] auto elementToJson(const unsigned int &Id) -> std::wstring;
  void elementFromJson(const unsigned int &Id, const std::filesystem::path &Path, const std::wstring &JSON);

  // Records
  enum class ConflictThis {
    CTUnknown = 0,
    CTIgnored = 1,
    CTNotDefined = 2,
    CTIdenticalToMaster = 3,
    CTOnlyOne = 4,
    CTHiddenByModGroup = 5,
    CTMaster = 6,
    CTConflictBenign = 7,
    CTOverride = 8,
    CTIdenticalToMasterWinsConflict = 9,
    CTConflictWins = 10,
    CTConflictLoses = 11
  };

  enum class ConflictAll {
    CAUnknown = 0,
    CAOnlyOne = 1,
    CANoConflict = 2,
    CAConflictBenign = 3,
    CAOverride = 4,
    CAConflict = 5,
    CAConflictCritical = 6
  };

  [[nodiscard]] auto getFormID(const unsigned int &Id, const bool &Local) -> unsigned int;
  void setFormID(const unsigned int &Id, const unsigned int &FormID, const bool &Local, const bool &FixRefs);
  [[nodiscard]] auto getRecord(const unsigned int &Id, const unsigned int &FormID,
                               const bool &SearchMasters) -> unsigned int;
  [[nodiscard]] auto getRecords(const unsigned int &Id, const std::wstring &Search,
                                const bool &IncludeOverrides) -> std::vector<unsigned int>;
  [[nodiscard]] auto getREFRs(const unsigned int &Id, const std::wstring &Search,
                              const unsigned int &Flags) -> std::vector<unsigned int>;
  [[nodiscard]] auto getOverrides(const unsigned int &Id) -> std::vector<unsigned int>;
  [[nodiscard]] auto getMasterRecord(const unsigned int &Id) -> unsigned int;
  [[nodiscard]] auto getWinningOverride(const unsigned int &Id) -> unsigned int;
  [[nodiscard]] auto getInjectionTarget(const unsigned int &Id) -> unsigned int;
  [[nodiscard]] auto findNextRecord(const unsigned int &Id, const std::wstring &Search, const bool &ByEdid,
                                    const bool &ByName) -> unsigned int;
  [[nodiscard]] auto findPreviousRecord(const unsigned int &Id, const std::wstring &Search, const bool &ByEdid,
                                        const bool &ByName) -> unsigned int;
  [[nodiscard]] auto findValidReferences(const unsigned int &Id, const std::wstring &Signature,
                                         const std::wstring &Search, const int &LimitTo) -> std::vector<unsigned int>;
  [[nodiscard]] auto getReferencedBy(const unsigned int &Id) -> std::vector<unsigned int>;
  void exchangeReferences(const unsigned int &Id, const unsigned int &OldFormID, const unsigned int &NewFormID);
  [[nodiscard]] auto isMaster(const unsigned int &Id) -> bool;
  [[nodiscard]] auto isInjected(const unsigned int &Id) -> bool;
  [[nodiscard]] auto isOverride(const unsigned int &Id) -> bool;
  [[nodiscard]] auto isWinningOverride(const unsigned int &Id) -> bool;
  [[nodiscard]] auto getNodes(const unsigned int &Id) -> unsigned int;
  [[nodiscard]] auto getConflictData(const unsigned int &Id,
                                     const unsigned int &Id2) -> std::pair<ConflictAll, ConflictThis>;
  [[nodiscard]] auto getNodeElements(const unsigned int &Id, const unsigned int &Id2) -> std::vector<unsigned int>;

  // Errors
  void checkForErrors(const unsigned int &Id);
  [[nodiscard]] auto getErrorThreadDone() -> bool;
  [[nodiscard]] auto getErrors() -> std::vector<std::wstring>;
  [[nodiscard]] auto getErrorString(const unsigned int &Id) -> std::wstring;
  void removeIdenticalRecords(const unsigned int &Id, const bool &RemoveITMs, const bool &RemoveITPOs);

  // Filters
  void filterRecord(const unsigned int &Id);
  void resetFilter();
};
