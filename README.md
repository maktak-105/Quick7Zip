# Quick7Zip

Quick7Zip is an adaptive Windows frontend for an existing 7-Zip installation. It analyzes storage type, CPU, memory, file count, total size, and file composition before selecting practical compression options.

> Stable release v2.2.1.
> [GitHub Releases](https://github.com/maktak-105/Quick7Zip/releases)

## What makes it different

- Integrates into Windows Explorer context menu (asks on first start whether to add it; can be changed later in Settings).
- Detects the installed `7z.exe`; 7-Zip is not bundled.
- Selects compression level, thread count, solid mode, and type sorting from the actual workload.
- Uses 7-Zip's own defaults for dictionary size, word size, and solid block size; these parameters are not benchmark-tuned by Quick7Zip.
- Supports AES-256 encryption and encrypted file names.
- Sends encryption passwords over the child process standard input, not as command-line arguments.
- Explains the selected profile instead of hiding it behind an unexplained "Auto" button.
- Stores JPEG, modern media, current Office files, and existing archives with `Copy`, while compressing other files with LZMA2.
- Updates elapsed compression time every 0.2 seconds and preserves the final time after completion.
- Analyzes immediately after folder selection; direct path entry triggers analysis on Enter or focus change, without a separate Analyze button.
- Replaces an explicitly selected existing output archive.
- Provides Japanese and English UI, GUI and CLI from one native engine.

## Using the binary release

If you do not need the source code or a build environment, download the distribution ZIP from GitHub Releases.

- [Latest releases](https://github.com/maktak-105/Quick7Zip/releases)
- [Quick7Zip v2.2.1](https://github.com/maktak-105/Quick7Zip/releases/tag/v2.2.1)
- [Direct download of Quick7Zip-binary.zip](https://github.com/maktak-105/Quick7Zip/releases/download/v2.2.1/Quick7Zip-binary.zip)

The ZIP contains all distribution files in one flat folder:

- `Quick7Zip.exe` - GUI version (embedded HTML UI)
- `Quick7Zip_cli.exe` - command-line version
- `WebView2Loader.dll` - WebView2 loader
- `readme.txt` / `readme_jp.txt` - distribution documentation
- `history.txt` / `history_jp.txt` - change log
- `LICENSE.txt` / `LICENSE_jp.txt` - MIT License files
- `THIRD_PARTY_NOTICES.txt` / `WEBVIEW2_LICENSE.txt` / `WEBVIEW2_NOTICE.txt` - third-party notices

### Integrity verification (SHA-256)

Official SHA-256 checksums for the distribution ZIP and binaries are automatically computed during the CI (GitHub Actions) build and published as `SHA256SUMS.txt` on each release page. Verify the downloaded package with PowerShell:

```powershell
Get-FileHash .\Quick7Zip-binary.zip -Algorithm SHA256
```

## Requirements

- Windows 10 or 11 (64-bit)
- 7-Zip installed separately; the supported baseline is 7-Zip 26.02 or newer
- Microsoft Edge WebView2 Runtime

## Build

Install MinGW-w64 and place the WebView2 SDK under `C:\tools\webview2`, or set `WEBVIEW2_INCLUDE` and `WEBVIEW2_LOADER`.

```powershell
scripts\build.bat
# or python scripts/build.py
```

Outputs are created in `dist`:

- `Quick7Zip.exe`
- `Quick7Zip_cli.exe`
- `WebView2Loader.dll`

The GUI HTML/CSS/JavaScript UI is embedded in `Quick7Zip.exe`; no separate `index.html` is required.

## CLI

```powershell
Quick7Zip_cli.exe --analyze C:\Data
Quick7Zip_cli.exe --input C:\Data --output D:\Backup\Data.7z
Quick7Zip_cli.exe --input C:\Data --output D:\Backup\Data.7z --encrypt --split 3900m
```

`--encrypt` prompts without echo and does not accept a password argument.

## Security model

Quick7Zip launches 7-Zip without elevation and without a shell. Paths are passed as individual Windows process arguments. Passwords are sent to 7-Zip through an inherited standard-input pipe and cleared from application buffers after use. Quick7Zip does not make 7-Zip a sandbox; keep 7-Zip updated and do not treat archives as trusted input.

## License and third-party software

Quick7Zip source code is MIT licensed. See [LICENSE](LICENSE).

7-Zip is installed separately and is not redistributed by this project. It is primarily licensed under GNU LGPL 2.1 or later, with additional licenses for some components. See [7-Zip's authoritative license](https://www.7-zip.org/license.txt).

The binary package includes Microsoft's `WebView2Loader.dll`; its license and notice are preserved in `docs/distribution`. See [THIRD_PARTY_NOTICES.txt](docs/distribution/THIRD_PARTY_NOTICES.txt).

## Disclaimer

This software is provided as-is. Compression is not a substitute for a verified backup. The author assumes no responsibility for data loss or other damage.
