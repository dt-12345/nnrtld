#!/usr/bin/env python3

import os
import shutil
import subprocess

def build_tools():
    if os.path.exists("tools/nso-tools/build"):
        return
    try:
        subprocess.run([
            "cmake", "-B", "tools/nso-tools/build", "-S", "tools/nso-tools", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"
        ]).check_returncode()
    except subprocess.CalledProcessError:
        shutil.rmtree("tools/nso-tools/build")
        raise
    subprocess.run([
        "ninja", "-C", "tools/nso-tools/build"
    ]).check_returncode()
    shutil.copy2("tools/nso-tools/build/elf2nso", "tools/elf2nso")

def build_rtld(clean: bool):
    if clean:
        shutil.rmtree("build")
    if not os.path.exists("build"):
        subprocess.run([
            "cmake", "-B", "build", "-S", ".", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release", "--toolchain=cmake/toolchain.cmake"
        ]).check_returncode()
    subprocess.run([
        "ninja", "-C", "build"
    ]).check_returncode()

if __name__ == "__main__":
    import sys

    clean: bool = False
    if len(sys.argv) > 1:
        clean = sys.argv[1].lower() == "clean"

    build_tools()
    build_rtld(clean)