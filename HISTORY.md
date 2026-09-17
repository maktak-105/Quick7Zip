# Quick7Zip Changelog
[日本語版 HISTORY_jp.md](HISTORY_jp.md)

## Versioning rules

- First digit: new features
- Second digit: bug fixes
- Third digit: documentation and other changes

## 1.1.2 (2026-09-17)

- Add Explorer context menu integration (compress selected folder, file, or drive directly from right-click).
- Add a Settings button (gear icon) in the header to toggle context menu integration, enabled by default.
- Automatically load and analyze command-line target paths on launch.

## 1.1.1 (2026-08-27)

- Add a help button in the header (usage guide and version display), and move the 7-Zip version badge to the footer center.
- Add an About dialog (version, environment, and author) reachable from the help menu.
- Fix window size appearing inconsistent across monitors with different DPI, caused by missing `WM_DPICHANGED` handling.
- Automatically resize the window to fit the actual content height instead of using a fixed size, so switching between Japanese and English no longer breaks the layout.
- Fix the split-volume dropdown being truncated in English.

## 1.0.0 (2026-08-19)

- First stable release of the native WebView2 GUI and CLI.
- Complete adaptive compression workflow with automatic analysis, hybrid storage, encryption, splitting, progress, ratio, and elapsed-time reporting.
- Compact horizontal default layout with bilingual UI and distribution documentation.

## 0.1.0 (2026-08-19)

- Initial native engine, WebView2 GUI, and CLI.
- Detect installed 7-Zip, CPU, memory, HDD/SSD, and input file profile.
- Automatically select compression level, threads, solid mode, and type sorting.
- Add extension-aware hybrid archives: Copy for already-compressed formats and LZMA2 for other files.
- Show final archive ratio and output size on completion.
- Update elapsed compression time every 0.2 seconds and retain the final time.
- Remove the Analyze button and idle prompt in favor of automatic analysis, and reduce the UI width by about 20%.
- Add AES-256 and header encryption without command-line password exposure.
- Add split-volume selection, cancellation, progress output, and bilingual documentation.
