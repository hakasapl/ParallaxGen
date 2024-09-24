#include "XEditLibCpp.hpp"

#include <boost/algorithm/string.hpp>
#include <memory>
#include <wtypes.h>

#include "ParallaxGenUtil.hpp"

using namespace std;

XEditLibCpp::XEditLibCpp() : HModule(LoadLibrary(TEXT("XEditLib.dll"))) {
  if (HModule != nullptr) {
    // Meta
    XELibInitXEdit = reinterpret_cast<void (*)()>(GetProcAddress(HModule, "InitXEdit"));
    XELibCloseXEdit = reinterpret_cast<void (*)()>(GetProcAddress(HModule, "CloseXEdit"));
    XELibGetResultString = reinterpret_cast<VARIANT_BOOL (*)(wchar_t *, int)>(GetProcAddress(HModule, "GetResultString"));
    XELibGetResultArray = reinterpret_cast<VARIANT_BOOL (*)(unsigned int *, int)>(GetProcAddress(HModule, "GetResultArray"));
    XELibGetResultBytes = reinterpret_cast<VARIANT_BOOL (*)(unsigned char *, int)>(GetProcAddress(HModule, "GetResultBytes"));
    XELibGetGlobal = reinterpret_cast<VARIANT_BOOL (*)(const wchar_t *, int *)>(GetProcAddress(HModule, "GetGlobal"));
    XELibGetGlobals = reinterpret_cast<VARIANT_BOOL (*)(int *)>(GetProcAddress(HModule, "GetGlobals"));
    XELibSetSortMode = reinterpret_cast<VARIANT_BOOL (*)(const unsigned char, const VARIANT_BOOL)>(GetProcAddress(HModule, "SetSortMode"));
    XELibRelease = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int)>(GetProcAddress(HModule, "Release"));
    XELibReleaseNodes = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int)>(GetProcAddress(HModule, "ReleaseNodes"));
    XELibSwitch = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const unsigned int)>(GetProcAddress(HModule, "Switch"));
    XELibGetDuplicateHandles =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetDuplicateHandles"));
    XELibResetStore = reinterpret_cast<VARIANT_BOOL (*)()>(GetProcAddress(HModule, "ResetStore"));

    // Message
    XELibGetMessagesLength = reinterpret_cast<void (*)(int *)>(GetProcAddress(HModule, "GetMessagesLength"));
    XELibGetMessages = reinterpret_cast<VARIANT_BOOL (*)(wchar_t *, int)>(GetProcAddress(HModule, "GetMessages"));
    XELibClearMessages = reinterpret_cast<void (*)()>(GetProcAddress(HModule, "ClearMessages"));
    XELibGetExceptionMessageLength = reinterpret_cast<void (*)(int *)>(GetProcAddress(HModule, "GetExceptionMessageLength"));
    XELibGetExceptionMessage = reinterpret_cast<VARIANT_BOOL (*)(wchar_t *, int)>(GetProcAddress(HModule, "GetExceptionMessage"));

    // Setup
    XELibGetGamePath = reinterpret_cast<VARIANT_BOOL (*)(const int, int *)>(GetProcAddress(HModule, "GetGamePath"));
    XELibSetGamePath = reinterpret_cast<VARIANT_BOOL (*)(const wchar_t *)>(GetProcAddress(HModule, "SetGamePath"));
    XELibGetGameLanguage = reinterpret_cast<VARIANT_BOOL (*)(const int, int *)>(GetProcAddress(HModule, "GetGameLanguage"));
    XELibSetLanguage = reinterpret_cast<VARIANT_BOOL (*)(const wchar_t *)>(GetProcAddress(HModule, "SetLanguage"));
    XELibSetBackupPath = reinterpret_cast<VARIANT_BOOL (*)(const wchar_t *)>(GetProcAddress(HModule, "SetBackupPath"));
    XELibSetGameMode = reinterpret_cast<VARIANT_BOOL (*)(const int)>(GetProcAddress(HModule, "SetGameMode"));
    XELibGetLoadOrder = reinterpret_cast<VARIANT_BOOL (*)(int *)>(GetProcAddress(HModule, "GetLoadOrder"));
    XELibGetActivePlugins = reinterpret_cast<VARIANT_BOOL (*)(int *)>(GetProcAddress(HModule, "GetActivePlugins"));
    XELibLoadPlugins = reinterpret_cast<VARIANT_BOOL (*)(const wchar_t *, const VARIANT_BOOL)>(GetProcAddress(HModule, "LoadPlugins"));
    XELibLoadPlugin = reinterpret_cast<VARIANT_BOOL (*)(const wchar_t *)>(GetProcAddress(HModule, "LoadPlugin"));
    XELibLoadPluginHeader =
        reinterpret_cast<VARIANT_BOOL (*)(const wchar_t *, unsigned int *)>(GetProcAddress(HModule, "LoadPluginHeader"));
    XELibBuildReferences =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const VARIANT_BOOL)>(GetProcAddress(HModule, "BuildReferences"));
    XELibGetLoaderStatus = reinterpret_cast<VARIANT_BOOL (*)(unsigned char *)>(GetProcAddress(HModule, "GetLoaderStatus"));
    XELibUnloadPlugin = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int)>(GetProcAddress(HModule, "UnloadPlugin"));

    // Files
    XELibAddFile = reinterpret_cast<VARIANT_BOOL (*)(const wchar_t *, unsigned int *)>(GetProcAddress(HModule, "AddFile"));
    XELibFileByIndex = reinterpret_cast<VARIANT_BOOL (*)(const int, unsigned int *)>(GetProcAddress(HModule, "FileByIndex"));
    XELibFileByLoadOrder = reinterpret_cast<VARIANT_BOOL (*)(const int, unsigned int *)>(GetProcAddress(HModule, "FileByLoadOrder"));
    XELibFileByName = reinterpret_cast<VARIANT_BOOL (*)(const wchar_t *, unsigned int *)>(GetProcAddress(HModule, "FileByName"));
    XELibFileByAuthor = reinterpret_cast<VARIANT_BOOL (*)(const wchar_t *, unsigned int *)>(GetProcAddress(HModule, "FileByAuthor"));
    XELibSaveFile = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *)>(GetProcAddress(HModule, "SaveFile"));
    XELibGetRecordCount = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetRecordCount"));
    XELibGetOverrideRecordCount =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetOverrideRecordCount"));
    XELibMD5Hash = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "MD5Hash"));
    XELibCRCHash = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "CRCHash"));
    XELibSortEditorIDs = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, wchar_t *)>(GetProcAddress(HModule, "SortEditorIDs"));
    XELibSortNames = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, wchar_t *)>(GetProcAddress(HModule, "SortNames"));
    XELibGetFileLoadOrder =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetFileLoadOrder"));

    // Archives
    XELibExtractContainer =
        reinterpret_cast<VARIANT_BOOL (*)(const wchar_t *, const wchar_t *, VARIANT_BOOL)>(GetProcAddress(HModule, "ExtractContainer"));
    XELibExtractFile = reinterpret_cast<VARIANT_BOOL (*)(const wchar_t *, const wchar_t *, const wchar_t *)>(
        GetProcAddress(HModule, "ExtractFile"));
    XELibGetContainerFiles = reinterpret_cast<VARIANT_BOOL (*)(const wchar_t *, const wchar_t *, int *)>(
        GetProcAddress(HModule, "GetContainerFiles"));
    XELibGetFileContainer = reinterpret_cast<VARIANT_BOOL (*)(const wchar_t *, int *)>(GetProcAddress(HModule, "GetFileContainer"));
    XELibGetLoadedContainers = reinterpret_cast<VARIANT_BOOL (*)(int *)>(GetProcAddress(HModule, "GetLoadedContainers"));
    XELibLoadContainer = reinterpret_cast<VARIANT_BOOL (*)(const wchar_t *)>(GetProcAddress(HModule, "LoadContainer"));
    XELibBuildArchive =
        reinterpret_cast<VARIANT_BOOL (*)(const wchar_t *, const wchar_t *, const wchar_t *, const int, const VARIANT_BOOL, const VARIANT_BOOL,
                                  const wchar_t *, const wchar_t *)>(GetProcAddress(HModule, "BuildArchive"));
    XELibGetTextureData =
        reinterpret_cast<VARIANT_BOOL (*)(const wchar_t *, int *, int *)>(GetProcAddress(HModule, "GetTextureData"));

    // Masters
    XELibCleanMasters = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int)>(GetProcAddress(HModule, "CleanMasters"));
    XELibSortMasters = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int)>(GetProcAddress(HModule, "SortMasters"));
    XELibAddMaster = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *)>(GetProcAddress(HModule, "AddMaster"));
    XELibAddMasters = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *)>(GetProcAddress(HModule, "AddMasters"));
    XELibAddRequiredMasters =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const unsigned int, VARIANT_BOOL)>(GetProcAddress(HModule, "AddRequiredMasters"));
    XELibGetMasters = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetMasters"));
    XELibGetRequiredBy = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetRequiredBy"));
    XELibGetMasterNames = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetMasterNames"));

    // Elements
    XELibHasElement =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, VARIANT_BOOL *)>(GetProcAddress(HModule, "HasElement"));
    XELibGetElement = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, unsigned int *)>(
        GetProcAddress(HModule, "GetElement"));
    XELibAddElement = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, unsigned int *)>(
        GetProcAddress(HModule, "AddElement"));
    XELibAddElementValue = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, const wchar_t *, unsigned int *)>(
        GetProcAddress(HModule, "AddElementValue"));
    XELibRemoveElement =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *)>(GetProcAddress(HModule, "RemoveElement"));
    XELibRemoveElementOrParent =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int)>(GetProcAddress(HModule, "RemoveElementOrParent"));
    XELibSetElement =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const unsigned int)>(GetProcAddress(HModule, "SetElement"));
    XELibGetElements = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, const VARIANT_BOOL, const VARIANT_BOOL, int *)>(
        GetProcAddress(HModule, "GetElements"));
    XELibGetDefNames = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetDefNames"));
    XELibGetAddList = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetAddList"));
    XELibGetLinksTo = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, unsigned int *)>(
        GetProcAddress(HModule, "GetLinksTo"));
    XELibSetLinksTo = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, const unsigned int)>(
        GetProcAddress(HModule, "SetLinksTo"));
    XELibGetElementIndex = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetElementIndex"));
    XELibGetContainer =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, unsigned int *)>(GetProcAddress(HModule, "GetContainer"));
    XELibGetElementFile =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, unsigned int *)>(GetProcAddress(HModule, "GetElementFile"));
    XELibGetElementRecord =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, unsigned int *)>(GetProcAddress(HModule, "GetElementRecord"));
    XELibElementCount = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "ElementCount"));
    XELibElementEquals = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const unsigned int, VARIANT_BOOL *)>(
        GetProcAddress(HModule, "ElementEquals"));
    XELibElementMatches = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, const wchar_t *, VARIANT_BOOL *)>(
        GetProcAddress(HModule, "ElementMatches"));
    XELibHasArrayItem =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, const wchar_t *, const wchar_t *, VARIANT_BOOL *)>(
            GetProcAddress(HModule, "HasArrayItem"));
    XELibGetArrayItem = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, const wchar_t *, const wchar_t *,
                                             unsigned int *)>(GetProcAddress(HModule, "GetArrayItem"));
    XELibAddArrayItem = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, const wchar_t *, const wchar_t *,
                                             unsigned int *)>(GetProcAddress(HModule, "AddArrayItem"));
    XELibRemoveArrayItem = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, const wchar_t *, const wchar_t *)>(
        GetProcAddress(HModule, "RemoveArrayItem"));
    XELibMoveArrayItem = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const int)>(GetProcAddress(HModule, "MoveArrayItem"));
    XELibCopyElement = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const unsigned int, const VARIANT_BOOL, unsigned int *)>(
        GetProcAddress(HModule, "CopyElement"));
    XELibGetSignatureAllowed = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, VARIANT_BOOL *)>(
        GetProcAddress(HModule, "GetSignatureAllowed"));
    XELibGetAllowedSignatures =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetAllowedSignatuRes"));
    XELibGetIsModified = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, VARIANT_BOOL *)>(GetProcAddress(HModule, "GetIsModified"));
    XELibGetIsEditable = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, VARIANT_BOOL *)>(GetProcAddress(HModule, "GetIsEditable"));
    XELibGetIsRemoveable =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, VARIANT_BOOL *)>(GetProcAddress(HModule, "GetIsRemoveable"));
    XELibGetCanAdd = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, VARIANT_BOOL *)>(GetProcAddress(HModule, "GetCanAdd"));
    XELibSortKey = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "SortKey"));
    XELibElementType =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, unsigned char *)>(GetProcAddress(HModule, "ElementType"));
    XELibDefType = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, unsigned char *)>(GetProcAddress(HModule, "DefType"));
    XELibSmashType = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, unsigned char *)>(GetProcAddress(HModule, "SmashType"));
    XELibValueType = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, unsigned char *)>(GetProcAddress(HModule, "ValueType"));
    XELibIsSorted = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, VARIANT_BOOL *)>(GetProcAddress(HModule, "IsSorted"));

    // Element Value
    XELibName = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "Name"));
    XELibLongName = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "LongName"));
    XELibDisplayName = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "DisplayName"));
    XELibPath =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const VARIANT_BOOL, const VARIANT_BOOL, int *)>(GetProcAddress(HModule, "Path"));
    XELibSignature = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "Signature"));
    XELibGetValue =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, int *)>(GetProcAddress(HModule, "GetValue"));
    XELibSetValue = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, const wchar_t *)>(
        GetProcAddress(HModule, "SetValue"));
    XELibGetIntValue =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, int *)>(GetProcAddress(HModule, "GetIntValue"));
    XELibSetIntValue = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, const int)>(
        GetProcAddress(HModule, "SetIntValue"));
    XELibGetUIntValue = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, unsigned int *)>(
        GetProcAddress(HModule, "GetUIntValue"));
    XELibSetUIntValue = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, const unsigned int)>(
        GetProcAddress(HModule, "SetUIntValue"));
    XELibGetFloatValue = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, double *)>(
        GetProcAddress(HModule, "GetFloatValue"));
    XELibSetFloatValue = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, const double)>(
        GetProcAddress(HModule, "SetFloatValue"));
    XELibGetFlag = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, const wchar_t *, VARIANT_BOOL *)>(
        GetProcAddress(HModule, "GetFlag"));
    XELibSetFlag = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, const wchar_t *, const VARIANT_BOOL)>(
        GetProcAddress(HModule, "SetFlag"));
    XELibGetAllFlags =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, int *)>(GetProcAddress(HModule, "GetAllFlags"));
    XELibGetEnabledFlags = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, int *)>(
        GetProcAddress(HModule, "GetEnabledFlags"));
    XELibSetEnabledFlags = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, const wchar_t *)>(
        GetProcAddress(HModule, "SetEnabledFlags"));
    XELibGetEnumOptions = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, int *)>(
        GetProcAddress(HModule, "GetEnumOptions"));
    XELibSignatureFromName =
        reinterpret_cast<VARIANT_BOOL (*)(const wchar_t *, int *)>(GetProcAddress(HModule, "SignatureFromName"));
    XELibNameFromSignature =
        reinterpret_cast<VARIANT_BOOL (*)(const wchar_t *, int *)>(GetProcAddress(HModule, "NameFromSignature"));
    XELibGetSignatureNameMap = reinterpret_cast<VARIANT_BOOL (*)(int *)>(GetProcAddress(HModule, "GetSignatureNameMap"));

    // Serialization
    XELibElementToJson = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "ElementToJson"));
    XELibElementFromJson = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, const wchar_t *)>(
        GetProcAddress(HModule, "ElementFromJson"));

    // Records
    XELibGetFormID = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, unsigned int *, const VARIANT_BOOL)>(
        GetProcAddress(HModule, "GetFormID"));
    XELibSetFormID = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const unsigned int, const VARIANT_BOOL, const VARIANT_BOOL)>(
        GetProcAddress(HModule, "SetFormID"));
    XELibGetRecord = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const unsigned int, const VARIANT_BOOL, unsigned int *)>(
        GetProcAddress(HModule, "GetRecord"));
    XELibGetRecords = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, const VARIANT_BOOL, int *)>(
        GetProcAddress(HModule, "GetRecords"));
    XELibGetREFRs = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, const unsigned int, int *)>(
        GetProcAddress(HModule, "GetREFRs"));
    XELibGetOverrides = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetOverrides"));
    XELibGetMasterRecord =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, unsigned int *)>(GetProcAddress(HModule, "GetMasterRecord"));
    XELibGetWinningOverride =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, unsigned int *)>(GetProcAddress(HModule, "GetWinningOverride"));
    XELibGetInjectionTarget =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, unsigned int *)>(GetProcAddress(HModule, "GetInjectionTarget"));
    XELibFindNextRecord =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, const VARIANT_BOOL, const VARIANT_BOOL, unsigned int *)>(
            GetProcAddress(HModule, "FindNextRecord"));
    XELibFindPreviousRecord =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, const VARIANT_BOOL, const VARIANT_BOOL, unsigned int *)>(
            GetProcAddress(HModule, "FindPreviousRecord"));
    XELibFindValidReferences =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const wchar_t *, const wchar_t *, const int, int *)>(
            GetProcAddress(HModule, "FindValidReferences"));
    XELibGetReferencedBy = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetReferencedBy"));
    XELibExchangeReferences = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const unsigned int, const unsigned int)>(
        GetProcAddress(HModule, "ExchangeReferences"));
    XELibIsMaster = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, VARIANT_BOOL *)>(GetProcAddress(HModule, "IsMaster"));
    XELibIsInjected = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, VARIANT_BOOL *)>(GetProcAddress(HModule, "IsInjected"));
    XELibIsOverride = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, VARIANT_BOOL *)>(GetProcAddress(HModule, "IsOverride"));
    XELibIsWinningOverride =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, VARIANT_BOOL *)>(GetProcAddress(HModule, "IsWinningOverride"));
    XELibGetNodes = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, unsigned int *)>(GetProcAddress(HModule, "GetNodes"));
    XELibGetConflictData =
        reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const unsigned int, unsigned char *, unsigned char *)>(
            GetProcAddress(HModule, "GetConflictData"));
    XELibGetNodeElements = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const unsigned int, int *)>(
        GetProcAddress(HModule, "GetNodeElements"));

    // Errors
    XELibCheckForErrors = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int)>(GetProcAddress(HModule, "CheckForErrors"));
    XELibGetErrorThreadDone = reinterpret_cast<VARIANT_BOOL (*)()>(GetProcAddress(HModule, "GetErrorThreadDone"));
    XELibGetErrors = reinterpret_cast<VARIANT_BOOL (*)(int *)>(GetProcAddress(HModule, "GetErrors"));
    XELibGetErrorString = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, int *)>(GetProcAddress(HModule, "GetErrorString"));
    XELibRemoveIdenticalRecords = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int, const VARIANT_BOOL, const VARIANT_BOOL)>(
        GetProcAddress(HModule, "RemoveIdenticalRecords"));

    // Filters
    XELibFilterRecord = reinterpret_cast<VARIANT_BOOL (*)(const unsigned int)>(GetProcAddress(HModule, "FilterRecord"));
    XELibResetFilter = reinterpret_cast<VARIANT_BOOL (*)()>(GetProcAddress(HModule, "ResetFilter"));
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
  int Len = 0;
  XELibGetExceptionMessageLength(&Len);

  if (Len == 0) {
    return {};
  }

  auto ExceptionMessage = make_unique<wchar_t[]>(Len);
  XELibGetExceptionMessage(ExceptionMessage.get(), Len);

  return ParallaxGenUtil::wstrToStr(ExceptionMessage.get());
}

