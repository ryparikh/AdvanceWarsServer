# Build And Test

This project can be built from the Visual Studio solution with MSBuild.

## Prerequisites

- Visual Studio 2022 with the v143 C++ toolset.
- Windows SDK 10.0.
- The local dependency paths referenced by `AdvanceWarsServer/AdvanceWarsServer.vcxproj`, including libtorch, nlohmann-json, asio, and via-httplib.

## Build

From the repository root:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'AdvanceWarsServer.sln' /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

Release x64:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'AdvanceWarsServer.sln' /m /p:Configuration=Release /p:Platform=x64 /v:minimal
```

## Run Tests

Run tests from the `AdvanceWarsServer` project directory so the relative `test/json` path resolves:

```powershell
Set-Location .\AdvanceWarsServer
..\x64\Debug\AdvanceWarsServer.exe -test test/json
```

Release:

```powershell
Set-Location .\AdvanceWarsServer
..\x64\Release\AdvanceWarsServer.exe -test test/json
```

You can also run a subset:

```powershell
..\x64\Debug\AdvanceWarsServer.exe -test test/json/transport
```
