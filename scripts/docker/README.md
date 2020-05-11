# Overview

This contains helper scripts to build & run cyclonedds core & example applications in docker.  
Additionally you can also build a docker image with pre-built cyclonedds examples. This could be useful in quickly trying the examples (both IPC & network communication).

# Bulid docker image 
There are two docker images you can build.  
- **ubuntu:cyclonedds** : ubuntu bionic based image that contains dependencies to build cyclonedds. 
- **cyclonedds:latest** : ubuntu bionic based image with pre-built cyclonedds core & applications (based on the checked out revision).

## How to build the images

### Pre-requisites
- Linux machine (native / runnin inside VM)
- Docker (I have tested with 19.03.8, however past versions also would work, as there is no specific dependency)

### Steps

You can use the helper script `./scripts/docker/build_docker_image.sh` to build the docker images.

- Navigate to root of the project.
- Run the script with -h to display usage.
```
./scripts/docker/build_docker_image.sh [images]
```
The following images are supported.  
`ubuntu` : Same as **ubuntu:cyclonedds** , mentioned above.  
`cyclonedds` : Same as **cyclonedds:latest**, mentioned above.  
You can run the script with `-h` option to display usage information.  
**NOTE** : *cyclonedds:latest* depends on image *ubuntu:cyclonedds*. You need to build *ubuntu:cyclonedds* first, if you want to use *cyclonedds:latest*.  
- Build cyclonedds core & examples (if you are using **ubuntu:cyclonedds**)  
Use the helper script `./scripts/docker/build_cyclonedds.sh` to build using the currently checked out revision.  
**NOTE** : You need to run this script from the root of the project.

# Run cyclonedds examples in docker
You can use either of the above docker images to run the examples.  
If you use **ubuntu:cyclonedds**, you need to build cyclonedds inside container to build examples.
Once you have built the cyclonedds , you are ready to run the examples inside docker container.  
  
Let's do that !  
  
## Run examples inside the same container (uses IPC)  
- Launch docker container, from the root of the project  

```
docker run --name cyclonedds -it --workdir /cyclonedds/build/bin cyclonedds:latest /bin/bash
```
This will open terminal connected to docker @ bin of the project, which contains sample applications.
It will look something like this
```
root@22ff409a33e1:/cyclonedds/build/bin# 
```
- From here you can run the example applications.
- To run another terminal , connected to same docker container, use the following command.
```
docker exec -it cyclonedds /bin/bash
```
Run the partner example here. This will establish communication over IPC channel.


## Run examples in different containers (uses docker networking)
- Follow the same steps as in the previous section , instead connecting to existing docker container create a new docker container.
e.g.  
`docker run --name cyclonedds1 -it --workdir /cyclonedds/build/bin cyclonedds:latest /bin/bash`  
**NOTE** : You need to run this script from the root of the project.  
At this point you can inspect the traffic using wireshark.  
Open docker network interface (`docker0` on my system) in wireshark.  
Good news is that wireshark has RTPS dissector.



