#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <WebView2.h>

#include "engine.h"

#include <atomic>
#include <cwctype>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <fstream>

using namespace quick7zip;
namespace fs = std::filesystem;

namespace {

void LogDebug(const std::wstring& text) {
    PWSTR localAppData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &localAppData))) {
        fs::path dir = fs::path(localAppData) / L"Quick7Zip";
        std::error_code ec;
        fs::create_directories(dir, ec);
        fs::path logFile = dir / L"launch.log";
        std::wofstream out(logFile, std::ios::app);
        if (out.is_open()) {
            out << text << L"\n";
        }
        CoTaskMemFree(localAppData);
    }
}

constexpr UINT WM_Q7Z_JSON = WM_APP + 41;
constexpr int kBaseWindowWidth = 620;
constexpr int kBaseWindowHeight = 620;
HWND g_window = nullptr;
ICoreWebView2Controller* g_controller = nullptr;
ICoreWebView2* g_webview = nullptr;
std::atomic_bool g_cancel{false};
std::atomic_bool g_busy{false};
std::mutex g_profileMutex;
std::wstring g_analyzedPath;
SystemProfile g_system;
FileProfile g_files;

std::wstring LoadBundledHtml(HINSTANCE instance) {
    HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(201), RT_RCDATA);
    if (!resource) return {};
    HGLOBAL handle = LoadResource(instance, resource);
    const DWORD size = SizeofResource(instance, resource);
    const char* bytes = handle ? static_cast<const char*>(LockResource(handle)) : nullptr;
    if (!bytes || !size) return {};
    const int chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes, static_cast<int>(size), nullptr, 0);
    if (!chars) return {};
    std::wstring html(static_cast<size_t>(chars), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes, static_cast<int>(size), html.data(), chars);
    return html;
}
std::wstring g_initialPath;
bool g_openSettings = false;

std::wstring GetCurrentExecutablePath() {
    wchar_t path[MAX_PATH * 4]{};
    GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    return std::wstring(path);
}

bool IsContextMenuEnabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\Quick7Zip", 0, KEY_READ, &key) == ERROR_SUCCESS) {
        RegCloseKey(key);
        return true;
    }
    return false;
}

void SetContextMenuEnabled(bool enable) {
    const wchar_t* shellKeys[] = {
        L"Software\\Classes\\Directory\\shell\\Quick7Zip",
        L"Software\\Classes\\Directory\\Background\\shell\\Quick7Zip",
        L"Software\\Classes\\Drive\\shell\\Quick7Zip",
        L"Software\\Classes\\*\\shell\\Quick7Zip"
    };

    if (enable) {
        const std::wstring exePath = GetCurrentExecutablePath();
        if (exePath.empty()) return;
        const std::wstring iconVal = L"\"" + exePath + L"\",0";
        const std::wstring menuTitle = L"Quick7Zip で圧縮";

        for (const auto* subKeyPath : shellKeys) {
            HKEY key = nullptr;
            if (RegCreateKeyExW(HKEY_CURRENT_USER, subKeyPath, 0, nullptr, REG_OPTION_NON_VOLATILE,
                               KEY_SET_VALUE | KEY_CREATE_SUB_KEY, nullptr, &key, nullptr) == ERROR_SUCCESS) {
                RegSetValueExW(key, nullptr, 0, REG_SZ,
                               reinterpret_cast<const BYTE*>(menuTitle.c_str()),
                               static_cast<DWORD>((menuTitle.size() + 1) * sizeof(wchar_t)));
                RegSetValueExW(key, L"Icon", 0, REG_SZ,
                               reinterpret_cast<const BYTE*>(iconVal.c_str()),
                               static_cast<DWORD>((iconVal.size() + 1) * sizeof(wchar_t)));

                HKEY cmdKey = nullptr;
                if (RegCreateKeyExW(key, L"command", 0, nullptr, REG_OPTION_NON_VOLATILE,
                                   KEY_SET_VALUE, nullptr, &cmdKey, nullptr) == ERROR_SUCCESS) {
                    const bool isBg = (wcsstr(subKeyPath, L"Background") != nullptr);
                    const std::wstring cmd = L"\"" + exePath + (isBg ? L"\" \"%V\"" : L"\" \"%1\"");
                    RegSetValueExW(cmdKey, nullptr, 0, REG_SZ,
                                   reinterpret_cast<const BYTE*>(cmd.c_str()),
                                   static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
                    RegCloseKey(cmdKey);
                }
                RegCloseKey(key);
            }
        }
    } else {
        for (const auto* subKeyPath : shellKeys) {
            RegDeleteTreeW(HKEY_CURRENT_USER, subKeyPath);
        }
    }

    HKEY appKey = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Quick7Zip", 0, nullptr, REG_OPTION_NON_VOLATILE,
                       KEY_SET_VALUE, nullptr, &appKey, nullptr) == ERROR_SUCCESS) {
        const DWORD configured = 1;
        const DWORD enabledVal = enable ? 1 : 0;
        RegSetValueExW(appKey, L"ContextMenuConfigured", 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&configured), sizeof(configured));
        RegSetValueExW(appKey, L"ContextMenuEnabled", 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&enabledVal), sizeof(enabledVal));
        RegCloseKey(appKey);
    }
}

