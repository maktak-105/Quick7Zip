#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define QUICK7ZIP_ENGINE_EXPORTS
#include "engine.h"

#include <windows.h>
#include <winioctl.h>
#include <shlwapi.h>

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

namespace quick7zip {
namespace {

std::wstring Lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return value;
}

bool IsFile(const std::wstring& path) {
    const DWORD attrs = GetFileAttributesW(path.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring ReadRegistryPath(HKEY root, const wchar_t* subkey, REGSAM view) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(root, subkey, 0, KEY_READ | view, &key) != ERROR_SUCCESS) return {};
    wchar_t value[32768]{};
    DWORD type = 0;
    DWORD bytes = sizeof(value);
    const LONG rc = RegQueryValueExW(key, L"Path", nullptr, &type,
                                     reinterpret_cast<BYTE*>(value), &bytes);
    RegCloseKey(key);
    if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) return {};
    std::wstring result(value);
    if (!result.empty() && result.back() != L'\\') result.push_back(L'\\');
    result += L"7z.exe";
    return result;
}

std::wstring FileVersion(const std::wstring& path) {
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (!size) return {};
    std::vector<BYTE> data(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data())) return {};
    VS_FIXEDFILEINFO* info = nullptr;
    UINT infoSize = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &infoSize) || !info) return {};
    std::wstringstream ss;
    ss << HIWORD(info->dwFileVersionMS) << L'.' << LOWORD(info->dwFileVersionMS);
    const WORD build = HIWORD(info->dwFileVersionLS);
    const WORD revision = LOWORD(info->dwFileVersionLS);
    if (build || revision) ss << L'.' << build << L'.' << revision;
    return ss.str();
}

std::wstring RootForPath(const std::wstring& path) {
    wchar_t full[MAX_PATH * 4]{};
    if (!GetFullPathNameW(path.c_str(), static_cast<DWORD>(std::size(full)), full, nullptr)) return {};
    wchar_t root[MAX_PATH]{};
    if (!GetVolumePathNameW(full, root, static_cast<DWORD>(std::size(root)))) return {};
    return root;
}

std::wstring FullPath(const std::wstring& path) {
    const DWORD needed = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (!needed) return {};
    std::wstring full(needed, L'\0');
    const DWORD written = GetFullPathNameW(path.c_str(), needed, full.data(), nullptr);
    if (!written) return {};
    full.resize(written);
    while (full.size() > 3 && (full.back() == L'\\' || full.back() == L'/')) full.pop_back();
    return full;
}

bool SameOrChildPath(const std::wstring& parentPath, const std::wstring& candidatePath) {
    const std::wstring parent = FullPath(parentPath);
    const std::wstring candidate = FullPath(candidatePath);
    if (parent.empty() || candidate.empty() || candidate.size() < parent.size()) return false;
    if (_wcsnicmp(parent.c_str(), candidate.c_str(), parent.size()) != 0) return false;
    return candidate.size() == parent.size() || candidate[parent.size()] == L'\\' || candidate[parent.size()] == L'/';
}

