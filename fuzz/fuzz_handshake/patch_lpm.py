#!/usr/bin/env python3

from pathlib import Path
import shutil
import sys


def replace_once(text, old, new, path):
    if new in text:
        return text
    if old not in text:
        raise SystemExit(f"cannot find expected text in {path}")
    return text.replace(old, new, 1)


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: patch_lpm.py LIBPROTOBUF_MUTATOR_DIR")

    lpm_source = Path(sys.argv[1])
    lpm_cmake = lpm_source / "CMakeLists.txt"
    protobuf_cmake = lpm_source / "cmake" / "external" / "protobuf.cmake"
    if not lpm_cmake.exists():
        raise SystemExit(f"cannot find {lpm_cmake}")
    if not protobuf_cmake.exists():
        raise SystemExit(f"cannot find {protobuf_cmake}")

    patch_script = Path(__file__).with_name("patch_abseil_randen_copts.cmake")
    copied_patch_script = protobuf_cmake.with_name("patch_abseil_randen_copts.cmake")
    shutil.copyfile(patch_script, copied_patch_script)

    text = protobuf_cmake.read_text()
    text = replace_once(
        text,
        "include_directories(${PROTOBUF_INCLUDE_DIRS})\n"
        "include_directories(${CMAKE_CURRENT_BINARY_DIR})",
        "include_directories(BEFORE ${PROTOBUF_INCLUDE_DIRS})\n"
        "include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})",
        protobuf_cmake,
    )
    text = replace_once(
        text,
        '    UPDATE_COMMAND ""\n',
        '    UPDATE_COMMAND ""\n'
        '    PATCH_COMMAND ${CMAKE_COMMAND} -DPROTOBUF_SOURCE_DIR=<SOURCE_DIR> -P ${CMAKE_CURRENT_LIST_DIR}/patch_abseil_randen_copts.cmake\n',
        protobuf_cmake,
    )
    text = replace_once(
        text,
        "        -Dprotobuf_BUILD_TESTS=OFF\n",
        "        -Dprotobuf_BUILD_TESTS=OFF\n"
        "        -Dprotobuf_WITH_ZLIB=OFF\n",
        protobuf_cmake,
    )
    protobuf_cmake.write_text(text)

    text = lpm_cmake.read_text()
    text = text.replace("include_directories(${LIBLZMA_INCLUDE_DIRS})\n", "")
    text = text.replace("include_directories(${ZLIB_INCLUDE_DIRS})\n", "")
    lpm_cmake.write_text(text)


if __name__ == "__main__":
    main()
