import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))
import build_native


def main():
    root = os.path.dirname(os.path.dirname(__file__))
    compiler = build_native.find_compiler()
    if not compiler:
        raise RuntimeError("MinGW-w64 compiler not found")
    output = os.path.join(root, "tests", "engine_tests.exe")
    command = [
        compiler, "-O2", "-std=c++17", "-static",
        os.path.join(root, "core", "native", "engine.cpp"),
        os.path.join(root, "tests", "engine_tests.cpp"),
        "-o", output, "-lkernel32", "-ladvapi32", "-lversion", "-lshlwapi",
    ]
    subprocess.run(command, check=True)
    subprocess.run([output], check=True)


if __name__ == "__main__":
    main()
