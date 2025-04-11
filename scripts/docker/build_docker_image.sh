#!/bin/bash

# Copyright(c) 2020 Prasanna Bhat

# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.

# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

set -e 

BUILD_UBUNTU=false
BUILD_CYCLONEDDS=false
CONTAINER_RUNTIME="docker"  # Default to docker

usage() {
	echo ""
	echo "Helper script to build docker images for cyclonedds"
	echo "Usage : "
	echo "<workspace>/scripts/docker/build_docker_image.sh [options] [images]"
	echo "Options:"
	echo "  -r, --runtime <runtime> : Specify the container runtime. Default is docker."
	echo "Supported images are "
	echo "ubuntu 		: ubuntu based image to build cyclone dds. Contains dependencies to build cyclonedds."
	echo "cyclonedds	: pre-built cyclonedds core libs & example applications. Can be quickly used to test cyclonedds apps."
	echo ""
}

parse_args() {
	while [[ $# -gt 0 ]]; do
		case $1 in
		    -h|--help)
			usage
			exit 0
			;;
			-r|--runtime)
			CONTAINER_RUNTIME="$2"
			shift 2
			;;
			ubuntu)
			BUILD_UBUNTU=true
			shift
			;;
			cyclonedds)
			BUILD_CYCLONEDDS=true
			shift
			;;
			*)
			echo "ERROR : unknown parameter $1"
			usage
			exit 1
			;;
		esac
	done
}



if [ "$#" -eq 0 ] 
then
	echo "Supply at least one argument"
	echo "Run with -h/--help for usage"
	exit 1
fi
parse_args "$@"

DOCKERFILE_DIR="$(dirname "$(realpath "$0")")"
WORKSPACE="$(dirname "$(dirname "${DOCKERFILE_DIR}")")"
echo "WORKSPACE is $WORKSPACE"
cd "$WORKSPACE"

if [ $BUILD_UBUNTU == true ]
then
	UBUNTU_IMAGE="ubuntu:cyclonedds"
	if [ -f "${DOCKERFILE_DIR}/Dockerfile" ]; then
		$CONTAINER_RUNTIME build --file "${DOCKERFILE_DIR}/Dockerfile" . --tag "${UBUNTU_IMAGE}"
	else
		echo "ERROR: Dockerfile for Ubuntu image not found at ${DOCKERFILE_DIR}/Dockerfile"
		exit 1
	fi
fi

if [ $BUILD_CYCLONEDDS == true ]
then
	CYCLONEDDS_IMAGE="cyclonedds:latest"
	if [ -f "${DOCKERFILE_DIR}/DockerfileCycloneDds" ]; then
		$CONTAINER_RUNTIME build --file "${DOCKERFILE_DIR}/DockerfileCycloneDds" . --tag "${CYCLONEDDS_IMAGE}"
	else
		echo "ERROR: Dockerfile for CycloneDDS image not found at ${DOCKERFILE_DIR}/DockerfileCycloneDds"
		exit 1
	fi
fi