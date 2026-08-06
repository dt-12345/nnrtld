cmake_minimum_required(VERSION 3.13)

project(rtld C CXX ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXECUTABLE_SUFFIX ".nss")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-fcolor-diagnostics)
endif()

set(MODULE_NAME nnrtld)
string(LENGTH ${MODULE_NAME} MODULE_NAME_LENGTH)

configure_file(${PROJECT_SOURCE_DIR}/Config/module.h.in Config/module.h @ONLY)

add_executable(${MODULE_NAME}
    Sources/ro/detail/detail_Init.cpp
    Sources/ro/detail/detail_ModuleHeader.cpp
    Sources/ro/detail/detail_RoModule.cpp
    Sources/ro/detail/detail_Rocrt.cpp
    Sources/ro/detail/detail_RuntimeResolver.S
    Sources/ro/detail/detail_UserExceptionHandler.S

    Sources/rocrt/detail/detail_Musl.cpp
    Sources/rocrt/detail/detail_Relro.cpp
    Sources/rocrt/rocrt_Nso.cpp

    Sources/svc/aarch64/lp64/aarch64-lp64_svc.S

    # the ordering on these is weird
    Sources/util/util_Memcpy.S
    Sources/util/util_Memset.S    # TODO: is this actually implemented in asm
    Sources/util/util_Strlen.S    # TODO: is this actually implemented in asm
    Sources/util/util_memcpy_aarch64.S
    Sources/util/util_Strcmp.S    # TODO: is this actually implemented in asm

    Sources/init/rocrt_Init.aarch64.S
    Sources/init/rtld_Init.aarch64.S
)

target_compile_options(${MODULE_NAME} PRIVATE
    -fno-rtti -fno-exceptions -fno-asynchronous-unwind-tables -fno-unwind-tables -ffunction-sections -fdata-sections -fvisibility=hidden
)
target_link_options(${MODULE_NAME} PRIVATE -Wl,--exclude-libs,ALL)
target_link_options(${MODULE_NAME} PRIVATE -Wl,--no-export-dynamic)
target_link_options(${MODULE_NAME} PRIVATE -Wl,--gc-sections)
target_link_options(${MODULE_NAME} PRIVATE -Wl,--shared)
target_link_options(${MODULE_NAME} PRIVATE -Wl,-z,now)
target_link_options(${MODULE_NAME} PRIVATE -Wl,-z,pack-relative-relocs)
target_link_options(${MODULE_NAME} PRIVATE -Wl,--hash-style=gnu)
target_link_options(${MODULE_NAME} PRIVATE -Wl,--build-id=sha1)
target_link_options(${MODULE_NAME} PRIVATE -T${PROJECT_SOURCE_DIR}/LinkerScripts/aarch64.ld)
target_link_options(${MODULE_NAME} PRIVATE -Wl,--soname=${MODULE_NAME}${CMAKE_EXECUTABLE_SUFFIX})
target_link_options(${MODULE_NAME} PRIVATE -Wl,-init=_init,-fini=_fini) # probably not strictly necessary

set_target_properties(${MODULE_NAME} PROPERTIES LINK_DEPENDS ${PROJECT_SOURCE_DIR}/LinkerScripts/aarch64.ld)

target_include_directories(${MODULE_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/ ${PROJECT_SOURCE_DIR}/Include/ ${PROJECT_SOURCE_DIR}/Libraries/aarch64/string/aarch64/)

add_custom_command(TARGET ${MODULE_NAME} POST_BUILD
    COMMAND ${PROJECT_SOURCE_DIR}/tools/elf2nso -o ${CMAKE_CURRENT_BINARY_DIR}/rtld ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}${CMAKE_EXECUTABLE_SUFFIX}
)