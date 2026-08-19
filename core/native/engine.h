#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#ifdef QUICK7ZIP_ENGINE_EXPORTS
#define Q7Z_API __declspec(dllexport)
#else
#define Q7Z_API
#endif

namespace quick7zip {

enum class DriveKind { Unknown, Hdd, Ssd, Network, Removable };

struct SevenZipInfo {
    bool found = false;
    std::wstring executable;
    std::wstring version;
};

struct SystemProfile {
    unsigned logicalProcessors = 1;
    std::uint64_t physicalMemoryBytes = 0;
    DriveKind driveKind = DriveKind::Unknown;
    std::wstring driveRoot;
};

struct FileProfile {
    std::uint64_t fileCount = 0;
    std::uint64_t directoryCount = 0;
    std::uint64_t totalBytes = 0;
    std::uint64_t smallFileCount = 0;
    std::uint64_t compressedLikeFileCount = 0;
    std::uint64_t compressedLikeBytes = 0;
    std::uint64_t inaccessibleCount = 0;
    std::uint64_t reparsePointCount = 0;
    std::uint64_t topLevelDirectoryCount = 0;
};

struct OptimizationPlan {
    int compressionLevel = 1;
    unsigned threads = 1;
    bool solid = true;
    bool sortByType = false;
    bool useHybridMethods = false;
    unsigned recommendedJobs = 1;
    std::wstring reason;
};

struct ArchiveRequest {
    std::wstring sevenZipPath;
    std::wstring inputPath;
    std::wstring outputPath;
    OptimizationPlan plan;
    bool encrypt = false;
    bool encryptHeaders = true;
    std::wstring password;
    std::wstring volumeSize;
};

using ProgressCallback = std::function<void(const std::wstring&)>;

Q7Z_API SevenZipInfo FindSevenZip();
Q7Z_API bool IsSupportedSevenZipVersion(const std::wstring& version);
Q7Z_API bool IsAlreadyCompressedPath(const std::wstring& path);
Q7Z_API SystemProfile DetectSystemProfile(const std::wstring& inputPath);
Q7Z_API bool AnalyzePath(const std::wstring& inputPath, FileProfile& result,
                         std::atomic_bool& cancel, const ProgressCallback& progress);
Q7Z_API OptimizationPlan ChoosePlan(const SystemProfile& system, const FileProfile& files);
Q7Z_API std::wstring ValidateArchiveRequest(const ArchiveRequest& request);
Q7Z_API std::vector<std::wstring> BuildArguments(const ArchiveRequest& request);
Q7Z_API int RunArchive(const ArchiveRequest& request, std::atomic_bool& cancel,
                       const ProgressCallback& output);
Q7Z_API std::wstring FormatBytes(std::uint64_t bytes);
Q7Z_API const wchar_t* DriveKindName(DriveKind kind);

} // namespace quick7zip
