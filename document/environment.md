# Development Environment
[日本語版 environment_jp.md](environment_jp.md)

## Requirements

- Windows 10/11 x64
- Python 3.11 or later for build orchestration
- MinGW-w64 C++17 compiler and `windres`
- Microsoft WebView2 SDK headers and x64 loader
- 7-Zip 26.02 or later for integration tests

The validated compiler is the WinGet package `BrechtSanders.WinLibs.MCF.UCRT`. The default WebView2 SDK path is `C:\tools\webview2`; override it with `WEBVIEW2_INCLUDE` and `WEBVIEW2_LOADER`.

## Build

```powershell
build.bat
```

The build script bundles the UI into an intermediate `dist\binary\index.html`, embeds that resource into `Quick7Zip.exe`, statically links the native engine into `Quick7Zip_cli.exe` and `Quick7Zip.exe`, and copies `WebView2Loader.dll` into `dist\binary`.

## Manual verification

```powershell
dist\binary\Quick7Zip_cli.exe --analyze C:\Data
dist\binary\Quick7Zip_cli.exe --input C:\Data --output archive.7z
& 'C:\Program Files\7-Zip\7z.exe' t archive.7z
```

Encrypted CLI runs use `--encrypt` and prompt for the password. Do not add password arguments to scripts or logs.

## Troubleshooting

- Missing `WebView2.h`: set `WEBVIEW2_INCLUDE` to the SDK `build\native\include` directory.
- Missing loader: set `WEBVIEW2_LOADER` to the x64 `WebView2Loader.dll`.
- 7-Zip not found: install the x64 edition from the official 7-Zip site.
- Blank GUI: rebuild so the bundled UI resource is refreshed; runtime does not require a separate `index.html`.