void EnsureInitialContextMenu() {
    HKEY appKey = nullptr;
    bool alreadyConfigured = false;
    bool isEnabled = true;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Quick7Zip", 0, KEY_READ, &appKey) == ERROR_SUCCESS) {
        DWORD configured = 0;
        DWORD size = sizeof(configured);
        DWORD type = REG_DWORD;
        if (RegQueryValueExW(appKey, L"ContextMenuConfigured", nullptr, &type,
                             reinterpret_cast<BYTE*>(&configured), &size) == ERROR_SUCCESS) {
            alreadyConfigured = (configured != 0);
        }
        DWORD enabledVal = 1;
        size = sizeof(enabledVal);
        if (RegQueryValueExW(appKey, L"ContextMenuEnabled", nullptr, &type,
                             reinterpret_cast<BYTE*>(&enabledVal), &size) == ERROR_SUCCESS) {
            isEnabled = (enabledVal != 0);
        }
        RegCloseKey(appKey);
    }
    if (!alreadyConfigured) {
        SetContextMenuEnabled(true);
    } else if (isEnabled) {
        SetContextMenuEnabled(true);
    }
}

OptimizationPlan g_plan;

std::wstring JsonEscape(const std::wstring& value) {
    std::wstringstream out;
    for (wchar_t c : value) {
        switch (c) {
            case L'\\': out << L"\\\\"; break;
            case L'\"': out << L"\\\""; break;
            case L'\r': out << L"\\r"; break;
            case L'\n': out << L"\\n"; break;
            case L'\t': out << L"\\t"; break;
            default:
                if (c < 0x20) {
                    out << L"\\u" << std::hex << std::setw(4) << std::setfill(L'0') << static_cast<int>(c);
                } else out << c;
        }
    }
    return out.str();
}

void QueueJson(std::wstring json) {
    if (!g_window) return;
    PostMessageW(g_window, WM_Q7Z_JSON, 0, reinterpret_cast<LPARAM>(new std::wstring(std::move(json))));
}

std::wstring JsonString(const std::wstring& json, const std::wstring& key) {
    const std::wstring marker = L"\"" + key + L"\"";
    std::size_t pos = json.find(marker);
    if (pos == std::wstring::npos) return {};
    pos = json.find(L':', pos + marker.size());
    if (pos == std::wstring::npos) return {};
    pos = json.find(L'\"', pos + 1);
    if (pos == std::wstring::npos) return {};
    ++pos;
    std::wstring result;
    while (pos < json.size()) {
        wchar_t c = json[pos++];
        if (c == L'\"') break;
        if (c != L'\\' || pos >= json.size()) { result.push_back(c); continue; }
        wchar_t escaped = json[pos++];
        switch (escaped) {
            case L'\"': result.push_back(L'\"'); break;
            case L'\\': result.push_back(L'\\'); break;
            case L'/': result.push_back(L'/'); break;
            case L'b': result.push_back(L'\b'); break;
            case L'f': result.push_back(L'\f'); break;
            case L'n': result.push_back(L'\n'); break;
            case L'r': result.push_back(L'\r'); break;
            case L't': result.push_back(L'\t'); break;
            case L'u': {
                if (pos + 4 <= json.size()) {
                    wchar_t hex[5] = {json[pos], json[pos + 1], json[pos + 2], json[pos + 3], 0};
                    result.push_back(static_cast<wchar_t>(wcstoul(hex, nullptr, 16)));
                    pos += 4;
                }
                break;
            }
            default: result.push_back(escaped);
        }
    }
    return result;
}

