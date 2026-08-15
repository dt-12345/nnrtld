#!/usr/bin/env python3

from common.setup_venv import enter_venv
if __name__ == "__main__":
    enter_venv()

from common import setup_common as setup
from pathlib import Path
import argparse
import hashlib
import os
import shutil
import subprocess

CONFIG_SITE: str = \
"""//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef _LIBCPP_CONFIG_SITE
#define _LIBCPP_CONFIG_SITE

/* #undef _LIBCPP_ABI_VERSION */
/* #undef _LIBCPP_ABI_UNSTABLE */
/* #undef _LIBCPP_ABI_FORCE_ITANIUM */
/* #undef _LIBCPP_ABI_FORCE_MICROSOFT */
/* #undef _LIBCPP_HIDE_FROM_ABI_PER_TU_BY_DEFAULT */
/* #undef _LIBCPP_HAS_NO_THREADS */
/* #undef _LIBCPP_HAS_NO_MONOTONIC_CLOCK */
/* #undef _LIBCPP_HAS_MUSL_LIBC */
/* #undef _LIBCPP_HAS_THREAD_API_PTHREAD */
/* #undef _LIBCPP_HAS_THREAD_API_EXTERNAL */
/* #undef _LIBCPP_HAS_THREAD_API_WIN32 */
/* #undef _LIBCPP_HAS_THREAD_LIBRARY_EXTERNAL */
/* #undef _LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS */
#define _LIBCPP_HAS_NO_VENDOR_AVAILABILITY_ANNOTATIONS
/* #undef _LIBCPP_NO_VCRUNTIME */
/* #undef _LIBCPP_TYPEINFO_COMPARISON_IMPLEMENTATION */
/* #undef _LIBCPP_ABI_NAMESPACE */
/* #undef _LIBCPP_HAS_NO_FILESYSTEM_LIBRARY */
/* #undef _LIBCPP_HAS_PARALLEL_ALGORITHMS */
/* #undef _LIBCPP_HAS_NO_RANDOM_DEVICE */
/* #undef _LIBCPP_HAS_NO_LOCALIZATION */
/* #undef _LIBCPP_HAS_NO_WIDE_CHARACTERS */
#define _LIBCPP_HAS_NO_INCOMPLETE_FORMAT
#define _LIBCPP_HAS_NO_INCOMPLETE_RANGES




#endif // _LIBCPP_CONFIG_SITE
"""

# some builds of clang don't include a __config_site for our target
def fix_config_site(version: str) -> None:
    config_path: Path = setup.ROOT / Path(f"Toolchain/clang-{version}/include/c++/v1/__config_site")
    if config_path.exists():
        return
    
    config_path.write_text(CONFIG_SITE)

def extract_major_ver(version: str) -> str:
    dot_index = version.find(".")
    if dot_index == -1:
        return version
    return version[:dot_index]

def clean_compiler_dir(version) -> None:
    compiler_dir: Path =  setup.ROOT / "Toolchain" / f"clang-{version}"
    if not compiler_dir.exists():
        return
    
    bin_dir: Path = compiler_dir / "bin"
    if bin_dir.exists():
        temp_bin_dir: Path = compiler_dir / "bin_"
        temp_bin_dir.mkdir(exist_ok=True)

        clang_bin_name: str = f"clang-{extract_major_ver(version)}"
        clang_path: Path = bin_dir / clang_bin_name
        if clang_path.exists():
            try:
                clang_path.rename(temp_bin_dir / clang_bin_name)
            except FileExistsError:
                pass
        clang_c_path: Path = temp_bin_dir / "clang"
        if not clang_c_path.exists() and not clang_c_path.is_symlink():
            clang_c_path.symlink_to(clang_path)
        clang_cxx_path: Path = temp_bin_dir / "clang++"
        if not clang_cxx_path.exists() and not clang_cxx_path.is_symlink():
            clang_cxx_path.symlink_to(clang_path)
        shutil.rmtree(bin_dir)
        temp_bin_dir.rename(bin_dir)
    
    libexec_dir: Path = compiler_dir / "libexec"
    if libexec_dir.exists():
        shutil.rmtree(libexec_dir)
    
    share_dir: Path = compiler_dir / "share"
    if share_dir.exists():
        shutil.rmtree(share_dir)

