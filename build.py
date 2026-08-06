#!/usr/bin/env python3

import os
import shutil
import subprocess

def build_tools():
    if os.path.exists("Tools/elf2nso"):
        return
    if not os.path.exists("Tools/elf2nso/build"):
        try:
            subprocess.run([
                "cmake", "-B", "Tools/nso-Tools/build", "-S", "Tools/nso-tools", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"
            ]).check_returncode()
        except subprocess.CalledProcessError:
            shutil.rmtree("Tools/nso-Tools/build")
            raise
    subprocess.run([
        "ninja", "-C", "Tools/nso-Tools/build"
    ]).check_returncode()
    shutil.copy2("Tools/nso-Tools/build/elf2nso", "Tools/elf2nso")

def build_rtld(clean: bool):
    if clean and os.path.exists("build"):
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