bool JsonBool(const std::wstring& json, const std::wstring& key, bool fallback = false) {
    const std::wstring marker = L"\"" + key + L"\"";
    std::size_t pos = json.find(marker);
    if (pos == std::wstring::npos) return fallback;
    pos = json.find(L':', pos + marker.size());
    if (pos == std::wstring::npos) return fallback;
    while (++pos < json.size() && iswspace(json[pos])) {}
    return json.compare(pos, 4, L"true") == 0;
}

double JsonNumber(const std::wstring& json, const std::wstring& key, double fallback = 0) {
    const std::wstring marker = L"\"" + key + L"\"";
    std::size_t pos = json.find(marker);
    if (pos == std::wstring::npos) return fallback;
    pos = json.find(L':', pos + marker.size());
    if (pos == std::wstring::npos) return fallback;
    ++pos;
    while (pos < json.size() && iswspace(json[pos])) ++pos;
    const std::size_t start = pos;
    if (pos < json.size() && (json[pos] == L'-' || json[pos] == L'+')) ++pos;
    while (pos < json.size() && (iswdigit(json[pos]) || json[pos] == L'.')) ++pos;
    if (pos == start) return fallback;
    try { return std::stod(json.substr(start, pos - start)); } catch (...) { return fallback; }
}

void ResizeToContentHeight(int clientHeight) {
    if (!g_window || clientHeight <= 0) return;
    RECT clientRect{};
    GetClientRect(g_window, &clientRect);
    const int clientWidth = clientRect.right - clientRect.left;
    const UINT dpi = GetDpiForWindow(g_window);
    const int scaledHeight = MulDiv(clientHeight, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
    RECT windowRect{0, 0, clientWidth, scaledHeight};
    const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(g_window, GWL_STYLE));
    const DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(g_window, GWL_EXSTYLE));
    AdjustWindowRectExForDpi(&windowRect, style, FALSE, exStyle, dpi);
    SetWindowPos(g_window, nullptr, 0, 0, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

std::wstring PickFolder() {
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dialog)))) return {};
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    std::wstring result;
    if (SUCCEEDED(dialog->Show(g_window))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                result = path;
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dialog->Release();
    return result;
}

std::wstring PickArchivePath() {
    IFileSaveDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dialog)))) return {};
    const COMDLG_FILTERSPEC filters[] = {{L"7-Zip archive (*.7z)", L"*.7z"}, {L"All files", L"*.*"}};
    dialog->SetFileTypes(2, filters);
    dialog->SetDefaultExtension(L"7z");
    dialog->SetFileName(L"archive.7z");
    std::wstring result;
    if (SUCCEEDED(dialog->Show(g_window))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                result = path;
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dialog->Release();
    return result;
}

std::wstring ProfileJson(const std::wstring& path, const SystemProfile& system,
                         const FileProfile& files, const OptimizationPlan& plan) {
    std::wstringstream json;
    json << L"{\"type\":\"analysis_complete\",\"path\":\"" << JsonEscape(path)
         << L"\",\"drive\":\"" << DriveKindName(system.driveKind)
         << L"\",\"driveRoot\":\"" << JsonEscape(system.driveRoot)
         << L"\",\"cpu\":" << system.logicalProcessors
         << L",\"memory\":" << system.physicalMemoryBytes
         << L",\"files\":" << files.fileCount
         << L",\"directories\":" << files.directoryCount
         << L",\"bytes\":" << files.totalBytes
         << L",\"smallFiles\":" << files.smallFileCount
         << L",\"compressedFiles\":" << files.compressedLikeFileCount
         << L",\"compressedBytes\":" << files.compressedLikeBytes
         << L",\"inaccessible\":" << files.inaccessibleCount
         << L",\"reparsePoints\":" << files.reparsePointCount
         << L",\"level\":" << plan.compressionLevel
         << L",\"threads\":" << plan.threads
         << L",\"solid\":" << (plan.solid ? L"true" : L"false")
         << L",\"sortByType\":" << (plan.sortByType ? L"true" : L"false")
         << L",\"hybrid\":" << (plan.useHybridMethods ? L"true" : L"false")
         << L",\"reason\":\"" << JsonEscape(plan.reason) << L"\"}";
    return json.str();
}

