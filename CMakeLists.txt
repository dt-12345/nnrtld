cmake_minimum_required(VERSION 3.13)

project(rtld C CXX ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXECUTABLE_SUFFIX ".nss")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-g -O3")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -O3")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-fcolor-diagnostics)
endif()

option(FOR_CHECK "Build for Tools/check" OFF)

set(MODULE_NAME nnrtld)
string(LENGTH ${MODULE_NAME} MODULE_NAME_LENGTH)

configure_file(${PROJECT_SOURCE_DIR}/Config/module.h.in Config/module.h @ONLY)

add_executable(${MODULE_NAME}
    Sources/ro/detail/detail_Init.cpp
    Sources/ro/detail/detail_ModuleHeader.cpp
    Sources/ro/detail/detail_RoModule.cpp
    Sources/ro/detail/detail_Rocrt.cpp
    Sources/ro/ro_Rocrt.aarch64.S
    Sources/ro/ro_Rtld.S
    Sources/ro/ro_UserExceptionHandler.S

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
)

if (FOR_CHECK)
    message("Building for check")
    target_compile_definitions(${MODULE_NAME} PRIVATE FOR_CHECK)
endif()

# TODO: test with -falign-loops=16
target_compile_options(${MODULE_NAME} PRIVATE
    -fno-rtti
    -fno-exceptions
    -ffunction-sections
    -fdata-sections
    -fvisibility=hidden
    -fno-builtin
)
target_compile_options(${MODULE_NAME} PRIVATE -Wall -Wextra -Werror)

target_link_options(${MODULE_NAME} PRIVATE -T${PROJECT_SOURCE_DIR}/LinkerScripts/aarch64.ld)
target_link_options(${MODULE_NAME} PRIVATE --shared)
target_link_options(${MODULE_NAME} PRIVATE -Wl,--build-id=sha1)
target_link_options(${MODULE_NAME} PRIVATE -Wl,--exclude-libs,ALL)
target_link_options(${MODULE_NAME} PRIVATE -Wl,--gc-sections)
target_link_options(${MODULE_NAME} PRIVATE -Wl,--hash-style=gnu)
target_link_options(${MODULE_NAME} PRIVATE -Wl,--no-export-dynamic)
target_link_options(${MODULE_NAME} PRIVATE -Wl,--soname=${MODULE_NAME}${CMAKE_EXECUTABLE_SUFFIX})
target_link_options(${MODULE_NAME} PRIVATE -Wl,-init=_init,-fini=_fini) # probably not strictly necessary
target_link_options(${MODULE_NAME} PRIVATE -Wl,-z,now)
target_link_options(${MODULE_NAME} PRIVATE -Wl,-z,pack-relative-relocs)

set_target_properties(${MODULE_NAME} PROPERTIES LINK_DEPENDS ${PROJECT_SOURCE_DIR}/LinkerScripts/aarch64.ld)

target_include_directories(${MODULE_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/ ${PROJECT_SOURCE_DIR}/Include/)

add_custom_command(TARGET ${MODULE_NAME} POST_BUILD
    COMMAND ${PROJECT_SOURCE_DIR}/Tools/elf2nso -o ${CMAKE_CURRENT_BINARY_DIR}/rtld ${CMAKE_CURRENT_BINARY_DIR}/${MODULE_NAME}${CMAKE_EXECUTABLE_SUFFIX}
)