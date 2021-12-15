#!/bin/bash -eu

#
# Copyright(c) 2006 to 2021 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

(
mkdir build
cd build
cmake \
    -DBUILD_IDLC=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_EXAMPLES=NO \
    -DENABLE_SECURITY=NO \
    -DCMAKE_INSTALL_PREFIX=/usr/local ..
cmake --build . 
cmake --build . --target install
cd ..
)

function copy_fuzzer {
  mkdir -p "$OUT/lib"
  cp -v "$1" "$OUT/"
  patchelf --set-rpath '$ORIGIN/lib' "$OUT/$(basename "$1")"

  ldd "$1" | grep '=>' | cut -d ' ' -f 3 | while read lib; do
    if [[ -f $lib ]]; then
      cp -v "$lib" "$OUT/lib/"
      patchelf --set-rpath '$ORIGIN' "$OUT/lib/$(basename "$lib")"
    fi
  done
}

# copy out stuff
find build/bin -type f -name 'fuzz_*' | while read fuzzer; do
    copy_fuzzer "$fuzzer"
done

find fuzz/ -type f -name 'fuzz_*_seed_corpus.zip' | xargs -I {} cp {} $OUT
find fuzz/ -type d -name 'fuzz_*_seed_corpus' | while read corpus_dir; do
  zip -j $OUT/$(basename "$corpus_dir").zip $corpus_dir/*
done