std::uint64_t ArchiveOutputBytes(const std::wstring& outputPath, const std::wstring& volumeSize) {
    std::error_code ec;
    if (volumeSize.empty() || volumeSize == L"none") {
        const auto size = fs::file_size(outputPath, ec);
        return ec ? 0 : size;
    }
    std::uint64_t total = 0;
    for (unsigned index = 1; index < 1000000; ++index) {
        std::wstringstream suffix;
        suffix << L'.' << std::setw(3) << std::setfill(L'0') << index;
        const fs::path part(outputPath + suffix.str());
        if (!fs::exists(part, ec)) break;
        const auto size = fs::file_size(part, ec);
        if (ec) return 0;
        total += size;
    }
    return total;
}

void BeginAnalysis(const std::wstring& path) {
    if (path.empty() || g_busy.exchange(true)) return;
    g_cancel.store(false);
    QueueJson(L"{\"type\":\"analysis_started\"}");
    std::thread([path] {
        FileProfile files;
        const auto system = DetectSystemProfile(path);
        const bool ok = AnalyzePath(path, files, g_cancel, [](const std::wstring& status) {
            QueueJson(L"{\"type\":\"analysis_progress\",\"status\":\"" + JsonEscape(status) + L"\"}");
        });
        if (ok) {
            const auto plan = ChoosePlan(system, files);
            {
                std::lock_guard<std::mutex> lock(g_profileMutex);
                g_analyzedPath = path; g_system = system; g_files = files; g_plan = plan;
            }
            QueueJson(ProfileJson(path, system, files, plan));
        } else {
            QueueJson(g_cancel.load() ? L"{\"type\":\"cancelled\"}" :
                                       L"{\"type\":\"error\",\"message\":\"analysis_failed\"}");
        }
        g_busy.store(false);
    }).detach();
}

void BeginArchive(const std::wstring& json) {
    if (g_busy.exchange(true)) return;
    ArchiveRequest request;
    request.inputPath = JsonString(json, L"input");
    request.outputPath = JsonString(json, L"output");
    request.encrypt = JsonBool(json, L"encrypt");
    request.encryptHeaders = JsonBool(json, L"encryptHeaders", true);
    request.password = JsonString(json, L"password");
    request.volumeSize = JsonString(json, L"split");
    std::uint64_t inputBytes = 0;
    {
        std::lock_guard<std::mutex> lock(g_profileMutex);
        if (request.inputPath != g_analyzedPath) {
            g_busy.store(false);
            QueueJson(L"{\"type\":\"error\",\"message\":\"reanalyze_required\"}");
            return;
        }
        request.plan = g_plan;
        inputBytes = g_files.totalBytes;
    }
    const auto sevenZip = FindSevenZip();
    if (!sevenZip.found || request.inputPath.empty() || request.outputPath.empty() ||
        (request.encrypt && request.password.empty())) {
        SecureZeroMemory(request.password.data(), request.password.size() * sizeof(wchar_t));
        g_busy.store(false);
        QueueJson(L"{\"type\":\"error\",\"message\":\"invalid_request\"}");
        return;
    }
    request.sevenZipPath = sevenZip.executable;
    const auto validationError = ValidateArchiveRequest(request);
    if (!validationError.empty()) {
        SecureZeroMemory(request.password.data(), request.password.size() * sizeof(wchar_t));
        g_busy.store(false);
        QueueJson(L"{\"type\":\"error\",\"message\":\"" + JsonEscape(validationError) + L"\"}");
        return;
    }
    g_cancel.store(false);
    QueueJson(L"{\"type\":\"archive_started\"}");
    std::thread([request = std::move(request), inputBytes]() mutable {
        const int exitCode = RunArchive(request, g_cancel, [](const std::wstring& output) {
            QueueJson(L"{\"type\":\"archive_output\",\"text\":\"" + JsonEscape(output) + L"\"}");
        });
        const std::uint64_t outputBytes = exitCode == 0 ? ArchiveOutputBytes(request.outputPath, request.volumeSize) : 0;
        SecureZeroMemory(request.password.data(), request.password.size() * sizeof(wchar_t));
        std::wstringstream result;
        result << L"{\"type\":\"archive_finished\",\"exitCode\":" << exitCode
               << L",\"cancelled\":" << (g_cancel.load() ? L"true" : L"false")
               << L",\"inputBytes\":" << inputBytes << L",\"outputBytes\":" << outputBytes << L"}";
        QueueJson(result.str());
        g_busy.store(false);
    }).detach();
}

