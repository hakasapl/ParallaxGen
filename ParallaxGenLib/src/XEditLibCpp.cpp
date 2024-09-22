#include "XEditLibCpp.hpp"
#include <memory>

using namespace std;

XEditLibCpp::XEditLibCpp() : HModule(LoadLibrary(TEXT("XEditLib.dll"))) {
  if (HModule != nullptr) {
    InitXEdit = reinterpret_cast<void (*)()>(GetProcAddress(HModule, "InitXEdit"));
    CloseXEdit = reinterpret_cast<void (*)()>(GetProcAddress(HModule, "CloseXEdit"));
    GetResultString = reinterpret_cast<bool (*)(wchar_t *, int)>(GetProcAddress(HModule, "GetResultString"));
    GetResultArray = reinterpret_cast<bool (*)(unsigned int *, int)>(GetProcAddress(HModule, "GetResultArray"));
    GetResultBytes = reinterpret_cast<bool (*)(unsigned char *, int)>(GetProcAddress(HModule, "GetResultBytes"));
    GetGlobal = reinterpret_cast<bool (*)(wchar_t *, int *)>(GetProcAddress(HModule, "GetGlobal"));
    GetGlobals = reinterpret_cast<bool (*)(int *)>(GetProcAddress(HModule, "GetGlobals"));
    SetSortMode = reinterpret_cast<bool (*)(unsigned char, bool)>(GetProcAddress(HModule, "SetSortMode"));
    Release = reinterpret_cast<bool (*)(unsigned int)>(GetProcAddress(HModule, "Release"));
    ReleaseNodes = reinterpret_cast<bool (*)(unsigned int)>(GetProcAddress(HModule, "ReleaseNodes"));
    Switch = reinterpret_cast<bool (*)(unsigned int, unsigned int)>(GetProcAddress(HModule, "Switch"));
    GetDuplicateHandles =
        reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "GetDuplicateHandles"));
    ResetStore = reinterpret_cast<bool (*)()>(GetProcAddress(HModule, "ResetStore"));
    GetMessagesLength = reinterpret_cast<void (*)(int *)>(GetProcAddress(HModule, "GetMessagesLength"));
    GetMessages = reinterpret_cast<bool (*)(wchar_t *, int)>(GetProcAddress(HModule, "GetMessages"));
    ClearMessages = reinterpret_cast<void (*)()>(GetProcAddress(HModule, "ClearMessages"));
    GetExceptionMessageLength = reinterpret_cast<void (*)(int *)>(GetProcAddress(HModule, "GetExceptionMessageLength"));
    GetExceptionMessage = reinterpret_cast<bool (*)(wchar_t *, int)>(GetProcAddress(HModule, "GetExceptionMessage"));
    GetGamePath = reinterpret_cast<bool (*)(int, int *)>(GetProcAddress(HModule, "GetGamePath"));
    SetGamePath = reinterpret_cast<bool (*)(wchar_t *)>(GetProcAddress(HModule, "SetGamePath"));
    GetGameLanguage = reinterpret_cast<bool (*)(int, int *)>(GetProcAddress(HModule, "GetGameLanguage"));
    SetLanguage = reinterpret_cast<bool (*)(wchar_t *)>(GetProcAddress(HModule, "SetLanguage"));
    SetBackupPath = reinterpret_cast<bool (*)(wchar_t *)>(GetProcAddress(HModule, "SetBackupPath"));
    SetGameMode = reinterpret_cast<bool (*)(int)>(GetProcAddress(HModule, "SetGameMode"));
    GetLoadOrder = reinterpret_cast<bool (*)(int *)>(GetProcAddress(HModule, "GetLoadOrder"));
    GetActivePlugins = reinterpret_cast<bool (*)(int *)>(GetProcAddress(HModule, "GetActivePlugins"));
    LoadPlugins = reinterpret_cast<bool (*)(wchar_t *, bool)>(GetProcAddress(HModule, "LoadPlugins"));
    LoadPlugin = reinterpret_cast<bool (*)(wchar_t *)>(GetProcAddress(HModule, "LoadPlugin"));
    LoadPluginHeader =
        reinterpret_cast<bool (*)(wchar_t *, unsigned int *)>(GetProcAddress(HModule, "LoadPluginHeader"));
    BuildReferences = reinterpret_cast<bool (*)(unsigned int, bool)>(GetProcAddress(HModule, "BuildReferences"));
    GetLoaderStatus = reinterpret_cast<bool (*)(unsigned char *)>(GetProcAddress(HModule, "GetLoaderStatus"));
    UnloadPlugin = reinterpret_cast<bool (*)(unsigned int)>(GetProcAddress(HModule, "UnloadPlugin"));
    AddFile = reinterpret_cast<bool (*)(wchar_t *, unsigned int *)>(GetProcAddress(HModule, "AddFile"));
    FileByIndex = reinterpret_cast<bool (*)(int, unsigned int *)>(GetProcAddress(HModule, "FileByIndex"));
    FileByLoadOrder = reinterpret_cast<bool (*)(int, unsigned int *)>(GetProcAddress(HModule, "FileByLoadOrder"));
    FileByName = reinterpret_cast<bool (*)(wchar_t *, unsigned int *)>(GetProcAddress(HModule, "FileByName"));
    FileByAuthor = reinterpret_cast<bool (*)(wchar_t *, unsigned int *)>(GetProcAddress(HModule, "FileByAuthor"));
    SaveFile = reinterpret_cast<bool (*)(unsigned int, wchar_t *)>(GetProcAddress(HModule, "SaveFile"));
    GetRecordCount = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "GetRecordCount"));
    GetOverrideRecordCount =
        reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "GetOverrideRecordCount"));
    MD5Hash = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "MD5Hash"));
    CRCHash = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "CRCHash"));
    SortEditorIDs = reinterpret_cast<bool (*)(unsigned int, wchar_t *)>(GetProcAddress(HModule, "SortEditorIDs"));
    SortNames = reinterpret_cast<bool (*)(unsigned int, wchar_t *)>(GetProcAddress(HModule, "SortNames"));
    GetFileLoadOrder = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "GetFileLoadOrder"));
    ExtractContainer =
        reinterpret_cast<bool (*)(wchar_t *, wchar_t *, bool)>(GetProcAddress(HModule, "ExtractContainer"));
    ExtractFile = reinterpret_cast<bool (*)(wchar_t *, wchar_t *, wchar_t *)>(GetProcAddress(HModule, "ExtractFile"));
    GetContainerFiles =
        reinterpret_cast<bool (*)(wchar_t *, wchar_t *, int *)>(GetProcAddress(HModule, "GetContainerFiles"));
    GetFileContainer = reinterpret_cast<bool (*)(wchar_t *, int *)>(GetProcAddress(HModule, "GetFileContainer"));
    GetLoadedContainers = reinterpret_cast<bool (*)(int *)>(GetProcAddress(HModule, "GetLoadedContainers"));
    LoadContainer = reinterpret_cast<bool (*)(wchar_t *)>(GetProcAddress(HModule, "LoadContainer"));
    BuildArchive = reinterpret_cast<bool (*)(wchar_t *, wchar_t *, wchar_t *, int, bool, bool, wchar_t *, wchar_t *)>(
        GetProcAddress(HModule, "BuildArchive"));
    GetTextureData = reinterpret_cast<bool (*)(wchar_t *, int *, int *)>(GetProcAddress(HModule, "GetTextureData"));
    CleanMasters = reinterpret_cast<bool (*)(unsigned int)>(GetProcAddress(HModule, "CleanMasters"));
    SortMasters = reinterpret_cast<bool (*)(unsigned int)>(GetProcAddress(HModule, "SortMasters"));
    AddMaster = reinterpret_cast<bool (*)(unsigned int, wchar_t *)>(GetProcAddress(HModule, "AddMaster"));
    AddMasters = reinterpret_cast<bool (*)(unsigned int, wchar_t *)>(GetProcAddress(HModule, "AddMasters"));
    AddRequiredMasters =
        reinterpret_cast<bool (*)(unsigned int, unsigned int, bool)>(GetProcAddress(HModule, "AddRequiredMasters"));
    GetMasters = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "GetMasters"));
    GetRequiredBy = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "GetRequiredBy"));
    GetMasterNames = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "GetMasterNames"));
    HasElement = reinterpret_cast<bool (*)(unsigned int, wchar_t *, bool *)>(GetProcAddress(HModule, "HasElement"));
    GetElement =
        reinterpret_cast<bool (*)(unsigned int, wchar_t *, unsigned int *)>(GetProcAddress(HModule, "GetElement"));
    AddElement =
        reinterpret_cast<bool (*)(unsigned int, wchar_t *, unsigned int *)>(GetProcAddress(HModule, "AddElement"));
    AddElementValue = reinterpret_cast<bool (*)(unsigned int, wchar_t *, wchar_t *, unsigned int *)>(
        GetProcAddress(HModule, "AddElementValue"));
    RemoveElement = reinterpret_cast<bool (*)(unsigned int, wchar_t *)>(GetProcAddress(HModule, "RemoveElement"));
    RemoveElementOrParent = reinterpret_cast<bool (*)(unsigned int)>(GetProcAddress(HModule, "RemoveElementOrParent"));
    SetElement = reinterpret_cast<bool (*)(unsigned int, unsigned int)>(GetProcAddress(HModule, "SetElement"));
    GetElements =
        reinterpret_cast<bool (*)(unsigned int, wchar_t *, bool, bool, int *)>(GetProcAddress(HModule, "GetElements"));
    GetDefNames = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "GetDefNames"));
    GetAddList = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "GetAddList"));
    GetLinksTo =
        reinterpret_cast<bool (*)(unsigned int, wchar_t *, unsigned int *)>(GetProcAddress(HModule, "GetLinksTo"));
    SetLinksTo =
        reinterpret_cast<bool (*)(unsigned int, wchar_t *, unsigned int)>(GetProcAddress(HModule, "SetLinksTo"));
    GetElementIndex = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "GetElementIndex"));
    GetContainer = reinterpret_cast<bool (*)(unsigned int, unsigned int *)>(GetProcAddress(HModule, "GetContainer"));
    GetElementFile =
        reinterpret_cast<bool (*)(unsigned int, unsigned int *)>(GetProcAddress(HModule, "GetElementFile"));
    GetElementRecord =
        reinterpret_cast<bool (*)(unsigned int, unsigned int *)>(GetProcAddress(HModule, "GetElementRecord"));
    ElementCount = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "ElementCount"));
    ElementEquals =
        reinterpret_cast<bool (*)(unsigned int, unsigned int, bool *)>(GetProcAddress(HModule, "ElementEquals"));
    ElementMatches = reinterpret_cast<bool (*)(unsigned int, wchar_t *, wchar_t *, bool *)>(
        GetProcAddress(HModule, "ElementMatches"));
    HasArrayItem = reinterpret_cast<bool (*)(unsigned int, wchar_t *, wchar_t *, wchar_t *, bool *)>(
        GetProcAddress(HModule, "HasArrayItem"));
    GetArrayItem = reinterpret_cast<bool (*)(unsigned int, wchar_t *, wchar_t *, wchar_t *, unsigned int *)>(
        GetProcAddress(HModule, "GetArrayItem"));
    AddArrayItem = reinterpret_cast<bool (*)(unsigned int, wchar_t *, wchar_t *, wchar_t *, unsigned int *)>(
        GetProcAddress(HModule, "AddArrayItem"));
    RemoveArrayItem = reinterpret_cast<bool (*)(unsigned int, wchar_t *, wchar_t *, wchar_t *)>(
        GetProcAddress(HModule, "RemoveArrayItem"));
    MoveArrayItem = reinterpret_cast<bool (*)(unsigned int, int)>(GetProcAddress(HModule, "MoveArrayItem"));
    CopyElement = reinterpret_cast<bool (*)(unsigned int, unsigned int, bool, unsigned int *)>(
        GetProcAddress(HModule, "CopyElement"));
    GetSignatureAllowed =
        reinterpret_cast<bool (*)(unsigned int, wchar_t *, bool *)>(GetProcAddress(HModule, "GetSignatureAllowed"));
    GetAllowedSignatuRes =
        reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "GetAllowedSignatuRes"));
    GetIsModified = reinterpret_cast<bool (*)(unsigned int, bool *)>(GetProcAddress(HModule, "GetIsModified"));
    GetIsEditable = reinterpret_cast<bool (*)(unsigned int, bool *)>(GetProcAddress(HModule, "GetIsEditable"));
    GetIsRemoveable = reinterpret_cast<bool (*)(unsigned int, bool *)>(GetProcAddress(HModule, "GetIsRemoveable"));
    GetCanAdd = reinterpret_cast<bool (*)(unsigned int, bool *)>(GetProcAddress(HModule, "GetCanAdd"));
    SortKey = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "SortKey"));
    ElementType = reinterpret_cast<bool (*)(unsigned int, unsigned char *)>(GetProcAddress(HModule, "ElementType"));
    DefType = reinterpret_cast<bool (*)(unsigned int, unsigned char *)>(GetProcAddress(HModule, "DefType"));
    SmashType = reinterpret_cast<bool (*)(unsigned int, unsigned char *)>(GetProcAddress(HModule, "SmashType"));
    ValueType = reinterpret_cast<bool (*)(unsigned int, unsigned char *)>(GetProcAddress(HModule, "ValueType"));
    IsSorted = reinterpret_cast<bool (*)(unsigned int, bool *)>(GetProcAddress(HModule, "IsSorted"));
    Name = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "Name"));
    LongName = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "LongName"));
    DisplayName = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "DisplayName"));
    Path = reinterpret_cast<bool (*)(unsigned int, bool, bool, int *)>(GetProcAddress(HModule, "Path"));
    Signature = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "Signature"));
    GetValue = reinterpret_cast<bool (*)(unsigned int, wchar_t *, int *)>(GetProcAddress(HModule, "GetValue"));
    SetValue = reinterpret_cast<bool (*)(unsigned int, wchar_t *, wchar_t *)>(GetProcAddress(HModule, "SetValue"));
    GetIntValue = reinterpret_cast<bool (*)(unsigned int, wchar_t *, int *)>(GetProcAddress(HModule, "GetIntValue"));
    SetIntValue = reinterpret_cast<bool (*)(unsigned int, wchar_t *, int)>(GetProcAddress(HModule, "SetIntValue"));
    GetUIntValue =
        reinterpret_cast<bool (*)(unsigned int, wchar_t *, unsigned int *)>(GetProcAddress(HModule, "GetUIntValue"));
    SetUIntValue =
        reinterpret_cast<bool (*)(unsigned int, wchar_t *, unsigned int)>(GetProcAddress(HModule, "SetUIntValue"));
    GetFloatValue =
        reinterpret_cast<bool (*)(unsigned int, wchar_t *, double *)>(GetProcAddress(HModule, "GetFloatValue"));
    SetFloatValue =
        reinterpret_cast<bool (*)(unsigned int, wchar_t *, double)>(GetProcAddress(HModule, "SetFloatValue"));
    GetFlag =
        reinterpret_cast<bool (*)(unsigned int, wchar_t *, wchar_t *, bool *)>(GetProcAddress(HModule, "GetFlag"));
    SetFlag = reinterpret_cast<bool (*)(unsigned int, wchar_t *, wchar_t *, bool)>(GetProcAddress(HModule, "SetFlag"));
    GetAllFlags = reinterpret_cast<bool (*)(unsigned int, wchar_t *, int *)>(GetProcAddress(HModule, "GetAllFlags"));
    GetEnabledFlags =
        reinterpret_cast<bool (*)(unsigned int, wchar_t *, int *)>(GetProcAddress(HModule, "GetEnabledFlags"));
    SetEnabledFlags =
        reinterpret_cast<bool (*)(unsigned int, wchar_t *, wchar_t *)>(GetProcAddress(HModule, "SetEnabledFlags"));
    GetEnumOptions =
        reinterpret_cast<bool (*)(unsigned int, wchar_t *, int *)>(GetProcAddress(HModule, "GetEnumOptions"));
    SignatureFromName = reinterpret_cast<bool (*)(wchar_t *, int *)>(GetProcAddress(HModule, "SignatureFromName"));
    NameFromSignature = reinterpret_cast<bool (*)(wchar_t *, int *)>(GetProcAddress(HModule, "NameFromSignature"));
    GetSignatureNameMap = reinterpret_cast<bool (*)(int *)>(GetProcAddress(HModule, "GetSignatureNameMap"));
    ElementToJson = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "ElementToJson"));
    ElementFromJson =
        reinterpret_cast<bool (*)(unsigned int, wchar_t *, wchar_t *)>(GetProcAddress(HModule, "ElementFromJson"));
    GetFormID = reinterpret_cast<bool (*)(unsigned int, unsigned int *, bool)>(GetProcAddress(HModule, "GetFormID"));
    SetFormID =
        reinterpret_cast<bool (*)(unsigned int, unsigned int, bool, bool)>(GetProcAddress(HModule, "SetFormID"));
    GetRecord = reinterpret_cast<bool (*)(unsigned int, unsigned int, bool, unsigned int *)>(
        GetProcAddress(HModule, "GetRecord"));
    GetRecords =
        reinterpret_cast<bool (*)(unsigned int, wchar_t *, bool, int *)>(GetProcAddress(HModule, "GetRecords"));
    GetREFRs =
        reinterpret_cast<bool (*)(unsigned int, wchar_t *, unsigned int, int *)>(GetProcAddress(HModule, "GetREFRs"));
    GetOverrides = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "GetOverrides"));
    GetMasterRecord =
        reinterpret_cast<bool (*)(unsigned int, unsigned int *)>(GetProcAddress(HModule, "GetMasterRecord"));
    GetWinningOverride =
        reinterpret_cast<bool (*)(unsigned int, unsigned int *)>(GetProcAddress(HModule, "GetWinningOverride"));
    GetInjectionTarget =
        reinterpret_cast<bool (*)(unsigned int, unsigned int *)>(GetProcAddress(HModule, "GetInjectionTarget"));
    FindNextRecord = reinterpret_cast<bool (*)(unsigned int, wchar_t *, bool, bool, unsigned int *)>(
        GetProcAddress(HModule, "FindNextRecord"));
    FindPreviousRecord = reinterpret_cast<bool (*)(unsigned int, wchar_t *, bool, bool, unsigned int *)>(
        GetProcAddress(HModule, "FindPreviousRecord"));
    FindValidReferences = reinterpret_cast<bool (*)(unsigned int, wchar_t *, wchar_t *, int, int *)>(
        GetProcAddress(HModule, "FindValidReferences"));
    GetReferencedBy = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "GetReferencedBy"));
    ExchangeReferences = reinterpret_cast<bool (*)(unsigned int, unsigned int, unsigned int)>(
        GetProcAddress(HModule, "ExchangeReferences"));
    IsMaster = reinterpret_cast<bool (*)(unsigned int, bool *)>(GetProcAddress(HModule, "IsMaster"));
    IsInjected = reinterpret_cast<bool (*)(unsigned int, bool *)>(GetProcAddress(HModule, "IsInjected"));
    IsOverride = reinterpret_cast<bool (*)(unsigned int, bool *)>(GetProcAddress(HModule, "IsOverride"));
    IsWinningOverride = reinterpret_cast<bool (*)(unsigned int, bool *)>(GetProcAddress(HModule, "IsWinningOverride"));
    GetNodes = reinterpret_cast<bool (*)(unsigned int, unsigned int *)>(GetProcAddress(HModule, "GetNodes"));
    GetConflictData = reinterpret_cast<bool (*)(unsigned int, unsigned int, unsigned char *, unsigned char *)>(
        GetProcAddress(HModule, "GetConflictData"));
    GetNodeElements =
        reinterpret_cast<bool (*)(unsigned int, unsigned int, int *)>(GetProcAddress(HModule, "GetNodeElements"));
    CheckForErrors = reinterpret_cast<bool (*)(unsigned int)>(GetProcAddress(HModule, "CheckForErrors"));
    GetErrorThreadDone = reinterpret_cast<bool (*)()>(GetProcAddress(HModule, "GetErrorThreadDone"));
    GetErrors = reinterpret_cast<bool (*)(int *)>(GetProcAddress(HModule, "GetErrors"));
    GetErrorString = reinterpret_cast<bool (*)(unsigned int, int *)>(GetProcAddress(HModule, "GetErrorString"));
    RemoveIdenticalRecords =
        reinterpret_cast<bool (*)(unsigned int, bool, bool)>(GetProcAddress(HModule, "RemoveIdenticalRecords"));
    FilterRecord = reinterpret_cast<bool (*)(unsigned int)>(GetProcAddress(HModule, "FilterRecord"));
    ResetFilter = reinterpret_cast<bool (*)()>(GetProcAddress(HModule, "ResetFilter"));
  }
}

XEditLibCpp::~XEditLibCpp() {
  if (HModule != nullptr) {
    CloseXEdit();
    FreeLibrary(HModule);
  }
}

// Helpers
auto XEditLibCpp::getResultStringSafe(int Len) -> std::wstring {
  auto Str = make_unique<wchar_t[]>(Len);
  GetResultString(Str.get(), Len);

  std::wstring OutStr(Str.get());
  OutStr.erase(Len);

  return OutStr;
}

auto XEditLibCpp::wstrToWchar(const std::wstring &Str) -> wchar_t * {
  auto WcharArray = make_unique<wchar_t[]>(Str.size() + 1);
  wcscpy_s(WcharArray.get(), Str.size() + 1, Str.c_str());
  return WcharArray.get();
}