void XEditLibCpp::throwExceptionIfExists() {
  auto ExceptionMessage = getExceptionMessageSafe();
  if (!ExceptionMessage.empty()) {
    throw std::runtime_error(ExceptionMessage);
  }
}

auto XEditLibCpp::boolToVariantBool(bool Value) -> VARIANT_BOOL {
  if (Value) {
    return VARIANT_TRUE;
  }

  return VARIANT_FALSE;
}

//
// Function Implementation
//

// Meta
auto XEditLibCpp::getProgramPath() -> std::filesystem::path {
  int Len = 0;
  XELibGetGlobal(L"ProgramPath", &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

auto XEditLibCpp::getVersion() -> std::wstring {
  int Len = 0;
  XELibGetGlobal(L"Version", &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

auto XEditLibCpp::getGameName() -> std::wstring {
  int Len = 0;
  XELibGetGlobal(L"GameName", &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

auto XEditLibCpp::getAppName() -> std::wstring {
  int Len = 0;
  XELibGetGlobal(L"AppName", &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

auto XEditLibCpp::getLongGameName() -> std::wstring {
  int Len = 0;
  XELibGetGlobal(L"LongGameName", &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

auto XEditLibCpp::getDataPath() -> std::filesystem::path {
  int Len = 0;
  XELibGetGlobal(L"DataPath", &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

auto XEditLibCpp::getAppDataPath() -> std::filesystem::path {
  int Len = 0;
  XELibGetGlobal(L"AppDataPath", &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

auto XEditLibCpp::getMyGamesPath() -> std::filesystem::path {
  int Len = 0;
  XELibGetGlobal(L"MyGamesPath", &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

auto XEditLibCpp::getGameIniPath() -> std::filesystem::path {
  int Len = 0;
  XELibGetGlobal(L"GameIniPath", &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

void XEditLibCpp::setSortMode(const SortBy &SortBy, bool Reverse) {
  XELibSetSortMode(static_cast<unsigned char>(SortBy), boolToVariantBool(Reverse));
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
  int Len = 0;
  XELibGetDuplicateHandles(Id, &Len);
  throwExceptionIfExists();

  return getResultArraySafe(Len);
}

// Messages

auto XEditLibCpp::getMessages() -> std::wstring {
  int Len = 0;
  XELibGetMessagesLength(&Len);
  throwExceptionIfExists();

  auto Messages = make_unique<wchar_t[]>(Len);
  XELibGetMessages(Messages.get(), Len);
  throwExceptionIfExists();

  return Messages.get();
}

// Setup

auto XEditLibCpp::getGamePath(const GameMode &Mode) -> std::filesystem::path {
  int Len = 0;
  XELibGetGamePath(static_cast<int>(Mode), &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

void XEditLibCpp::setGamePath(const std::filesystem::path &Path) {
  XELibSetGamePath(Path.wstring().c_str());
  throwExceptionIfExists();
}

auto XEditLibCpp::getGameLanguage(const GameMode &Mode) -> wstring {
  int Len = 0;
  XELibGetGameLanguage(static_cast<int>(Mode), &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
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
  int Len = 0;
  XELibGetLoadOrder(&Len);
  throwExceptionIfExists();

  const auto LoadOrderStr = getResultStringSafe(Len);

  vector<wstring> OutputLoadOrder;
  boost::split(OutputLoadOrder, LoadOrderStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputLoadOrder;
}

auto XEditLibCpp::getActivePlugins() -> std::vector<wstring> {
  int Len = 0;
  XELibGetActivePlugins(&Len);
  throwExceptionIfExists();

  const auto ActivePluginsStr = getResultStringSafe(Len);

  vector<wstring> OutputActivePlugins;
  boost::split(OutputActivePlugins, ActivePluginsStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputActivePlugins;
}

void XEditLibCpp::loadPlugins(const std::vector<wstring> &LoadOrder, bool SmartLoad) {
  const wstring LoadOrderStr = boost::algorithm::join(LoadOrder, L"\r\n");
  XELibLoadPlugins(LoadOrderStr.c_str(), boolToVariantBool(SmartLoad));
  throwExceptionIfExists();
}

void XEditLibCpp::loadPlugin(const std::wstring &Filename) {
  XELibLoadPlugin(Filename.c_str());
  throwExceptionIfExists();
}

auto XEditLibCpp::loadPluginHeader(const std::wstring &Filename) -> unsigned int {
  unsigned int Res = 0;
  XELibLoadPluginHeader(Filename.c_str(), &Res);
  throwExceptionIfExists();

  return Res;
}

void XEditLibCpp::buildReferences(const unsigned int &Id, const bool &Synchronous) {
  XELibBuildReferences(Id, boolToVariantBool(Synchronous));
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
  unsigned int Res = 0;
  XELibAddFile(Path.wstring().c_str(), &Res);
  throwExceptionIfExists();

  return Res;
}

auto XEditLibCpp::fileByIndex(int Index) -> unsigned int {
  unsigned int Res = 0;
  XELibFileByIndex(Index, &Res);
  throwExceptionIfExists();

  return Res;
}

auto XEditLibCpp::fileByLoadOrder(int LoadOrder) -> unsigned int {
  unsigned int Res = 0;
  XELibFileByLoadOrder(LoadOrder, &Res);
  throwExceptionIfExists();

  return Res;
}

auto XEditLibCpp::fileByName(const std::wstring &Filename) -> unsigned int {
  unsigned int Res = 0;
  XELibFileByName(Filename.c_str(), &Res);
  throwExceptionIfExists();

  return Res;
}

auto XEditLibCpp::fileByAuthor(const std::wstring &Author) -> unsigned int {
  unsigned int Res = 0;
  XELibFileByAuthor(Author.c_str(), &Res);
  throwExceptionIfExists();

  return Res;
}

void XEditLibCpp::saveFile(const unsigned int &Id, const std::filesystem::path &Path) {
  XELibSaveFile(Id, Path.wstring().c_str());
  throwExceptionIfExists();
}

auto XEditLibCpp::getRecordCount(const unsigned int &Id) -> int {
  int Res = 0;
  XELibGetRecordCount(Id, &Res);
  throwExceptionIfExists();

  return Res;
}

auto XEditLibCpp::getOverrideRecordCount(const unsigned int &Id) -> int {
  int Res = 0;
  XELibGetOverrideRecordCount(Id, &Res);
  throwExceptionIfExists();

  return Res;
}

auto XEditLibCpp::md5Hash(const unsigned int &Id) -> wstring {
  int Len = 0;
  XELibMD5Hash(Id, &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

auto XEditLibCpp::crcHash(const unsigned int &Id) -> wstring {
  int Len = 0;
  XELibCRCHash(Id, &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

auto XEditLibCpp::getFileLoadOrder(const unsigned int &Id) -> int {
  int Res = 0;
  XELibGetFileLoadOrder(Id, &Res);
  throwExceptionIfExists();

  return Res;
}

// Archives

void XEditLibCpp::extractContainer(const std::wstring &Name, const std::filesystem::path &Destination,
                                   const bool &Replace) {
  XELibExtractContainer(Name.c_str(), Destination.wstring().c_str(), boolToVariantBool(Replace));
  throwExceptionIfExists();
}

void XEditLibCpp::extractFile(const std::wstring &Name, const std::filesystem::path &Source,
                              const std::filesystem::path &Destination) {
  XELibExtractFile(Name.c_str(), Source.c_str(), Destination.wstring().c_str());
  throwExceptionIfExists();
}

auto XEditLibCpp::getContainerFiles(const std::wstring &Name,
                                    const std::filesystem::path &Path) -> std::vector<std::wstring> {
  int Len = 0;
  XELibGetContainerFiles(Name.c_str(), Path.wstring().c_str(), &Len);
  throwExceptionIfExists();

  const auto ContainerFilesStr = getResultStringSafe(Len);

  vector<wstring> OutputContainerFiles;
  boost::split(OutputContainerFiles, ContainerFilesStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputContainerFiles;
}

auto XEditLibCpp::getFileContainer(const std::filesystem::path &Path) -> std::wstring {
  int Len = 0;
  XELibGetFileContainer(Path.wstring().c_str(), &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

auto XEditLibCpp::getLoadedContainers() -> std::vector<std::wstring> {
  int Len = 0;
  XELibGetLoadedContainers(&Len);
  throwExceptionIfExists();

  const auto LoadedContainersStr = getResultStringSafe(Len);

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
               static_cast<int>(Type), boolToVariantBool(BCompress), boolToVariantBool(BShare), AF.c_str(), FF.c_str());
  throwExceptionIfExists();
}

auto XEditLibCpp::getTextureData(const std::wstring &ResourceName) -> std::pair<int, int> {
  auto Width = make_unique<int>();
  auto Height = make_unique<int>();
  XELibGetTextureData(ResourceName.c_str(), Width.get(), Height.get());
  throwExceptionIfExists();

  return {*Width, *Height};
}

// Masters

void XEditLibCpp::cleanMasters(const unsigned int &Id) {
  XELibCleanMasters(Id);
  throwExceptionIfExists();
}

void XEditLibCpp::sortMasters(const unsigned int &Id) {
  XELibSortMasters(Id);
  throwExceptionIfExists();
}

void XEditLibCpp::addMaster(const unsigned int &Id, const std::wstring &MasterName) {
  XELibAddMaster(Id, MasterName.c_str());
  throwExceptionIfExists();
}

void XEditLibCpp::addMasters(const unsigned int &Id, const std::wstring &Masters) {
  XELibAddMasters(Id, Masters.c_str());
  throwExceptionIfExists();
}

void XEditLibCpp::addRequiredMasters(const unsigned int &Id, const unsigned int &Id2, const bool &AsNew) {
  XELibAddRequiredMasters(Id, Id2, boolToVariantBool(AsNew));
  throwExceptionIfExists();
}

auto XEditLibCpp::getMasters(const unsigned int &Id) -> std::vector<std::wstring> {
  int Len = 0;
  XELibGetMasters(Id, &Len);
  throwExceptionIfExists();

  const auto MastersStr = getResultStringSafe(Len);

  vector<wstring> OutputMasters;
  boost::split(OutputMasters, MastersStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputMasters;
}

auto XEditLibCpp::getRequiredBy(const unsigned int &Id) -> std::vector<std::wstring> {
  int Len = 0;
  XELibGetRequiredBy(Id, &Len);
  throwExceptionIfExists();

  const auto RequiredByStr = getResultStringSafe(Len);

  vector<wstring> OutputRequiredBy;
  boost::split(OutputRequiredBy, RequiredByStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputRequiredBy;
}

auto XEditLibCpp::getMasterNames(const unsigned int &Id) -> std::vector<std::wstring> {
  int Len = 0;
  XELibGetMasterNames(Id, &Len);
  throwExceptionIfExists();

  const auto MasterNamesStr = getResultStringSafe(Len);

  vector<wstring> OutputMasterNames;
  boost::split(OutputMasterNames, MasterNamesStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputMasterNames;
}

// Elements

auto XEditLibCpp::hasElement(const unsigned int &Id, const std::wstring &Key) -> bool {
  auto Res = VARIANT_FALSE;
  XELibHasElement(Id, Key.c_str(), &Res);
  throwExceptionIfExists();

  return Res == VARIANT_TRUE;
}

auto XEditLibCpp::getElement(const unsigned int &Id, const std::wstring &Key) -> unsigned int {
  unsigned int Res = 0;
  XELibGetElement(Id, Key.c_str(), &Res);
  throwExceptionIfExists();

  return Res;
}

auto XEditLibCpp::addElement(const unsigned int &Id, const std::wstring &Key) -> unsigned int {
  unsigned int Res = 0;
  XELibAddElement(Id, Key.c_str(), &Res);
  throwExceptionIfExists();

  return Res;
}

auto XEditLibCpp::addElementValue(const unsigned int &Id, const std::wstring &Key,
                                  const std::wstring &Value) -> unsigned int {
  unsigned int Res = 0;
  XELibAddElementValue(Id, Key.c_str(), Value.c_str(), &Res);
  throwExceptionIfExists();

  return Res;
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
  int Len = 0;
  XELibGetElements(Id, Key.c_str(), boolToVariantBool(Sort), boolToVariantBool(Filter), &Len);
  throwExceptionIfExists();

  return getResultArraySafe(Len);
}

auto XEditLibCpp::getDefNames(const unsigned int &Id) -> std::vector<std::wstring> {
  int Len = 0;
  XELibGetDefNames(Id, &Len);
  throwExceptionIfExists();

  const auto DefNamesStr = getResultStringSafe(Len);

  vector<wstring> OutputDefNames;
  boost::split(OutputDefNames, DefNamesStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputDefNames;
}

auto XEditLibCpp::getAddList(const unsigned int &Id) -> std::vector<std::wstring> {
  int Len = 0;
  XELibGetAddList(Id, &Len);
  throwExceptionIfExists();

  const auto AddListStr = getResultStringSafe(Len);

  vector<wstring> OutputAddList;
  boost::split(OutputAddList, AddListStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputAddList;
}

auto XEditLibCpp::getLinksTo(const unsigned int &Id, const std::wstring &Key) -> unsigned int {
  unsigned int Res = 0;
  XELibGetLinksTo(Id, Key.c_str(), &Res);
  throwExceptionIfExists();

  return Res;
}

void XEditLibCpp::setLinksTo(const unsigned int &Id, const std::wstring &Key, const unsigned int &Id2) {
  XELibSetLinksTo(Id, Key.c_str(), Id2);
  throwExceptionIfExists();
}

auto XEditLibCpp::getElementIndex(const unsigned int &Id) -> int {
  int Res = 0;
  XELibGetElementIndex(Id, &Res);
  throwExceptionIfExists();

  return Res;
}

auto XEditLibCpp::getContainer(const unsigned int &Id) -> unsigned int {
  unsigned int Res = 0;
  XELibGetContainer(Id, &Res);
  throwExceptionIfExists();

  return Res;
}

auto XEditLibCpp::getElementFile(const unsigned int &Id) -> unsigned int {
  unsigned int Res = 0;
  XELibGetElementFile(Id, &Res);
  throwExceptionIfExists();

  return Res;
}

auto XEditLibCpp::getElementRecord(const unsigned int &Id) -> unsigned int {
  unsigned int Res = 0;
  XELibGetElementRecord(Id, &Res);
  throwExceptionIfExists();

  return Res;
}

auto XEditLibCpp::elementCount(const unsigned int &Id) -> int {
  int Res = 0;
  XELibElementCount(Id, &Res);
  throwExceptionIfExists();

  return Res;
}

auto XEditLibCpp::elementEquals(const unsigned int &Id, const unsigned int &Id2) -> bool {
  auto Res = VARIANT_FALSE;
  XELibElementEquals(Id, Id2, &Res);
  throwExceptionIfExists();

  return Res == VARIANT_TRUE;
}

auto XEditLibCpp::elementMatches(const unsigned int &Id, const std::wstring &Key, const std::wstring &Value) -> bool {
  auto Res = VARIANT_FALSE;
  XELibElementMatches(Id, Key.c_str(), Value.c_str(), &Res);
  throwExceptionIfExists();

  return Res == VARIANT_TRUE;
}

auto XEditLibCpp::hasArrayItem(const unsigned int &Id, const std::wstring &Key, const std::wstring &SubKey,
                               const std::wstring &Value) -> bool {
  auto Res = VARIANT_FALSE;
  XELibHasArrayItem(Id, Key.c_str(), SubKey.c_str(), Value.c_str(), &Res);
  throwExceptionIfExists();

  return Res == VARIANT_TRUE;
}

auto XEditLibCpp::getArrayItem(const unsigned int &Id, const std::wstring &Key, const std::wstring &SubKey,
                               const std::wstring &Value) -> unsigned int {
  unsigned int Res = 0;
  XELibGetArrayItem(Id, Key.c_str(), SubKey.c_str(), Value.c_str(), &Res);
  throwExceptionIfExists();

  return Res;
}

auto XEditLibCpp::addArrayItem(const unsigned int &Id, const std::wstring &Key, const std::wstring &SubKey,
                               const std::wstring &Value) -> unsigned int {
  unsigned int Res = 0;
  XELibAddArrayItem(Id, Key.c_str(), SubKey.c_str(), Value.c_str(), &Res);
  throwExceptionIfExists();

  return Res;
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
  unsigned int Res = 0;
  XELibCopyElement(Id, Id2, boolToVariantBool(AsNew), &Res);
  throwExceptionIfExists();

  return Res;
}

auto XEditLibCpp::getSignatureAllowed(const unsigned int &Id, const std::wstring &Signature) -> bool {
  auto Res = VARIANT_FALSE;
  XELibGetSignatureAllowed(Id, Signature.c_str(), &Res);
  throwExceptionIfExists();

  return Res == VARIANT_TRUE;
}

auto XEditLibCpp::getAllowedSignatures(const unsigned int &Id) -> std::vector<std::wstring> {
  int Len = 0;
  XELibGetAllowedSignatures(Id, &Len);
  throwExceptionIfExists();

  const auto AllowedSignaturesStr = getResultStringSafe(Len);

  vector<wstring> OutputAllowedSignatures;
  boost::split(OutputAllowedSignatures, AllowedSignaturesStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputAllowedSignatures;
}

auto XEditLibCpp::getIsModified(const unsigned int &Id) -> bool {
  auto Res = VARIANT_FALSE;
  XELibGetIsModified(Id, &Res);
  throwExceptionIfExists();

  return Res == VARIANT_TRUE;
}

auto XEditLibCpp::getIsEditable(const unsigned int &Id) -> bool {
  auto Res = VARIANT_FALSE;
  XELibGetIsEditable(Id, &Res);
  throwExceptionIfExists();

  return Res == VARIANT_TRUE;
}

auto XEditLibCpp::getIsRemoveable(const unsigned int &Id) -> bool {
  auto Res = VARIANT_FALSE;
  XELibGetIsRemoveable(Id, &Res);
  throwExceptionIfExists();

  return Res == VARIANT_TRUE;
}

auto XEditLibCpp::getCanAdd(const unsigned int &Id) -> bool {
  auto Res = VARIANT_FALSE;
  XELibGetCanAdd(Id, &Res);
  throwExceptionIfExists();

  return Res == VARIANT_TRUE;
}

auto XEditLibCpp::sortKey(const unsigned int &Id) -> wstring {
  int Len = 0;
  XELibSortKey(Id, &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

auto XEditLibCpp::elementType(const unsigned int &Id) -> ElementType {
  unsigned char Res = 0;
  XELibElementType(Id, &Res);
  throwExceptionIfExists();

  return static_cast<ElementType>(Res);
}

auto XEditLibCpp::defType(const unsigned int &Id) -> DefType {
  unsigned char Res = 0;
  XELibDefType(Id, &Res);
  throwExceptionIfExists();

  return static_cast<DefType>(Res);
}

auto XEditLibCpp::smashType(const unsigned int &Id) -> SmashType {
  unsigned char Res = 0;
  XELibSmashType(Id, &Res);
  throwExceptionIfExists();

  return static_cast<SmashType>(Res);
}

auto XEditLibCpp::valueType(const unsigned int &Id) -> ValueType {
  unsigned char Res = 0;
  XELibValueType(Id, &Res);
  throwExceptionIfExists();

  return static_cast<ValueType>(Res);
}

auto XEditLibCpp::isSorted(const unsigned int &Id) -> bool {
  auto Res = VARIANT_FALSE;
  XELibIsSorted(Id, &Res);
  throwExceptionIfExists();

  return Res == VARIANT_TRUE;
}

// Element Value

auto XEditLibCpp::name(const unsigned int &Id) -> wstring {
  int Len = 0;
  XELibName(Id, &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

auto XEditLibCpp::longName(const unsigned int &Id) -> wstring {
  int Len = 0;
  XELibLongName(Id, &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

auto XEditLibCpp::displayName(const unsigned int &Id) -> wstring {
  int Len = 0;
  XELibDisplayName(Id, &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

auto XEditLibCpp::path(const unsigned int &Id, const bool &ShortPath, const bool &Local) -> std::wstring {
  int Len = 0;
  XELibPath(Id, boolToVariantBool(ShortPath), boolToVariantBool(Local), &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

auto XEditLibCpp::signature(const unsigned int &Id) -> wstring {
  int Len = 0;
  XELibSignature(Id, &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

auto XEditLibCpp::getValue(const unsigned int &Id, const std::wstring &Key) -> wstring {
  int Len = 0;
  XELibGetValue(Id, Key.c_str(), &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

void XEditLibCpp::setValue(const unsigned int &Id, const std::wstring &Key, const std::wstring &Value) {
  XELibSetValue(Id, Key.c_str(), Value.c_str());
  throwExceptionIfExists();
}

auto XEditLibCpp::getIntValue(const unsigned int &Id, const std::wstring &Key) -> int {
  int Res = 0;
  XELibGetIntValue(Id, Key.c_str(), &Res);
  throwExceptionIfExists();

  return Res;
}

void XEditLibCpp::setIntValue(const unsigned int &Id, const std::wstring &Key, const int &Value) {
  XELibSetIntValue(Id, Key.c_str(), Value);
  throwExceptionIfExists();
}

auto XEditLibCpp::getUIntValue(const unsigned int &Id, const std::wstring &Key) -> unsigned int {
  unsigned int Res = 0;
  XELibGetUIntValue(Id, Key.c_str(), &Res);
  throwExceptionIfExists();

  return Res;
}

void XEditLibCpp::setUIntValue(const unsigned int &Id, const std::wstring &Key, const unsigned int &Value) {
  XELibSetUIntValue(Id, Key.c_str(), Value);
  throwExceptionIfExists();
}

auto XEditLibCpp::getFloatValue(const unsigned int &Id, const std::wstring &Key) -> double {
  double Res = 0;
  XELibGetFloatValue(Id, Key.c_str(), &Res);
  throwExceptionIfExists();

  return Res;
}

void XEditLibCpp::setFloatValue(const unsigned int &Id, const std::wstring &Key, const double &Value) {
  XELibSetFloatValue(Id, Key.c_str(), Value);
  throwExceptionIfExists();
}

auto XEditLibCpp::getFlag(const unsigned int &Id, const std::wstring &Key, const std::wstring &Flag) -> bool {
  auto Res = VARIANT_FALSE;
  XELibGetFlag(Id, Key.c_str(), Flag.c_str(), &Res);
  throwExceptionIfExists();

  return Res == VARIANT_TRUE;
}

void XEditLibCpp::setFlag(const unsigned int &Id, const std::wstring &Key, const std::wstring &Flag,
                          const bool &Enabled) {
  XELibSetFlag(Id, Key.c_str(), Flag.c_str(), boolToVariantBool(Enabled));
  throwExceptionIfExists();
}

auto XEditLibCpp::getAllFlags(const unsigned int &Id, const std::wstring &Key) -> std::vector<std::wstring> {
  int Len = 0;
  XELibGetAllFlags(Id, Key.c_str(), &Len);
  throwExceptionIfExists();

  const auto AllFlagsStr = getResultStringSafe(Len);

  vector<wstring> OutputAllFlags;
  boost::split(OutputAllFlags, AllFlagsStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputAllFlags;
}

auto XEditLibCpp::getEnabledFlags(const unsigned int &Id, const std::wstring &Key) -> std::vector<std::wstring> {
  int Len = 0;
  XELibGetEnabledFlags(Id, Key.c_str(), &Len);
  throwExceptionIfExists();

  const auto EnabledFlagsStr = getResultStringSafe(Len);

  vector<wstring> OutputEnabledFlags;
  boost::split(OutputEnabledFlags, EnabledFlagsStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputEnabledFlags;
}

void XEditLibCpp::setEnabledFlags(const unsigned int &Id, const std::wstring &Key, const std::wstring &Flags) {
  XELibSetEnabledFlags(Id, Key.c_str(), Flags.c_str());
  throwExceptionIfExists();
}

auto XEditLibCpp::getEnumOptions(const unsigned int &Id, const std::wstring &Key) -> std::vector<std::wstring> {
  int Len = 0;
  XELibGetEnumOptions(Id, Key.c_str(), &Len);
  throwExceptionIfExists();

  const auto EnumOptionsStr = getResultStringSafe(Len);

  vector<wstring> OutputEnumOptions;
  boost::split(OutputEnumOptions, EnumOptionsStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputEnumOptions;
}

auto XEditLibCpp::signatureFromName(const std::wstring &Name) -> wstring {
  int Len = 0;
  XELibSignatureFromName(Name.c_str(), &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

auto XEditLibCpp::nameFromSignature(const std::wstring &Signature) -> wstring {
  int Len = 0;
  XELibNameFromSignature(Signature.c_str(), &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

auto XEditLibCpp::getSignatureNameMap() -> vector<wstring> {
  int Len = 0;
  XELibGetSignatureNameMap(&Len);
  throwExceptionIfExists();

  const auto SignatureNameMapStr = getResultStringSafe(Len);

  vector<wstring> OutputSignatureNameMap;
  boost::split(OutputSignatureNameMap, SignatureNameMapStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputSignatureNameMap;
}

// Serialization

auto XEditLibCpp::elementToJson(const unsigned int &Id) -> wstring {
  int Len = 0;
  XELibElementToJson(Id, &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

void XEditLibCpp::elementFromJson(const unsigned int &Id, const std::filesystem::path &Path, const std::wstring &JSON) {
  XELibElementFromJson(Id, Path.wstring().c_str(), JSON.c_str());
  throwExceptionIfExists();
}

// Records

auto XEditLibCpp::getFormID(const unsigned int &Id, const bool &Local) -> unsigned int {
  unsigned int Res = 0;
  XELibGetFormID(Id, &Res, boolToVariantBool(Local));
  throwExceptionIfExists();

  return Res;
}

void XEditLibCpp::setFormID(const unsigned int &Id, const unsigned int &FormID, const bool &Local,
                            const bool &FixRefs) {
  XELibSetFormID(Id, FormID, boolToVariantBool(Local), boolToVariantBool(FixRefs));
  throwExceptionIfExists();
}

auto XEditLibCpp::getRecord(const unsigned int &Id, const unsigned int &FormID,
                            const bool &SearchMasters) -> unsigned int {
  unsigned int Res = 0;
  XELibGetRecord(Id, FormID, boolToVariantBool(SearchMasters), &Res);
  throwExceptionIfExists();

  return Res;
}

auto XEditLibCpp::getRecords(const unsigned int &Id, const std::wstring &Search,
                             const bool &IncludeOverrides) -> std::vector<unsigned int> {
  int Len = 0;
  XELibGetRecords(Id, Search.c_str(), boolToVariantBool(IncludeOverrides), &Len);
  throwExceptionIfExists();

  return getResultArraySafe(Len);
}

auto XEditLibCpp::getREFRs(const unsigned int &Id, const std::wstring &Search,
                           const unsigned int &Flags) -> std::vector<unsigned int> {
  int Len = 0;
  XELibGetREFRs(Id, Search.c_str(), Flags, &Len);
  throwExceptionIfExists();

  return getResultArraySafe(Len);
}

auto XEditLibCpp::getOverrides(const unsigned int &Id) -> std::vector<unsigned int> {
  int Len = 0;
  XELibGetOverrides(Id, &Len);
  throwExceptionIfExists();

  return getResultArraySafe(Len);
}

auto XEditLibCpp::getMasterRecord(const unsigned int &Id) -> unsigned int {
  unsigned int Res = 0;
  XELibGetMasterRecord(Id, &Res);
  throwExceptionIfExists();

  return Res;
}

auto XEditLibCpp::getWinningOverride(const unsigned int &Id) -> unsigned int {
  unsigned int Res = 0;
  XELibGetWinningOverride(Id, &Res);
  throwExceptionIfExists();

  return Res;
}

auto XEditLibCpp::getInjectionTarget(const unsigned int &Id) -> unsigned int {
  unsigned int Res = 0;
  XELibGetInjectionTarget(Id, &Res);
  throwExceptionIfExists();

  return Res;
}

auto XEditLibCpp::findNextRecord(const unsigned int &Id, const std::wstring &Search, const bool &ByEdid,
                                 const bool &ByName) -> unsigned int {
  unsigned int Res = 0;
  XELibFindNextRecord(Id, Search.c_str(), boolToVariantBool(ByEdid), boolToVariantBool(ByName), &Res);
  throwExceptionIfExists();

  return Res;
}

auto XEditLibCpp::findPreviousRecord(const unsigned int &Id, const std::wstring &Search, const bool &ByEdid,
                                     const bool &ByName) -> unsigned int {
  unsigned int Res = 0;
  XELibFindPreviousRecord(Id, Search.c_str(), boolToVariantBool(ByEdid), boolToVariantBool(ByName), &Res);
  throwExceptionIfExists();

  return Res;
}

auto XEditLibCpp::findValidReferences(const unsigned int &Id, const std::wstring &Signature, const std::wstring &Search,
                                      const int &LimitTo) -> std::vector<unsigned int> {
  int Len = 0;
  XELibFindValidReferences(Id, Signature.c_str(), Search.c_str(), LimitTo, &Len);
  throwExceptionIfExists();

  return getResultArraySafe(Len);
}

auto XEditLibCpp::getReferencedBy(const unsigned int &Id) -> std::vector<unsigned int> {
  int Len = 0;
  XELibGetReferencedBy(Id, &Len);
  throwExceptionIfExists();

  return getResultArraySafe(Len);
}

void XEditLibCpp::exchangeReferences(const unsigned int &Id, const unsigned int &OldFormID,
                                     const unsigned int &NewFormID) {
  XELibExchangeReferences(Id, OldFormID, NewFormID);
  throwExceptionIfExists();
}

auto XEditLibCpp::isMaster(const unsigned int &Id) -> bool {
  auto Res = VARIANT_FALSE;
  XELibIsMaster(Id, &Res);
  throwExceptionIfExists();

  return Res == VARIANT_TRUE;
}

auto XEditLibCpp::isInjected(const unsigned int &Id) -> bool {
  auto Res = VARIANT_FALSE;
  XELibIsInjected(Id, &Res);
  throwExceptionIfExists();

  return Res == VARIANT_TRUE;
}

auto XEditLibCpp::isOverride(const unsigned int &Id) -> bool {
  auto Res = VARIANT_FALSE;
  XELibIsOverride(Id, &Res);
  throwExceptionIfExists();

  return Res == VARIANT_TRUE;
}

auto XEditLibCpp::isWinningOverride(const unsigned int &Id) -> bool {
  auto Res = VARIANT_FALSE;
  XELibIsWinningOverride(Id, &Res);
  throwExceptionIfExists();

  return Res == VARIANT_TRUE;
}

auto XEditLibCpp::getNodes(const unsigned int &Id) -> unsigned int {
  unsigned int Res = 0;
  XELibGetNodes(Id, &Res);
  throwExceptionIfExists();

  return Res;
}

auto XEditLibCpp::getConflictData(const unsigned int &Id,
                                  const unsigned int &Id2) -> std::pair<ConflictAll, ConflictThis> {
  unsigned char Res1 = 0;
  unsigned char Res2 = 0;
  XELibGetConflictData(Id, Id2, &Res1, &Res2);
  throwExceptionIfExists();

  return {static_cast<ConflictAll>(Res1), static_cast<ConflictThis>(Res2)};
}

auto XEditLibCpp::getNodeElements(const unsigned int &Id, const unsigned int &Id2) -> vector<unsigned int> {
  int Res = 0;
  XELibGetNodeElements(Id, Id2, &Res);
  throwExceptionIfExists();

  return getResultArraySafe(Res);
}

// Errors

void XEditLibCpp::checkForErrors(const unsigned int &Id) {
  XELibCheckForErrors(Id);
  throwExceptionIfExists();
}

auto XEditLibCpp::getErrorThreadDone() -> bool {
  bool Result = XELibGetErrorThreadDone() != 0;
  throwExceptionIfExists();
  return Result;
}

auto XEditLibCpp::getErrors() -> std::vector<std::wstring> {
  int Len = 0;
  XELibGetErrors(&Len);
  throwExceptionIfExists();

  const auto ErrorsStr = getResultStringSafe(Len);

  vector<wstring> OutputErrors;
  boost::split(OutputErrors, ErrorsStr, boost::is_any_of("\r\n"), boost::token_compress_on);

  return OutputErrors;
}

auto XEditLibCpp::getErrorString(const unsigned int &Id) -> std::wstring {
  int Len = 0;
  XELibGetErrorString(Id, &Len);
  throwExceptionIfExists();

  return getResultStringSafe(Len);
}

void XEditLibCpp::removeIdenticalRecords(const unsigned int &Id, const bool &RemoveITMs, const bool &RemoveITPOs) {
  XELibRemoveIdenticalRecords(Id, boolToVariantBool(RemoveITMs), boolToVariantBool(RemoveITPOs));
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
