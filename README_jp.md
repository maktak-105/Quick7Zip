# Quick7Zip
[English README.md](README.md)

Quick7Zipは、インストール済みの7-Zipを利用するWindows向け自動最適化フロントエンドです。ストレージ種別、CPU、メモリ、ファイル数、合計サイズ、ファイル構成を分析してから、実用的な圧縮オプションを選択します。

> v2.1.0正式リリースです。

## 差別化ポイント

- エクスプローラーの右クリックメニューから直接起動可能（設定画面でON/OFF切替可能、初期値ON）。複数ファイル選択時も1つのウィンドウに集約して一括圧縮します。
- 複数ファイル・フォルダの一括選択に対応（カンマ区切り・横スクロール表示）。
- インストール済みの`7z.exe`を検出し、7-Zip本体は同梱しません。
- 実データから圧縮レベル、スレッド数、ソリッド圧縮、種類順ソートを選択します。
- 辞書サイズ、ワードサイズ、ソリッドブロックサイズはQuick7Zipで実測調整せず、7-Zipの既定値を使用します。
- AES-256暗号化とファイル名暗号化に対応します。
- パスワードをコマンドライン引数へ載せず、子プロセスの標準入力で渡します。
- 単なる「自動」ではなく、選択した設定と理由を表示します。
- JPEG、動画、現行Office、既存アーカイブなどは再圧縮せず`Copy`格納し、その他をLZMA2で圧縮します。
- 圧縮中の経過時間を0.2秒ごとに更新し、完了後も最終時間を表示します。
- フォルダ選択時に自動分析し、直接入力時はEnterまたはフォーカス移動で分析します。独立した分析ボタンはありません。
- 明示的に指定した既存の出力アーカイブは置き換えます。ただし入力フォルダ内や入力ファイル自身への出力は引き続き拒否します。
- 日本語／英語UI、GUI／CLIで同じネイティブエンジンを利用します。

## 動作環境

- Windows 10 / 11（64-bit）
- 別途インストールした7-Zip（対応基準は7-Zip 26.02以降）
- Microsoft Edge WebView2 Runtime

## ビルド

MinGW-w64を導入し、WebView2 SDKを`C:\tools\webview2`へ配置するか、`WEBVIEW2_INCLUDE`と`WEBVIEW2_LOADER`を設定します。

```powershell
build.bat
```

`dist\binary`へ次を生成します。

- `Quick7Zip.exe`
- `Quick7Zip_cli.exe`
- `WebView2Loader.dll`

GUIのHTML/CSS/JavaScript UIは`Quick7Zip.exe`へ埋め込まれており、別途`index.html`を配置する必要はありません。

## CLI

```powershell
Quick7Zip_cli.exe --analyze C:\Data
Quick7Zip_cli.exe --input C:\Data --output D:\Backup\Data.7z
Quick7Zip_cli.exe --input C:\Data --output D:\Backup\Data.7z --encrypt --split 3900m
```

`--encrypt`は画面に表示しないパスワード入力を行い、パスワード引数は受け付けません。

## セキュリティ設計

Quick7Zipは管理者権限やシェルを使わずに7-Zipを起動します。パスはWindowsプロセスの個別引数として渡します。パスワードは継承した標準入力パイプで7-Zipへ送り、使用後にアプリ側バッファを消去します。ただし7-Zipをサンドボックス化するものではありません。7-Zipを常に更新し、アーカイブを信頼済み入力として扱わないでください。

## ライセンスとサードパーティソフトウェア

Quick7ZipのソースコードはMIT Licenseです。[LICENSE](LICENSE)を参照してください。

7-Zipは利用者が別途インストールし、本プロジェクトでは再配布しません。主なライセンスはGNU LGPL 2.1以降で、一部コンポーネントには追加ライセンスがあります。[7-Zip公式ライセンス](https://www.7-zip.org/license.txt)を参照してください。

配布パッケージにはMicrosoftの`WebView2Loader.dll`を含めます。ライセンスとNOTICEは`dist/documents`に保持します。[THIRD_PARTY_NOTICES.txt](dist/documents/THIRD_PARTY_NOTICES.txt)を参照してください。

## 免責事項

本ソフトウェアは現状有姿で提供されます。圧縮は検証済みバックアップの代替ではありません。データ消失その他の損害について作者は責任を負いません。
