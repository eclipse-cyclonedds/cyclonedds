#!/bin/bash

# Copyright(c) 2020 Prasanna Bhat

# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.

# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

set -e

# It is assumed that this script is called from the root of the project
WORKSPACE=$(pwd)
echo "WORKSPACE is $WORKSPACE"

CYCLONEDDS_INSTALL_PREFIX=$WORKSPACE/build/install/
CLEAN_BUILD=false

usage() {
	echo ""
	echo "Helper script to build cyclonedds"
	echo "Usage : "
	echo "<workspace>/scripts/build.sh [options]"
	echo "Supported options are "
	echo "clean 		: remove build folder (Default : keep build folder)"
	echo ""
}

parse_args() {
	for arg in "$@"
	do
		case $arg in
		    -h | --help)
			usage
			exit 0
			;;
			clean)
			CLEAN_BUILD=true
			;;
			*)
			echo "ERROR : unknown parameter $arg"
			usage
			exit 1
			;;
		esac
	done
}

parse_args $@

if [ $CLEAN_BUILD == true ]
then
    rm -rf build/
fi

mkdir -p build
cd build
cmake -DCMAKE_INSTALL_PREFIX=$CYCLONEDDS_INSTALL_PREFIX -DBUILD_IDLC=ON ..
cmake --build .
cmake --build . --target install
