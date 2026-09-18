import glob
import os
import shutil
import subprocess
import sys


APP_NAME = "Quick7Zip"


def find_compiler():
    for name in ("g++", "clang++"):
        found = shutil.which(name)
        if found:
            return found
    local = os.environ.get("LOCALAPPDATA", "")
    pattern = os.path.join(local, "Microsoft", "WinGet", "Packages",
                           "BrechtSanders.WinLibs.MCF.UCRT_*", "mingw64", "bin", "g++.exe")
    candidates = glob.glob(pattern)
    if candidates:
        return candidates[0]
    for candidate in (r"C:\msys64\ucrt64\bin\g++.exe", r"C:\msys64\mingw64\bin\g++.exe"):
        if os.path.isfile(candidate):
            return candidate
    return None


def run(command, cwd=None):
    print("\n>", " ".join(command))
    result = subprocess.run(command, cwd=cwd, text=True, capture_output=True)
    if result.stdout:
        print(result.stdout)
    if result.returncode:
        print(result.stderr)
        raise RuntimeError(f"Command failed with exit code {result.returncode}")


def build():
    root = os.path.dirname(__file__)
    native = os.path.join(root, "core", "native")
    binary = os.path.join(root, "dist", "binary")
    os.makedirs(binary, exist_ok=True)
    compiler = find_compiler()
    if not compiler:
        raise RuntimeError("MinGW-w64 g++ was not found")
    compiler_dir = os.path.dirname(compiler)
    windres = next((path for path in (
        os.path.join(compiler_dir, "windres.exe"),
        os.path.join(compiler_dir, "llvm-windres.exe"),
    ) if os.path.isfile(path)), None)
    if not windres:
        raise RuntimeError("windres was not found next to the compiler")
    include = os.environ.get("WEBVIEW2_INCLUDE", r"C:\tools\webview2\build\native\include")
    if not os.path.isfile(os.path.join(include, "WebView2.h")):
        raise RuntimeError(f"WebView2 SDK headers were not found: {include}")

    import bundle_html
    bundle_html.bundle(binary)

    resource = os.path.join(native, "Quick7Zip_res.o")
    run([windres, "Quick7Zip.rc", "-O", "coff", "-o", resource], cwd=native)

    common = [compiler, "-O3", "-std=c++17", "-static", "-municode"]
    engine = os.path.join(native, "engine.cpp")
    libraries = ["-lkernel32", "-ladvapi32", "-lversion", "-lshlwapi"]

    run(common + [engine, os.path.join(native, "main_cli.cpp"), resource,
         "-o", os.path.join(binary, f"{APP_NAME}_cli.exe")] + libraries)

    gui_libraries = libraries + ["-luser32", "-lgdi32", "-lole32", "-loleaut32", "-luuid", "-lshell32", "-lcomctl32"]
    run(common + ["-mwindows", f"-I{include}", engine, os.path.join(native, "webview_main.cpp"), resource,
         "-o", os.path.join(binary, f"{APP_NAME}.exe")] + gui_libraries)

    loader = os.environ.get("WEBVIEW2_LOADER") or os.path.join(os.path.dirname(include), "x64", "WebView2Loader.dll")
    if not os.path.isfile(loader):
        loader = r"C:\tools\webview2\build\native\x64\WebView2Loader.dll"
    if not os.path.isfile(loader):
        raise RuntimeError(f"WebView2Loader.dll was not found: {loader}")
    shutil.copy2(loader, os.path.join(binary, "WebView2Loader.dll"))
    obsolete_dll = os.path.join(binary, "engine_x64.dll")
    if os.path.isfile(obsolete_dll):
        os.remove(obsolete_dll)
    print(f"\n[OK] Built Quick7Zip into {binary}")


if __name__ == "__main__":
    try:
        build()
    except Exception as exc:
        print(f"[ERROR] {exc}")
        sys.exit(1)
