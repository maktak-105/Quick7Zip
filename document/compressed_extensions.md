# Already-compressed extension classification
[日本語 compressed_extensions_jp.md](compressed_extensions_jp.md)

Quick7Zip stores file types that can be identified as already compressed with high confidence using the 7z `Copy` method. Other files are passed to LZMA2.

## Stored with Copy

- Archives: `7z zip rar gz gzip bz2 bzip2 xz lzma zst zstd lz4 br tgz tbz tbz2 txz tzst cab arj lzh lha`
- Comics and Java packages: `cbz cbr cb7 jar war ear`
- Images: `jpg jpeg jpe jfif png gif webp heic heif avif jxl jp2 j2k jpf jpm jpx`
- Audio: `mp3 aac m4a m4b ogg oga opus flac wma ape wv tta`
- Video: `mp4 m4v mkv webm ogv wmv flv 3gp 3g2 mpg mpeg m2v ts mts m2ts vob`
- Microsoft Office Open XML: `docx xlsx pptx docm xlsm pptm dotx dotm xltx xltm potx potm ppsx ppsm thmx xlsb`
- OpenDocument and ebooks: `odt ods odp odg odf odb ott ots otp epub`
- Application and ZIP containers: `apk aab ipa xpi crx vsix nupkg appx appxbundle msix msixbundle kmz xps oxps 3mf`
- Other: `woff woff2 wim esd swm`

## Deliberately excluded

`pdf iso avi mov tiff tif dmg vhd vhdx` can vary substantially by content or encoding and are not classified as Copy from their extension alone. Legacy Office `doc xls ppt`, commonly uncompressed audio `wav aiff`, and bitmap `bmp` remain LZMA2 candidates.

A future content classifier can supplement extension matching with a small compression probe or entropy estimate.