void HandleWebMessage(const std::wstring& json) {
    LogDebug(L"HandleWebMessage: " + json);
    const std::wstring type = JsonString(json, L"type");
    if (type == L"initialize") {
        EnsureInitialContextMenu();
        const auto sevenZip = FindSevenZip();
        const bool contextMenu = IsContextMenuEnabled();
        std::wstringstream reply;
        reply << L"{\"type\":\"initialized\",\"found\":" << (sevenZip.found ? L"true" : L"false")
              << L",\"supported\":" << (IsSupportedSevenZipVersion(sevenZip.version) ? L"true" : L"false")
              << L",\"path\":\"" << JsonEscape(sevenZip.executable)
              << L"\",\"version\":\"" << JsonEscape(sevenZip.version)
              << L"\",\"contextMenu\":" << (contextMenu ? L"true" : L"false")
              << L",\"openSettings\":" << (g_openSettings ? L"true" : L"false")
              << L",\"initialPath\":\"" << JsonEscape(g_initialPath) << L"\"}";
        LogDebug(L"Reply to initialize: " + reply.str());
        QueueJson(reply.str());
        if (g_openSettings) {
            QueueJson(L"{\"type\":\"open_settings_modal\"}");
        }
    } else if (type == L"set_context_menu") {
        const bool enabled = JsonBool(json, L"enabled", true);
        SetContextMenuEnabled(enabled);
        std::wstringstream reply;
        reply << L"{\"type\":\"context_menu_updated\",\"enabled\":" << (IsContextMenuEnabled() ? L"true" : L"false") << L"}";
        QueueJson(reply.str());
    } else if (type == L"browse_input") {
        const auto path = PickFolder();
        if (!path.empty()) QueueJson(L"{\"type\":\"input_selected\",\"path\":\"" + JsonEscape(path) + L"\"}");
    } else if (type == L"browse_output") {
        const auto path = PickArchivePath();
        if (!path.empty()) QueueJson(L"{\"type\":\"output_selected\",\"path\":\"" + JsonEscape(path) + L"\"}");
    } else if (type == L"analyze") {
        BeginAnalysis(JsonString(json, L"path"));
    } else if (type == L"start_archive") {
        BeginArchive(json);
    } else if (type == L"cancel") {
        g_cancel.store(true);
    } else if (type == L"resize") {
        ResizeToContentHeight(static_cast<int>(JsonNumber(json, L"height", 0)));
    }
}

class WebMessageHandler final : public ICoreWebView2WebMessageReceivedEventHandler {
    std::atomic<ULONG> refs_{1};
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override {
        if (!object) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2WebMessageReceivedEventHandler) {
            *object = static_cast<ICoreWebView2WebMessageReceivedEventHandler*>(this); AddRef(); return S_OK;
        }
        *object = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override { const ULONG value = --refs_; if (!value) delete this; return value; }
    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) override {
        LPWSTR raw = nullptr;
        if (SUCCEEDED(args->get_WebMessageAsJson(&raw)) && raw) {
            HandleWebMessage(raw);
            CoTaskMemFree(raw);
        }
        return S_OK;
    }
};

