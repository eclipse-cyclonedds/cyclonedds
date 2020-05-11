#!/bin/bash

set -e

# It is assumed that this script is run from root of the project
WORKSPACE=$(pwd)
IMAGE_NAME="ubuntu:cyclonedds"
CONTAINER_NAME="cyclonedds"

echo "WORKSPACE is $WORKSPACE"

docker run -it --rm -v $WORKSPACE/:/cyclonedds --workdir /cyclonedds $IMAGE_NAME /bin/bash -c "./scripts/build.sh clean"
# Launch the docker after build
docker run --name $CONTAINER_NAME -it -v $WORKSPACE/:/cyclonedds --workdir /cyclonedds $IMAGE_NAME /bin/bash
# If you want to connect to the above docker to run cyclonedds examples (multiple apps in separate terminals) , use the below command
# docker exec -it cyclonedds /bin/bash