#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "engine.h"
#include <windows.h>

#include <atomic>
#include <iostream>
#include <string>

using namespace quick7zip;

namespace {

void PrintHelp() {
    std::wcout << LR"(Quick7Zip CLI v0.1.0

Usage:
  Quick7Zip_cli.exe --analyze <folder>
  Quick7Zip_cli.exe --input <folder> --output <archive.7z> [options]

Options:
  --encrypt       Prompt for a password without placing it on the command line
  --no-headers    Do not encrypt archive file names
  --split <size>  Split volumes, for example 3900m or 10g
  --dry-run       Print the selected settings without creating an archive
  --help          Show this help
)";
}

std::wstring ReadPassword() {
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    const bool console = GetConsoleMode(input, &mode) != FALSE;
    if (console) SetConsoleMode(input, mode & ~ENABLE_ECHO_INPUT);
    std::wstring password;
    std::getline(std::wcin, password);
    if (console) {
        SetConsoleMode(input, mode);
        std::wcout << L"\n";
    }
    return password;
}

void PrintProfile(const SystemProfile& system, const FileProfile& files, const OptimizationPlan& plan) {
    std::wcout << L"Drive: " << DriveKindName(system.driveKind) << L" (" << system.driveRoot << L")\n"
               << L"CPU threads: " << system.logicalProcessors << L"\n"
               << L"Memory: " << FormatBytes(system.physicalMemoryBytes) << L"\n"
               << L"Files: " << files.fileCount << L"\n"
               << L"Directories: " << files.directoryCount << L"\n"
               << L"Input size: " << FormatBytes(files.totalBytes) << L"\n"
               << L"Already-compressed: " << files.compressedLikeFileCount << L" files / "
               << FormatBytes(files.compressedLikeBytes) << L"\n"
               << L"Recommended: mx=" << plan.compressionLevel << L", mmt=" << plan.threads
               << L", solid=" << (plan.solid ? L"on" : L"off")
               << L", sort-by-type=" << (plan.sortByType ? L"on" : L"off")
               << L", hybrid=" << (plan.useHybridMethods ? L"on" : L"off")
               << L", jobs=" << plan.recommendedJobs << L"\n";
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    std::wstring inputPath, outputPath, split;
    bool analyzeOnly = false, encrypt = false, encryptHeaders = true, dryRun = false;
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];
        if (arg == L"--help" || arg == L"-h") { PrintHelp(); return 0; }
        if (arg == L"--analyze" && i + 1 < argc) { analyzeOnly = true; inputPath = argv[++i]; continue; }
        if (arg == L"--input" && i + 1 < argc) { inputPath = argv[++i]; continue; }
        if (arg == L"--output" && i + 1 < argc) { outputPath = argv[++i]; continue; }
        if (arg == L"--split" && i + 1 < argc) { split = argv[++i]; continue; }
        if (arg == L"--encrypt") { encrypt = true; continue; }
        if (arg == L"--no-headers") { encryptHeaders = false; continue; }
        if (arg == L"--dry-run") { dryRun = true; continue; }
        std::wcerr << L"Unknown or incomplete option: " << arg << L"\n";
        return 2;
    }
    if (inputPath.empty() || (!analyzeOnly && outputPath.empty())) { PrintHelp(); return 2; }

    const auto sevenZip = FindSevenZip();
    if (!sevenZip.found) {
        std::wcerr << L"7-Zip was not found. Install 7-Zip first.\n";
        return 3;
    }
    if (!IsSupportedSevenZipVersion(sevenZip.version)) {
        std::wcerr << L"Quick7Zip requires 7-Zip 26.02 or newer for the current security baseline.\n";
        return 3;
    }
    std::wcout << L"7-Zip: " << sevenZip.version << L" (" << sevenZip.executable << L")\n";
    std::atomic_bool cancel{false};
    FileProfile files;
    std::wcout << L"Analyzing...\n";
    if (!AnalyzePath(inputPath, files, cancel, [](const std::wstring& status) {
            std::wcout << L"\r" << status << L"        " << std::flush;
        })) {
        std::wcerr << L"\nAnalysis failed.\n";
        return 4;
    }
    std::wcout << L"\n";
    const auto system = DetectSystemProfile(inputPath);
    const auto plan = ChoosePlan(system, files);
    PrintProfile(system, files, plan);
    if (analyzeOnly || dryRun) return 0;

    ArchiveRequest request;
    request.sevenZipPath = sevenZip.executable;
    request.inputPath = inputPath;
    request.outputPath = outputPath;
    request.plan = plan;
    request.encrypt = encrypt;
    request.encryptHeaders = encryptHeaders;
    request.volumeSize = split;
    if (encrypt) {
        std::wcout << L"Password: " << std::flush;
        request.password = ReadPassword();
        if (request.password.empty()) {
            std::wcerr << L"Password must not be empty.\n";
            return 5;
        }
    }
    const auto validationError = ValidateArchiveRequest(request);
    if (!validationError.empty()) {
        SecureZeroMemory(request.password.data(), request.password.size() * sizeof(wchar_t));
        std::wcerr << L"Archive request rejected: " << validationError << L"\n";
        return 6;
    }
    const int exitCode = RunArchive(request, cancel, [](const std::wstring& text) {
        std::wcout << text << std::flush;
    });
    SecureZeroMemory(request.password.data(), request.password.size() * sizeof(wchar_t));
    if (exitCode != 0) std::wcerr << L"\n7-Zip failed with exit code " << exitCode << L".\n";
    return exitCode;
}