DriveKind DetectDriveKind(const std::wstring& root) {
    if (root.empty()) return DriveKind::Unknown;
    const UINT type = GetDriveTypeW(root.c_str());
    if (type == DRIVE_REMOTE) return DriveKind::Network;
    if (type == DRIVE_REMOVABLE || type == DRIVE_CDROM) return DriveKind::Removable;
    if (root.size() < 2 || root[1] != L':') return DriveKind::Unknown;

    std::wstring volume = L"\\\\.\\";
    volume += root.substr(0, 2);
    HANDLE handle = CreateFileW(volume.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return DriveKind::Unknown;

    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceSeekPenaltyProperty;
    query.QueryType = PropertyStandardQuery;
    DEVICE_SEEK_PENALTY_DESCRIPTOR descriptor{};
    DWORD returned = 0;
    const BOOL ok = DeviceIoControl(handle, IOCTL_STORAGE_QUERY_PROPERTY,
                                    &query, sizeof(query), &descriptor, sizeof(descriptor),
                                    &returned, nullptr);
    CloseHandle(handle);
    if (!ok) return DriveKind::Unknown;
    return descriptor.IncursSeekPenalty ? DriveKind::Hdd : DriveKind::Ssd;
}

bool IsCompressedLike(const fs::path& path) {
    static const std::set<std::wstring> extensions = {
        // Archive and transport formats that already contain compressed streams.
        L".7z", L".zip", L".rar", L".gz", L".gzip", L".bz2", L".bzip2",
        L".xz", L".lzma", L".zst", L".zstd", L".lz4", L".br", L".tgz",
        L".tbz", L".tbz2", L".txz", L".tzst", L".cab", L".arj", L".lzh",
        L".lha", L".cbz", L".cbr", L".cb7", L".jar", L".war", L".ear",

        // Images with mandatory or overwhelmingly typical compression.
        L".jpg", L".jpeg", L".jpe", L".jfif", L".png", L".gif", L".webp",
        L".heic", L".heif", L".avif", L".jxl", L".jp2", L".j2k", L".jpf",
        L".jpm", L".jpx",

        // Compressed audio and video. AVI/MOV are deliberately omitted: they can contain raw media.
        L".mp3", L".aac", L".m4a", L".m4b", L".ogg", L".oga", L".opus",
        L".flac", L".wma", L".ape", L".wv", L".tta", L".mp4", L".m4v",
        L".mkv", L".webm", L".ogv", L".wmv", L".flv", L".3gp", L".3g2",
        L".mpg", L".mpeg", L".m2v", L".ts", L".mts", L".m2ts", L".vob",

        // ZIP-container documents and application packages.
        L".docx", L".xlsx", L".pptx", L".docm", L".xlsm", L".pptm",
        L".dotx", L".dotm", L".xltx", L".xltm", L".potx", L".potm",
        L".ppsx", L".ppsm", L".thmx", L".xlsb", L".odt", L".ods", L".odp",
        L".odg", L".odf", L".odb", L".ott", L".ots", L".otp", L".epub",
        L".apk", L".aab", L".ipa", L".xpi", L".crx", L".vsix", L".nupkg",
        L".appx", L".appxbundle", L".msix", L".msixbundle", L".kmz", L".xps",
        L".oxps", L".3mf", L".woff", L".woff2", L".wim", L".esd", L".swm"
    };
    return extensions.count(Lower(path.extension().wstring())) != 0;
}

std::wstring QuoteWindowsArgument(const std::wstring& arg) {
    if (arg.empty()) return L"\"\"";
    if (arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) return arg;
    std::wstring result = L"\"";
    unsigned slashes = 0;
    for (wchar_t c : arg) {
        if (c == L'\\') {
            ++slashes;
        } else if (c == L'\"') {
            result.append(slashes * 2 + 1, L'\\');
            result.push_back(L'\"');
            slashes = 0;
        } else {
            result.append(slashes, L'\\');
            slashes = 0;
            result.push_back(c);
        }
    }
    result.append(slashes * 2, L'\\');
    result.push_back(L'\"');
    return result;
}

std::wstring Utf8ToWide(const std::string& input) {
    if (input.empty()) return {};
    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(),
                                    static_cast<int>(input.size()), nullptr, 0);
    UINT codePage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;
    if (count <= 0) {
        codePage = GetOEMCP();
        flags = 0;
        count = MultiByteToWideChar(codePage, flags, input.data(),
                                    static_cast<int>(input.size()), nullptr, 0);
    }
    std::wstring output(static_cast<std::size_t>(std::max(count, 0)), L'\0');
    if (count > 0) MultiByteToWideChar(codePage, flags, input.data(), static_cast<int>(input.size()), output.data(), count);
    return output;
}

} // namespace

