namespace ParallaxGenMutagenWrapper;

// Base Imports
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;

// Mutagen
using Mutagen.Bethesda;
using Mutagen.Bethesda.Skyrim;
using Mutagen.Bethesda.Environments;
using Mutagen.Bethesda.Plugins;
using Mutagen.Bethesda.Plugins.Binary.Parameters;
using Mutagen.Bethesda.Plugins.Records;
using Noggog;
using Mutagen.Bethesda.Plugins.Utility;
using Mutagen.Bethesda.Plugins.Analysis;

public static class ExceptionHandler
{
  private static string? LastExceptionMessage;

  public static void SetLastException(Exception? ex)
  {
    // Find the deepest inner exception
    while (ex is not null)
    {
      LastExceptionMessage += "\n" + ex.Message + "\n" + ex.StackTrace;
      ex = ex.InnerException;
    }
  }

  [UnmanagedCallersOnly(EntryPoint = "GetLastException", CallConvs = [typeof(CallConvCdecl)])]
  public static unsafe void GetLastException([DNNE.C99Type("wchar_t**")] IntPtr* errorMessagePtr)
  {
    string? errorMessage = LastExceptionMessage ?? string.Empty;

    if (LastExceptionMessage.IsNullOrEmpty())
    {
      return;
    }

    *errorMessagePtr = Marshal.StringToHGlobalUni(errorMessage);
  }
}

public static class MessageHandler
{
  private static Queue<Tuple<string, int>> LogQueue = [];

  public static void Log(string message, int level = 0)
  {
    // Level values
    // 0: Trace
    // 1: Debug
    // 2: Info
    // 3: Warning
    // 4: Error
    // 5: Critical
    LogQueue.Enqueue(new Tuple<string, int>("[ParallaxGenMutagenWrapper] " + message, level));
  }

  [UnmanagedCallersOnly(EntryPoint = "GetLogMessage", CallConvs = [typeof(CallConvCdecl)])]
  public static unsafe void GetLogMessage([DNNE.C99Type("wchar_t**")] IntPtr* messagePtr, [DNNE.C99Type("int*")] int* level)
  {
    if (LogQueue.Count == 0)
    {
      return;
    }

    var logMessage = LogQueue.Dequeue();
    string message = logMessage.Item1;
    *messagePtr = Marshal.StringToHGlobalUni(logMessage.Item1);
    *level = logMessage.Item2;
  }
}

public class PGMutagen
{
  // "Class vars" actually static because p/invoke doesn't support instance methods
  private static SkyrimMod? OutMod;
  private static IGameEnvironment<ISkyrimMod, ISkyrimModGetter>? Env;
  private static List<ITextureSetGetter> TXSTObjs = [];
  private static List<Tuple<IAlternateTextureGetter, int>> AltTexRefs = [];
  private static List<IMajorRecord> ModelCopies = [];
  private static List<IMajorRecord> ModelCopiesEditable = [];
  private static Dictionary<Tuple<string, string, int>, List<Tuple<int, int>>>? TXSTRefs;
  private static HashSet<int> ModifiedModeledRecords = [];

  private static IEnumerable<IMajorRecordGetter> EnumerateModelRecords()
  {
    if (Env is null)
    {
      return [];
    }

    return Env.LoadOrder.PriorityOrder.Activator().WinningOverrides()
            .Concat<IMajorRecordGetter>(Env.LoadOrder.PriorityOrder.AddonNode().WinningOverrides())
            .Concat<IMajorRecordGetter>(Env.LoadOrder.PriorityOrder.Ammunition().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.AnimatedObject().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.Armor().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.ArmorAddon().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.ArtObject().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.BodyPartData().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.Book().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.CameraShot().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.Climate().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.Container().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.Door().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.Explosion().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.Flora().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.Furniture().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.Grass().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.Hazard().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.HeadPart().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.IdleMarker().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.Impact().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.Ingestible().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.Ingredient().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.Key().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.LeveledNpc().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.Light().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.MaterialObject().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.MiscItem().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.MoveableStatic().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.Projectile().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.Scroll().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.SoulGem().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.Static().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.TalkingActivator().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.Tree().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.Weapon().WinningOverrides());
  }