class ControllerHandler final : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
    std::atomic<ULONG> refs_{1};
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override {
        if (!object) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler) {
            *object = static_cast<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*>(this); AddRef(); return S_OK;
        }
        *object = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override { const ULONG value = --refs_; if (!value) delete this; return value; }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Controller* controller) override {
        LogDebug(L"ControllerHandler Invoke entered with result: " + std::to_wstring(result));
        if (FAILED(result) || !controller) {
            LogDebug(L"ControllerHandler failed with result: " + std::to_wstring(result));
            return result;
        }
        g_controller = controller; g_controller->AddRef();
        controller->get_CoreWebView2(&g_webview);
        RECT bounds{}; GetClientRect(g_window, &bounds); controller->put_Bounds(bounds);
        ICoreWebView2Settings* settings = nullptr;
        if (SUCCEEDED(g_webview->get_Settings(&settings)) && settings) {
            settings->put_AreDefaultContextMenusEnabled(FALSE);
            settings->put_AreDevToolsEnabled(TRUE);
            settings->put_IsStatusBarEnabled(FALSE);
            settings->Release();
        }
        EventRegistrationToken token{};
        g_webview->add_WebMessageReceived(new WebMessageHandler(), &token);
        const std::wstring html = LoadBundledHtml(GetModuleHandleW(nullptr));
        if (!html.empty()) g_webview->NavigateToString(html.c_str());
        else MessageBoxW(g_window, L"Bundled UI resource could not be loaded.", L"Quick7Zip", MB_ICONERROR);
        return S_OK;
    }
};