SevenZipInfo FindSevenZip() {
    std::vector<std::wstring> candidates;
    for (HKEY root : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE}) {
        for (REGSAM view : {static_cast<REGSAM>(KEY_WOW64_64KEY), static_cast<REGSAM>(KEY_WOW64_32KEY)}) {
            const auto candidate = ReadRegistryPath(root, L"SOFTWARE\\7-Zip", view);
            if (!candidate.empty()) candidates.push_back(candidate);
        }
    }
    wchar_t programFiles[MAX_PATH]{};
    if (GetEnvironmentVariableW(L"ProgramFiles", programFiles, MAX_PATH))
        candidates.emplace_back(std::wstring(programFiles) + L"\\7-Zip\\7z.exe");
    wchar_t found[MAX_PATH * 4]{};
    if (SearchPathW(nullptr, L"7z.exe", nullptr, static_cast<DWORD>(std::size(found)), found, nullptr))
        candidates.emplace_back(found);

    for (const auto& candidate : candidates) {
        if (IsFile(candidate)) return {true, candidate, FileVersion(candidate)};
    }
    return {};
}

bool IsSupportedSevenZipVersion(const std::wstring& version) {
    unsigned major = 0, minor = 0;
    wchar_t dot = 0;
    std::wstringstream parser(version);
    parser >> major >> dot >> minor;
    if (!parser || dot != L'.') return false;
    return major > 26 || (major == 26 && minor >= 2);
}

bool IsAlreadyCompressedPath(const std::wstring& path) {
    return IsCompressedLike(fs::path(path));
}

SystemProfile DetectSystemProfile(const std::wstring& inputPath) {
    SystemProfile result;
    SYSTEM_INFO info{};
    GetNativeSystemInfo(&info);
    result.logicalProcessors = std::max<DWORD>(1, info.dwNumberOfProcessors);
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) result.physicalMemoryBytes = memory.ullTotalPhys;
    result.driveRoot = RootForPath(inputPath);
    result.driveKind = DetectDriveKind(result.driveRoot);
    return result;
}

bool AnalyzePath(const std::wstring& inputPath, FileProfile& result,
                 std::atomic_bool& cancel, const ProgressCallback& progress) {
    result = {};
    std::error_code ec;
    const fs::path root(inputPath);
    if (!fs::exists(root, ec)) return false;
    if (fs::is_regular_file(root, ec)) {
        result.fileCount = 1;
        result.totalBytes = fs::file_size(root, ec);
        result.smallFileCount = result.totalBytes < 128ull * 1024;
        if (IsCompressedLike(root)) {
            result.compressedLikeFileCount = 1;
            result.compressedLikeBytes = result.totalBytes;
        }
        return !ec;
    }

    for (const auto& child : fs::directory_iterator(root, fs::directory_options::skip_permission_denied, ec)) {
        if (child.is_directory(ec)) ++result.topLevelDirectoryCount;
    }
    ec.clear();
    const auto options = fs::directory_options::skip_permission_denied;
    fs::recursive_directory_iterator it(root, options, ec), end;
    for (; it != end && !cancel.load(); it.increment(ec)) {
        if (ec) {
            ++result.inaccessibleCount;
            ec.clear();
            continue;
        }
        const auto status = it->symlink_status(ec);
        if (ec) {
            ++result.inaccessibleCount;
            ec.clear();
            continue;
        }
        if (fs::is_symlink(status)) {
            ++result.reparsePointCount;
            it.disable_recursion_pending();
            continue;
        }
        if (fs::is_directory(status)) {
            ++result.directoryCount;
            continue;
        }
        if (!fs::is_regular_file(status)) continue;
        const auto size = it->file_size(ec);
        if (ec) {
            ++result.inaccessibleCount;
            ec.clear();
            continue;
        }
        ++result.fileCount;
        result.totalBytes += size;
        if (size < 128ull * 1024) ++result.smallFileCount;
        if (IsCompressedLike(it->path())) {
            ++result.compressedLikeFileCount;
            result.compressedLikeBytes += size;
        }
        if (progress && (result.fileCount % 500 == 0)) {
            progress(L"files=" + std::to_wstring(result.fileCount) + L";bytes=" + std::to_wstring(result.totalBytes));
        }
    }
    return !cancel.load();
}

