<p align="center">
  <p align="center">
	<img width="150" height="150" src="./assets/logo.png" alt="Logo">
  </p>
  <h1 align="center"><b>Roblox ModLoader</b></h1>
  <p align="center">
    A modding framework for Roblox Studio, enabling native, C#, and internal Luau script mods.
  </p>
</p>

<div align="center">

![GitHub Workflow Status](https://img.shields.io/github/actions/workflow/status/revolutionxk/roblox-modloader/nightly.yml?style=for-the-badge&branch=develop&logo=github&label=develop%20build)
![GitHub Workflow Status](https://img.shields.io/github/actions/workflow/status/revolutionxk/roblox-modloader/release.yml?style=for-the-badge&branch=main&logo=github&label=main%20build)
[![GitHub License](https://img.shields.io/github/license/revolutionxk/roblox-modloader?style=for-the-badge)](LICENSE)

</div>

<div align="center">

[![Discord](https://img.shields.io/discord/1405678257545936916?style=for-the-badge&logo=discord&logoColor=white)](https://robloxmodloader.com/)

</div>

> [!NOTE]
> This project is still in development and may contain bugs or incomplete features.

## Cross-Platform Support

- [x] Windows
- [x] Linux Vinegar
- [x] macOS

## Quick Start

### Installation

RML targets your local Roblox **Studio** installation (typically under
`%LOCALAPPDATA%\Roblox\Versions\<version>\`).

### Using the launcher (Not finished yet)

The launcher handles placement and keeps the loader up to date.

1. Download the latest [rml-launcher release](https://github.com/revolutionxk/rml-launcher/releases).
2. Run it and follow the prompts.
3. Start Roblox Studio through the launcher — the loader and your mods are applied automatically.

### Manual installation

1. Download the latest [RML release](https://github.com/revolutionxk/roblox-modloader/releases).
2. Open your Studio directory — the folder that contains `RobloxStudioBeta.exe`, usually
   `%LOCALAPPDATA%\Roblox\Versions\<version>\`.
3. Extract the archive into that folder.
4. Start Roblox Studio.

The release archive is already laid out so a plain extract lands everything in the right place:

```
<Studio directory>/
├── RobloxStudioBeta.exe
├── dwmapi.dll                    proxy, loaded by Studio
└── RobloxModLoader/
    ├── roblox_modloader.dll      native core
    ├── config.toml               created with defaults if missing
    ├── runtime/                  the .NET host and bundled runtime
    └── mods/                     your mods, one folder each
        └── your-mod/
            ├── native/           native C++ mod DLLs
            ├── dotnet/           .NET mod assemblies
            └── scripts/          Luau scripts
```

## Writing a mod

The quickest way to start is to copy one of the [examples](#examples) and adapt it. Full guides:
[Writing .NET mods](docs/writing-dotnet-mods.md) and
[Writing native mods](docs/writing-native-mods.md).

## Examples

| Example                                             | Surface    | What it shows                                                     |
|-----------------------------------------------------|------------|-------------------------------------------------------------------|
| [`basic-mod`](examples/basic-mod)                   | C++        | Minimal native mod skeleton and the hooking entry points          |
| [`internal_developer`](examples/internal_developer) | C++        | Enables Studio's internal developer tools                         |
| [`discord_rpc`](examples/discord_rpc)               | C++ / Luau | Discord Rich Presence, native + script bridge                     |
| [`example_dotnet`](examples/example_dotnet)         | C#         | Services, instances, properties, and events through the typed API |
| [`discord_rpc_dotnet`](examples/discord_rpc_dotnet) | C#         | A managed Discord Rich Presence integration, desktop app or Discord Web |

## Building from source

### Prerequisites

- Windows with Visual Studio 2022 (MSVC, x64)
- CMake 3.22.1 or newer
- .NET 10 SDK (for the managed runtime and mods)
- Git

### Steps

```powershell
git clone https://github.com/revolutionxk/roblox-modloader.git
cd roblox-modloader

cmake -B build -S . -G "Visual Studio 17 2022"
cmake --build build --config Release
```

The build generates the `dwmapi.dll` proxy and the loader. Dependencies are fetched automatically
via CMake.

### Build options

| Option                                   | Description                          | Default |
|------------------------------------------|--------------------------------------|---------|
| `ROBLOX_MODLOADER_BUILD_PROXY_GENERATOR` | Build the proxy generator tool       | ON      |
| `ROBLOX_MODLOADER_BUILD_PROXY_DLL`       | Auto-generate the `dwmapi.dll` proxy | ON      |
| `ROBLOX_MODLOADER_BUILD_EXAMPLES`        | Build the example mods               | ON      |

## Roadmap

Current focus areas:

- Finish the [dumper](docs/dumper.md)
- Re-enable Luau/internal scripting support
- Add macOS support (long-term goal)
- Finish .NET modding support (Better API, async support, etc.)
- Add a mod browser and installer

## Contributing

Contributions are welcome. Please open an issue to discuss substantial changes first, keep pull
requests focused, follow the existing code style, and add tests where it makes sense. The
[`docs/`](docs/) directory is the best starting point for understanding the codebase.

## License

Released under the MIT License. See [LICENSE](LICENSE).

## Disclaimer

This project is provided for educational and research purposes. You are responsible for complying
with Roblox's Terms of Service and any applicable laws. It is not affiliated with or endorsed by
Roblox Corporation.