def build_tools() -> None:
    elf2nso: Path = setup.ROOT / "Tools" / "elf2nso"
    nso2elf: Path = setup.ROOT / "Tools" / "nso2elf"

    if elf2nso.is_file() and nso2elf.is_file():
        return
    
    tool_dir: Path = setup.ROOT / "Tools" / "nso-tools"
    build_path: Path = tool_dir / "build"
    if not build_path.is_dir():
        try:
            subprocess.check_call([
                "cmake", "-B", build_path, "-S", tool_dir, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"
            ])
        except subprocess.CalledProcessError:
            shutil.rmtree(build_path)
            raise
    subprocess.check_call(["ninja", "-C", build_path])
    shutil.copy2(build_path / "elf2nso", elf2nso)
    shutil.copy2(build_path / "nso2elf", nso2elf)

# TODO: other version support?
# maybe we mask out the build id and compare those? idk
def check_nso_hash(path: str) -> bool:
    EXPECTED: str = "e01d34bb149c30ad27c0872604e795a4a22e35fedc0440f78e32b610a94ecad6"
    return hashlib.sha256(Path(path).read_bytes()).hexdigest() == EXPECTED

def setup_rtld_elf(rtld_path: str, override: bool) -> None:
    nso2elf: Path = setup.ROOT / "Tools" / "nso2elf"
    elf_path: Path = setup.ROOT / "data" / "rtld.nss"

    if elf_path.is_file() and not override:
        return

    if not override and not rtld_path:
        raise ValueError("rtld.nss does not exist but no path provided for RTLD NSO")

    if not check_nso_hash(rtld_path):
        raise ValueError("RTLD binary is not correct - should be from an SDK version 21.4.0 game like Rhythm Heaven Groove compiled with -O3")

    try:
        subprocess.check_call([nso2elf, "-o", elf_path, rtld_path])
    except subprocess.CalledProcessError:
        os.unlink(elf_path)
        raise

def build_rtld(clean: bool, for_check: bool, compiler_version: str) -> None:
    build_path: Path = setup.ROOT / "build_check" if for_check else "build"
    build_type: str = "-DCMAKE_BUILD_TYPE=RelWithDebInfo" if for_check else "-DCMAKE_BUILD_TYPE=Release"
    if clean and build_path.is_dir():
        shutil.rmtree(build_path)

    if not build_path.is_dir():
        env = os.environ.copy()
        env["RTLD_CLANG"] = setup.ROOT / "Toolchain" / f"clang-{compiler_version}"

        defines = "-DFOR_CHECK=ON" if for_check else ""

        try:
            subprocess.check_call([
                "cmake", "-B", build_path, "-S", ".", "-G", "Ninja", build_type, "--toolchain=cmake/toolchain.cmake", defines,
            ], env=env)
        except subprocess.CalledProcessError:
            shutil.rmtree(build_path)
            raise

    subprocess.check_call(["ninja", "-C", build_path])

def main():
    parser = argparse.ArgumentParser("setup.py")
    parser.add_argument("--clean", help="Clean build directory", action="store_true", default=False)
    parser.add_argument("--nso-path", help="Path to original RTLD NSO", default="")
    parser.add_argument("--for-check", help="Build for Tools/check", action="store_true", default=False)
    args = parser.parse_args()

    compiler_version: str = "16.0.0"

    build_tools()
    setup.set_up_compiler(compiler_version)
    fix_config_site(compiler_version)
    clean_compiler_dir(compiler_version)

    if args.for_check:
        setup.install_viking()
        setup_rtld_elf(args.nso_path, False)

    build_rtld(args.clean, args.for_check, compiler_version)

if __name__ == "__main__":
    main()