OptimizationPlan ChoosePlan(const SystemProfile& system, const FileProfile& files) {
    OptimizationPlan plan;
    const double smallShare = files.fileCount ? static_cast<double>(files.smallFileCount) / files.fileCount : 0.0;
    const double compressedShare = files.totalBytes ? static_cast<double>(files.compressedLikeBytes) / files.totalBytes : 0.0;
    const std::uint64_t average = files.fileCount ? files.totalBytes / files.fileCount : files.totalBytes;
    const std::uint64_t memoryGiB = system.physicalMemoryBytes / (1024ull * 1024 * 1024);

    plan.compressionLevel = (files.fileCount >= 10000 || average < 256ull * 1024 ||
                             files.totalBytes >= 100ull * 1024 * 1024 * 1024 || compressedShare >= 0.55) ? 1 : 3;
    unsigned memoryCap = memoryGiB < 4 ? 2 : (memoryGiB < 8 ? 4 : system.logicalProcessors);
    const unsigned responsivenessCap = system.logicalProcessors > 4 ? system.logicalProcessors - 2 : system.logicalProcessors;
    plan.threads = std::max(1u, std::min(memoryCap, responsivenessCap));
    plan.solid = true;
    plan.sortByType = system.driveKind == DriveKind::Ssd && smallShare >= 0.50;
    plan.useHybridMethods = files.compressedLikeFileCount > 0;
    plan.recommendedJobs = (system.driveKind == DriveKind::Ssd && system.logicalProcessors >= 8 &&
                            memoryGiB >= 8 && files.topLevelDirectoryCount >= 2) ? 2 : 1;

    std::wstringstream reason;
    reason << L"level=" << plan.compressionLevel << L";threads=" << plan.threads
           << L";small_share=" << std::fixed << std::setprecision(1) << smallShare * 100.0
           << L";compressed_share=" << compressedShare * 100.0
           << L";drive=" << DriveKindName(system.driveKind);
    plan.reason = reason.str();
    return plan;
}

std::wstring ValidateArchiveRequest(const ArchiveRequest& request) {
    if (!IsFile(request.sevenZipPath)) return L"sevenzip_missing";
    const DWORD inputAttributes = GetFileAttributesW(request.inputPath.c_str());
    if (inputAttributes == INVALID_FILE_ATTRIBUTES) return L"input_missing";
    if (request.outputPath.empty()) return L"output_missing";
    if ((inputAttributes & FILE_ATTRIBUTE_DIRECTORY) && SameOrChildPath(request.inputPath, request.outputPath))
        return L"output_inside_input";
    if (!(inputAttributes & FILE_ATTRIBUTE_DIRECTORY) && _wcsicmp(FullPath(request.inputPath).c_str(), FullPath(request.outputPath).c_str()) == 0)
        return L"output_equals_input";
    const fs::path output(request.outputPath);
    const fs::path parent = output.parent_path();
    std::error_code ec;
    if (!parent.empty() && !fs::is_directory(parent, ec)) return L"output_directory_missing";
    if (GetFileAttributesW(request.outputPath.c_str()) != INVALID_FILE_ATTRIBUTES) return L"output_exists";
    if (!request.volumeSize.empty() && request.volumeSize != L"none" &&
        GetFileAttributesW((request.outputPath + L".001").c_str()) != INVALID_FILE_ATTRIBUTES) return L"output_exists";
    if (request.encrypt && request.password.empty()) return L"password_empty";
    return {};
}

std::vector<std::wstring> BuildArguments(const ArchiveRequest& request) {
    std::vector<std::wstring> args = {
        L"a", L"-t7z", L"-y", L"-sccUTF-8", L"-bsp1", L"-bso1", L"-bse1",
        L"-mx=" + std::to_wstring(request.plan.compressionLevel), L"-m0=lzma2",
        L"-mmt=" + std::to_wstring(request.plan.threads), request.plan.solid ? L"-ms=on" : L"-ms=off"
    };
    if (request.plan.sortByType) args.push_back(L"-mqs");
    if (request.encrypt) {
        args.push_back(L"-p");
        if (request.encryptHeaders) args.push_back(L"-mhe=on");
    }
    if (!request.volumeSize.empty() && request.volumeSize != L"none") args.push_back(L"-v" + request.volumeSize);
    args.push_back(L"--");
    args.push_back(request.outputPath);
    args.push_back(request.inputPath);
    return args;
}

