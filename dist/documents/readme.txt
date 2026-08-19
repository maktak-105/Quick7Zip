Quick7Zip - Adaptive 7-Zip frontend for Windows
Distribution package  v0.1.0 development preview

GitHub
------
https://github.com/maktak-105/Quick7Zip

Requirements
------------
- Windows 10 / 11 (64-bit)
- 7-Zip 26.02 or newer, installed separately
- Microsoft Edge WebView2 Runtime

Usage
-----
1. Keep Quick7Zip.exe, WebView2Loader.dll, and index.html together.
2. Run Quick7Zip.exe without administrator privileges.
3. Select a folder or drive and an output .7z archive.
4. Analyze the source, review the automatically selected profile, and start compression.
5. Optionally enable AES-256 encryption and split volumes.

The CLI is Quick7Zip_cli.exe. Run `Quick7Zip_cli.exe --help` for its options.

Important behavior
------------------
- Quick7Zip does not bundle 7-Zip. It detects the user's installed 7z.exe.
- Encryption passwords are sent to 7-Zip through standard input, not command-line arguments.
- Split volumes are one logical archive and do not improve compression speed.
- Compression is not a substitute for a verified backup or a system image.

Distribution files
------------------
- Quick7Zip.exe / Quick7Zip_cli.exe
- WebView2Loader.dll
- index.html
- readme.txt / readme_jp.txt
- history.txt / history_jp.txt
- LICENSE.txt / LICENSE_jp.txt
- THIRD_PARTY_NOTICES.txt
- WEBVIEW2_LICENSE.txt / WEBVIEW2_NOTICE.txt

License
-------
Quick7Zip is MIT licensed. 7-Zip is separate software and is not redistributed.
See THIRD_PARTY_NOTICES.txt for authoritative links and WebView2 redistribution notices.

Disclaimer
----------
This software is provided as-is. Always keep independent backups of important data.
