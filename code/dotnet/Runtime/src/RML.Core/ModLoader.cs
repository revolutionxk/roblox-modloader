using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.Loader;

using RML.Core.Api;
using RML.Core.Internal;
using RML.Core.Modding;

using Roblox;

namespace RML.Core;

internal static class ModLoader
{
    private static readonly ModRegistry _registry = new();
    private static string _modsRoot = string.Empty;

    internal static void Initialize(string modsRoot) => _modsRoot = modsRoot;

    internal static void LoadMod(string path)
    {
        if (_registry.Contains(path))
        {
            RuntimeLog.Warn($"Mod at path {path} is already loaded.");
            return;
        }

        try
        {
            var sharedAssemblies = AssemblyLoadContext.Default.Assemblies
                .Concat(GetCurrentAlcAssemblies())
                .ToList();

            var context = new ModAssemblyContext(path, sharedAssemblies);

            Assembly assembly;
            try
            {
                assembly = context.LoadFromAssemblyPath(path);
            }
            catch (BadImageFormatException)
            {
                // A mod folder also carries its native dependencies; they are not assemblies and
                // the loader has no way of telling before trying.
                RuntimeLog.Warn($"Skipping {path}: not a managed assembly.");
                context.Unload();
                return;
            }

            var modType = GetLoadableTypes(assembly, path)
                .FirstOrDefault(t =>
                    t.GetCustomAttribute<ModAttribute>() is not null && t.IsAssignableTo(typeof(IMod)) &&
                    !t.IsAbstract);

            if (modType is null)
            {
                RuntimeLog.Warn($"No [Mod] class implementing IMod found in assembly {assembly.FullName}.");
                context.Unload();
                return;
            }

            var mod = (IMod)Activator.CreateInstance(modType)!;

            var attr = modType.GetCustomAttribute<ModAttribute>()!;
            var loadIn = attr.LoadInDataModels;

            var modContext = new ModContext(
                new ModDescriptor(attr.Id, attr.Version, attr.Author, attr.Description), path, assembly);

            ModContext.Register(modContext);
            if (mod is ModBase modBase)
            {
                modBase.Context = modContext;
            }

            var info = new ModInfo(context, mod, loadIn, assembly);

            if (!_registry.TryAdd(path, info))
            {
                RuntimeLog.Warn($"Mod at path {path} is already loaded.");
                ModContext.Unregister(assembly);
                context.Unload();
                return;
            }

            if (loadIn == null || loadIn.Length == 0)
            {
                try
                {
                    mod.OnLoad();
                    _registry.SetInitialized(info, true);
                }
                catch (Exception e)
                {
                    RuntimeLog.Error($"Error while calling OnLoad for mod at {path}: {e}");
                }
            }
        }
        catch (Exception e)
        {
            RuntimeLog.Error($"Failed to load mod at {path}: {e}");
            throw;
        }
    }

    private static IEnumerable<Type> GetLoadableTypes(Assembly assembly, string path)
    {
        try
        {
            return assembly.GetTypes();
        }
        catch (ReflectionTypeLoadException e)
        {
            RuntimeLog.Warn($"Some types in {path} could not be loaded: {e.Message}");
            return e.Types.Where(t => t is not null).Select(t => t!);
        }
    }

    internal static void UnloadMod(string path)
    {
        if (!_registry.TryRemove(path, out var modInfo) || modInfo is null)
        {
            RuntimeLog.Warn($"No mod loaded from path {path}.");
            return;
        }

        var weakContext = UnloadModCore(path, modInfo);

        for (var i = 0; weakContext.IsAlive && i < 10; i++)
        {
            GC.Collect();
            GC.WaitForPendingFinalizers();
        }

        if (weakContext.IsAlive)
        {
            RuntimeLog.Warn(
                $"The AssemblyLoadContext for '{path}' did not unload — it is still rooted " +
                "(a leaked event handler, GCHandle, static reference, or running thread).");
        }
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static WeakReference UnloadModCore(string path, ModInfo modInfo)
    {
        var wasInitialized = _registry.GetInitialized(modInfo);

        try
        {
            if (wasInitialized)
            {
                modInfo.Instance.OnUnload();
            }
        }
        catch (Exception e)
        {
            RuntimeLog.Error($"Error while unloading mod from {path}: {e}");
        }

        ModContext.Unregister(modInfo.ModAssembly);

        var weak = new WeakReference(modInfo.Context);
        modInfo.Context.Unload();
        return weak;
    }

    internal static void Shutdown()
    {
        var paths = _registry.Keys();

        foreach (var path in paths)
        {
            UnloadMod(path);
        }
    }

    internal static void OnDataModelChanged(ulong oldDataModelPtr, ulong newDataModelPtr, DataModelType dataModelType)
    {
        var oldModel = DataModel.FromHandle((nuint)oldDataModelPtr);
        var newModel = DataModel.FromHandle((nuint)newDataModelPtr);

        var snapshot = _registry.Snapshot();

        foreach (var modInfo in snapshot)
        {
            var interested = modInfo.LoadInDataModels;
            if (interested is { Length: > 0 } && !interested.Contains(dataModelType))
                continue;

            if (newModel != null)
            {
                var wasInitialized = _registry.GetInitialized(modInfo);

                if (!wasInitialized)
                {
                    try
                    {
                        modInfo.Instance.OnLoad();
                    }
                    catch (Exception e)
                    {
                        RuntimeLog.Error($"Error while calling OnLoad for mod: {e}");
                    }

                    _registry.SetInitialized(modInfo, true);
                }

                if (modInfo.Instance is IDataModelAware aware)
                {
                    try
                    {
                        aware.OnDataModelLoaded(newModel, dataModelType);
                    }
                    catch (Exception e)
                    {
                        RuntimeLog.Error($"Error in IDataModelAware.OnDataModelLoaded: {e}");
                    }
                }
            }
            else
            {
                if (oldModel != null && modInfo.Instance is IDataModelAware aware)
                {
                    try
                    {
                        aware.OnDataModelUnloaded(oldModel, dataModelType);
                    }
                    catch (Exception e)
                    {
                        RuntimeLog.Error($"Error in IDataModelAware.OnDataModelUnloaded: {e}");
                    }
                }

                var wasInitialized = _registry.GetInitialized(modInfo);

                if (wasInitialized)
                {
                    try
                    {
                        modInfo.Instance.OnUnload();
                    }
                    catch (Exception e)
                    {
                        RuntimeLog.Error($"Error while calling OnUnload for mod: {e}");
                    }

                    _registry.SetInitialized(modInfo, false);
                }
            }
        }
    }

    private static IEnumerable<Assembly> GetCurrentAlcAssemblies()
    {
        var currentAlc = AssemblyLoadContext.GetLoadContext(
            typeof(ModLoader).Assembly);
        return currentAlc?.Assemblies ?? [];
    }
}