namespace {

struct HybridFileLists {
    fs::path workingDirectory;
    std::vector<std::wstring> compress;
    std::vector<std::wstring> store;
    std::uint64_t compressBytes = 0;
    std::uint64_t storeBytes = 0;
};

bool IsReparsePoint(const fs::path& path) {
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

bool AddRelativePath(const fs::path& path, const fs::path& workingDirectory,
                     std::vector<std::wstring>& destination) {
    fs::path relative = path.lexically_relative(workingDirectory);
    if (relative.empty()) relative = path.filename();
    if (relative.empty() || relative == L".") return false;
    destination.push_back(relative.wstring());
    return true;
}

bool CollectHybridFileLists(const std::wstring& inputPath, HybridFileLists& lists,
                            std::atomic_bool& cancel) {
    const fs::path root(FullPath(inputPath));
    std::error_code ec;
    if (root.empty() || !fs::exists(root, ec)) return false;
    if (fs::is_regular_file(root, ec)) {
        lists.workingDirectory = root.parent_path();
        const auto size = fs::file_size(root, ec);
        if (ec) return false;
        auto& destination = IsCompressedLike(root) ? lists.store : lists.compress;
        if (!AddRelativePath(root, lists.workingDirectory, destination)) return false;
        if (IsCompressedLike(root)) lists.storeBytes = size;
        else lists.compressBytes = size;
        return true;
    }
    if (!fs::is_directory(root, ec)) return false;

    lists.workingDirectory = root.parent_path();
    if (lists.workingDirectory.empty() || lists.workingDirectory == root)
        lists.workingDirectory = root;

    const auto options = fs::directory_options::skip_permission_denied;
    fs::recursive_directory_iterator it(root, options, ec), end;
    bool foundEntry = false;
    for (; it != end && !cancel.load(); it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        const fs::path path = it->path();
        if (IsReparsePoint(path)) {
            if (it->is_directory(ec)) it.disable_recursion_pending();
            ec.clear();
            continue;
        }
        const auto status = it->status(ec);
        if (ec) { ec.clear(); continue; }
        if (fs::is_directory(status)) {
            if (fs::is_empty(path, ec) && !ec) {
                foundEntry |= AddRelativePath(path, lists.workingDirectory, lists.compress);
            }
            ec.clear();
            continue;
        }
        if (!fs::is_regular_file(status)) continue;
        const auto size = fs::file_size(path, ec);
        if (ec) { ec.clear(); continue; }
        if (IsCompressedLike(path)) {
            foundEntry |= AddRelativePath(path, lists.workingDirectory, lists.store);
            lists.storeBytes += size;
        } else {
            foundEntry |= AddRelativePath(path, lists.workingDirectory, lists.compress);
            lists.compressBytes += size;
        }
    }
    if (cancel.load()) return false;
    if (!foundEntry && lists.workingDirectory != root)
        foundEntry = AddRelativePath(root, lists.workingDirectory, lists.compress);
    return foundEntry;
}

struct TempListFile {
    std::wstring path;
    ~TempListFile() { if (!path.empty()) DeleteFileW(path.c_str()); }
};

bool WriteUtf8List(const std::vector<std::wstring>& entries, TempListFile& file) {
    wchar_t tempDirectory[MAX_PATH * 4]{};
    if (!GetTempPathW(static_cast<DWORD>(std::size(tempDirectory)), tempDirectory)) return false;
    wchar_t tempPath[MAX_PATH * 4]{};
    if (!GetTempFileNameW(tempDirectory, L"Q7Z", 0, tempPath)) return false;
    file.path = tempPath;
    HANDLE handle = CreateFileW(file.path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;
    bool ok = true;
    for (const auto& entry : entries) {
        const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, entry.data(),
                                              static_cast<int>(entry.size()), nullptr, 0, nullptr, nullptr);
        if (count <= 0) { ok = false; break; }
        std::string line(static_cast<std::size_t>(count), '\0');
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, entry.data(), static_cast<int>(entry.size()),
                            line.data(), count, nullptr, nullptr);
        line += "\r\n";
        DWORD offset = 0;
        while (offset < line.size()) {
            DWORD written = 0;
            if (!WriteFile(handle, line.data() + offset,
                           static_cast<DWORD>(line.size() - offset), &written, nullptr) || written == 0) {
                ok = false;
                break;
            }
            offset += written;
        }
        if (!ok) break;
    }
    CloseHandle(handle);
    return ok;
}

std::vector<std::wstring> BuildPhaseArguments(const ArchiveRequest& request,
                                              const std::wstring& listPath,
                                              bool copyMethod, bool includeVolumes) {
    std::vector<std::wstring> args = {
        L"a", L"-t7z", L"-y", L"-sccUTF-8", L"-scsUTF-8", L"-bsp1", L"-bso1", L"-bse1"
    };
    if (copyMethod) {
        args.insert(args.end(), {L"-mx=0", L"-m0=Copy", L"-ms=off"});
    } else {
        args.push_back(L"-mx=" + std::to_wstring(request.plan.compressionLevel));
        args.push_back(L"-m0=lzma2");
        args.push_back(L"-mmt=" + std::to_wstring(request.plan.threads));
        args.push_back(request.plan.solid ? L"-ms=on" : L"-ms=off");
        if (request.plan.sortByType) args.push_back(L"-mqs");
    }
    if (request.encrypt) {
        args.push_back(L"-p");
        if (request.encryptHeaders) args.push_back(L"-mhe=on");
    }
    if (includeVolumes && !request.volumeSize.empty() && request.volumeSize != L"none")
        args.push_back(L"-v" + request.volumeSize);
    args.push_back(FullPath(request.outputPath));
    args.push_back(L"@" + listPath);
    return args;
}

std::wstring ScalePercents(const std::wstring& text, unsigned base, unsigned span) {
    std::wstring result;
    result.reserve(text.size() + 16);
    for (std::size_t i = 0; i < text.size();) {
        if (std::iswdigit(text[i])) {
            std::size_t end = i;
            while (end < text.size() && std::iswdigit(text[end])) ++end;
            if (end < text.size() && text[end] == L'%' && end - i <= 3) {
                const unsigned value = static_cast<unsigned>(std::stoul(text.substr(i, end - i)));
                if (value <= 100) {
                    result += std::to_wstring(std::min(100u, base + value * span / 100));
                    result.push_back(L'%');
                    i = end + 1;
                    continue;
                }
            }
        }
        result.push_back(text[i++]);
    }
    return result;
}

int RunSevenZipProcess(const ArchiveRequest& request, const std::vector<std::wstring>& args,
                       const fs::path& workingDirectory, std::atomic_bool& cancel,
                       const ProgressCallback& output, unsigned progressBase, unsigned progressSpan) {
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE stdoutRead = nullptr, stdoutWrite = nullptr, stdinRead = nullptr, stdinWrite = nullptr;
    if (!CreatePipe(&stdoutRead, &stdoutWrite, &security, 0)) return -1;
    if (!SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0) ||
        !CreatePipe(&stdinRead, &stdinWrite, &security, 0) ||
        !SetHandleInformation(stdinWrite, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(stdoutRead); CloseHandle(stdoutWrite);
        return -1;
    }

    std::wstring command = QuoteWindowsArgument(request.sevenZipPath);
    for (const auto& arg : args) command += L" " + QuoteWindowsArgument(arg);
    std::vector<wchar_t> mutableCommand(command.begin(), command.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    startup.hStdOutput = stdoutWrite;
    startup.hStdError = stdoutWrite;
    startup.hStdInput = stdinRead;
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(request.sevenZipPath.c_str(), mutableCommand.data(), nullptr, nullptr,
                                        TRUE, CREATE_NO_WINDOW, nullptr,
                                        workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
                                        &startup, &process);
    CloseHandle(stdoutWrite);
    CloseHandle(stdinRead);
    if (!created) {
        CloseHandle(stdoutRead); CloseHandle(stdinWrite);
        return -2;
    }

    if (request.encrypt) {
        int byteCount = WideCharToMultiByte(CP_UTF8, 0, request.password.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string utf8(static_cast<std::size_t>(std::max(byteCount, 0)), '\0');
        if (byteCount > 1) {
            WideCharToMultiByte(CP_UTF8, 0, request.password.c_str(), -1, utf8.data(), byteCount, nullptr, nullptr);
            utf8.resize(static_cast<std::size_t>(byteCount - 1));
        }
        utf8 += "\r\n";
        utf8 += utf8;
        DWORD written = 0;
        WriteFile(stdinWrite, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
        SecureZeroMemory(utf8.data(), utf8.size());
    }
    CloseHandle(stdinWrite);

    std::thread reader([&] {
        char buffer[4096];
        DWORD read = 0;
        while (ReadFile(stdoutRead, buffer, sizeof(buffer), &read, nullptr) && read) {
            if (output) output(ScalePercents(Utf8ToWide(std::string(buffer, buffer + read)),
                                             progressBase, progressSpan));
        }
    });

    while (WaitForSingleObject(process.hProcess, 100) == WAIT_TIMEOUT) {
        if (cancel.load()) {
            TerminateProcess(process.hProcess, ERROR_CANCELLED);
            break;
        }
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (reader.joinable()) reader.join();
    CloseHandle(stdoutRead);
    return static_cast<int>(exitCode);
}

} // namespace

int RunArchive(const ArchiveRequest& request, std::atomic_bool& cancel, const ProgressCallback& output) {
    if (!ValidateArchiveRequest(request).empty()) return -3;
    HybridFileLists lists;
    if (!CollectHybridFileLists(request.inputPath, lists, cancel)) return cancel.load() ? ERROR_CANCELLED : -4;

    const bool hasCompress = !lists.compress.empty();
    const bool hasStore = !lists.store.empty();
    const bool split = !request.volumeSize.empty() && request.volumeSize != L"none";

    if (hasCompress && hasStore && split) {
        std::vector<std::wstring> combined = lists.compress;
        combined.insert(combined.end(), lists.store.begin(), lists.store.end());
        TempListFile allList;
        if (!WriteUtf8List(combined, allList)) return -5;
        if (output) output(L"[Quick7Zip] Split output: using one LZMA2 pass; hybrid archive updates are not supported for volumes.\n");
        return RunSevenZipProcess(request, BuildPhaseArguments(request, allList.path, false, true),
                                  lists.workingDirectory, cancel, output, 0, 100);
    }

    unsigned compressSpan = 100;
    const std::uint64_t totalBytes = lists.compressBytes + lists.storeBytes;
    if (hasCompress && hasStore && totalBytes)
        compressSpan = std::clamp<unsigned>(static_cast<unsigned>(lists.compressBytes * 100 / totalBytes), 1, 99);

    if (hasCompress) {
        TempListFile compressList;
        if (!WriteUtf8List(lists.compress, compressList)) return -5;
        if (output && hasStore) output(L"[Quick7Zip] Phase 1/2: compressible files -> LZMA2\n");
        const int result = RunSevenZipProcess(
            request, BuildPhaseArguments(request, compressList.path, false, !hasStore),
            lists.workingDirectory, cancel, output, 0, hasStore ? compressSpan : 100);
        if (result != 0 || cancel.load()) return result;
    }
    if (hasStore) {
        TempListFile storeList;
        if (!WriteUtf8List(lists.store, storeList)) return -5;
        if (output && hasCompress) output(L"[Quick7Zip] Phase 2/2: already-compressed files -> Copy\n");
        return RunSevenZipProcess(
            request, BuildPhaseArguments(request, storeList.path, true, true),
            lists.workingDirectory, cancel, output, hasCompress ? compressSpan : 0,
            hasCompress ? 100 - compressSpan : 100);
    }
    return 0;
}

std::wstring FormatBytes(std::uint64_t bytes) {
    static const wchar_t* units[] = {L"B", L"KiB", L"MiB", L"GiB", L"TiB", L"PiB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 5) { value /= 1024.0; ++unit; }
    std::wstringstream ss;
    ss << std::fixed << std::setprecision(unit == 0 ? 0 : 1) << value << L' ' << units[unit];
    return ss.str();
}

const wchar_t* DriveKindName(DriveKind kind) {
    switch (kind) {
        case DriveKind::Hdd: return L"HDD";
        case DriveKind::Ssd: return L"SSD";
        case DriveKind::Network: return L"Network";
        case DriveKind::Removable: return L"Removable";
        default: return L"Unknown";
    }
}

} // namespace quick7zip
