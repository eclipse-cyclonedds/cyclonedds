#!/bin/bash -eu

prepare_fuzz_deser() {
    apt-get -y install python3 python3-pip libssl-dev
    mkdir -p build_python install
    export CYCLONEDDS_HOME="$(pwd)/install"
    fuzzer="$(pwd)/fuzz/fuzz_sample_deser"
    cd build_python
    
    # This builds cyclone for the python tool, the fuzzer is built later
    cmake \
        -DBUILD_IDLC=ON \
        -DBUILD_TESTING=OFF \
        -DBUILD_SHARED_LIBS=ON \
        -DBUILD_EXAMPLES=NO \
        -DENABLE_SECURITY=NO \
        -DENABLE_SSL=NO \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DCMAKE_INSTALL_PREFIX=../install ..
    cmake --build .
    cmake --build . --target install
    
    rm -rf cyclonedds-python
    git clone --depth=1 https://github.com/eclipse-cyclonedds/cyclonedds-python.git
    pip3 install ./cyclonedds-python
    ln -sf "$(pwd)/cyclonedds-python/tests/support_modules/fuzz_tools" "${fuzzer}/fuzz_tools"
}

export -f prepare_fuzz_deser
env -u CC -u CXX -u CFLAGS -u CXXFLAGS -u LIB_FUZZING_ENGINE bash -euc prepare_fuzz_deser

export CYCLONEDDS_HOME="$(pwd)/install"
export PATH="${CYCLONEDDS_HOME}/bin:$PATH"
