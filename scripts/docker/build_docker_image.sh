#!/bin/bash

set -e 

BUILD_UBUNTU=false
BUILD_CYCLONEDDS=false

usage() {
	echo ""
	echo "Helper script to build docker images for cyclonedds"
	echo "Usage : "
	echo "<workspace>/scripts/docker/build_docker_image.sh [images]"
	echo "Supported images are "
	echo "ubuntu 		: ubuntu based image to build cyclone dds. Contains dependencies to build cyclonedds."
	echo "cyclonedds	: pre-built cyclonedds core libs & example applications. Can be quickly used to test cyclonedds apps."
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
			ubuntu)
			BUILD_UBUNTU=true
			;;
			cyclonedds)
			BUILD_CYCLONEDDS=true
			;;
			*)
			echo "ERROR : unknown parameter $arg"
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
parse_args $@


DOCKERFILE_DIR="$(dirname "$(readlink -fm "$0")")"
WORKSPACE="$(dirname "$(dirname "${DOCKERFILE_DIR}")")"
echo "WORKSPACE is $WORKSPACE"
cd "$WORKSPACE"

if [ $BUILD_UBUNTU == true ]
then
	UBUNTU_IMAGE="ubuntu:cyclonedds"
	docker build --file "${DOCKERFILE_DIR}/Dockerfile" . --tag "${UBUNTU_IMAGE}"
fi

if [ $BUILD_CYCLONEDDS == true ]
then
	CYCLONEDDS_IMAGE="cyclonedds:latest"
	docker build --file "${DOCKERFILE_DIR}/DockerfileCycloneDds" . --tag "${CYCLONEDDS_IMAGE}"
fi




