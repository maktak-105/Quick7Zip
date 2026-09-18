# 開発環境
[English environment.md](environment.md)

## 必要環境

- Windows 10/11 x64
- ビルド制御用Python 3.11以降
- MinGW-w64 C++17コンパイラと`windres`
- Microsoft WebView2 SDKヘッダーとx64ローダー
- 統合テスト用7-Zip 26.02以降

確認済みコンパイラはWinGetパッケージ`BrechtSanders.WinLibs.MCF.UCRT`です。WebView2 SDKの既定パスは`C:\tools\webview2`で、`WEBVIEW2_INCLUDE`と`WEBVIEW2_LOADER`で変更できます。

## ビルド

```powershell
build.bat
```

UIをビルド用の中間`dist\binary\index.html`へ統合してから`Quick7Zip.exe`へ埋め込み、ネイティブエンジンを`Quick7Zip_cli.exe`と`Quick7Zip.exe`へ静的リンクし、`WebView2Loader.dll`を`dist\binary`へコピーします。

## 手動検証

```powershell
dist\binary\Quick7Zip_cli.exe --analyze C:\Data
dist\binary\Quick7Zip_cli.exe --input C:\Data --output archive.7z
& 'C:\Program Files\7-Zip\7z.exe' t archive.7z
```

CLI暗号化は`--encrypt`でパスワードを対話入力します。スクリプトやログへパスワード引数を追加しないでください。

## トラブルシューティング

- `WebView2.h`がない：`WEBVIEW2_INCLUDE`をSDKの`build\native\include`へ設定。
- ローダーがない：`WEBVIEW2_LOADER`をx64版`WebView2Loader.dll`へ設定。
- 7-Zipが見つからない：7-Zip公式サイトからx64版をインストール。
- GUIが白い：再ビルドして埋め込みUIを更新してください。実行時に別途`index.html`は不要です。