class EnvironmentHandler final : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
    std::atomic<ULONG> refs_{1};
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override {
        if (!object) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler) {
            *object = static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*>(this); AddRef(); return S_OK;
        }
        *object = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override { const ULONG value = --refs_; if (!value) delete this; return value; }
    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, ICoreWebView2Environment* environment) override {
        LogDebug(L"EnvironmentHandler Invoke called with result: " + std::to_wstring(result));
        if (FAILED(result) || !environment) return result;
        HRESULT hr = environment->CreateCoreWebView2Controller(g_window, new ControllerHandler());
        LogDebug(L"CreateCoreWebView2Controller returned hr: " + std::to_wstring(hr));
        return hr;
    }
};

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_SIZE:
            if (g_controller) { RECT bounds{}; GetClientRect(window, &bounds); g_controller->put_Bounds(bounds); }
            return 0;
        case WM_DPICHANGED: {
            const RECT* suggested = reinterpret_cast<RECT*>(lParam);
            if (suggested) {
                SetWindowPos(window, nullptr, suggested->left, suggested->top,
                             suggested->right - suggested->left, suggested->bottom - suggested->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
            return 0;
        }
        case WM_Q7Z_JSON: {
            auto* json = reinterpret_cast<std::wstring*>(lParam);
            if (g_webview && json) g_webview->PostWebMessageAsJson(json->c_str());
            delete json;
            return 0;
        }
        case WM_CLOSE:
            if (g_busy.load()) {
                if (MessageBoxW(window, L"A task is running. Cancel and exit?", L"Quick7Zip",
                                MB_YESNO | MB_ICONWARNING) != IDYES) return 0;
                g_cancel.store(true);
                return 0;
            }
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            g_window = nullptr;
            if (g_webview) { g_webview->Release(); g_webview = nullptr; }
            if (g_controller) { g_controller->Release(); g_controller = nullptr; }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    LogDebug(L"--- wWinMain Started ---");
    LogDebug(L"CommandLine: " + std::wstring(GetCommandLineW()));
    SetProcessDPIAware();
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        if (argc > 1 && argv[1] && argv[1][0] != L'\0') {
            g_initialPath = argv[1];
            std::wstring rawPath = argv[1];
            while (!rawPath.empty() && (rawPath.back() == L'\"' || rawPath.back() == L' ' || rawPath.back() == L'\t')) {
                rawPath.pop_back();
        for (int i = 1; i < argc; ++i) {
            if (!argv[i]) continue;
            if (_wcsicmp(argv[i], L"--settings") == 0 || _wcsicmp(argv[i], L"/settings") == 0) {
                g_openSettings = true;
            } else if (g_initialPath.empty() && argv[i][0] != L'\0') {
                std::wstring rawPath = argv[i];
                while (!rawPath.empty() && (rawPath.back() == L'\"' || rawPath.back() == L' ' || rawPath.back() == L'\t')) {
                    rawPath.pop_back();
                }
                if (rawPath.size() > 3 && (rawPath.back() == L'\\' || rawPath.back() == L'/')) {
                    rawPath.pop_back();
                }
                g_initialPath = rawPath;
            }
            if (rawPath.size() > 3 && (rawPath.back() == L'\\' || rawPath.back() == L'/')) {
                rawPath.pop_back();
            }
            g_initialPath = rawPath;
        }
        LocalFree(argv);
    }
    LogDebug(L"g_initialPath: " + g_initialPath);
    EnsureInitialContextMenu();
    SetCurrentProcessExplicitAppUserModelID(L"maktak-105.Quick7Zip");
    const wchar_t className[] = L"Quick7ZipWindow";
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_ICON,
                                             GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
    wc.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_ICON,
                                               GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = className;
    RegisterClassExW(&wc);
    g_window = CreateWindowExW(0, className, L"Quick7Zip", WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, kBaseWindowWidth, kBaseWindowHeight,
                               nullptr, nullptr, instance, nullptr);
    if (!g_window) { CoUninitialize(); return 2; }
    if (!g_window) {
        LogDebug(L"CreateWindowExW failed");
        CoUninitialize();
        return 2;
    }
    const UINT windowDpi = GetDpiForWindow(g_window);
    if (windowDpi != USER_DEFAULT_SCREEN_DPI) {
        SetWindowPos(g_window, nullptr, 0, 0,
                     MulDiv(kBaseWindowWidth, windowDpi, USER_DEFAULT_SCREEN_DPI),
                     MulDiv(kBaseWindowHeight, windowDpi, USER_DEFAULT_SCREEN_DPI),
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    SendMessageW(g_window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(wc.hIcon));
    SendMessageW(g_window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(wc.hIconSm));
    ShowWindow(g_window, show);
    UpdateWindow(g_window);

    const fs::path loaderPath = fs::path([] {
        wchar_t path[MAX_PATH * 4]{}; GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path))); return std::wstring(path);
    }()).parent_path() / L"WebView2Loader.dll";
    HMODULE loader = LoadLibraryW(loaderPath.c_str());
    if (!loader) {
        LogDebug(L"WebView2Loader.dll not found at: " + loaderPath.wstring());
        MessageBoxW(g_window, L"WebView2Loader.dll was not found.", L"Quick7Zip", MB_ICONERROR);
        DestroyWindow(g_window);
    } else {
        using CreateEnvironment = HRESULT(STDMETHODCALLTYPE*)(PCWSTR, PCWSTR,
            ICoreWebView2EnvironmentOptions*, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);
        auto create = reinterpret_cast<CreateEnvironment>(GetProcAddress(loader, "CreateCoreWebView2EnvironmentWithOptions"));
        PWSTR localAppDataRaw = nullptr;
        std::wstring userDataFolder;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &localAppDataRaw))) {
            userDataFolder = (fs::path(localAppDataRaw) / L"Quick7Zip" / L"WebView2").wstring();
            CoTaskMemFree(localAppDataRaw);
            std::error_code directoryError;
            fs::create_directories(userDataFolder, directoryError);
        }
        if (!create || FAILED(create(nullptr, userDataFolder.empty() ? nullptr : userDataFolder.c_str(), nullptr, new EnvironmentHandler()))) {
            LogDebug(L"CreateCoreWebView2EnvironmentWithOptions failed");
            MessageBoxW(g_window, L"Microsoft Edge WebView2 Runtime could not be initialized.", L"Quick7Zip", MB_ICONERROR);
            DestroyWindow(g_window);
        } else {
            LogDebug(L"CreateCoreWebView2EnvironmentWithOptions called successfully");
        }
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (loader) FreeLibrary(loader);
    CoUninitialize();
    return static_cast<int>(message.wParam);
}
