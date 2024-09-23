#include "XEditLibCpp.hpp"

#include <boost/algorithm/string.hpp>
#include <memory>

#include "ParallaxGenUtil.hpp"

using namespace std;

XEditLibCpp::XEditLibCpp() : HModule(LoadLibrary(TEXT("XEditLib.dll"))) {
  if (HModule != nullptr) {
    // Meta
    XELibInitXEdit = reinterpret_cast<void (*)()>(GetProcAddress(HModule, "InitXEdit"));
    XELibCloseXEdit = reinterpret_cast<void (*)()>(GetProcAddress(HModule, "CloseXEdit"));
    XELibGetResultString = reinterpret_cast<bool (*)(wchar_t *, int)>(GetProcAddress(HModule, "GetResultString"));
    XELibGetResultArray = reinterpret_cast<bool (*)(unsigned int *, int)>(GetProcAddress(HModule, "GetResultArray"));
    XELibGetResultBytes = reinterpret_cast<bool (*)(unsigned char *, int)>(GetProcAddress(HModule, "GetResultBytes"));
    XELibGetGlobal = reinterpret_cast<bool (*)(const wchar_t *, int *)>(GetProcAddress(HModule, "GetGlobal"));
    XELibGetGlobals = reinterpret_cast<bool (*)(int *)>(GetProcAddress(HModule, "GetGlobals"));
    XELibSetSortMode = reinterpret_cast<bool (*)(const unsigned char, const bool)>(GetProcAddress(HModule, "SetSortMode"));
    XELibRelease = reinterpret_cast<bool (*)(const unsigned int)>(GetProcAddress(HModule, "Release"));
    XELibReleaseNodes = reinterpret_cast<bool (*)(const unsigned int)>(GetProcAddress(HModule, "ReleaseNodes"));
    XELibSwitch = reinterpret_cast<bool (*)(const unsigned int, const unsigned int)>(GetProcAddress(HModule, "Switch"));
    XELibGetDuplicateHandles =
        reinterpret_cast<bool (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetDuplicateHandles"));
    XELibResetStore = reinterpret_cast<bool (*)()>(GetProcAddress(HModule, "ResetStore"));

    // Message
    XELibGetMessagesLength = reinterpret_cast<void (*)(int *)>(GetProcAddress(HModule, "GetMessagesLength"));
    XELibGetMessages = reinterpret_cast<bool (*)(wchar_t *, int)>(GetProcAddress(HModule, "GetMessages"));
    XELibClearMessages = reinterpret_cast<void (*)()>(GetProcAddress(HModule, "ClearMessages"));
    XELibGetExceptionMessageLength = reinterpret_cast<void (*)(int *)>(GetProcAddress(HModule, "GetExceptionMessageLength"));
    XELibGetExceptionMessage = reinterpret_cast<bool (*)(wchar_t *, int)>(GetProcAddress(HModule, "GetExceptionMessage"));

    // Setup
    XELibGetGamePath = reinterpret_cast<bool (*)(const int, int *)>(GetProcAddress(HModule, "GetGamePath"));
    XELibSetGamePath = reinterpret_cast<bool (*)(const wchar_t *)>(GetProcAddress(HModule, "SetGamePath"));
    XELibGetGameLanguage = reinterpret_cast<bool (*)(const int, int *)>(GetProcAddress(HModule, "GetGameLanguage"));
    XELibSetLanguage = reinterpret_cast<bool (*)(const wchar_t *)>(GetProcAddress(HModule, "SetLanguage"));
    XELibSetBackupPath = reinterpret_cast<bool (*)(const wchar_t *)>(GetProcAddress(HModule, "SetBackupPath"));
    XELibSetGameMode = reinterpret_cast<bool (*)(const int)>(GetProcAddress(HModule, "SetGameMode"));
    XELibGetLoadOrder = reinterpret_cast<bool (*)(int *)>(GetProcAddress(HModule, "GetLoadOrder"));
    XELibGetActivePlugins = reinterpret_cast<bool (*)(int *)>(GetProcAddress(HModule, "GetActivePlugins"));
    XELibLoadPlugins = reinterpret_cast<bool (*)(const wchar_t *, const bool)>(GetProcAddress(HModule, "LoadPlugins"));
    XELibLoadPlugin = reinterpret_cast<bool (*)(const wchar_t *)>(GetProcAddress(HModule, "LoadPlugin"));
    XELibLoadPluginHeader =
        reinterpret_cast<bool (*)(const wchar_t *, unsigned int *)>(GetProcAddress(HModule, "LoadPluginHeader"));
    XELibBuildReferences =
        reinterpret_cast<bool (*)(const unsigned int, const bool)>(GetProcAddress(HModule, "BuildReferences"));
    XELibGetLoaderStatus = reinterpret_cast<bool (*)(unsigned char *)>(GetProcAddress(HModule, "GetLoaderStatus"));
    XELibUnloadPlugin = reinterpret_cast<bool (*)(const unsigned int)>(GetProcAddress(HModule, "UnloadPlugin"));

    // Files
    XELibAddFile = reinterpret_cast<bool (*)(const wchar_t *, unsigned int *)>(GetProcAddress(HModule, "AddFile"));
    XELibFileByIndex = reinterpret_cast<bool (*)(const int, unsigned int *)>(GetProcAddress(HModule, "FileByIndex"));
    XELibFileByLoadOrder = reinterpret_cast<bool (*)(const int, unsigned int *)>(GetProcAddress(HModule, "FileByLoadOrder"));
    XELibFileByName = reinterpret_cast<bool (*)(const wchar_t *, unsigned int *)>(GetProcAddress(HModule, "FileByName"));
    XELibFileByAuthor = reinterpret_cast<bool (*)(const wchar_t *, unsigned int *)>(GetProcAddress(HModule, "FileByAuthor"));
    XELibSaveFile = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *)>(GetProcAddress(HModule, "SaveFile"));
    XELibGetRecordCount = reinterpret_cast<bool (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetRecordCount"));
    XELibGetOverrideRecordCount =
        reinterpret_cast<bool (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetOverrideRecordCount"));
    XELibMD5Hash = reinterpret_cast<bool (*)(const unsigned int, int *)>(GetProcAddress(HModule, "MD5Hash"));
    XELibCRCHash = reinterpret_cast<bool (*)(const unsigned int, int *)>(GetProcAddress(HModule, "CRCHash"));
    XELibSortEditorIDs = reinterpret_cast<bool (*)(const unsigned int, wchar_t *)>(GetProcAddress(HModule, "SortEditorIDs"));
    XELibSortNames = reinterpret_cast<bool (*)(const unsigned int, wchar_t *)>(GetProcAddress(HModule, "SortNames"));
    XELibGetFileLoadOrder =
        reinterpret_cast<bool (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetFileLoadOrder"));

    // Archives
    XELibExtractContainer =
        reinterpret_cast<bool (*)(const wchar_t *, const wchar_t *, bool)>(GetProcAddress(HModule, "ExtractContainer"));
    XELibExtractFile = reinterpret_cast<bool (*)(const wchar_t *, const wchar_t *, const wchar_t *)>(
        GetProcAddress(HModule, "ExtractFile"));
    XELibGetContainerFiles = reinterpret_cast<bool (*)(const wchar_t *, const wchar_t *, int *)>(
        GetProcAddress(HModule, "GetContainerFiles"));
    XELibGetFileContainer = reinterpret_cast<bool (*)(const wchar_t *, int *)>(GetProcAddress(HModule, "GetFileContainer"));
    XELibGetLoadedContainers = reinterpret_cast<bool (*)(int *)>(GetProcAddress(HModule, "GetLoadedContainers"));
    XELibLoadContainer = reinterpret_cast<bool (*)(const wchar_t *)>(GetProcAddress(HModule, "LoadContainer"));
    XELibBuildArchive =
        reinterpret_cast<bool (*)(const wchar_t *, const wchar_t *, const wchar_t *, const int, const bool, const bool,
                                  const wchar_t *, const wchar_t *)>(GetProcAddress(HModule, "BuildArchive"));
    XELibGetTextureData =
        reinterpret_cast<bool (*)(const wchar_t *, int *, int *)>(GetProcAddress(HModule, "GetTextureData"));

    // Masters
    XELibCleanMasters = reinterpret_cast<bool (*)(unsigned int)>(GetProcAddress(HModule, "CleanMasters"));
    XELibSortMasters = reinterpret_cast<bool (*)(unsigned int)>(GetProcAddress(HModule, "SortMasters"));
    XELibAddMaster = reinterpret_cast<bool (*)(unsigned int, wchar_t *)>(GetProcAddress(HModule, "AddMaster"));
    XELibAddMasters = reinterpret_cast<bool (*)(unsigned int, wchar_t *)>(GetProcAddress(HModule, "AddMasters"));
    XELibAddRequiredMasters =
        reinterpret_cast<bool (*)(unsigned int, unsigned int, bool)>(GetProcAddress(HModule, "AddRequiredMasters"));
    XELibGetMasters = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "GetMasters"));
    XELibGetRequiredBy = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "GetRequiredBy"));
    XELibGetMasterNames = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "GetMasterNames"));

    // Elements
    XELibHasElement =
        reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, bool *)>(GetProcAddress(HModule, "HasElement"));
    XELibGetElement = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, unsigned int *)>(
        GetProcAddress(HModule, "GetElement"));
    XELibAddElement = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, unsigned int *)>(
        GetProcAddress(HModule, "AddElement"));
    XELibAddElementValue = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, const wchar_t *, unsigned int *)>(
        GetProcAddress(HModule, "AddElementValue"));
    XELibRemoveElement =
        reinterpret_cast<bool (*)(const unsigned int, const wchar_t *)>(GetProcAddress(HModule, "RemoveElement"));
    XELibRemoveElementOrParent =
        reinterpret_cast<bool (*)(const unsigned int)>(GetProcAddress(HModule, "RemoveElementOrParent"));
    XELibSetElement =
        reinterpret_cast<bool (*)(const unsigned int, const unsigned int)>(GetProcAddress(HModule, "SetElement"));
    XELibGetElements = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, const bool, const bool, int *)>(
        GetProcAddress(HModule, "GetElements"));
    XELibGetDefNames = reinterpret_cast<bool (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetDefNames"));
    XELibGetAddList = reinterpret_cast<bool (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetAddList"));
    XELibGetLinksTo = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, unsigned int *)>(
        GetProcAddress(HModule, "GetLinksTo"));
    XELibSetLinksTo = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, const unsigned int)>(
        GetProcAddress(HModule, "SetLinksTo"));
    XELibGetElementIndex = reinterpret_cast<bool (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetElementIndex"));
    XELibGetContainer =
        reinterpret_cast<bool (*)(const unsigned int, unsigned int *)>(GetProcAddress(HModule, "GetContainer"));
    XELibGetElementFile =
        reinterpret_cast<bool (*)(const unsigned int, unsigned int *)>(GetProcAddress(HModule, "GetElementFile"));
    XELibGetElementRecord =
        reinterpret_cast<bool (*)(const unsigned int, unsigned int *)>(GetProcAddress(HModule, "GetElementRecord"));
    XELibElementCount = reinterpret_cast<bool (*)(const unsigned int, int *)>(GetProcAddress(HModule, "ElementCount"));
    XELibElementEquals = reinterpret_cast<bool (*)(const unsigned int, const unsigned int, bool *)>(
        GetProcAddress(HModule, "ElementEquals"));
    XELibElementMatches = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, const wchar_t *, bool *)>(
        GetProcAddress(HModule, "ElementMatches"));
    XELibHasArrayItem =
        reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, const wchar_t *, const wchar_t *, bool *)>(
            GetProcAddress(HModule, "HasArrayItem"));
    XELibGetArrayItem = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, const wchar_t *, const wchar_t *,
                                             unsigned int *)>(GetProcAddress(HModule, "GetArrayItem"));
    XELibAddArrayItem = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, const wchar_t *, const wchar_t *,
                                             unsigned int *)>(GetProcAddress(HModule, "AddArrayItem"));
    XELibRemoveArrayItem = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, const wchar_t *, const wchar_t *)>(
        GetProcAddress(HModule, "RemoveArrayItem"));
    XELibMoveArrayItem = reinterpret_cast<bool (*)(const unsigned int, const int)>(GetProcAddress(HModule, "MoveArrayItem"));
    XELibCopyElement = reinterpret_cast<bool (*)(const unsigned int, const unsigned int, const bool, unsigned int *)>(
        GetProcAddress(HModule, "CopyElement"));
    XELibGetSignatureAllowed = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, bool *)>(
        GetProcAddress(HModule, "GetSignatureAllowed"));
    XELibGetAllowedSignatures =
        reinterpret_cast<bool (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetAllowedSignatuRes"));
    XELibGetIsModified = reinterpret_cast<bool (*)(const unsigned int, bool *)>(GetProcAddress(HModule, "GetIsModified"));
    XELibGetIsEditable = reinterpret_cast<bool (*)(const unsigned int, bool *)>(GetProcAddress(HModule, "GetIsEditable"));
    XELibGetIsRemoveable =
        reinterpret_cast<bool (*)(const unsigned int, bool *)>(GetProcAddress(HModule, "GetIsRemoveable"));
    XELibGetCanAdd = reinterpret_cast<bool (*)(const unsigned int, bool *)>(GetProcAddress(HModule, "GetCanAdd"));
    XELibSortKey = reinterpret_cast<bool (*)(const unsigned int, int *)>(GetProcAddress(HModule, "SortKey"));
    XELibElementType =
        reinterpret_cast<bool (*)(const unsigned int, unsigned char *)>(GetProcAddress(HModule, "ElementType"));
    XELibDefType = reinterpret_cast<bool (*)(const unsigned int, unsigned char *)>(GetProcAddress(HModule, "DefType"));
    XELibSmashType = reinterpret_cast<bool (*)(const unsigned int, unsigned char *)>(GetProcAddress(HModule, "SmashType"));
    XELibValueType = reinterpret_cast<bool (*)(const unsigned int, unsigned char *)>(GetProcAddress(HModule, "ValueType"));
    XELibIsSorted = reinterpret_cast<bool (*)(const unsigned int, bool *)>(GetProcAddress(HModule, "IsSorted"));

    // Element Value
    XELibName = reinterpret_cast<bool (*)(const unsigned int, int *)>(GetProcAddress(HModule, "Name"));
    XELibLongName = reinterpret_cast<bool (*)(const unsigned int, int *)>(GetProcAddress(HModule, "LongName"));
    XELibDisplayName = reinterpret_cast<bool (*)(const unsigned int, int *)>(GetProcAddress(HModule, "DisplayName"));
    XELibPath =
        reinterpret_cast<bool (*)(const unsigned int, const bool, const bool, int *)>(GetProcAddress(HModule, "Path"));
    XELibSignature = reinterpret_cast<bool (*)(const unsigned int, int *)>(GetProcAddress(HModule, "Signature"));
    XELibGetValue =
        reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, int *)>(GetProcAddress(HModule, "GetValue"));
    XELibSetValue = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, const wchar_t *)>(
        GetProcAddress(HModule, "SetValue"));
    XELibGetIntValue =
        reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, int *)>(GetProcAddress(HModule, "GetIntValue"));
    XELibSetIntValue = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, const int)>(
        GetProcAddress(HModule, "SetIntValue"));
    XELibGetUIntValue = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, unsigned int *)>(
        GetProcAddress(HModule, "GetUIntValue"));
    XELibSetUIntValue = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, const unsigned int)>(
        GetProcAddress(HModule, "SetUIntValue"));
    XELibGetFloatValue = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, double *)>(
        GetProcAddress(HModule, "GetFloatValue"));
    XELibSetFloatValue = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, const double)>(
        GetProcAddress(HModule, "SetFloatValue"));
    XELibGetFlag = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, const wchar_t *, bool *)>(
        GetProcAddress(HModule, "GetFlag"));
    XELibSetFlag = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, const wchar_t *, const bool)>(
        GetProcAddress(HModule, "SetFlag"));
    XELibGetAllFlags =
        reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, int *)>(GetProcAddress(HModule, "GetAllFlags"));
    XELibGetEnabledFlags = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, int *)>(
        GetProcAddress(HModule, "GetEnabledFlags"));
    XELibSetEnabledFlags = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, const wchar_t *)>(
        GetProcAddress(HModule, "SetEnabledFlags"));
    XELibGetEnumOptions = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, int *)>(
        GetProcAddress(HModule, "GetEnumOptions"));
    XELibSignatureFromName =
        reinterpret_cast<bool (*)(const wchar_t *, int *)>(GetProcAddress(HModule, "SignatureFromName"));
    XELibNameFromSignature =
        reinterpret_cast<bool (*)(const wchar_t *, int *)>(GetProcAddress(HModule, "NameFromSignature"));
    XELibGetSignatureNameMap = reinterpret_cast<bool (*)(int *)>(GetProcAddress(HModule, "GetSignatureNameMap"));

    // Serialization
    XELibElementToJson = reinterpret_cast<bool (*)(const unsigned int, int *)>(GetProcAddress(HModule, "ElementToJson"));
    XELibElementFromJson = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, const wchar_t *)>(
        GetProcAddress(HModule, "ElementFromJson"));

    // Records
    XELibGetFormID = reinterpret_cast<bool (*)(const unsigned int, unsigned int *, const bool)>(
        GetProcAddress(HModule, "GetFormID"));
    XELibSetFormID = reinterpret_cast<bool (*)(const unsigned int, const unsigned int, const bool, const bool)>(
        GetProcAddress(HModule, "SetFormID"));
    XELibGetRecord = reinterpret_cast<bool (*)(const unsigned int, const unsigned int, const bool, unsigned int *)>(
        GetProcAddress(HModule, "GetRecord"));
    XELibGetRecords = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, const bool, int *)>(
        GetProcAddress(HModule, "GetRecords"));
    XELibGetREFRs = reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, const unsigned int, int *)>(
        GetProcAddress(HModule, "GetREFRs"));
    XELibGetOverrides = reinterpret_cast<bool (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetOverrides"));
    XELibGetMasterRecord =
        reinterpret_cast<bool (*)(const unsigned int, unsigned int *)>(GetProcAddress(HModule, "GetMasterRecord"));
    XELibGetWinningOverride =
        reinterpret_cast<bool (*)(const unsigned int, unsigned int *)>(GetProcAddress(HModule, "GetWinningOverride"));
    XELibGetInjectionTarget =
        reinterpret_cast<bool (*)(const unsigned int, unsigned int *)>(GetProcAddress(HModule, "GetInjectionTarget"));
    XELibFindNextRecord =
        reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, const bool, const bool, unsigned int *)>(
            GetProcAddress(HModule, "FindNextRecord"));
    XELibFindPreviousRecord =
        reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, const bool, const bool, unsigned int *)>(
            GetProcAddress(HModule, "FindPreviousRecord"));
    XELibFindValidReferences =
        reinterpret_cast<bool (*)(const unsigned int, const wchar_t *, const wchar_t *, const int, int *)>(
            GetProcAddress(HModule, "FindValidReferences"));
    XELibGetReferencedBy = reinterpret_cast<bool (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetReferencedBy"));
    XELibExchangeReferences = reinterpret_cast<bool (*)(const unsigned int, const unsigned int, const unsigned int)>(
        GetProcAddress(HModule, "ExchangeReferences"));
    XELibIsMaster = reinterpret_cast<bool (*)(const unsigned int, bool *)>(GetProcAddress(HModule, "IsMaster"));
    XELibIsInjected = reinterpret_cast<bool (*)(const unsigned int, bool *)>(GetProcAddress(HModule, "IsInjected"));
    XELibIsOverride = reinterpret_cast<bool (*)(const unsigned int, bool *)>(GetProcAddress(HModule, "IsOverride"));
    XELibIsWinningOverride =
        reinterpret_cast<bool (*)(const unsigned int, bool *)>(GetProcAddress(HModule, "IsWinningOverride"));
    XELibGetNodes = reinterpret_cast<bool (*)(const unsigned int, unsigned int *)>(GetProcAddress(HModule, "GetNodes"));
    XELibGetConflictData =
        reinterpret_cast<bool (*)(const unsigned int, const unsigned int, unsigned char *, unsigned char *)>(
            GetProcAddress(HModule, "GetConflictData"));
    XELibGetNodeElements = reinterpret_cast<bool (*)(const unsigned int, const unsigned int, int *)>(
        GetProcAddress(HModule, "GetNodeElements"));

    // Errors
    XELibCheckForErrors = reinterpret_cast<bool (*)(const unsigned int)>(GetProcAddress(HModule, "CheckForErrors"));
    XELibGetErrorThreadDone = reinterpret_cast<bool (*)()>(GetProcAddress(HModule, "GetErrorThreadDone"));
    XELibGetErrors = reinterpret_cast<bool (*)(int *)>(GetProcAddress(HModule, "GetErrors"));
    XELibGetErrorString = reinterpret_cast<bool (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetErrorString"));
    XELibRemoveIdenticalRecords = reinterpret_cast<bool (*)(const unsigned int, const bool, const bool)>(
        GetProcAddress(HModule, "RemoveIdenticalRecords"));

    // Filters
    XELibFilterRecord = reinterpret_cast<bool (*)(const unsigned int)>(GetProcAddress(HModule, "FilterRecord"));
    XELibResetFilter = reinterpret_cast<bool (*)()>(GetProcAddress(HModule, "ResetFilter"));
  }

  XELibInitXEdit();
}

