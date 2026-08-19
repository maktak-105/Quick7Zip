# Quick7Zip Specification
[日本語版 spec_jp.md](spec_jp.md)

## Overview

Quick7Zip is a Windows 10/11 x64 application that analyzes a source folder or drive and runs an existing 7-Zip installation with adaptive options. Version: v0.1.0 development preview.

The UI uses a compact 710 px maximum width. Folder selection starts analysis immediately; direct path entry starts it on Enter or focus change. No separate Analyze button or idle analysis prompt is shown.

## Architecture

```text
HTML/CSS/JavaScript UI <-> WebMessage JSON <-> Win32/WebView2 host
                                                |
                                      native C++ analysis engine
                                                |
                                     installed 7z.exe subprocess
```

The engine is GUI-independent and shared by the GUI and CLI. The password is never included in the child command line; it is written to an inherited standard-input pipe.

## Automatic profile

Inputs include logical CPU count, physical memory, seek-penalty storage detection, total bytes, file count, average file size, small-file share, already-compressed extension share, and top-level directory count.

- Large/small-file-heavy or mostly compressed workloads select `mx=1`; other workloads select `mx=3`.
- Threads reserve two logical processors where possible and are capped on low-memory systems.
- Solid compression is enabled.
- Type sorting is enabled for SSD workloads dominated by small files, but disabled for HDDs.
- Formats identified as already compressed with high confidence use `Copy`; other files use LZMA2 in a hybrid archive.
- Because 7-Zip cannot update split volumes, mixed split workloads safely fall back to one LZMA2 pass.
- Two independent jobs are recommended only for capable SSD systems. v0.1.0 reports this recommendation but executes one archive per operation.

See [already-compressed extensions](compressed_extensions.md) for classification details.

## Security

- Runs at the current user privilege level and never requests elevation.
- Launches `7z.exe` directly without `cmd.exe` or PowerShell.
- Quotes Windows arguments and transmits passwords through standard input.
- Does not follow symbolic links during analysis.
- Does not sandbox 7-Zip and does not claim that archives are backups.

## WebMessage protocol

JS to native: `initialize`, `browse_input`, `browse_output`, `analyze`, `start_archive`, `cancel`.

Native to JS: `initialized`, selection messages, analysis progress/result, archive output/result, `cancelled`, and `error`.

Elapsed compression time is measured in JavaScript from the Start button click, refreshed every 0.2 seconds in `X.XXs` format, and frozen on completion, failure, or cancellation. Successful results retain the final time alongside the archive ratio and output size.

## Planned work

- Independent archive chunking and two-job scheduler.
- Estimated output size and elapsed time based on local calibration.
- Optional post-write `7z t` verification.
- Installer/version policy and automated engine security warning.
