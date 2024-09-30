namespace ParallaxGenMutagenWrapper;

// TODO debug CLI
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
using FluentResults;
using Noggog;

public static class ExceptionHandler
{
  private static string? LastExceptionMessage;

  public static void SetLastException(Exception? ex)
  {
    // Find the deepest inner exception
    while (ex is not null)
    {
      LastExceptionMessage += "\n" + ex.Message;
      ex = ex.InnerException;
    }
  }

  [UnmanagedCallersOnly(EntryPoint = "GetLastExceptionLength", CallConvs = [typeof(CallConvCdecl)])]
  public static unsafe void GetLastExceptionLength([DNNE.C99Type("int*")] int* length)
  {
    *length = LastExceptionMessage?.Length ?? 0;
  }

  [UnmanagedCallersOnly(EntryPoint = "GetLastException", CallConvs = [typeof(CallConvCdecl)])]
  public static void GetLastException([DNNE.C99Type("wchar_t*")] IntPtr errorMessagePtr)
  {
    string? errorMessage = LastExceptionMessage ?? string.Empty;

    if (errorMessagePtr == IntPtr.Zero)
    {
      return;
    }

    // Copy the error message to the provided memory
    Marshal.Copy(errorMessage.ToCharArray(), 0, errorMessagePtr, errorMessage.Length);
    Marshal.WriteInt16(errorMessagePtr + errorMessage.Length * sizeof(char), '\0');  // Null-terminate

    LastExceptionMessage = null;
  }
}

public class PGMutagen
{
  // "Class vars" actually static because p/invoke doesn't support instance methods
  private static SkyrimMod? OutMod;
  private static IGameEnvironment<ISkyrimMod, ISkyrimModGetter>? Env;
  private static List<ITextureSetGetter> TXSTObjs = [];
  private static List<IAlternateTextureGetter> AltTexRefs = [];
  private static List<IMajorRecord> ModelCopies = [];
  private static Dictionary<Tuple<string, string, int>, List<Tuple<int, int>>>? TXSTRefs;
  private static Dictionary<int, int> AltTexParents = [];
  private static HashSet<int> ModifiedModeledRecords = [];

