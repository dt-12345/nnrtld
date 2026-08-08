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

def build_rtld(clean: bool, test: bool):
    build_path: str = "build_test" if test else "build"
    build_type: str = "-DCMAKE_BUILD_TYPE=RelWithDebugInfo" if test else "-DCMAKE_BUILD_TYPE=Release"
    if clean and os.path.exists(build_path):
        shutil.rmtree(build_path)
    if not os.path.exists(build_path):
        subprocess.run([
            "cmake", "-B", build_path, "-S", ".", "-G", "Ninja", build_type, "--toolchain=cmake/toolchain.cmake"
        ]).check_returncode()
    subprocess.run([
        "ninja", "-C", build_path
    ]).check_returncode()

if __name__ == "__main__":
    import sys

    clean: bool = "--clean" in sys.argv
    testing: bool = "--test" in sys.argv

    build_tools()
    build_rtld(clean, testing)