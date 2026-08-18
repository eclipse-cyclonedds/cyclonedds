#!/bin/bash

# Copyright(c) 2026 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: scripts/compare-idlc-outputs.sh BISON_BUILD HAND_BUILD IDL...

Compare idlc generated output from a build using the generated Bison parser
with output from a build using the hand-written parser.

The script runs both compilers with the same output path for each IDL file,
copies the Bison output aside, then compares the copied Bison output with the
hand-parser output using diff -ru. Use TMPDIR to choose where temporary output
is created. Set KEEP_IDLC_COMPARE_OUTPUTS=1 to keep successful output.
USAGE
}

resolve_path() {
  case "$1" in
    /*) printf '%s\n' "$1" ;;
    *) printf '%s/%s\n' "$PWD" "$1" ;;
  esac
}

find_idlc() {
  local build_dir=$1

  if [ -x "$build_dir/bin/idlc" ]; then
    printf '%s/bin/idlc\n' "$build_dir"
    return 0
  fi

  printf 'Cannot find idlc executable in %s\n' "$build_dir" >&2
  return 1
}

find_generator() {
  local build_dir=$1
  local generator

  generator=$(
    find "$build_dir" -type f \
      \( -name 'libcycloneddsidlc.dylib' \
      -o -name 'libcycloneddsidlc.so' \
      -o -name 'libcycloneddsidlc.so.*' \
      -o -name 'libcycloneddsidlc.*.dylib' \
      -o -name 'cycloneddsidlc.dll' \) \
      | sort \
      | head -n 1
  )

  if [ -n "$generator" ]; then
    printf '%s\n' "$generator"
    return 0
  fi

  printf 'Cannot find libcycloneddsidlc generator in %s\n' "$build_dir" >&2
  return 1
}

if [ "$#" -lt 3 ]; then
  usage >&2
  exit 64
fi

bison_build=$(resolve_path "$1")
shift
hand_build=$(resolve_path "$1")
shift

bison_idlc=$(find_idlc "$bison_build")
hand_idlc=$(find_idlc "$hand_build")
bison_generator=$(find_generator "$bison_build")
hand_generator=$(find_generator "$hand_build")

tmp_parent=${TMPDIR:-/tmp}
mkdir -p "$tmp_parent"
work_dir=$(mktemp -d "$tmp_parent/cdds-idlc-compare.XXXXXX")

cleanup() {
  local status=$?

  if [ "$status" -eq 0 ] && [ "${KEEP_IDLC_COMPARE_OUTPUTS:-0}" = "0" ]; then
    rm -rf "$work_dir"
  else
    printf 'Comparison output kept in %s\n' "$work_dir" >&2
  fi
  exit "$status"
}
trap cleanup EXIT

count=0
for idl in "$@"; do
  count=$((count + 1))
  idl_path=$(resolve_path "$idl")
  name=$(basename "$idl_path" .idl)
  case_dir=$(printf '%s/%03d-%s' "$work_dir" "$count" "$name")
  out_dir="$case_dir/out"
  bison_dir="$case_dir/bison"

  if [ ! -f "$idl_path" ]; then
    printf 'IDL file does not exist: %s\n' "$idl_path" >&2
    exit 1
  fi

  mkdir -p "$out_dir" "$bison_dir"
  "$bison_idlc" -l"$bison_generator" -o"$out_dir" "$idl_path"
  cp -R "$out_dir/." "$bison_dir/"

  rm -rf "$out_dir"
  mkdir -p "$out_dir"
  "$hand_idlc" -l"$hand_generator" -o"$out_dir" "$idl_path"

  diff -ru "$bison_dir" "$out_dir"
done

printf 'Compared %d IDL file(s) successfully\n' "$count"
