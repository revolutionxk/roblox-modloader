# Writing .NET mods

A .NET mod is an ordinary class library that the loader discovers, loads into its own isolated
context, and drives through a small lifecycle. You write against a strongly-typed view of the
engine in the `Roblox` namespace.

A complete, runnable reference is [`examples/example_dotnet`](../examples/example_dotnet); a real
integration is [`examples/discord_rpc_dotnet`](../examples/discord_rpc_dotnet).

## Project setup

Target .NET 10 and reference `RML.Core`. That single reference also brings in the typed `Roblox`
API.

```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net10.0</TargetFramework>
    <Nullable>enable</Nullable>
    <ImplicitUsings>enable</ImplicitUsings>
  </PropertyGroup>
  <ItemGroup>
    <ProjectReference Include="path/to/code/dotnet/Runtime/src/RML.Core/RML.Core.csproj" />
  </ItemGroup>
</Project>
```

Build the project, then copy the resulting assembly into a mod folder under the loader:
`RobloxModLoader/mods/<your-mod>/dotnet/`.

## The mod class

Annotate a class with `[Mod]` and implement the lifecycle. You can implement `IMod` directly or
extend the `Mod` base class, which adds a ready-to-use `Logger`.

```csharp
using RML.Core.Api;
using RML.Core.Modding;

[Mod("my-mod", "1.0.0", Author = "You", Description = "What it does")]
public sealed class MyMod : Mod
{
    public override int OnLoad()
    {
        Logger.Info("Loaded.");
        return 0; // non-zero signals a load failure
    }

    public override void OnUnload()
    {
        Logger.Info("Unloaded.");
    }
}
```

- `OnLoad` runs when the mod is loaded; return `0` for success.
- `OnUnload` runs when the mod is unloaded. Release anything you started here — the mod's context is
  collectible, so leaks keep it alive.

## Reacting to a place

Most mods want to act when a place (a `DataModel`) becomes available. Implement `IDataModelAware`:

```csharp
using RML.Core.Api;
using RML.Core.Modding;
using Roblox;

[Mod("place-watcher", "1.0.0", Author = "You")]
public sealed class PlaceWatcher : Mod, IDataModelAware
{
    public override int OnLoad() => 0;
    public override void OnUnload() { }

    public void OnDataModelLoaded(DataModel game, DataModelType type)
    {
        Logger.Info($"Place loaded as {type}.");

        var workspace = game.GetService("Workspace")?.As<Workspace>();
        Logger.Info($"Camera: {workspace?.CurrentCamera?.CFrame}");
    }

    public void OnDataModelUnloaded(DataModel game, DataModelType type) { }
}
```

`DataModelType` distinguishes the context (for example, an edit session versus play-testing), so you
can enable behavior only where it makes sense.

## Working with the engine

The `Roblox` namespace mirrors the engine's object model with ordinary C# types.

- **Services and lookups:** `game.GetService("Workspace")`, `instance.FindFirstChild("Name")`,
  `instance.GetChildren()`.
- **Typed casts:** `instance.As<Part>()` returns the typed wrapper (or `null`).
- **Properties:** read and write them directly — `part.Name`, `part.Position`, `part.CFrame`,
  `part.BrickColor`.
- **Value types:** `Vector3`, `Vector2`, `CFrame`, `Color3`, `UDim`, `UDim2`, `Rect`, `Region3`,
  `Ray`, `NumberRange`, `Faces`, `Axes`, `BrickColor`, `NumberSequence`, `ColorSequence` are all
  real structs you can construct and pass around.
- **Events:** subscribe with `+=` and unsubscribe with `-=`.

```csharp
var workspace = game.GetService("Workspace")?.As<Workspace>();

workspace!.DescendantAdded += instance =>
    Logger.Info($"Added {instance.Name} ({instance.ClassName})");

var part = workspace.FindFirstChild("Baseplate")?.As<Part>();
if (part is not null)
{
    part.BrickColor = BrickColor.FromName("Bright red");
    part.CFrame = new CFrame(new Vector3(0, 10, 0));
}
```

## Driving the engine from Luau

Some engine surfaces are best reached from Luau rather than through the reflection bridge:
Studio-only APIs that check the calling thread's capabilities, anything that yields, and anything
that would otherwise cost one interop call per instance. `LuauScriptManager` evaluates a chunk in a
place's own Luau VM and hands back whatever it returns, so a mod can keep a table of functions on
the engine side and call them by reference.

```csharp
var result = await LuauScriptManager.EvaluateAsync(DataModelType.Edit, source, "@mymod/agent");
var agent = result.AsRef()!;               // the table the chunk returned
var poll = (await agent.IndexAsync("poll")).AsRef()!;

var payload = (await poll.InvokeAsync()).AsString();
```

- **Values cross as values; anything else crosses as a handle.** `nil`, booleans, numbers and
  strings marshal directly; a table or function comes back as a `LuauRef` you can index and call.
  JSON in a string is the simplest way to move a structure.
- **A yielding call answers with nothing.** The moment the Luau thread yields, the call you awaited
  completes with `nil` and the engine owns the resume. Keep the functions you call from .NET
  non-yielding: let them queue work, and report what finished on a later call (the
  `script_editor_webview` example queues document edits and drains the results from a `poll`).
- **Capabilities follow the thread.** Chunks and callbacks run with full capabilities and Studio
  identity, which is what lets a mod call plugin-security APIs such as
  `ScriptDocument:EditTextAsync`.
- **The chunk's environment survives.** Every chunk a mod evaluates shares one sandboxed
  environment per mod, so state a chunk leaves behind is still there for the next one.

`examples/script_editor_webview` is the worked example: `luau/agent.luau` owns every engine touch
and the .NET half only moves JSON in and out of it.

## Notes

- **Threading.** You can do background work (for example with `Task.Run`), but engine objects are
  not free-threaded — keep engine access on the appropriate thread and avoid blocking it.
- **Unsubscribe and dispose on unload.** Anything you connect or start in `OnLoad` /
  `OnDataModelLoaded` should be undone in `OnUnload`, so the mod can be cleanly unloaded.
