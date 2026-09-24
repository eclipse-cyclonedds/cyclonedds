#!/bin/bash -eu

#
# Copyright(c) 2006 to 2021 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

source fuzz/fuzz_sample_deser/prepare.sh
source fuzz/fuzz_handshake/prepare.sh
(
mkdir build || echo "build directory already exists"
cd build
seed=$(git ls-remote https://github.com/eclipse-cyclonedds/cyclonedds HEAD |cut -f1)
python=$(command -v python3)
cmake \
    -DBUILD_IDLC=ON \
    -DEXPORT_ALL_SYMBOLS=ON \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_EXAMPLES=NO \
    -DENABLE_SECURITY=ON \
    -DENABLE_SSL=ON \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DPython3_EXECUTABLE="$python" \
    -DFUZZ_SAMPLE_DESER_SEED="$seed" \
    -DCMAKE_INSTALL_PREFIX=/usr/local ..
cmake --build .
cmake --build . --target install
cd ..
)

cp fuzz/*.options $OUT

# copy fuzzer executables to $OUT
find build/bin -type f -name 'fuzz_*' | while read fuzzer; do
  cp -v "$fuzzer" "$OUT/"
done

find fuzz/ -type f -name 'fuzz_*_seed_corpus.zip' ! -path 'fuzz/fuzz_sample_deser/*' | xargs -I {} cp {} $OUT
find fuzz/ -type d -name 'fuzz_*_seed_corpus' ! -path 'fuzz/fuzz_sample_deser/*' | while read corpus_dir; do
  zip -j $OUT/$(basename "$corpus_dir").zip $corpus_dir/*
done
if [ -d build/fuzz ] ; then
  find build/fuzz -type f -name 'fuzz_*_seed_corpus.zip' | xargs -I {} cp {} $OUT
  find build/fuzz -type d -name 'fuzz_*_seed_corpus' | while read corpus_dir; do
    zip -j $OUT/$(basename "$corpus_dir").zip $corpus_dir/*
  done
fi
