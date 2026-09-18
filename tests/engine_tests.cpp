#include "../core/native/engine.h"

#include <algorithm>
#include <iostream>

using namespace quick7zip;

int main() {
    SystemProfile ssd{12, 16ull * 1024 * 1024 * 1024, DriveKind::Ssd, L"C:\\"};
    FileProfile smallFiles;
    smallFiles.fileCount = 100000;
    smallFiles.smallFileCount = 90000;
    smallFiles.totalBytes = 2ull * 1024 * 1024 * 1024;
    smallFiles.topLevelDirectoryCount = 20;
    const auto plan = ChoosePlan(ssd, smallFiles);
    if (plan.compressionLevel != 1 || !plan.solid || !plan.sortByType) return 1;

    SystemProfile hdd = ssd;
    hdd.driveKind = DriveKind::Hdd;
    const auto hddPlan = ChoosePlan(hdd, smallFiles);
    if (hddPlan.sortByType) return 2;

    if (!IsAlreadyCompressedPath(L"photo.JPEG") || !IsAlreadyCompressedPath(L"book.epub") ||
        !IsAlreadyCompressedPath(L"sheet.xlsm") || !IsAlreadyCompressedPath(L"video.mkv")) return 8;
    if (IsAlreadyCompressedPath(L"disk.iso") || IsAlreadyCompressedPath(L"document.pdf") ||
        IsAlreadyCompressedPath(L"raw-video.avi") || IsAlreadyCompressedPath(L"notes.txt")) return 9;

    FileProfile mixedFiles = smallFiles;
    mixedFiles.compressedLikeFileCount = 10;
    mixedFiles.compressedLikeBytes = 1024;
    if (!ChoosePlan(ssd, mixedFiles).useHybridMethods) return 10;

    ArchiveRequest request;
    request.inputPath = L"C:\\input folder";
    request.outputPath = L"D:\\archive.7z";
    request.plan = plan;
    request.encrypt = true;
    request.password = L"must-not-appear";
    request.encryptHeaders = true;
    request.volumeSize = L"3900m";
    const auto args = BuildArguments(request);
    if (std::find(args.begin(), args.end(), request.password) != args.end()) return 3;
    if (std::find(args.begin(), args.end(), L"-p") == args.end()) return 4;
    if (std::find(args.begin(), args.end(), L"-mhe=on") == args.end()) return 5;
    if (std::find(args.begin(), args.end(), L"-v3900m") == args.end()) return 6;
    if (!IsSupportedSevenZipVersion(L"26.02") || IsSupportedSevenZipVersion(L"26.01")) return 7;

    ArchiveRequest multiRequest;
    multiRequest.inputPaths = {L"C:\\data\\file1.txt", L"C:\\data\\file2.txt"};
    multiRequest.outputPath = L"C:\\data\\archive.7z";
    multiRequest.plan = plan;
    const auto multiArgs = BuildArguments(multiRequest);
    if (std::find(multiArgs.begin(), multiArgs.end(), L"C:\\data\\file1.txt") == multiArgs.end()) return 11;
    if (std::find(multiArgs.begin(), multiArgs.end(), L"C:\\data\\file2.txt") == multiArgs.end()) return 12;

    const auto multiProfile = DetectSystemProfileForPaths({L"C:\\data\\file1.txt", L"C:\\data\\file2.txt"});
    if (multiProfile.driveRoot != L"C:\\") return 13;

    std::cout << "Quick7Zip engine tests passed\n";
    return 0;
}
