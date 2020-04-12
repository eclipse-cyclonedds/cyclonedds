#!/bin/bash
set -e

# It is assumed that this script is called from the root of the project
WORKSPACE=$(pwd)
echo "WORKSPACE is $WORKSPACE"

CYCLONEDDS_INSTALL_PREFIX=$WORKSPACE/build/install/

rm -rf build/
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=$CYCLONEDDS_INSTALL_PREFIX -DBUILD_IDLC=ON ..
cmake --build .
cmake --build . --target install
