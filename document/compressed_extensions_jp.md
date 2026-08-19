# 圧縮済み拡張子の判定
[English compressed_extensions.md](compressed_extensions.md)

Quick7Zipは、拡張子だけで圧縮済みと高い確度で判断できるファイルを7zの`Copy`方式で格納します。その他はLZMA2へ渡します。

## Copy対象

- アーカイブ: `7z zip rar gz gzip bz2 bzip2 xz lzma zst zstd lz4 br tgz tbz tbz2 txz tzst cab arj lzh lha`
- コミック・Java: `cbz cbr cb7 jar war ear`
- 画像: `jpg jpeg jpe jfif png gif webp heic heif avif jxl jp2 j2k jpf jpm jpx`
- 音声: `mp3 aac m4a m4b ogg oga opus flac wma ape wv tta`
- 動画: `mp4 m4v mkv webm ogv wmv flv 3gp 3g2 mpg mpeg m2v ts mts m2ts vob`
- Microsoft Office Open XML: `docx xlsx pptx docm xlsm pptm dotx dotm xltx xltm potx potm ppsx ppsm thmx xlsb`
- OpenDocument・電子書籍: `odt ods odp odg odf odb ott ots otp epub`
- アプリ配布・ZIPコンテナ: `apk aab ipa xpi crx vsix nupkg appx appxbundle msix msixbundle kmz xps oxps 3mf`
- その他: `woff woff2 wim esd swm`

## 意図的に対象外

`pdf iso avi mov tiff tif dmg vhd vhdx`は、中身や作成方式によって圧縮効果が大きく異なるため、拡張子だけではCopyにしません。古いOffice形式の`doc xls ppt`、非圧縮音声になりやすい`wav aiff`、ビットマップの`bmp`もLZMA2対象です。