XEditLibCpp::~XEditLibCpp() {
  if (HModule != nullptr) {
    XELibCloseXEdit();
    FreeLibrary(HModule);
  }
}

//
// Helpers
//

auto XEditLibCpp::getResultStringSafe(int Len) -> std::wstring {
  auto Str = make_unique<wchar_t[]>(Len);
  XELibGetResultString(Str.get(), Len);
  throwExceptionIfExists();

  std::wstring OutStr(Str.get());
  OutStr.erase(Len);

  return OutStr;
}

auto XEditLibCpp::getResultArraySafe(int Len) -> std::vector<unsigned int> {
  auto Arr = make_unique<unsigned int[]>(Len);
  XELibGetResultArray(Arr.get(), Len);
  throwExceptionIfExists();

  vector<unsigned int> OutArr;
  OutArr.resize(Len);
  for (int I = 0; I < Len; I++) {
    OutArr[I] = Arr.get()[I];
  }

  return OutArr;
}

auto XEditLibCpp::getResultBytesSafe(int Len) -> std::vector<unsigned char> {
  auto Bytes = make_unique<unsigned char[]>(Len);
  XELibGetResultBytes(Bytes.get(), Len);
  throwExceptionIfExists();

  vector<unsigned char> OutBytes;
  OutBytes.resize(Len);
  for (int I = 0; I < Len; I++) {
    OutBytes[I] = Bytes.get()[I];
  }

  return OutBytes;
}