  [UnmanagedCallersOnly(EntryPoint = "Initialize", CallConvs = [typeof(CallConvCdecl)])]
  public static unsafe void Initialize(
    [DNNE.C99Type("const int")] int gameType,
    [DNNE.C99Type("const wchar_t*")] IntPtr dataPathPtr,
    [DNNE.C99Type("const wchar_t*")] IntPtr outputPluginPtr,
    [DNNE.C99Type("const wchar_t**")] IntPtr* loadOrder)
  {
    try
    {
      string dataPath = Marshal.PtrToStringUni(dataPathPtr) ?? string.Empty;
      MessageHandler.Log("[Initialize] Data Path: " + dataPath, 0);
      string outputPlugin = Marshal.PtrToStringUni(outputPluginPtr) ?? string.Empty;
      MessageHandler.Log("[Initialize] Output Plugin: " + dataPath, 0);

      // check if loadorder is defined
      if (loadOrder is not null)
      {
        // Load order is defined
        List<ModKey> loadOrderList = [];
        for (int i = 0; loadOrder[i] != IntPtr.Zero; i++)
        {
          loadOrderList.Add(Marshal.PtrToStringUni(loadOrder[i]) ?? string.Empty);
        }

        Env = GameEnvironment.Typical.Builder<ISkyrimMod, ISkyrimModGetter>((GameRelease)gameType)
          .WithTargetDataFolder(dataPath)
          .WithLoadOrder(loadOrderList.ToArray())
          .Build();
      }
      else
      {
        Env = GameEnvironment.Typical.Builder<ISkyrimMod, ISkyrimModGetter>((GameRelease)gameType)
            .WithTargetDataFolder(dataPath)
            .Build();
      }

      OutMod = new SkyrimMod(ModKey.FromFileName(outputPlugin), (SkyrimRelease)gameType);
    }
    catch (Exception ex)
    {
      ExceptionHandler.SetLastException(ex);
    }
  }

  private static List<IModelGetter> GetModelElems(IMajorRecordGetter Rec)
  {
    // Will store models to check later
    List<IModelGetter> ModelRecs = [];

    // Figure out special cases for nested models
    if (Rec is IArmorGetter armorObj && armorObj.WorldModel is not null)
    {
      if (armorObj.WorldModel.Male is not null && armorObj.WorldModel.Male.Model is not null)
      {
        ModelRecs.Add(armorObj.WorldModel.Male.Model);
      }

      if (armorObj.WorldModel.Female is not null && armorObj.WorldModel.Female.Model is not null)
      {
        ModelRecs.Add(armorObj.WorldModel.Female.Model);
      }
    }
    else if (Rec is IArmorAddonGetter armorAddonObj && armorAddonObj.WorldModel is not null)
    {
      if (armorAddonObj.WorldModel.Male is not null)
      {
        ModelRecs.Add(armorAddonObj.WorldModel.Male);
      }

      if (armorAddonObj.WorldModel.Female is not null)
      {
        ModelRecs.Add(armorAddonObj.WorldModel.Female);
      }
    }
    else if (Rec is IModeledGetter modeledObj && modeledObj.Model is not null)
    {
      ModelRecs.Add(modeledObj.Model);
    }

    return ModelRecs;
  }

  private static List<IModel> GetModelElems(IMajorRecord Rec)
  {
    // Will store models to check later
    List<IModel> ModelRecs = [];

    // Figure out special cases for nested models
    if (Rec is IArmor armorObj && armorObj.WorldModel is not null)
    {
      if (armorObj.WorldModel.Male is not null && armorObj.WorldModel.Male.Model is not null)
      {
        ModelRecs.Add(armorObj.WorldModel.Male.Model);
      }

      if (armorObj.WorldModel.Female is not null && armorObj.WorldModel.Female.Model is not null)
      {
        ModelRecs.Add(armorObj.WorldModel.Female.Model);
      }
    }
    else if (Rec is IArmorAddon armorAddonObj && armorAddonObj.WorldModel is not null)
    {
      if (armorAddonObj.WorldModel.Male is not null)
      {
        ModelRecs.Add(armorAddonObj.WorldModel.Male);
      }

      if (armorAddonObj.WorldModel.Female is not null)
      {
        ModelRecs.Add(armorAddonObj.WorldModel.Female);
      }
    }
    else if (Rec is IModeled modeledObj && modeledObj.Model is not null)
    {
      ModelRecs.Add(modeledObj.Model);
    }

    return ModelRecs;
  }

  private static HashSet<int> TXSTErrorTracker = [];

