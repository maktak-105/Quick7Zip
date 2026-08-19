# Quick7Zip Changelog
[日本語版 HISTORY_jp.md](HISTORY_jp.md)

## Versioning rules

- First digit: new features
- Second digit: bug fixes
- Third digit: documentation and other changes

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
