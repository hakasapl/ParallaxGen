#pragma once

#include <string>
#include <windows.h>


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
  // Declare function pointers
  void (*InitXEdit)();
  void (*CloseXEdit)();
  bool (*GetResultString)(wchar_t *Str, int Len);
  bool (*GetResultArray)(unsigned int *Res, int Len);
  bool (*GetResultBytes)(unsigned char *Res, int Len);
  bool (*GetGlobal)(wchar_t *Key, int *Len);
  bool (*GetGlobals)(int *Len);
  bool (*SetSortMode)(unsigned char SortBy, bool Reverse);
  bool (*Release)(unsigned int Id);
  bool (*ReleaseNodes)(unsigned int Id);
  bool (*Switch)(unsigned int Id, unsigned int Id2);
  bool (*GetDuplicateHandles)(unsigned int Id, int *Len);
  bool (*ResetStore)();
  void (*GetMessagesLength)(int *Len);
  bool (*GetMessages)(wchar_t *Str, int Len);
  void (*ClearMessages)();
  void (*GetExceptionMessageLength)(int *Len);
  bool (*GetExceptionMessage)(wchar_t *Str, int Len);
  bool (*GetGamePath)(int Mode, int *Len);
  bool (*SetGamePath)(wchar_t *Path);
  bool (*GetGameLanguage)(int Mode, int *Len);
  bool (*SetLanguage)(wchar_t *Lang);
  bool (*SetBackupPath)(wchar_t *Path);
  bool (*SetGameMode)(int Mode);
  bool (*GetLoadOrder)(int *Len);
  bool (*GetActivePlugins)(int *Len);
  bool (*LoadPlugins)(wchar_t *LoadOrder, bool SmartLoad);
  bool (*LoadPlugin)(wchar_t *Filename);
  bool (*LoadPluginHeader)(wchar_t *Filename, unsigned int *Res);
  bool (*BuildReferences)(unsigned int Id, bool Synchronous);
  bool (*GetLoaderStatus)(unsigned char *Status);
  bool (*UnloadPlugin)(unsigned int Id);
  bool (*AddFile)(wchar_t *Filename, unsigned int *Res);
  bool (*FileByIndex)(int Index, unsigned int *Res);
  bool (*FileByLoadOrder)(int LoadOrder, unsigned int *Res);
  bool (*FileByName)(wchar_t *Name, unsigned int *Res);
  bool (*FileByAuthor)(wchar_t *Author, unsigned int *Res);
  bool (*SaveFile)(unsigned int Id, wchar_t *FilePath);
  bool (*GetRecordCount)(unsigned int Id, int *Count);
  bool (*GetOverrideRecordCount)(unsigned int Id, int *Count);
  bool (*MD5Hash)(unsigned int Id, int *Len);
  bool (*CRCHash)(unsigned int Id, int *Len);
  bool (*SortEditorIDs)(unsigned int Id, wchar_t *Sig);
  bool (*SortNames)(unsigned int Id, wchar_t *Sig);
  bool (*GetFileLoadOrder)(unsigned int Id, int *LoadOrder);
  bool (*ExtractContainer)(wchar_t *Name, wchar_t *Destination, bool Replace);
  bool (*ExtractFile)(wchar_t *Name, wchar_t *Source, wchar_t *Destination);
  bool (*GetContainerFiles)(wchar_t *Name, wchar_t *Path, int *Len);
  bool (*GetFileContainer)(wchar_t *Path, int *Len);
  bool (*GetLoadedContainers)(int *Len);
  bool (*LoadContainer)(wchar_t *FilePath);
  bool (*BuildArchive)(wchar_t *Name, wchar_t *Folder, wchar_t *FilePaths, int ArchiveType, bool BCompress, bool BShare,
                       wchar_t *AF, wchar_t *FF);
  bool (*GetTextureData)(wchar_t *ResourceName, int *Width, int *Height);
  bool (*CleanMasters)(unsigned int Id);
  bool (*SortMasters)(unsigned int Id);
  bool (*AddMaster)(unsigned int Id, wchar_t *MasterName);
  bool (*AddMasters)(unsigned int Id, wchar_t *Masters);
  bool (*AddRequiredMasters)(unsigned int Id, unsigned int Id2, bool AsNew);
  bool (*GetMasters)(unsigned int Id, int *Len);
  bool (*GetRequiredBy)(unsigned int Id, int *Len);
  bool (*GetMasterNames)(unsigned int Id, int *Len);
  bool (*HasElement)(unsigned int Id, wchar_t *Key, bool *BoolVal);
  bool (*GetElement)(unsigned int Id, wchar_t *Key, unsigned int *Res);
  bool (*AddElement)(unsigned int Id, wchar_t *Key, unsigned int *Res);
  bool (*AddElementValue)(unsigned int Id, wchar_t *Key, wchar_t *Value, unsigned int *Res);
  bool (*RemoveElement)(unsigned int Id, wchar_t *Key);
  bool (*RemoveElementOrParent)(unsigned int Id);
  bool (*SetElement)(unsigned int Id, unsigned int Id2);
  bool (*GetElements)(unsigned int Id, wchar_t *Key, bool Sort, bool Filter, int *Len);
  bool (*GetDefNames)(unsigned int Id, int *Len);
  bool (*GetAddList)(unsigned int Id, int *Len);
  bool (*GetLinksTo)(unsigned int Id, wchar_t *Key, unsigned int *Res);
  bool (*SetLinksTo)(unsigned int Id, wchar_t *Key, unsigned int Id2);
  bool (*GetElementIndex)(unsigned int Id, int *Index);
  bool (*GetContainer)(unsigned int Id, unsigned int *Res);
  bool (*GetElementFile)(unsigned int Id, unsigned int *Res);
  bool (*GetElementRecord)(unsigned int Id, unsigned int *Res);
  bool (*ElementCount)(unsigned int Id, int *Count);
  bool (*ElementEquals)(unsigned int Id, unsigned int Id2, bool *BoolVal);
  bool (*ElementMatches)(unsigned int Id, wchar_t *Path, wchar_t *Value, bool *BoolVal);
  bool (*HasArrayItem)(unsigned int Id, wchar_t *Path, wchar_t *SubPath, wchar_t *Value, bool *BoolVal);
  bool (*GetArrayItem)(unsigned int Id, wchar_t *Path, wchar_t *SubPath, wchar_t *Value, unsigned int *Res);
  bool (*AddArrayItem)(unsigned int Id, wchar_t *Path, wchar_t *SubPath, wchar_t *Value, unsigned int *Res);
  bool (*RemoveArrayItem)(unsigned int Id, wchar_t *Path, wchar_t *SubPath, wchar_t *Value);
  bool (*MoveArrayItem)(unsigned int Id, int Index);
  bool (*CopyElement)(unsigned int Id, unsigned int Id2, bool AsNew, unsigned int *Res);
  bool (*GetSignatureAllowed)(unsigned int Id, wchar_t *Sig, bool *BoolVal);
  bool (*GetAllowedSignatuRes)(unsigned int Id, int *Len);
  bool (*GetIsModified)(unsigned int Id, bool *BoolVal);
  bool (*GetIsEditable)(unsigned int Id, bool *BoolVal);
  bool (*GetIsRemoveable)(unsigned int Id, bool *BoolVal);
  bool (*GetCanAdd)(unsigned int Id, bool *BoolVal);
  bool (*SortKey)(unsigned int Id, int *Len);
  bool (*ElementType)(unsigned int Id, unsigned char *EnumVal);
  bool (*DefType)(unsigned int Id, unsigned char *EnumVal);
  bool (*SmashType)(unsigned int Id, unsigned char *EnumVal);
  bool (*ValueType)(unsigned int Id, unsigned char *EnumVal);
  bool (*IsSorted)(unsigned int Id, bool *BoolVal);
  bool (*Name)(unsigned int Id, int *Len);
  bool (*LongName)(unsigned int Id, int *Len);
  bool (*DisplayName)(unsigned int Id, int *Len);
  bool (*Path)(unsigned int Id, bool ShortPath, bool Local, int *Len);
  bool (*Signature)(unsigned int Id, int *Len);
  bool (*GetValue)(unsigned int Id, wchar_t *Path, int *Len);
  bool (*SetValue)(unsigned int Id, wchar_t *Path, wchar_t *Value);
  bool (*GetIntValue)(unsigned int Id, wchar_t *Path, int *Value);
  bool (*SetIntValue)(unsigned int Id, wchar_t *Path, int Value);
  bool (*GetUIntValue)(unsigned int Id, wchar_t *Path, unsigned int *Value);
  bool (*SetUIntValue)(unsigned int Id, wchar_t *Path, unsigned int Value);
  bool (*GetFloatValue)(unsigned int Id, wchar_t *Path, double *Value);
  bool (*SetFloatValue)(unsigned int Id, wchar_t *Path, double Value);
  bool (*GetFlag)(unsigned int Id, wchar_t *Path, wchar_t *Name, bool *Enabled);
  bool (*SetFlag)(unsigned int Id, wchar_t *Path, wchar_t *Name, bool Enabled);
  bool (*GetAllFlags)(unsigned int Id, wchar_t *Path, int *Len);
  bool (*GetEnabledFlags)(unsigned int Id, wchar_t *Path, int *Len);
  bool (*SetEnabledFlags)(unsigned int Id, wchar_t *Path, wchar_t *Flags);
  bool (*GetEnumOptions)(unsigned int Id, wchar_t *Path, int *Len);
  bool (*SignatureFromName)(wchar_t *Name, int *Len);
  bool (*NameFromSignature)(wchar_t *Sig, int *Len);
  bool (*GetSignatureNameMap)(int *Len);
  bool (*ElementToJson)(unsigned int Id, int *Len);
  bool (*ElementFromJson)(unsigned int Id, wchar_t *Path, wchar_t *JSON);
  bool (*GetFormID)(unsigned int Id, unsigned int *FormID, bool Local);
  bool (*SetFormID)(unsigned int Id, unsigned int FormID, bool Local, bool FixReferences);
  bool (*GetRecord)(unsigned int Id, unsigned int FormID, bool SearchMasters, unsigned int *Res);
  bool (*GetRecords)(unsigned int Id, wchar_t *Search, bool IncludeOverrides, int *Len);
  bool (*GetREFRs)(unsigned int Id, wchar_t *Search, unsigned int Flags, int *Len);
  bool (*GetOverrides)(unsigned int Id, int *Count);
  bool (*GetMasterRecord)(unsigned int Id, unsigned int *Res);
  bool (*GetWinningOverride)(unsigned int Id, unsigned int *Res);
  bool (*GetInjectionTarget)(unsigned int Id, unsigned int *Res);
  bool (*FindNextRecord)(unsigned int Id, wchar_t *Search, bool ByEdid, bool ByName, unsigned int *Res);
  bool (*FindPreviousRecord)(unsigned int Id, wchar_t *Search, bool ByEdid, bool ByName, unsigned int *Res);
  bool (*FindValidReferences)(unsigned int Id, wchar_t *Signature, wchar_t *Search, int LimitTo, int *Len);
  bool (*GetReferencedBy)(unsigned int Id, int *Len);
  bool (*ExchangeReferences)(unsigned int Id, unsigned int OldFormID, unsigned int NewFormID);
  bool (*IsMaster)(unsigned int Id, bool *BoolVal);
  bool (*IsInjected)(unsigned int Id, bool *BoolVal);
  bool (*IsOverride)(unsigned int Id, bool *BoolVal);
  bool (*IsWinningOverride)(unsigned int Id, bool *BoolVal);
  bool (*GetNodes)(unsigned int Id, unsigned int *Res);
  bool (*GetConflictData)(unsigned int Id, unsigned int Id2, unsigned char *ConflictAll, unsigned char *ConflictThis);
  bool (*GetNodeElements)(unsigned int Id, unsigned int Id2, int *Len);
  bool (*CheckForErrors)(unsigned int Id);
  bool (*GetErrorThreadDone)();
  bool (*GetErrors)(int *Len);
  bool (*GetErrorString)(unsigned int Id, int *Len);
  bool (*RemoveIdenticalRecords)(unsigned int Id, bool RemoveITMs, bool RemoveITPOs);
  bool (*FilterRecord)(unsigned int Id);
  bool (*ResetFilter)();

  // Helpers
  auto getResultStringSafe(int Len) -> std::wstring;

  static auto wstrToWchar(const std::wstring &Str) -> wchar_t *;
};