  [UnmanagedCallersOnly(EntryPoint = "PopulateObjs", CallConvs = [typeof(CallConvCdecl)])]
  public static void PopulateObjs()
  {
    try
    {
      if (Env is null)
      {
        throw new Exception("Initialize must be called before PopulateObjs");
      }

      TXSTObjs = [];
      foreach (var textureSet in Env.LoadOrder.PriorityOrder.TextureSet().WinningOverrides())
      {
        MessageHandler.Log("[PopulateObjs] Adding TXST record as index " + TXSTObjs.Count + ": " + GetRecordDesc(textureSet), 0);
        TXSTObjs.Add(textureSet);
      }

      TXSTRefs = [];
      foreach (var txstRefObj in EnumerateModelRecords())
      {
        // Will store models to check later
        List<IModelGetter> ModelRecs = GetModelElems(txstRefObj);

        if (ModelRecs.Count == 0)
        {
          // Skip if there is nothing to search
          MessageHandler.Log("[PopulateObjs] No models found for record: " + GetRecordDesc(txstRefObj), 0);
          continue;
        }

        bool CopiedRecord = false;
        var DCIdx = -1;
        foreach (var modelRec in ModelRecs)
        {
          if (modelRec.AlternateTextures is null)
          {
            // no alternate textures
            MessageHandler.Log("[PopulateObjs] No alternate textures found for a model record: " + GetRecordDesc(txstRefObj), 0);
            continue;
          }

          if (!CopiedRecord)
          {
            // Deep copy major recor
            try
            {
              var DeepCopy = txstRefObj.DeepCopy();
              ModelCopies.Add(DeepCopy);
              var DeepCopyEditable = txstRefObj.DeepCopy();
              ModelCopiesEditable.Add(DeepCopyEditable);
              DCIdx = ModelCopies.Count - 1;
              CopiedRecord = true;
            }
            catch (Exception)
            {
              MessageHandler.Log("Failed to copy record: " + GetRecordDesc(txstRefObj), 4);
              break;
            }
          }

          if (DCIdx < 0)
          {
            throw new Exception("DCIdx is negative: this shouldn't happen");
          }

          // find lowercase nifname
          string nifName = modelRec.File;

          // Otherwise this causes issues with deepcopy
          nifName = RemovePrefixIfExists("\\", nifName);

          nifName = nifName.ToLower();
          nifName = AddPrefixIfNotExists("meshes\\", nifName);

          MessageHandler.Log("[PopulateObjs] NIF Name '" + nifName + "' found in model record in record " + GetRecordDesc(txstRefObj), 0);

          foreach (var alternateTexture in modelRec.AlternateTextures)
          {
            // Add to global
            var AltTexEntry = new Tuple<IAlternateTextureGetter, int>(alternateTexture, DCIdx);
            AltTexRefs.Add(AltTexEntry);
            var AltTexId = AltTexRefs.Count - 1;

            string name3D = alternateTexture.Name;
            int index3D = alternateTexture.Index;
            var newTXST = alternateTexture.NewTexture;

            var key = new Tuple<string, string, int>(nifName, name3D, index3D);

            MessageHandler.Log("[PopulateObjs] Adding to AltTexRefs as index " + AltTexId + ": " + key.ToString(), 0);

            int newTXSTIndex = TXSTObjs.FindIndex(x => x.FormKey == newTXST.FormKey);
            if (newTXSTIndex < 0)
            {
              var CurHashCode = newTXST.GetHashCode();
              if (TXSTErrorTracker.Add(CurHashCode))
              {
                MessageHandler.Log("Referenced TXST record in " + GetRecordDesc(txstRefObj) + " / " + key.ToString() + " does not exist", 3);
              }
              continue;
            }

            var newTXSTObj = TXSTObjs[newTXSTIndex];

            if (newTXSTIndex >= 0)
            {
              if (!TXSTRefs.ContainsKey(key))
              {
                TXSTRefs[key] = [];
              }
              TXSTRefs[key].Add(new Tuple<int, int>(newTXSTIndex, AltTexId));
              MessageHandler.Log("[PopulateObjs] Adding Alt Tex Reference: " + key.ToString() + " -> " + GetRecordDesc(newTXSTObj), 0);
            }
          }
        }
      }
    }
    catch (Exception ex)
    {
      ExceptionHandler.SetLastException(ex);
    }
  }

