#!/bin/bash -eu


prepare_fuzz_handshake() {
    apt-get -y install ninja-build liblzma-dev libz-dev libtool
    rm -rf libprotobuf-mutator LPM
    git clone --depth 1 https://github.com/google/libprotobuf-mutator.git
    mkdir LPM && cd LPM
    cmake ../libprotobuf-mutator -GNinja \
        -DCMAKE_BUILD_TYPE=Release \
        -DLIB_PROTO_MUTATOR_DOWNLOAD_PROTOBUF=ON \
        -DLIB_PROTO_MUTATOR_TESTING=OFF \
        -DLIB_PROTO_MUTATOR_FUZZER_LIBRARIES=FuzzingEngine
    ninja
}
export -f prepare_fuzz_handshake

env -u CFLAGS -u CXXFLAGS -u LIB_FUZZING_ENGINE CXXFLAGS="-O1 -fno-omit-frame-pointer -gline-tables-only -stdlib=libc++" bash -euc prepare_fuzz_handshake
