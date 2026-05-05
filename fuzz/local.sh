#!/usr/bin/bash

# Local build
#
# sudo apt install clang libfuzzer-18-dev (replace 18 with clang version)

err=
if [ ! -f ../src/core/ddsi/src/ddsi_receive.c -o ! -d ../fuzz ] ; then
    echo "This expects to be run in a build directory that is a subdirectory of the Cyclone repo" 2>&1
    err=1
fi
if [ -z "$CYCLONEDDS_HOME" ] ; then
    echo "Need CYCLONEDDS_HOME to be set" 2>&1
    err=1
fi
if [ -z "$CYCLONEDDS_PYTHON" -o ! -d "$CYCLONEDDS_PYTHON/tests/support_modules/fuzz_tools" ] ; then
    echo "need CYCLONEDDS_PYTHON to point to the cyclone python binding sources" 2>&1
    err=1
fi
[ -n "$err" ] && exit 1

set -ex

if [ -z "${CC:-}" ] && [ -x /opt/homebrew/opt/llvm/bin/clang ] ; then
    export CC=/opt/homebrew/opt/llvm/bin/clang
else
    export CC="${CC:-clang}"
fi
if [ -z "${CXX:-}" ] && [ -x /opt/homebrew/opt/llvm/bin/clang++ ] ; then
    export CXX=/opt/homebrew/opt/llvm/bin/clang++
else
    export CXX="${CXX:-clang++}"
fi
if [ -z "${LIB_FUZZING_ENGINE:-}" ] && [ -f /usr/lib/llvm-18/lib/libFuzzer.a ] ; then
    export LIB_FUZZING_ENGINE=/usr/lib/llvm-18/lib/libFuzzer.a
else
    export LIB_FUZZING_ENGINE="${LIB_FUZZING_ENGINE:-}"
fi

# hopefully Cyclone will never gain a "libprotobuf-mutator" or "LPM" subdirectory so that
# we can keep populate it to match oss-fuzz here and not have to deal with
# absl/utf8_range/protobuf horrors any more than this
if [ ! -d ../LPM ] ; then
    [ ! -d ../libprotobuf-mutator ] && \
         git clone --depth 1 https://github.com/google/libprotobuf-mutator.git ../libprotobuf-mutator
    python3 ../fuzz/fuzz_handshake/patch_lpm.py ../libprotobuf-mutator
    mkdir ../LPM
    (cd ../LPM && \
         cmake ../libprotobuf-mutator -GNinja \
               -DLIB_PROTO_MUTATOR_DOWNLOAD_PROTOBUF=ON \
               -DLIB_PROTO_MUTATOR_TESTING=OFF \
               -DLIB_PROTO_MUTATOR_EXAMPLES=OFF \
               -DCMAKE_BUILD_TYPE=Release && \
         ninja)
fi

export PATH="$CYCLONEDDS_HOME/bin:$PATH"
export LD_LIBRARY_PATH="$CYCLONEDDS_HOME/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PATH="$CYCLONEDDS_HOME/lib:$PATH"
export PYTHONPATH="$CYCLONEDDS_PYTHON/tests/support_modules${PYTHONPATH:+:$PYTHONPATH}"

if [ -z "${PYTHON:-}" ] ; then
    for p in \
        "$CYCLONEDDS_PYTHON/venv.3.14/bin/python" \
        "$CYCLONEDDS_PYTHON/venv.3.13/bin/python" \
        "$CYCLONEDDS_PYTHON/venv.3.12/bin/python" \
        "$CYCLONEDDS_PYTHON/venv.3.11/bin/python" \
        "$CYCLONEDDS_PYTHON/venv.3.10/bin/python" \
        "$CYCLONEDDS_PYTHON/venv/bin/python" \
        python3.14 python3.13 python3.12 python3.11 python3.10 python3 ; do
        if command -v "$p" >/dev/null 2>&1 ; then
            PYTHON=$p
            break
        fi
    done
fi
if [ -z "${PYTHON:-}" ] ; then
    echo "Need Python 3.10 or newer" 2>&1
    exit 1
fi
"$PYTHON" - <<'PY'
import sys
if sys.version_info < (3, 10):
    raise SystemExit("need Python 3.10 or newer")
PY
export PATH="$(dirname "$PYTHON"):$PATH"

# Use current git HEAD hash as seed
[ -z "$SEED" ] && SEED=$(git ls-remote https://github.com/eclipse-cyclonedds/cyclonedds HEAD |cut -f1)
"$PYTHON" "../fuzz/fuzz_sample_deser/generate_idl.py" $SEED "../fuzz/fuzz_sample_deser"

cmake -G Ninja \
      -DSANITIZER=address,undefined,fuzzer \
      -DEXPORT_ALL_SYMBOLS=ON \
      -DBUILD_SHARED_LIBS=OFF \
      -DBUILD_EXAMPLES=NO \
      -DENABLE_SECURITY=ON \
      -DENABLE_SSL=ON \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      -DBUILD_IDLC=NO \
      -DBUILD_DDSPERF=NO \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_PREFIX_PATH=$PWD/host_install \
      -DCMAKE_INSTALL_PREFIX=$PWD/install ..

cmake --build .