  [UnmanagedCallersOnly(EntryPoint = "Finalize", CallConvs = [typeof(CallConvCdecl)])]
  public static void Finalize([DNNE.C99Type("const wchar_t*")] IntPtr outputPathPtr)
  {
    try
    {
      if (Env is null || OutMod is null)
      {
        throw new Exception("Initialize must be called before Finalize");
      }

      // Add all modified model records to the output mod
      foreach (var recId in ModifiedModeledRecords)
      {
        var ModifiedRecord = ModelCopiesEditable[recId];
        if (ModifiedRecord is Mutagen.Bethesda.Skyrim.Activator)
        {
          OutMod.Activators.Add((Mutagen.Bethesda.Skyrim.Activator)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Activator: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is Ammunition)
        {
          OutMod.Ammunitions.Add((Ammunition)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Ammunition: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is AnimatedObject)
        {
          OutMod.AnimatedObjects.Add((AnimatedObject)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Animated Object: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is Armor)
        {
          OutMod.Armors.Add((Armor)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Armor: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is ArmorAddon)
        {
          OutMod.ArmorAddons.Add((ArmorAddon)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Armor Addon: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is ArtObject)
        {
          OutMod.ArtObjects.Add((ArtObject)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Art Object: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is BodyPartData)
        {
          OutMod.BodyParts.Add((BodyPartData)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Body Part Data: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is Book)
        {
          OutMod.Books.Add((Book)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Book: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is CameraShot)
        {
          OutMod.CameraShots.Add((CameraShot)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Camera Shot: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is Climate)
        {
          OutMod.Climates.Add((Climate)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Climate: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is Container)
        {
          OutMod.Containers.Add((Container)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Container: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is Door)
        {
          OutMod.Doors.Add((Door)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Door: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is Explosion)
        {
          OutMod.Explosions.Add((Explosion)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Explosion: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is Flora)
        {
          OutMod.Florae.Add((Flora)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Flora: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is Furniture)
        {
          OutMod.Furniture.Add((Furniture)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Furniture: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is Grass)
        {
          OutMod.Grasses.Add((Grass)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Grass: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is Hazard)
        {
          OutMod.Hazards.Add((Hazard)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Hazard: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is HeadPart)
        {
          OutMod.HeadParts.Add((HeadPart)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Head Part: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is IdleMarker)
        {
          OutMod.IdleMarkers.Add((IdleMarker)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Idle Marker: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is Impact)
        {
          OutMod.Impacts.Add((Impact)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Impact: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is Ingestible)
        {
          OutMod.Ingestibles.Add((Ingestible)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Ingestible: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is Ingredient)
        {
          OutMod.Ingredients.Add((Ingredient)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Ingredient: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is Key)
        {
          OutMod.Keys.Add((Key)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Key: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is LeveledNpc)
        {
          OutMod.LeveledNpcs.Add((LeveledNpc)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Leveled NPC: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is Light)
        {
          OutMod.Lights.Add((Light)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Light: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is MaterialObject)
        {
          OutMod.MaterialObjects.Add((MaterialObject)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Material Object: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is MiscItem)
        {
          OutMod.MiscItems.Add((MiscItem)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Misc Item: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is MoveableStatic)
        {
          OutMod.MoveableStatics.Add((MoveableStatic)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Moveable Static: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is Projectile)
        {
          OutMod.Projectiles.Add((Projectile)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Projectile: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is Scroll)
        {
          OutMod.Scrolls.Add((Scroll)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Scroll: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is SoulGem)
        {
          OutMod.SoulGems.Add((SoulGem)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Soul Gem: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is Static)
        {
          OutMod.Statics.Add((Static)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Static: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is TalkingActivator)
        {
          OutMod.TalkingActivators.Add((TalkingActivator)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Talking Activator: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is Tree)
        {
          OutMod.Trees.Add((Tree)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Tree: " + GetRecordDesc(ModifiedRecord), 0);
        }
        else if (ModifiedRecord is Weapon)
        {
          OutMod.Weapons.Add((Weapon)ModifiedRecord);
          MessageHandler.Log("[Finalize] Adding Weapon: " + GetRecordDesc(ModifiedRecord), 0);
        }
      }

      if (RecordCompactionCompatibilityDetection.CouldBeSmallMasterCompatible(OutMod)) {
        // Can be light
        MessageHandler.Log("[Finalize] Output Plugin can be compacted to a small master", 0);
        ModCompaction.CompactToSmallMaster(OutMod);
      }

      // Write the output plugin
      MessageHandler.Log("[Finalize] Writing Output Plugin", 0);
      string outputPath = Marshal.PtrToStringUni(outputPathPtr) ?? string.Empty;
      OutMod?.WriteToBinary(Path.Combine(outputPath, OutMod.ModKey.FileName),
          new BinaryWriteParameters()
          {
            MastersListOrdering = new MastersListOrderingByLoadOrder(Env.LoadOrder)
          });
    }
    catch (Exception ex)
    {
      ExceptionHandler.SetLastException(ex);
    }
  }

  [UnmanagedCallersOnly(EntryPoint = "GetMatchingTXSTObjs", CallConvs = [typeof(CallConvCdecl)])]
  public static unsafe void GetMatchingTXSTObjs(
    [DNNE.C99Type("const wchar_t*")] IntPtr nifNamePtr,
    [DNNE.C99Type("const wchar_t*")] IntPtr name3DPtr,
    [DNNE.C99Type("const int")] int index3D,
    [DNNE.C99Type("int*")] int* TXSTHandles,
    [DNNE.C99Type("int*")] int* AltTexHandles,
    [DNNE.C99Type("int*")] int* length)
  {
    try
    {
      if (TXSTRefs is null)
      {
        throw new Exception("PopulateObjs must be called before GetMatchingTXSTObjs");
      }

      if (length is not null)
      {
        *length = 0;
      }

      string nifName = Marshal.PtrToStringUni(nifNamePtr) ?? string.Empty;
      string name3D = Marshal.PtrToStringUni(name3DPtr) ?? string.Empty;
      var key = new Tuple<string, string, int>(nifName, name3D, index3D);

      // Log input params
      MessageHandler.Log("[GetMatchingTXSTObjs] Getting Matching TXST Objects for: " + key.ToString(), 0);

      if (TXSTRefs.ContainsKey(key))
      {
        var txstList = TXSTRefs[key];
        if (length is not null)
        {
          *length = txstList.Count;
          MessageHandler.Log("[GetMatchingTXSTObjs] Found " + txstList.Count + " Matching TXST Objects", 0);
        }

        if (TXSTHandles is null || AltTexHandles is null)
        {
          return;
        }

        // Manually copy the elements from the list to the pointer
        for (int i = 0; i < txstList.Count; i++)
        {
          TXSTHandles[i] = txstList[i].Item1;
          AltTexHandles[i] = txstList[i].Item2;

          MessageHandler.Log("[GetMatchingTXSTObjs] Found Matching TXST: " + key.ToString() + " -> " + GetRecordDesc(TXSTObjs[txstList[i].Item1]), 0);
        }
      }
    }
    catch (Exception ex)
    {
      ExceptionHandler.SetLastException(ex);
      if (length is not null)
      {
        *length = 0;
      }
    }
  }

  [UnmanagedCallersOnly(EntryPoint = "GetTXSTSlots", CallConvs = [typeof(CallConvCdecl)])]
  public static unsafe void GetTXSTSlots(
    [DNNE.C99Type("const int")] int txstIndex,
    [DNNE.C99Type("wchar_t**")] IntPtr* slotsArray)
  {
    try
    {
      if (TXSTObjs == null || txstIndex < 0 || txstIndex >= TXSTObjs.Count)
      {
        throw new Exception("Invalid TXST index or not populated");
      }

      // print txst Index
      MessageHandler.Log("[GetTXSTSlots] [TXST Index: " + txstIndex + "]", 0);

      var txstObj = TXSTObjs[txstIndex];

      // Populate the slotsArray with string pointers
      if (!txstObj.Diffuse.IsNullOrEmpty())
      {
        var Diffuse = AddPrefixIfNotExists("textures\\", txstObj.Diffuse).ToLower();
        MessageHandler.Log("[GetTXSTSlots] [TXST Index: " + txstIndex + "] Diffuse: " + Diffuse, 0);
        slotsArray[0] = Marshal.StringToHGlobalUni(Diffuse);
      }
      if (!txstObj.NormalOrGloss.IsNullOrEmpty())
      {
        var NormalOrGloss = AddPrefixIfNotExists("textures\\", txstObj.NormalOrGloss).ToLower();
        MessageHandler.Log("[GetTXSTSlots] [TXST Index: " + txstIndex + "] NormalOrGloss: " + NormalOrGloss, 0);
        slotsArray[1] = Marshal.StringToHGlobalUni(NormalOrGloss);
      }
      if (!txstObj.GlowOrDetailMap.IsNullOrEmpty())
      {
        var GlowOrDetailMap = AddPrefixIfNotExists("textures\\", txstObj.GlowOrDetailMap).ToLower();
        MessageHandler.Log("[GetTXSTSlots] [TXST Index: " + txstIndex + "] GlowOrDetailMap: " + GlowOrDetailMap, 0);
        slotsArray[2] = Marshal.StringToHGlobalUni(GlowOrDetailMap);
      }
      if (!txstObj.Height.IsNullOrEmpty())
      {
        var Height = AddPrefixIfNotExists("textures\\", txstObj.Height).ToLower();
        MessageHandler.Log("[GetTXSTSlots] [TXST Index: " + txstIndex + "] Height: " + Height, 0);
        slotsArray[3] = Marshal.StringToHGlobalUni(Height);
      }
      if (!txstObj.Environment.IsNullOrEmpty())
      {
        var Environment = AddPrefixIfNotExists("textures\\", txstObj.Environment).ToLower();
        MessageHandler.Log("[GetTXSTSlots] [TXST Index: " + txstIndex + "] Environment: " + Environment, 0);
        slotsArray[4] = Marshal.StringToHGlobalUni(Environment);
      }
      if (!txstObj.EnvironmentMaskOrSubsurfaceTint.IsNullOrEmpty())
      {
        var EnvironmentMaskOrSubsurfaceTint = AddPrefixIfNotExists("textures\\", txstObj.EnvironmentMaskOrSubsurfaceTint).ToLower();
        MessageHandler.Log("[GetTXSTSlots] [TXST Index: " + txstIndex + "] EnvironmentMaskOrSubsurfaceTint: " + EnvironmentMaskOrSubsurfaceTint, 0);
        slotsArray[5] = Marshal.StringToHGlobalUni(EnvironmentMaskOrSubsurfaceTint);
      }
      if (!txstObj.Multilayer.IsNullOrEmpty())
      {
        var Multilayer = AddPrefixIfNotExists("textures\\", txstObj.Multilayer).ToLower();
        MessageHandler.Log("[GetTXSTSlots] [TXST Index: " + txstIndex + "] Multilayer: " + Multilayer, 0);
        slotsArray[6] = Marshal.StringToHGlobalUni(Multilayer);
      }
      if (!txstObj.BacklightMaskOrSpecular.IsNullOrEmpty())
      {
        var BacklightMaskOrSpecular = AddPrefixIfNotExists("textures\\", txstObj.BacklightMaskOrSpecular).ToLower();
        MessageHandler.Log("[GetTXSTSlots] [TXST Index: " + txstIndex + "] BacklightMaskOrSpecular: " + BacklightMaskOrSpecular, 0);
        slotsArray[7] = Marshal.StringToHGlobalUni(BacklightMaskOrSpecular);
      }
      slotsArray[8] = IntPtr.Zero;
    }
    catch (Exception ex)
    {
      ExceptionHandler.SetLastException(ex);

      // Set all slots to IntPtr.Zero on failure
      for (int i = 0; i < 9; i++)
      {
        slotsArray[i] = IntPtr.Zero;
      }
    }
  }

  [UnmanagedCallersOnly(EntryPoint = "CreateTXSTPatch", CallConvs = [typeof(CallConvCdecl)])]
  public static unsafe void CreateTXSTPatch(
    [DNNE.C99Type("const int")] int txstIndex,
    [DNNE.C99Type("const wchar_t**")] IntPtr* slots)
  {
    try
    {
      if (OutMod is null)
      {
        throw new Exception("Initialize must be called before CreateTXSTPatch");
      }

      // print txstIndex
      MessageHandler.Log("[CreateTXSTPatch] [TXST Index: " + txstIndex + "]", 0);

      var origTXSTObj = TXSTObjs[txstIndex];

      // Create a new TXST record
      var newTXSTObj = OutMod.TextureSets.GetOrAddAsOverride(origTXSTObj);

      // TODO maybe there's a way to not have to have a value for every empty value? (check old slots?)
      // Define slot actions for assigning texture set slots
      string? NewDiffuse = Marshal.PtrToStringUni(slots[0]);
      if (NewDiffuse is not null)
      {
        var Diffuse = RemovePrefixIfExists("textures\\", NewDiffuse);
        MessageHandler.Log("[CreateTXSTPatch] [TXST Index: " + txstIndex + "] Diffuse: " + Diffuse, 0);
        newTXSTObj.Diffuse = Diffuse;
      }
      string? NewNormalOrGloss = Marshal.PtrToStringUni(slots[1]);
      if (NewNormalOrGloss is not null)
      {
        var NormalOrGloss = RemovePrefixIfExists("textures\\", NewNormalOrGloss);
        MessageHandler.Log("[CreateTXSTPatch] [TXST Index: " + txstIndex + "] NormalOrGloss: " + NormalOrGloss, 0);
        newTXSTObj.NormalOrGloss = NormalOrGloss;
      }
      string? NewGlowOrDetailMap = Marshal.PtrToStringUni(slots[2]);
      if (NewGlowOrDetailMap is not null)
      {
        var GlowOrDetailMap = RemovePrefixIfExists("textures\\", NewGlowOrDetailMap);
        MessageHandler.Log("[CreateTXSTPatch] [TXST Index: " + txstIndex + "] GlowOrDetailMap: " + GlowOrDetailMap, 0);
        newTXSTObj.GlowOrDetailMap = GlowOrDetailMap;
      }
      string? NewHeight = Marshal.PtrToStringUni(slots[3]);
      if (NewHeight is not null)
      {
        var Height = RemovePrefixIfExists("textures\\", NewHeight);
        MessageHandler.Log("[CreateTXSTPatch] [TXST Index: " + txstIndex + "] Height: " + Height, 0);
        newTXSTObj.Height = Height;
      }
      string? NewEnvironment = Marshal.PtrToStringUni(slots[4]);
      if (NewEnvironment is not null)
      {
        var Environment = RemovePrefixIfExists("textures\\", NewEnvironment);
        MessageHandler.Log("[CreateTXSTPatch] [TXST Index: " + txstIndex + "] Environment: " + Environment, 0);
        newTXSTObj.Environment = Environment;
      }
      string? NewEnvironmentMaskOrSubsurfaceTint = Marshal.PtrToStringUni(slots[5]);
      if (NewEnvironmentMaskOrSubsurfaceTint is not null)
      {
        var EnvironmentMaskOrSubsurfaceTint = RemovePrefixIfExists("textures\\", NewEnvironmentMaskOrSubsurfaceTint);
        MessageHandler.Log("[CreateTXSTPatch] [TXST Index: " + txstIndex + "] EnvironmentMaskOrSubsurfaceTint: " + EnvironmentMaskOrSubsurfaceTint, 0);
        newTXSTObj.EnvironmentMaskOrSubsurfaceTint = EnvironmentMaskOrSubsurfaceTint;
      }
      string? NewMultilayer = Marshal.PtrToStringUni(slots[6]);
      if (NewMultilayer is not null)
      {
        var Multilayer = RemovePrefixIfExists("textures\\", NewMultilayer);
        MessageHandler.Log("[CreateTXSTPatch] [TXST Index: " + txstIndex + "] Multilayer: " + Multilayer, 0);
        newTXSTObj.Multilayer = Multilayer;
      }
      string? NewBacklightMaskOrSpecular = Marshal.PtrToStringUni(slots[7]);
      if (NewBacklightMaskOrSpecular is not null)
      {
        var BacklightMaskOrSpecular = RemovePrefixIfExists("textures\\", NewBacklightMaskOrSpecular);
        MessageHandler.Log("[CreateTXSTPatch] [TXST Index: " + txstIndex + "] BacklightMaskOrSpecular: " + BacklightMaskOrSpecular, 0);
        newTXSTObj.BacklightMaskOrSpecular = BacklightMaskOrSpecular;
      }
    }
    catch (Exception ex)
    {
      ExceptionHandler.SetLastException(ex);
    }
  }

  [UnmanagedCallersOnly(EntryPoint = "CreateNewTXSTPatch", CallConvs = [typeof(CallConvCdecl)])]
  public static unsafe void CreateNewTXSTPatch([DNNE.C99Type("const int")] int AltTexHandle, [DNNE.C99Type("const wchar_t**")] IntPtr* slots, [DNNE.C99Type("int*")] int* ResultTXSTId)
  {
    try
    {
      if (OutMod is null)
      {
        throw new Exception("Initialize must be called before CreateNewTXSTPatch");
      }

      // Create a new TXST record
      var newTXSTObj = OutMod.TextureSets.AddNew();
      MessageHandler.Log("[CreateNewTXSTPatch] [Alt Tex Index: " + AltTexHandle + "]", 0);

      // Define slot actions for assigning texture set slots
      string? NewDiffuse = Marshal.PtrToStringUni(slots[0]);
      if (!NewDiffuse.IsNullOrEmpty())
      {
        var Diffuse = RemovePrefixIfExists("textures\\", NewDiffuse);
        MessageHandler.Log("[CreateNewTXSTPatch] [Alt Tex Index: " + AltTexHandle + "] Diffuse: " + Diffuse, 0);
        newTXSTObj.Diffuse = Diffuse;
      }
      string? NewNormalOrGloss = Marshal.PtrToStringUni(slots[1]);
      if (!NewNormalOrGloss.IsNullOrEmpty())
      {
        var NormalOrGloss = RemovePrefixIfExists("textures\\", NewNormalOrGloss);
        MessageHandler.Log("[CreateNewTXSTPatch] [Alt Tex Index: " + AltTexHandle + "] NormalOrGloss: " + NormalOrGloss, 0);
        newTXSTObj.NormalOrGloss = NormalOrGloss;
      }
      string? NewGlowOrDetailMap = Marshal.PtrToStringUni(slots[2]);
      if (!NewGlowOrDetailMap.IsNullOrEmpty())
      {
        var GlowOrDetailMap = RemovePrefixIfExists("textures\\", NewGlowOrDetailMap);
        MessageHandler.Log("[CreateNewTXSTPatch] [Alt Tex Index: " + AltTexHandle + "] GlowOrDetailMap: " + GlowOrDetailMap, 0);
        newTXSTObj.GlowOrDetailMap = GlowOrDetailMap;
      }
      string? NewHeight = Marshal.PtrToStringUni(slots[3]);
      if (!NewHeight.IsNullOrEmpty())
      {
        var Height = RemovePrefixIfExists("textures\\", NewHeight);
        MessageHandler.Log("[CreateNewTXSTPatch] [Alt Tex Index: " + AltTexHandle + "] Height: " + Height, 0);
        newTXSTObj.Height = Height;
      }
      string? NewEnvironment = Marshal.PtrToStringUni(slots[4]);
      if (!NewEnvironment.IsNullOrEmpty())
      {
        var Environment = RemovePrefixIfExists("textures\\", NewEnvironment);
        MessageHandler.Log("[CreateNewTXSTPatch] [Alt Tex Index: " + AltTexHandle + "] Environment: " + Environment, 0);
        newTXSTObj.Environment = Environment;
      }
      string? NewEnvironmentMaskOrSubsurfaceTint = Marshal.PtrToStringUni(slots[5]);
      if (!NewEnvironmentMaskOrSubsurfaceTint.IsNullOrEmpty())
      {
        var EnvironmentMaskOrSubsurfaceTint = RemovePrefixIfExists("textures\\", NewEnvironmentMaskOrSubsurfaceTint);
        MessageHandler.Log("[CreateNewTXSTPatch] [Alt Tex Index: " + AltTexHandle + "] EnvironmentMaskOrSubsurfaceTint: " + EnvironmentMaskOrSubsurfaceTint, 0);
        newTXSTObj.EnvironmentMaskOrSubsurfaceTint = EnvironmentMaskOrSubsurfaceTint;
      }
      string? NewMultilayer = Marshal.PtrToStringUni(slots[6]);
      if (!NewMultilayer.IsNullOrEmpty())
      {
        var Multilayer = RemovePrefixIfExists("textures\\", NewMultilayer);
        MessageHandler.Log("[CreateNewTXSTPatch] [Alt Tex Index: " + AltTexHandle + "] Multilayer: " + Multilayer, 0);
        newTXSTObj.Multilayer = Multilayer;
      }
      string? NewBacklightMaskOrSpecular = Marshal.PtrToStringUni(slots[7]);
      if (!NewBacklightMaskOrSpecular.IsNullOrEmpty())
      {
        var BacklightMaskOrSpecular = RemovePrefixIfExists("textures\\", NewBacklightMaskOrSpecular);
        MessageHandler.Log("[CreateNewTXSTPatch] [Alt Tex Index: " + AltTexHandle + "] BacklightMaskOrSpecular: " + BacklightMaskOrSpecular, 0);
        newTXSTObj.BacklightMaskOrSpecular = BacklightMaskOrSpecular;
      }

      TXSTObjs.Add(newTXSTObj);
      *ResultTXSTId = TXSTObjs.Count - 1;
      MessageHandler.Log("[CreateNewTXSTPatch] [Alt Tex Index: " + AltTexHandle + "] Created TXST with ID: " + *ResultTXSTId, 0);

      SetModelAltTexManage(AltTexHandle, *ResultTXSTId);
    }
    catch (Exception ex)
    {
      ExceptionHandler.SetLastException(ex);
    }
  }

  private static void SetModelAltTexManage(int AltTexHandle, int TXSTHandle)
  {
    try
    {
      MessageHandler.Log("[SetModelAltTexManage] [Alt Tex Index: " + AltTexHandle + "] [TXST Index: " + TXSTHandle + "]", 0);

      var AltTexObj = GetAltTexFromHandle(AltTexHandle);

      if (AltTexObj.Item1 is null)
      {
        MessageHandler.Log("[SetModelAltTexManage] [Alt Tex Index: " + AltTexHandle + "] Alt Texture Not Found", 4);
        return;
      }

      AltTexObj.Item1.NewTexture.SetTo(TXSTObjs[TXSTHandle]);
      ModifiedModeledRecords.Add(AltTexObj.Item2);
    }
    catch (Exception ex)
    {
      ExceptionHandler.SetLastException(ex);
    }
  }

  [UnmanagedCallersOnly(EntryPoint = "SetModelAltTex", CallConvs = [typeof(CallConvCdecl)])]
  public static void SetModelAltTex([DNNE.C99Type("const int")] int AltTexHandle, [DNNE.C99Type("const int")] int TXSTHandle)
  {
    MessageHandler.Log("[SetModelAltTex] [Alt Tex Index: " + AltTexHandle + "] [TXST Index: " + TXSTHandle + "]", 0);
    SetModelAltTexManage(AltTexHandle, TXSTHandle);
  }

  [UnmanagedCallersOnly(EntryPoint = "Set3DIndex", CallConvs = [typeof(CallConvCdecl)])]
  public static void Set3DIndex([DNNE.C99Type("const int")] int AltTexHandle, [DNNE.C99Type("const int")] int NewIndex)
  {
    try
    {
      MessageHandler.Log("[Set3DIndex] [Alt Tex Index: " + AltTexHandle + "] [New Index: " + NewIndex + "]", 0);

      var AltTexObj = GetAltTexFromHandle(AltTexHandle);

      if (AltTexObj.Item1 is null)
      {
        MessageHandler.Log("[Set3DIndex] [Alt Tex Index: " + AltTexHandle + "] Alt Texture Not Found", 4);
        return;
      }

      AltTexObj.Item1.Index = NewIndex;
      ModifiedModeledRecords.Add(AltTexObj.Item2);
    }
    catch (Exception ex)
    {
      ExceptionHandler.SetLastException(ex);
    }
  }

  [UnmanagedCallersOnly(EntryPoint = "GetTXSTFormID", CallConvs = [typeof(CallConvCdecl)])]
  public static unsafe void GetTXSTFormID([DNNE.C99Type("const int")] int TXSTHandle, [DNNE.C99Type("unsigned int*")] uint* FormID, [DNNE.C99Type("wchar_t**")] IntPtr* PluginName)
  {
    try
    {
      MessageHandler.Log("[GetTXSTFormID] [TXST Index: " + TXSTHandle + "]", 0);

      var txstObj = TXSTObjs[TXSTHandle];
      var PluginNameStr = txstObj.FormKey.ModKey.FileName;
      *PluginName = Marshal.StringToHGlobalUni(PluginNameStr);
      var FormIDStr = txstObj.FormKey.ID;
      *FormID = FormIDStr;

      MessageHandler.Log("[GetTXSTFormID] [TXST Index: " + TXSTHandle + "] Plugin Name: " + PluginNameStr + " FormID: " + FormIDStr.ToString("X6"), 0);
    }
    catch (Exception ex)
    {
      ExceptionHandler.SetLastException(ex);
    }
  }

  // Helpers

  private static (AlternateTexture?, int) GetAltTexFromHandle(int AltTexHandle)
  {
    var oldAltTex = AltTexRefs[AltTexHandle].Item1;
    var ModeledRecordId = AltTexRefs[AltTexHandle].Item2;

    var ModeledRecord = ModelCopies[ModeledRecordId];

    // loop through alternate textures to find the one to replace
    var modelElems = GetModelElems(ModeledRecord);
    for (int i = 0; i < modelElems.Count; i++)
    {
      var modelRec = modelElems[i];
      if (modelRec.AlternateTextures is null)
      {
        continue;
      }

      for (int j = 0; j < modelRec.AlternateTextures.Count; j++)
      {
        var alternateTexture = modelRec.AlternateTextures[j];
        if (alternateTexture.Name == oldAltTex.Name &&
            alternateTexture.Index == oldAltTex.Index &&
            alternateTexture.NewTexture.FormKey.ID.ToString() == oldAltTex.NewTexture.FormKey.ID.ToString())
        {
          // Found the one to update
          var EditableObj = GetModelElems(ModelCopiesEditable[ModeledRecordId])[i].AlternateTextures?[j];
          return (EditableObj, ModeledRecordId);
        }
      }
    }

    return (null, -1);
  }

  private static string GetRecordDesc(IMajorRecordGetter rec)
  {
    return rec.FormKey.ModKey.FileName + " / " + rec.FormKey.ID.ToString("X6");
  }

  private static string RemovePrefixIfExists(string prefix, string str)
  {
    if (str.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
    {
      return str[prefix.Length..];
    }
    return str;
  }

  private static string AddPrefixIfNotExists(string prefix, string str)
  {
    if (!str.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
    {
      return prefix + str;
    }
    return str;
  }
}
