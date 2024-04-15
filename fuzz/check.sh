#!/usr/bin/bash

if [ ! -f "infra/helper.py" ] ; then
  echo "this script must be executed in the oss-fuzz directory" 2>&1
  exit 33
fi

if [ "$1" = "build-image" ] ; then
  shift
  python3 infra/helper.py build_image cyclonedds
fi

if [ ! -d "$1" -o ! -f "$1/src/core/ddsi/src/ddsi_init.c" ] ; then
  echo "usage: $0 [build-image] cyclone-source-dir" 2>&1
  exit 33
fi
srcdir="$1"

set -x
engines="libfuzzer afl honggfuzz centipede"
for eng in $engines ; do
  echo "********** ENGINE = $eng **********"
  sudo rm -rf $srcdir/{build,install,build_python}
  python3 infra/helper.py build_fuzzers --sanitizer address --engine $eng cyclonedds $srcdir || break
  python3 infra/helper.py check_build --engine $eng cyclonedds || break
done