auto XEditLibCpp::getExceptionMessageSafe() -> std::string {
  auto Len = make_unique<int>();
  XELibGetExceptionMessageLength(Len.get());

  if (Len == nullptr || *Len == 0) {
    return {};
  }

  auto ExceptionMessage = make_unique<wchar_t[]>(*Len);
  XELibGetExceptionMessage(ExceptionMessage.get(), *Len);

  return ParallaxGenUtil::wstrToStr(ExceptionMessage.get());
}

void XEditLibCpp::throwExceptionIfExists() {
  auto ExceptionMessage = getExceptionMessageSafe();
  if (!ExceptionMessage.empty()) {
    throw std::runtime_error(ExceptionMessage);
  }
}

//
// Function Implementation
//

// Meta
auto XEditLibCpp::getProgramPath() -> std::filesystem::path {
  auto Len = make_unique<int>();
  XELibGetGlobal(L"ProgramPath", Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

auto XEditLibCpp::getVersion() -> std::wstring {
  auto Len = make_unique<int>();
  XELibGetGlobal(L"Version", Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

auto XEditLibCpp::getGameName() -> std::wstring {
  auto Len = make_unique<int>();
  XELibGetGlobal(L"GameName", Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

auto XEditLibCpp::getAppName() -> std::wstring {
  auto Len = make_unique<int>();
  XELibGetGlobal(L"AppName", Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

auto XEditLibCpp::getLongGameName() -> std::wstring {
  auto Len = make_unique<int>();
  XELibGetGlobal(L"LongGameName", Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

auto XEditLibCpp::getDataPath() -> std::filesystem::path {
  auto Len = make_unique<int>();
  XELibGetGlobal(L"DataPath", Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

auto XEditLibCpp::getAppDataPath() -> std::filesystem::path {
  auto Len = make_unique<int>();
  XELibGetGlobal(L"AppDataPath", Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

auto XEditLibCpp::getMyGamesPath() -> std::filesystem::path {
  auto Len = make_unique<int>();
  XELibGetGlobal(L"MyGamesPath", Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

auto XEditLibCpp::getGameIniPath() -> std::filesystem::path {
  auto Len = make_unique<int>();
  XELibGetGlobal(L"GameIniPath", Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

void XEditLibCpp::setSortMode(const SortBy &SortBy, bool Reverse) {
  XELibSetSortMode(static_cast<unsigned char>(SortBy), Reverse);
  throwExceptionIfExists();
}

void XEditLibCpp::release(unsigned int Id) {
  XELibRelease(Id);
  throwExceptionIfExists();
}

void XEditLibCpp::releaseNodes(unsigned int Id) {
  XELibReleaseNodes(Id);
  throwExceptionIfExists();
}

auto XEditLibCpp::getDuplicateHandles(unsigned int Id) -> std::vector<unsigned int> {
  auto Len = make_unique<int>();
  XELibGetDuplicateHandles(Id, Len.get());
  throwExceptionIfExists();

  return getResultArraySafe(*Len);
}

// Messages

auto XEditLibCpp::getMessages() -> std::wstring {
  auto Len = make_unique<int>();
  XELibGetMessagesLength(Len.get());
  throwExceptionIfExists();

  auto Messages = make_unique<wchar_t[]>(*Len);
  XELibGetMessages(Messages.get(), *Len);
  throwExceptionIfExists();

  return Messages.get();
}

// Setup

auto XEditLibCpp::getGamePath(const GameMode &Mode) -> std::filesystem::path {
  auto Len = make_unique<int>();
  XELibGetGamePath(static_cast<int>(Mode), Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

void XEditLibCpp::setGamePath(const std::filesystem::path &Path) {
  XELibSetGamePath(Path.wstring().c_str());
  throwExceptionIfExists();
}

auto XEditLibCpp::getGameLanguage(const GameMode &Mode) -> wstring {
  auto Len = make_unique<int>();
  XELibGetGameLanguage(static_cast<int>(Mode), Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

void XEditLibCpp::setLanguage(const wstring &Lang) {
  XELibSetLanguage(Lang.c_str());
  throwExceptionIfExists();
}

void XEditLibCpp::setBackupPath(const std::filesystem::path &Path) {
  XELibSetBackupPath(Path.wstring().c_str());
  throwExceptionIfExists();
}

void XEditLibCpp::setGameMode(const GameMode &Mode) {
  XELibSetGameMode(static_cast<int>(Mode));
  throwExceptionIfExists();
}

auto XEditLibCpp::getLoadOrder() -> std::vector<wstring> {
  auto Len = make_unique<int>();
  XELibGetLoadOrder(Len.get());
  throwExceptionIfExists();

  const auto LoadOrderStr = getResultStringSafe(*Len);

  vector<wstring> OutputLoadOrder;
  boost::split(OutputLoadOrder, LoadOrderStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputLoadOrder;
}

auto XEditLibCpp::getActivePlugins() -> std::vector<wstring> {
  auto Len = make_unique<int>();
  XELibGetActivePlugins(Len.get());
  throwExceptionIfExists();

  const auto ActivePluginsStr = getResultStringSafe(*Len);

  vector<wstring> OutputActivePlugins;
  boost::split(OutputActivePlugins, ActivePluginsStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputActivePlugins;
}

void XEditLibCpp::loadPlugins(const std::vector<wstring> &LoadOrder, bool SmartLoad) {
  const wstring LoadOrderStr = boost::algorithm::join(LoadOrder, L"\r\n");
  XELibLoadPlugins(LoadOrderStr.c_str(), SmartLoad);
  throwExceptionIfExists();
}

void XEditLibCpp::loadPlugin(const std::wstring &Filename) {
  XELibLoadPlugin(Filename.c_str());
  throwExceptionIfExists();
}

auto XEditLibCpp::loadPluginHeader(const std::wstring &Filename) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibLoadPluginHeader(Filename.c_str(), Res.get());
  throwExceptionIfExists();

  return *Res;
}

void XEditLibCpp::buildReferences(const unsigned int &Id, const bool &Synchronous) {
  XELibBuildReferences(Id, Synchronous);
  throwExceptionIfExists();
}

auto XEditLibCpp::getLoaderStatus() -> LoaderStates {
  auto Status = make_unique<unsigned char>();
  XELibGetLoaderStatus(Status.get());
  throwExceptionIfExists();

  return static_cast<LoaderStates>(*Status);
}

void XEditLibCpp::unloadPlugin(const unsigned int &Id) {
  XELibUnloadPlugin(Id);
  throwExceptionIfExists();
}

// Files

auto XEditLibCpp::addFile(const std::filesystem::path &Path) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibAddFile(Path.wstring().c_str(), Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::fileByIndex(int Index) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibFileByIndex(Index, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::fileByLoadOrder(int LoadOrder) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibFileByLoadOrder(LoadOrder, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::fileByName(const std::wstring &Filename) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibFileByName(Filename.c_str(), Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::fileByAuthor(const std::wstring &Author) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibFileByAuthor(Author.c_str(), Res.get());
  throwExceptionIfExists();

  return *Res;
}

void XEditLibCpp::saveFile(const unsigned int &Id, const std::filesystem::path &Path) {
  XELibSaveFile(Id, Path.wstring().c_str());
  throwExceptionIfExists();
}

auto XEditLibCpp::getRecordCount(const unsigned int &Id) -> int {
  auto Res = make_unique<int>();
  XELibGetRecordCount(Id, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::getOverrideRecordCount(const unsigned int &Id) -> int {
  auto Res = make_unique<int>();
  XELibGetOverrideRecordCount(Id, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::md5Hash(const unsigned int &Id) -> wstring {
  auto Len = make_unique<int>();
  XELibMD5Hash(Id, Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

auto XEditLibCpp::crcHash(const unsigned int &Id) -> wstring {
  auto Len = make_unique<int>();
  XELibCRCHash(Id, Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

auto XEditLibCpp::getFileLoadOrder(const unsigned int &Id) -> int {
  auto Res = make_unique<int>();
  XELibGetFileLoadOrder(Id, Res.get());
  throwExceptionIfExists();

  return *Res;
}

// Archives

void XEditLibCpp::extractContainer(const std::wstring &Name, const std::filesystem::path &Destination,
                                   const bool &Replace) {
  XELibExtractContainer(Name.c_str(), Destination.wstring().c_str(), Replace);
  throwExceptionIfExists();
}

void XEditLibCpp::extractFile(const std::wstring &Name, const std::filesystem::path &Source,
                              const std::filesystem::path &Destination) {
  XELibExtractFile(Name.c_str(), Source.c_str(), Destination.wstring().c_str());
  throwExceptionIfExists();
}

auto XEditLibCpp::getContainerFiles(const std::wstring &Name,
                                    const std::filesystem::path &Path) -> std::vector<std::wstring> {
  auto Len = make_unique<int>();
  XELibGetContainerFiles(Name.c_str(), Path.wstring().c_str(), Len.get());
  throwExceptionIfExists();

  const auto ContainerFilesStr = getResultStringSafe(*Len);

  vector<wstring> OutputContainerFiles;
  boost::split(OutputContainerFiles, ContainerFilesStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputContainerFiles;
}

auto XEditLibCpp::getFileContainer(const std::filesystem::path &Path) -> std::wstring {
  auto Len = make_unique<int>();
  XELibGetFileContainer(Path.wstring().c_str(), Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

auto XEditLibCpp::getLoadedContainers() -> std::vector<std::wstring> {
  auto Len = make_unique<int>();
  XELibGetLoadedContainers(Len.get());
  throwExceptionIfExists();

  const auto LoadedContainersStr = getResultStringSafe(*Len);

  vector<wstring> OutputLoadedContainers;
  boost::split(OutputLoadedContainers, LoadedContainersStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputLoadedContainers;
}

void XEditLibCpp::loadContainer(const std::filesystem::path &Path) {
  XELibLoadContainer(Path.wstring().c_str());
  throwExceptionIfExists();
}

void XEditLibCpp::buildArchive(const std::wstring &Name, const std::filesystem::path &Folder,
                               const std::vector<std::filesystem::path> &FilePaths, const ArchiveType &Type,
                               bool BCompress, bool BShare, const std::wstring &AF, const std::wstring &FF) {
  // convert filesystem vector to wstring vector
  vector<wstring> FilePathsStr;
  FilePathsStr.reserve(FilePaths.size());
for (const auto &FilePath : FilePaths) {
    FilePathsStr.push_back(FilePath.wstring());
  }

  XELibBuildArchive(Name.c_str(), Folder.wstring().c_str(), boost::algorithm::join(FilePathsStr, L"\r\n").c_str(),
               static_cast<int>(Type), BCompress, BShare, AF.c_str(), FF.c_str());
  throwExceptionIfExists();
}

auto XEditLibCpp::getTextureData(const std::wstring &ResourceName) -> std::pair<int, int> {
  auto Width = make_unique<int>();
  auto Height = make_unique<int>();
  XELibGetTextureData(ResourceName.c_str(), Width.get(), Height.get());
  throwExceptionIfExists();

  return {*Width, *Height};
}

// Elements

auto XEditLibCpp::hasElement(const unsigned int &Id, const std::wstring &Key) -> bool {
  auto Res = make_unique<bool>();
  XELibHasElement(Id, Key.c_str(), Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::getElement(const unsigned int &Id, const std::wstring &Key) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibGetElement(Id, Key.c_str(), Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::addElement(const unsigned int &Id, const std::wstring &Key) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibAddElement(Id, Key.c_str(), Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::addElementValue(const unsigned int &Id, const std::wstring &Key,
                                  const std::wstring &Value) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibAddElementValue(Id, Key.c_str(), Value.c_str(), Res.get());
  throwExceptionIfExists();

  return *Res;
}

void XEditLibCpp::removeElement(const unsigned int &Id, const std::wstring &Key) {
  XELibRemoveElement(Id, Key.c_str());
  throwExceptionIfExists();
}

void XEditLibCpp::removeElementOrParent(const unsigned int &Id) {
  XELibRemoveElementOrParent(Id);
  throwExceptionIfExists();
}

void XEditLibCpp::setElement(const unsigned int &Id, const unsigned int &Id2) {
  XELibSetElement(Id, Id2);
  throwExceptionIfExists();
}

auto XEditLibCpp::getElements(const unsigned int &Id, const std::wstring &Key, const bool &Sort,
                              const bool &Filter) -> std::vector<unsigned int> {
  auto Len = make_unique<int>();
  XELibGetElements(Id, Key.c_str(), Sort, Filter, Len.get());
  throwExceptionIfExists();

  return getResultArraySafe(*Len);
}

auto XEditLibCpp::getDefNames(const unsigned int &Id) -> std::vector<std::wstring> {
  auto Len = make_unique<int>();
  XELibGetDefNames(Id, Len.get());
  throwExceptionIfExists();

  const auto DefNamesStr = getResultStringSafe(*Len);

  vector<wstring> OutputDefNames;
  boost::split(OutputDefNames, DefNamesStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputDefNames;
}

auto XEditLibCpp::getAddList(const unsigned int &Id) -> std::vector<std::wstring> {
  auto Len = make_unique<int>();
  XELibGetAddList(Id, Len.get());
  throwExceptionIfExists();

  const auto AddListStr = getResultStringSafe(*Len);

  vector<wstring> OutputAddList;
  boost::split(OutputAddList, AddListStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputAddList;
}

auto XEditLibCpp::getLinksTo(const unsigned int &Id, const std::wstring &Key) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibGetLinksTo(Id, Key.c_str(), Res.get());
  throwExceptionIfExists();

  return *Res;
}

void XEditLibCpp::setLinksTo(const unsigned int &Id, const std::wstring &Key, const unsigned int &Id2) {
  XELibSetLinksTo(Id, Key.c_str(), Id2);
  throwExceptionIfExists();
}

auto XEditLibCpp::getElementIndex(const unsigned int &Id) -> int {
  auto Res = make_unique<int>();
  XELibGetElementIndex(Id, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::getContainer(const unsigned int &Id) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibGetContainer(Id, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::getElementFile(const unsigned int &Id) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibGetElementFile(Id, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::getElementRecord(const unsigned int &Id) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibGetElementRecord(Id, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::elementCount(const unsigned int &Id) -> int {
  auto Res = make_unique<int>();
  XELibElementCount(Id, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::elementEquals(const unsigned int &Id, const unsigned int &Id2) -> bool {
  auto Res = make_unique<bool>();
  XELibElementEquals(Id, Id2, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::elementMatches(const unsigned int &Id, const std::wstring &Key, const std::wstring &Value) -> bool {
  auto Res = make_unique<bool>();
  XELibElementMatches(Id, Key.c_str(), Value.c_str(), Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::hasArrayItem(const unsigned int &Id, const std::wstring &Key, const std::wstring &SubKey,
                               const std::wstring &Value) -> bool {
  auto Res = make_unique<bool>();
  XELibHasArrayItem(Id, Key.c_str(), SubKey.c_str(), Value.c_str(), Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::getArrayItem(const unsigned int &Id, const std::wstring &Key, const std::wstring &SubKey,
                               const std::wstring &Value) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibGetArrayItem(Id, Key.c_str(), SubKey.c_str(), Value.c_str(), Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::addArrayItem(const unsigned int &Id, const std::wstring &Key, const std::wstring &SubKey,
                               const std::wstring &Value) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibAddArrayItem(Id, Key.c_str(), SubKey.c_str(), Value.c_str(), Res.get());
  throwExceptionIfExists();

  return *Res;
}

void XEditLibCpp::removeArrayItem(const unsigned int &Id, const std::wstring &Key, const std::wstring &SubKey,
                                  const std::wstring &Value) {
  XELibRemoveArrayItem(Id, Key.c_str(), SubKey.c_str(), Value.c_str());
  throwExceptionIfExists();
}

void XEditLibCpp::moveArrayItem(const unsigned int &Id, const int &Index) {
  XELibMoveArrayItem(Id, Index);
  throwExceptionIfExists();
}

auto XEditLibCpp::copyElement(const unsigned int &Id, const unsigned int &Id2, const bool &AsNew) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibCopyElement(Id, Id2, AsNew, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::getSignatureAllowed(const unsigned int &Id, const std::wstring &Signature) -> bool {
  auto Res = make_unique<bool>();
  XELibGetSignatureAllowed(Id, Signature.c_str(), Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::getAllowedSignatures(const unsigned int &Id) -> std::vector<std::wstring> {
  auto Len = make_unique<int>();
  XELibGetAllowedSignatures(Id, Len.get());
  throwExceptionIfExists();

  const auto AllowedSignaturesStr = getResultStringSafe(*Len);

  vector<wstring> OutputAllowedSignatures;
  boost::split(OutputAllowedSignatures, AllowedSignaturesStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputAllowedSignatures;
}

auto XEditLibCpp::getIsModified(const unsigned int &Id) -> bool {
  auto Res = make_unique<bool>();
  XELibGetIsModified(Id, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::getIsEditable(const unsigned int &Id) -> bool {
  auto Res = make_unique<bool>();
  XELibGetIsEditable(Id, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::getIsRemoveable(const unsigned int &Id) -> bool {
  auto Res = make_unique<bool>();
  XELibGetIsRemoveable(Id, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::getCanAdd(const unsigned int &Id) -> bool {
  auto Res = make_unique<bool>();
  XELibGetCanAdd(Id, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::sortKey(const unsigned int &Id) -> wstring {
  auto Len = make_unique<int>();
  XELibSortKey(Id, Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

auto XEditLibCpp::elementType(const unsigned int &Id) -> ElementType {
  auto Res = make_unique<unsigned char>();
  XELibElementType(Id, Res.get());
  throwExceptionIfExists();

  return static_cast<ElementType>(*Res);
}

auto XEditLibCpp::defType(const unsigned int &Id) -> DefType {
  auto Res = make_unique<unsigned char>();
  XELibDefType(Id, Res.get());
  throwExceptionIfExists();

  return static_cast<DefType>(*Res);
}

auto XEditLibCpp::smashType(const unsigned int &Id) -> SmashType {
  auto Res = make_unique<unsigned char>();
  XELibSmashType(Id, Res.get());
  throwExceptionIfExists();

  return static_cast<SmashType>(*Res);
}

auto XEditLibCpp::valueType(const unsigned int &Id) -> ValueType {
  auto Res = make_unique<unsigned char>();
  XELibValueType(Id, Res.get());
  throwExceptionIfExists();

  return static_cast<ValueType>(*Res);
}

auto XEditLibCpp::isSorted(const unsigned int &Id) -> bool {
  auto Res = make_unique<bool>();
  XELibIsSorted(Id, Res.get());
  throwExceptionIfExists();

  return *Res;
}

// Element Value

auto XEditLibCpp::name(const unsigned int &Id) -> wstring {
  auto Len = make_unique<int>();
  XELibName(Id, Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

auto XEditLibCpp::longName(const unsigned int &Id) -> wstring {
  auto Len = make_unique<int>();
  XELibLongName(Id, Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

auto XEditLibCpp::displayName(const unsigned int &Id) -> wstring {
  auto Len = make_unique<int>();
  XELibDisplayName(Id, Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

auto XEditLibCpp::path(const unsigned int &Id, const bool &ShortPath, const bool &Local) -> std::wstring {
  auto Len = make_unique<int>();
  XELibPath(Id, ShortPath, Local, Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

auto XEditLibCpp::signature(const unsigned int &Id) -> wstring {
  auto Len = make_unique<int>();
  XELibSignature(Id, Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

auto XEditLibCpp::getValue(const unsigned int &Id, const std::wstring &Key) -> wstring {
  auto Len = make_unique<int>();
  XELibGetValue(Id, Key.c_str(), Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

void XEditLibCpp::setValue(const unsigned int &Id, const std::wstring &Key, const std::wstring &Value) {
  XELibSetValue(Id, Key.c_str(), Value.c_str());
  throwExceptionIfExists();
}

auto XEditLibCpp::getIntValue(const unsigned int &Id, const std::wstring &Key) -> int {
  auto Res = make_unique<int>();
  XELibGetIntValue(Id, Key.c_str(), Res.get());
  throwExceptionIfExists();

  return *Res;
}

void XEditLibCpp::setIntValue(const unsigned int &Id, const std::wstring &Key, const int &Value) {
  XELibSetIntValue(Id, Key.c_str(), Value);
  throwExceptionIfExists();
}

auto XEditLibCpp::getUIntValue(const unsigned int &Id, const std::wstring &Key) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibGetUIntValue(Id, Key.c_str(), Res.get());
  throwExceptionIfExists();

  return *Res;
}

void XEditLibCpp::setUIntValue(const unsigned int &Id, const std::wstring &Key, const unsigned int &Value) {
  XELibSetUIntValue(Id, Key.c_str(), Value);
  throwExceptionIfExists();
}

auto XEditLibCpp::getFloatValue(const unsigned int &Id, const std::wstring &Key) -> double {
  auto Res = make_unique<double>();
  XELibGetFloatValue(Id, Key.c_str(), Res.get());
  throwExceptionIfExists();

  return *Res;
}

void XEditLibCpp::setFloatValue(const unsigned int &Id, const std::wstring &Key, const double &Value) {
  XELibSetFloatValue(Id, Key.c_str(), Value);
  throwExceptionIfExists();
}

auto XEditLibCpp::getFlag(const unsigned int &Id, const std::wstring &Key, const std::wstring &Flag) -> bool {
  auto Res = make_unique<bool>();
  XELibGetFlag(Id, Key.c_str(), Flag.c_str(), Res.get());
  throwExceptionIfExists();

  return *Res;
}

void XEditLibCpp::setFlag(const unsigned int &Id, const std::wstring &Key, const std::wstring &Flag,
                          const bool &Enabled) {
  XELibSetFlag(Id, Key.c_str(), Flag.c_str(), Enabled);
  throwExceptionIfExists();
}

auto XEditLibCpp::getAllFlags(const unsigned int &Id, const std::wstring &Key) -> std::vector<std::wstring> {
  auto Len = make_unique<int>();
  XELibGetAllFlags(Id, Key.c_str(), Len.get());
  throwExceptionIfExists();

  const auto AllFlagsStr = getResultStringSafe(*Len);

  vector<wstring> OutputAllFlags;
  boost::split(OutputAllFlags, AllFlagsStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputAllFlags;
}

auto XEditLibCpp::getEnabledFlags(const unsigned int &Id, const std::wstring &Key) -> std::vector<std::wstring> {
  auto Len = make_unique<int>();
  XELibGetEnabledFlags(Id, Key.c_str(), Len.get());
  throwExceptionIfExists();

  const auto EnabledFlagsStr = getResultStringSafe(*Len);

  vector<wstring> OutputEnabledFlags;
  boost::split(OutputEnabledFlags, EnabledFlagsStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputEnabledFlags;
}

void XEditLibCpp::setEnabledFlags(const unsigned int &Id, const std::wstring &Key, const std::wstring &Flags) {
  XELibSetEnabledFlags(Id, Key.c_str(), Flags.c_str());
  throwExceptionIfExists();
}

auto XEditLibCpp::getEnumOptions(const unsigned int &Id, const std::wstring &Key) -> std::vector<std::wstring> {
  auto Len = make_unique<int>();
  XELibGetEnumOptions(Id, Key.c_str(), Len.get());
  throwExceptionIfExists();

  const auto EnumOptionsStr = getResultStringSafe(*Len);

  vector<wstring> OutputEnumOptions;
  boost::split(OutputEnumOptions, EnumOptionsStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputEnumOptions;
}

auto XEditLibCpp::signatureFromName(const std::wstring &Name) -> wstring {
  auto Len = make_unique<int>();
  XELibSignatureFromName(Name.c_str(), Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

auto XEditLibCpp::nameFromSignature(const std::wstring &Signature) -> wstring {
  auto Len = make_unique<int>();
  XELibNameFromSignature(Signature.c_str(), Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

auto XEditLibCpp::getSignatureNameMap() -> vector<wstring> {
  auto Len = make_unique<int>();
  XELibGetSignatureNameMap(Len.get());
  throwExceptionIfExists();

  const auto SignatureNameMapStr = getResultStringSafe(*Len);

  vector<wstring> OutputSignatureNameMap;
  boost::split(OutputSignatureNameMap, SignatureNameMapStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputSignatureNameMap;
}

// Serialization

auto XEditLibCpp::elementToJson(const unsigned int &Id) -> wstring {
  auto Len = make_unique<int>();
  XELibElementToJson(Id, Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

void XEditLibCpp::elementFromJson(const unsigned int &Id, const std::filesystem::path &Path, const std::wstring &JSON) {
  XELibElementFromJson(Id, Path.wstring().c_str(), JSON.c_str());
  throwExceptionIfExists();
}

// Records

auto XEditLibCpp::getFormID(const unsigned int &Id, const bool &Local) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibGetFormID(Id, Res.get(), Local);
  throwExceptionIfExists();

  return *Res;
}

void XEditLibCpp::setFormID(const unsigned int &Id, const unsigned int &FormID, const bool &Local,
                            const bool &FixRefs) {
  XELibSetFormID(Id, FormID, Local, FixRefs);
  throwExceptionIfExists();
}

auto XEditLibCpp::getRecord(const unsigned int &Id, const unsigned int &FormID,
                            const bool &SearchMasters) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibGetRecord(Id, FormID, SearchMasters, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::getRecords(const unsigned int &Id, const std::wstring &Search,
                             const bool &IncludeOverrides) -> std::vector<unsigned int> {
  auto Len = make_unique<int>();
  XELibGetRecords(Id, Search.c_str(), IncludeOverrides, Len.get());
  throwExceptionIfExists();

  return getResultArraySafe(*Len);
}

auto XEditLibCpp::getREFRs(const unsigned int &Id, const std::wstring &Search,
                           const unsigned int &Flags) -> std::vector<unsigned int> {
  auto Len = make_unique<int>();
  XELibGetREFRs(Id, Search.c_str(), Flags, Len.get());
  throwExceptionIfExists();

  return getResultArraySafe(*Len);
}

auto XEditLibCpp::getOverrides(const unsigned int &Id) -> std::vector<unsigned int> {
  auto Len = make_unique<int>();
  XELibGetOverrides(Id, Len.get());
  throwExceptionIfExists();

  return getResultArraySafe(*Len);
}

auto XEditLibCpp::getMasterRecord(const unsigned int &Id) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibGetMasterRecord(Id, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::getWinningOverride(const unsigned int &Id) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibGetWinningOverride(Id, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::getInjectionTarget(const unsigned int &Id) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibGetInjectionTarget(Id, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::findNextRecord(const unsigned int &Id, const std::wstring &Search, const bool &ByEdid,
                                 const bool &ByName) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibFindNextRecord(Id, Search.c_str(), ByEdid, ByName, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::findPreviousRecord(const unsigned int &Id, const std::wstring &Search, const bool &ByEdid,
                                     const bool &ByName) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibFindPreviousRecord(Id, Search.c_str(), ByEdid, ByName, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::findValidReferences(const unsigned int &Id, const std::wstring &Signature, const std::wstring &Search,
                                      const int &LimitTo) -> std::vector<unsigned int> {
  auto Len = make_unique<int>();
  XELibFindValidReferences(Id, Signature.c_str(), Search.c_str(), LimitTo, Len.get());
  throwExceptionIfExists();

  return getResultArraySafe(*Len);
}

auto XEditLibCpp::getReferencedBy(const unsigned int &Id) -> std::vector<unsigned int> {
  auto Len = make_unique<int>();
  XELibGetReferencedBy(Id, Len.get());
  throwExceptionIfExists();

  return getResultArraySafe(*Len);
}

void XEditLibCpp::exchangeReferences(const unsigned int &Id, const unsigned int &OldFormID,
                                     const unsigned int &NewFormID) {
  XELibExchangeReferences(Id, OldFormID, NewFormID);
  throwExceptionIfExists();
}

auto XEditLibCpp::isMaster(const unsigned int &Id) -> bool {
  auto Res = make_unique<bool>();
  XELibIsMaster(Id, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::isInjected(const unsigned int &Id) -> bool {
  auto Res = make_unique<bool>();
  XELibIsInjected(Id, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::isOverride(const unsigned int &Id) -> bool {
  auto Res = make_unique<bool>();
  XELibIsOverride(Id, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::isWinningOverride(const unsigned int &Id) -> bool {
  auto Res = make_unique<bool>();
  XELibIsWinningOverride(Id, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::getNodes(const unsigned int &Id) -> unsigned int {
  auto Res = make_unique<unsigned int>();
  XELibGetNodes(Id, Res.get());
  throwExceptionIfExists();

  return *Res;
}

auto XEditLibCpp::getConflictData(const unsigned int &Id,
                                  const unsigned int &Id2) -> std::pair<ConflictAll, ConflictThis> {
  auto Res1 = make_unique<unsigned char>();
  auto Res2 = make_unique<unsigned char>();
  XELibGetConflictData(Id, Id2, Res1.get(), Res2.get());
  throwExceptionIfExists();

  return {static_cast<ConflictAll>(*Res1), static_cast<ConflictThis>(*Res2)};
}

auto XEditLibCpp::getNodeElements(const unsigned int &Id, const unsigned int &Id2) -> vector<unsigned int> {
  auto Res = make_unique<int>();
  XELibGetNodeElements(Id, Id2, Res.get());
  throwExceptionIfExists();

  return getResultArraySafe(*Res);
}

// Errors

void XEditLibCpp::checkForErrors(const unsigned int &Id) {
  XELibCheckForErrors(Id);
  throwExceptionIfExists();
}

auto XEditLibCpp::getErrorThreadDone() -> bool {
  bool Result = XELibGetErrorThreadDone();
  throwExceptionIfExists();
  return Result;
}

auto XEditLibCpp::getErrors() -> std::vector<std::wstring> {
  auto Len = make_unique<int>();
  XELibGetErrors(Len.get());
  throwExceptionIfExists();

  const auto ErrorsStr = getResultStringSafe(*Len);

  vector<wstring> OutputErrors;
  boost::split(OutputErrors, ErrorsStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputErrors;
}

auto XEditLibCpp::getErrorString(const unsigned int &Id) -> std::wstring {
  auto Len = make_unique<int>();
  XELibGetErrorString(Id, Len.get());
  throwExceptionIfExists();

  return getResultStringSafe(*Len);
}

void XEditLibCpp::removeIdenticalRecords(const unsigned int &Id, const bool &RemoveITMs, const bool &RemoveITPOs) {
  XELibRemoveIdenticalRecords(Id, RemoveITMs, RemoveITPOs);
  throwExceptionIfExists();
}

// Filters

void XEditLibCpp::filterRecord(const unsigned int &Id) {
  XELibFilterRecord(Id);
  throwExceptionIfExists();
}

void XEditLibCpp::resetFilter() {
  XELibResetFilter();
  throwExceptionIfExists();
}
