# Quick7Zip Changelog
[日本語版 HISTORY_jp.md](HISTORY_jp.md)

## Versioning rules

- First digit: new features
- Second digit: bug fixes
- Third digit: documentation and other changes

## 2.3.0 (2026-09-23)

- On first start, ask before adding the Explorer right-click menu entry (previously added without asking). The answer is saved and can be changed later in Settings.
- Aligned the version shown in the executable and documents (v2.2.2 still carried 2.2.1 metadata).

## 2.2.1 (2026-09-18)

- Adopted Quick series standard project directory template (reorganized source, scripts, and documentation).
- Isolated build intermediate artifacts and removed redundant HTML file from distribution package.
- Cleaned up README and documentation (added Releases link, simplified output replacement description).

## 2.2.0 (2026-09-18)

- Improved folder and file selection handling in the explorer browse dialog (allow selecting a folder directly without navigating into it).
- Updated input placeholder text to "Folder or file (Browse)".

## 2.1.0 (2026-09-18)

- Modernized the input Browse dialog using the native Windows Explorer style (`IFileOpenDialog`).
- Enabled direct selection of both files and folders visible within the browse dialog.
- Supported multi-item selection with CTRL+click, SHIFT+click, and mouse drag rectangle in the browse dialog.
- Fixed a bug where the Start Compression button remained disabled when a single file was selected or entered manually.

## 2.0.0 (2026-09-18)

- Added support for batch 7z compression of multiple files and folders.
- Enhanced Explorer context menu handling to aggregate multi-selected items into a single application instance for batch archiving (via Mutex and WM_COPYDATA).
- Improved UI display of selected multiple files to be comma-separated with horizontal scrolling support.
- Fixed an issue where the first file was duplicated in the archive request causing 7-Zip to terminate with an error.
- Set the default browse directory to the user's profile home folder when no path is selected.
- Fixed an issue where the browse dialog reopened upon cancellation due to duplicate event listeners.
- Fixed a registry key formatting bug when configuring Explorer context menu integration.

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