  private static IEnumerable<IMajorRecordGetter> EnumerateModelRecords()
  {
    if (Env is null)
    {
      return [];
    }

    return Env.LoadOrder.PriorityOrder.Activator().WinningOverrides()
            .Concat<IMajorRecordGetter>(Env.LoadOrder.PriorityOrder.AddonNode().WinningOverrides())
            .Concat(Env.LoadOrder.PriorityOrder.Ammunition().WinningOverrides())
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
  public static void Initialize(
    [DNNE.C99Type("const int")] int gameType,
    [DNNE.C99Type("const wchar_t*")] IntPtr dataPathPtr,
    [DNNE.C99Type("const wchar_t*")] IntPtr outputPluginPtr)
  {
    try
    {
      string dataPath = Marshal.PtrToStringUni(dataPathPtr) ?? string.Empty;
      string outputPlugin = Marshal.PtrToStringUni(outputPluginPtr) ?? string.Empty;

      Env = GameEnvironment.Typical.Builder<ISkyrimMod, ISkyrimModGetter>((GameRelease)gameType)
          .WithTargetDataFolder(dataPath)
          .Build();

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

  [UnmanagedCallersOnly(EntryPoint = "PopulateObjs", CallConvs = [typeof(CallConvCdecl)])]
  public static void PopulateObjs()
  {
    try
    {
      if (Env is null)
      {
        throw new Exception("Initialize must be called before PopulateObjs");
      }

      TXSTObjs = [.. Env.LoadOrder.PriorityOrder.TextureSet().WinningOverrides()];

      TXSTRefs = [];
      foreach (var txstRefObj in EnumerateModelRecords())
      {
        // Will store models to check later
        List<IModelGetter> ModelRecs = GetModelElems(txstRefObj);

        if (ModelRecs.Count == 0)
        {
          // Skip if there is nothing to search
          continue;
        }

        // Deep copy master record
        ModelCopies.Add(txstRefObj.DeepCopy());
        var DCIdx = ModelCopies.Count - 1;

        foreach (var modelRec in ModelRecs)
        {
          // find lowercase nifname
          string nifName = modelRec.File;
          nifName = nifName.ToLower();
          nifName = addPrefixIfNotExists("meshes\\", nifName);

          if (modelRec.AlternateTextures is null)
          {
            // no alternate textures
            continue;
          }

          foreach (var alternateTexture in modelRec.AlternateTextures)
          {
            // Add to global
            AltTexRefs.Add(alternateTexture);
            var AltTexId = AltTexRefs.Count - 1;

            string name3D = alternateTexture.Name;
            int index3D = alternateTexture.Index;
            var newTXST = alternateTexture.NewTexture;

            var key = new Tuple<string, string, int>(nifName, name3D, index3D);

            int newTXSTIndex = TXSTObjs.FindIndex(x => x.FormKey == newTXST.FormKey);

            if (newTXSTIndex >= 0)
            {
              if (!TXSTRefs.ContainsKey(key))
              {
                TXSTRefs[key] = [];
              }
              TXSTRefs[key].Add(new Tuple<int, int>(newTXSTIndex, AltTexId));
              AltTexParents[AltTexId] = DCIdx;
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
        var ModifiedRecord = ModelCopies[recId];
        if (ModifiedRecord is Ammunition)
        {
          OutMod.Ammunitions.Add((Ammunition)ModifiedRecord);
        }
        else if (ModifiedRecord is AnimatedObject)
        {
          OutMod.AnimatedObjects.Add((AnimatedObject)ModifiedRecord);
        }
        else if (ModifiedRecord is Armor)
        {
          OutMod.Armors.Add((Armor)ModifiedRecord);
        }
        else if (ModifiedRecord is ArmorAddon)
        {
          OutMod.ArmorAddons.Add((ArmorAddon)ModifiedRecord);
        }
        else if (ModifiedRecord is ArtObject)
        {
          OutMod.ArtObjects.Add((ArtObject)ModifiedRecord);
        }
        else if (ModifiedRecord is BodyPartData)
        {
          OutMod.BodyParts.Add((BodyPartData)ModifiedRecord);
        }
        else if (ModifiedRecord is Book)
        {
          OutMod.Books.Add((Book)ModifiedRecord);
        }
        else if (ModifiedRecord is CameraShot)
        {
          OutMod.CameraShots.Add((CameraShot)ModifiedRecord);
        }
        else if (ModifiedRecord is Climate)
        {
          OutMod.Climates.Add((Climate)ModifiedRecord);
        }
        else if (ModifiedRecord is Container)
        {
          OutMod.Containers.Add((Container)ModifiedRecord);
        }
        else if (ModifiedRecord is Door)
        {
          OutMod.Doors.Add((Door)ModifiedRecord);
        }
        else if (ModifiedRecord is Explosion)
        {
          OutMod.Explosions.Add((Explosion)ModifiedRecord);
        }
        else if (ModifiedRecord is Flora)
        {
          OutMod.Florae.Add((Flora)ModifiedRecord);
        }
        else if (ModifiedRecord is Furniture)
        {
          OutMod.Furniture.Add((Furniture)ModifiedRecord);
        }
        else if (ModifiedRecord is Grass)
        {
          OutMod.Grasses.Add((Grass)ModifiedRecord);
        }
        else if (ModifiedRecord is Hazard)
        {
          OutMod.Hazards.Add((Hazard)ModifiedRecord);
        }
        else if (ModifiedRecord is HeadPart)
        {
          OutMod.HeadParts.Add((HeadPart)ModifiedRecord);
        }
        else if (ModifiedRecord is IdleMarker)
        {
          OutMod.IdleMarkers.Add((IdleMarker)ModifiedRecord);
        }
        else if (ModifiedRecord is Impact)
        {
          OutMod.Impacts.Add((Impact)ModifiedRecord);
        }
        else if (ModifiedRecord is Ingestible)
        {
          OutMod.Ingestibles.Add((Ingestible)ModifiedRecord);
        }
        else if (ModifiedRecord is Ingredient)
        {
          OutMod.Ingredients.Add((Ingredient)ModifiedRecord);
        }
        else if (ModifiedRecord is Key)
        {
          OutMod.Keys.Add((Key)ModifiedRecord);
        }
        else if (ModifiedRecord is LeveledNpc)
        {
          OutMod.LeveledNpcs.Add((LeveledNpc)ModifiedRecord);
        }
        else if (ModifiedRecord is Light)
        {
          OutMod.Lights.Add((Light)ModifiedRecord);
        }
        else if (ModifiedRecord is MaterialObject)
        {
          OutMod.MaterialObjects.Add((MaterialObject)ModifiedRecord);
        }
        else if (ModifiedRecord is MiscItem)
        {
          OutMod.MiscItems.Add((MiscItem)ModifiedRecord);
        }
        else if (ModifiedRecord is MoveableStatic)
        {
          OutMod.MoveableStatics.Add((MoveableStatic)ModifiedRecord);
        }
        else if (ModifiedRecord is Projectile)
        {
          OutMod.Projectiles.Add((Projectile)ModifiedRecord);
        }
        else if (ModifiedRecord is Scroll)
        {
          OutMod.Scrolls.Add((Scroll)ModifiedRecord);
        }
        else if (ModifiedRecord is SoulGem)
        {
          OutMod.SoulGems.Add((SoulGem)ModifiedRecord);
        }
        else if (ModifiedRecord is Static)
        {
          OutMod.Statics.Add((Static)ModifiedRecord);
        }
        else if (ModifiedRecord is TalkingActivator)
        {
          OutMod.TalkingActivators.Add((TalkingActivator)ModifiedRecord);
        }
        else if (ModifiedRecord is Tree)
        {
          OutMod.Trees.Add((Tree)ModifiedRecord);
        }
        else if (ModifiedRecord is Weapon)
        {
          OutMod.Weapons.Add((Weapon)ModifiedRecord);
        }
      }

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

  [UnmanagedCallersOnly(EntryPoint = "GetMatchingTXSTObjsLength", CallConvs = new[] { typeof(CallConvCdecl) })]
  public static unsafe void GetMatchingTXSTObjsLength(
    [DNNE.C99Type("const wchar_t*")] IntPtr nifNamePtr,
    [DNNE.C99Type("const wchar_t*")] IntPtr name3DPtr,
    [DNNE.C99Type("const int")] int index3D,
    [DNNE.C99Type("int*")] int* length)
  {
    try
    {
      *length = 0;
      if (TXSTRefs is null)
      {
        throw new Exception("PopulateObjs must be called before GetMatchingTXSTObjsLength");
      }

      string nifName = Marshal.PtrToStringUni(nifNamePtr) ?? string.Empty;
      string name3D = Marshal.PtrToStringUni(name3DPtr) ?? string.Empty;
      var key = new Tuple<string, string, int>(nifName, name3D, index3D);

      if (TXSTRefs.ContainsKey(key))
      {
        *length = TXSTRefs[key].Count;
      }
    }
    catch (Exception ex)
    {
      ExceptionHandler.SetLastException(ex);
      *length = 0;
    }
  }

  [UnmanagedCallersOnly(EntryPoint = "GetMatchingTXSTObjs", CallConvs = [typeof(CallConvCdecl)])]
  public static unsafe void GetMatchingTXSTObjs(
    [DNNE.C99Type("const wchar_t*")] IntPtr nifNamePtr,
    [DNNE.C99Type("const wchar_t*")] IntPtr name3DPtr,
    [DNNE.C99Type("const int")] int index3D,
    [DNNE.C99Type("int*")] int* TXSTHandles,
    [DNNE.C99Type("int*")] int* AltTexHandles)
  {
    try
    {
      if (TXSTRefs is null)
      {
        throw new Exception("PopulateObjs must be called before GetMatchingTXSTObjs");
      }

      string nifName = Marshal.PtrToStringUni(nifNamePtr) ?? string.Empty;
      string name3D = Marshal.PtrToStringUni(name3DPtr) ?? string.Empty;
      var key = new Tuple<string, string, int>(nifName, name3D, index3D);

      if (TXSTRefs.ContainsKey(key))
      {
        var txstList = TXSTRefs[key];

        // Manually copy the elements from the list to the pointer
        for (int i = 0; i < txstList.Count; i++)
        {
          TXSTHandles[i] = txstList[i].Item1;
          AltTexHandles[i] = txstList[i].Item2;
        }
      }
    }
    catch (Exception ex)
    {
      ExceptionHandler.SetLastException(ex);
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

      var txstObj = TXSTObjs[txstIndex];

      // Populate the slotsArray with string pointers
      if (!txstObj.Diffuse.IsNullOrEmpty())
      {
        slotsArray[0] = Marshal.StringToHGlobalUni(addPrefixIfNotExists("textures\\", txstObj.Diffuse).ToLower());
      }
      if (!txstObj.NormalOrGloss.IsNullOrEmpty())
      {
        slotsArray[1] = Marshal.StringToHGlobalUni(addPrefixIfNotExists("textures\\", txstObj.NormalOrGloss).ToLower());
      }
      if (!txstObj.GlowOrDetailMap.IsNullOrEmpty())
      {
        slotsArray[2] = Marshal.StringToHGlobalUni(addPrefixIfNotExists("textures\\", txstObj.GlowOrDetailMap).ToLower());
      }
      if (!txstObj.Height.IsNullOrEmpty())
      {
        slotsArray[3] = Marshal.StringToHGlobalUni(addPrefixIfNotExists("textures\\", txstObj.Height).ToLower());
      }
      if (!txstObj.Environment.IsNullOrEmpty())
      {
        slotsArray[4] = Marshal.StringToHGlobalUni(addPrefixIfNotExists("textures\\", txstObj.Environment).ToLower());
      }
      if (!txstObj.EnvironmentMaskOrSubsurfaceTint.IsNullOrEmpty())
      {
        slotsArray[5] = Marshal.StringToHGlobalUni(addPrefixIfNotExists("textures\\", txstObj.EnvironmentMaskOrSubsurfaceTint).ToLower());
      }
      if (!txstObj.Multilayer.IsNullOrEmpty())
      {
        slotsArray[6] = Marshal.StringToHGlobalUni(addPrefixIfNotExists("textures\\", txstObj.Multilayer).ToLower());
      }
      if (!txstObj.BacklightMaskOrSpecular.IsNullOrEmpty())
      {
        slotsArray[7] = Marshal.StringToHGlobalUni(addPrefixIfNotExists("textures\\", txstObj.BacklightMaskOrSpecular).ToLower());
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

      var origTXSTObj = TXSTObjs[txstIndex];
      var newTXSTObj = OutMod.TextureSets.GetOrAddAsOverride(origTXSTObj);

      // Define slot actions for assigning texture set slots
      string? NewDiffuse = Marshal.PtrToStringUni(slots[0]);
      if (!NewDiffuse.IsNullOrEmpty())
      {
        newTXSTObj.Diffuse = removePrefixIfExists("textures\\", NewDiffuse);
      }
      string? NewNormalOrGloss = Marshal.PtrToStringUni(slots[1]);
      if (!NewNormalOrGloss.IsNullOrEmpty())
      {
        newTXSTObj.NormalOrGloss = removePrefixIfExists("textures\\", NewNormalOrGloss);
      }
      string? NewGlowOrDetailMap = Marshal.PtrToStringUni(slots[2]);
      if (!NewGlowOrDetailMap.IsNullOrEmpty())
      {
        newTXSTObj.GlowOrDetailMap = removePrefixIfExists("textures\\", NewGlowOrDetailMap);
      }
      string? NewHeight = Marshal.PtrToStringUni(slots[3]);
      if (!NewHeight.IsNullOrEmpty())
      {
        newTXSTObj.Height = removePrefixIfExists("textures\\", NewHeight);
      }
      string? NewEnvironment = Marshal.PtrToStringUni(slots[4]);
      if (!NewEnvironment.IsNullOrEmpty())
      {
        newTXSTObj.Environment = removePrefixIfExists("textures\\", NewEnvironment);
      }
      string? NewEnvironmentMaskOrSubsurfaceTint = Marshal.PtrToStringUni(slots[5]);
      if (!NewEnvironmentMaskOrSubsurfaceTint.IsNullOrEmpty())
      {
        newTXSTObj.EnvironmentMaskOrSubsurfaceTint = removePrefixIfExists("textures\\", NewEnvironmentMaskOrSubsurfaceTint);
      }
      string? NewMultilayer = Marshal.PtrToStringUni(slots[6]);
      if (!NewMultilayer.IsNullOrEmpty())
      {
        newTXSTObj.Multilayer = removePrefixIfExists("textures\\", NewMultilayer);
      }
      string? NewBacklightMaskOrSpecular = Marshal.PtrToStringUni(slots[7]);
      if (!NewBacklightMaskOrSpecular.IsNullOrEmpty())
      {
        newTXSTObj.BacklightMaskOrSpecular = removePrefixIfExists("textures\\", NewBacklightMaskOrSpecular);
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
      // Define slot actions for assigning texture set slots
      string? NewDiffuse = Marshal.PtrToStringUni(slots[0]);
      if (!NewDiffuse.IsNullOrEmpty())
      {
        newTXSTObj.Diffuse = removePrefixIfExists("textures\\", NewDiffuse);
      }
      string? NewNormalOrGloss = Marshal.PtrToStringUni(slots[1]);
      if (!NewNormalOrGloss.IsNullOrEmpty())
      {
        newTXSTObj.NormalOrGloss = removePrefixIfExists("textures\\", NewNormalOrGloss);
      }
      string? NewGlowOrDetailMap = Marshal.PtrToStringUni(slots[2]);
      if (!NewGlowOrDetailMap.IsNullOrEmpty())
      {
        newTXSTObj.GlowOrDetailMap = removePrefixIfExists("textures\\", NewGlowOrDetailMap);
      }
      string? NewHeight = Marshal.PtrToStringUni(slots[3]);
      if (!NewHeight.IsNullOrEmpty())
      {
        newTXSTObj.Height = removePrefixIfExists("textures\\", NewHeight);
      }
      string? NewEnvironment = Marshal.PtrToStringUni(slots[4]);
      if (!NewEnvironment.IsNullOrEmpty())
      {
        newTXSTObj.Environment = removePrefixIfExists("textures\\", NewEnvironment);
      }
      string? NewEnvironmentMaskOrSubsurfaceTint = Marshal.PtrToStringUni(slots[5]);
      if (!NewEnvironmentMaskOrSubsurfaceTint.IsNullOrEmpty())
      {
        newTXSTObj.EnvironmentMaskOrSubsurfaceTint = removePrefixIfExists("textures\\", NewEnvironmentMaskOrSubsurfaceTint);
      }
      string? NewMultilayer = Marshal.PtrToStringUni(slots[6]);
      if (!NewMultilayer.IsNullOrEmpty())
      {
        newTXSTObj.Multilayer = removePrefixIfExists("textures\\", NewMultilayer);
      }
      string? NewBacklightMaskOrSpecular = Marshal.PtrToStringUni(slots[7]);
      if (!NewBacklightMaskOrSpecular.IsNullOrEmpty())
      {
        newTXSTObj.BacklightMaskOrSpecular = removePrefixIfExists("textures\\", NewBacklightMaskOrSpecular);
      }

      TXSTObjs.Add(newTXSTObj);
      *ResultTXSTId = TXSTObjs.Count - 1;

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
      var newTXSTObj = TXSTObjs[TXSTHandle];

      var oldAltTex = AltTexRefs[AltTexHandle];
      var ModeledRecordId = AltTexParents[AltTexHandle];
      var ModeledRecord = ModelCopies[ModeledRecordId];

      // loop through alternate textures to find the one to replace
      foreach (var modelRec in GetModelElems(ModeledRecord))
      {
        if (modelRec.AlternateTextures is null)
        {
          // This shouldn't happen, but it's here just in case
          continue;
        }

        foreach (var alternateTexture in modelRec.AlternateTextures)
        {
          if (alternateTexture.Name == oldAltTex.Name && alternateTexture.Index == oldAltTex.Index && alternateTexture.NewTexture.FormKey.ID == oldAltTex.NewTexture.FormKey.ID)
          {
            // Found the one to update
            ModifiedModeledRecords.Add(ModeledRecordId);
            alternateTexture.NewTexture.SetTo(newTXSTObj);
            return;
          }
        }
      }
    }
    catch (Exception ex)
    {
      ExceptionHandler.SetLastException(ex);
    }
  }

  [UnmanagedCallersOnly(EntryPoint = "SetModelAltTex", CallConvs = [typeof(CallConvCdecl)])]
  public static void SetModelAltTex([DNNE.C99Type("const int")] int AltTexHandle, [DNNE.C99Type("const int")] int TXSTHandle)
  {
    SetModelAltTexManage(AltTexHandle, TXSTHandle);
  }

  [UnmanagedCallersOnly(EntryPoint = "Set3DIndex", CallConvs = [typeof(CallConvCdecl)])]
  public static void Set3DIndex([DNNE.C99Type("const int")] int AltTexHandle, [DNNE.C99Type("const int")] int NewIndex)
  {
    // TODO we very likely need to redo all the indexes here instead of just matching
    try
    {
      var oldAltTex = AltTexRefs[AltTexHandle];
      var ModeledRecordId = AltTexParents[AltTexHandle];
      var ModeledRecord = ModelCopies[ModeledRecordId];

      // loop through alternate textures to find the one to replace
      foreach (var modelRec in GetModelElems(ModeledRecord))
      {
        if (modelRec.AlternateTextures is null)
        {
          // This shouldn't happen, but it's here just in case
          continue;
        }

        foreach (var alternateTexture in modelRec.AlternateTextures)
        {
          if (alternateTexture.Name == oldAltTex.Name && alternateTexture.Index == oldAltTex.Index && alternateTexture.NewTexture == oldAltTex.NewTexture)
          {
            // Found the one to update
            ModifiedModeledRecords.Add(ModeledRecordId);
            alternateTexture.Index = NewIndex;
            return;
          }
        }
      }
    }
    catch (Exception ex)
    {
      ExceptionHandler.SetLastException(ex);
    }
  }

  [UnmanagedCallersOnly(EntryPoint = "GetTXSTFormID", CallConvs = [typeof(CallConvCdecl)])]
  public static unsafe void GetTXSTFormID([DNNE.C99Type("const int")] int TXSTHandle, [DNNE.C99Type("unsigned int*")] uint* FormID)
  {
    try
    {
      var txstObj = TXSTObjs[TXSTHandle];
      *FormID = txstObj.FormKey.ID;
    }
    catch (Exception ex)
    {
      ExceptionHandler.SetLastException(ex);
    }
  }

  // Helpers

  private static string removePrefixIfExists(string prefix, string str)
  {
    if (str.StartsWith(prefix))
    {
      return str[prefix.Length..];
    }
    return str;
  }

  private static string addPrefixIfNotExists(string prefix, string str)
  {
    if (!str.StartsWith(prefix))
    {
      return prefix + str;
    }
    return str;
  }
}
