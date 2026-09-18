import os
import subprocess
import sys

root = os.path.dirname(os.path.dirname(__file__))
sys.path.insert(0, os.path.join(root, "scripts"))
import build


def main():
    compiler = build.find_compiler()
    if not compiler:
        raise RuntimeError("MinGW-w64 compiler not found")
    output = os.path.join(root, "tests", "engine_tests.exe")
    command = [
        compiler, "-O2", "-std=c++17", "-static",
        os.path.join(root, "src", "engine", "engine.cpp"),
        os.path.join(root, "tests", "engine_tests.cpp"),
        "-o", output, "-lkernel32", "-ladvapi32", "-lversion", "-lshlwapi",
    ]
    subprocess.run(command, check=True)
    subprocess.run([output], check=True)
    print("Quick7Zip engine tests passed")


if __name__ == "__main__":
    main()
