Quick7Zip - Windows向け7-Zip自動最適化フロントエンド
配布パッケージ  v1.0.0 正式リリース

GitHub
------
https://github.com/maktak-105/Quick7Zip

動作環境
--------
- Windows 10 / 11（64-bit）
- 別途インストールした7-Zip 26.02以降
- Microsoft Edge WebView2 Runtime

使い方
------
1. Quick7Zip.exeとWebView2Loader.dllを同じフォルダへ置きます。GUIはQuick7Zip.exeに埋め込まれています。
2. Quick7Zip.exeを管理者権限なしで起動します。
3. 圧縮対象のフォルダ／ドライブと、出力.7zファイルを指定します。
4. 自動分析された設定を確認して圧縮を開始します。
5. 必要に応じてAES-256暗号化と分割ボリュームを有効にします。

CLIはQuick7Zip_cli.exeです。`Quick7Zip_cli.exe --help`でオプションを表示します。

重要な動作
----------
- Quick7Zipは7-Zipを同梱せず、利用者がインストールした7z.exeを検出します。
- 明示的に指定した既存の出力アーカイブは置き換えます。入力フォルダ内への出力は引き続き拒否します。
- 暗号化パスワードはコマンドライン引数ではなく、標準入力で7-Zipへ渡します。
- 分割ボリュームは1つの論理アーカイブであり、圧縮速度を向上させません。
- 圧縮は検証済みバックアップやシステムイメージの代替ではありません。

配布ファイル
------------
- Quick7Zip.exe / Quick7Zip_cli.exe
- WebView2Loader.dll
- readme.txt / readme_jp.txt
- history.txt / history_jp.txt
- LICENSE.txt / LICENSE_jp.txt
- THIRD_PARTY_NOTICES.txt
- WEBVIEW2_LICENSE.txt / WEBVIEW2_NOTICE.txt

ライセンス
----------
Quick7ZipはMIT Licenseです。7-Zipは別ソフトウェアであり再配布しません。
公式リンクおよびWebView2再配布表示はTHIRD_PARTY_NOTICES.txtを参照してください。

免責事項
--------
本ソフトウェアは現状有姿で提供されます。重要なデータは独立したバックアップを保持してください。
