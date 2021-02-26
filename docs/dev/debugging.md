# Troubleshooting Travis CI builds locally in a Docker image

This document describes how to run a Travis CI build locally using docker.

Most of the information is available in the Getting Started guide under
[Common Build Problems](https://docs.travis-ci.com/user/common-build-problems/#running-a-container-based-docker-image-locally),
but this guide is specifically tailored towards building Cyclone.

> Ensure [Docker](https://www.docker.com/) is installed, the daemon is running
> and you have the required privileges.

> See this [entry](https://developer.fedoraproject.org/tools/docker/about.html)
> on the Fedora Developer Portal for setting up and using Docker on Fedora.

> The *instance:* line under *Worker information* in the *Job log* will show
> exactly which instance of the *ci-garnet* image is used, but the latest image
> will do just fine.

```
Worker information
...
instance: travis-job-70cf1506-780a-445e-8f0d-da928d16ed65 travis-ci-garnet-trusty-1512502259-986baf0 (via amqp)
...
```

1. Start a Docker container detached with `/sbin/init`.

 ```
$ docker run --name travis-debug --cap-add=SYS_PTRACE --security-opt seccomp=unconfined -dit travisci/ci-garnet:packer-1515445631-7dfb2e1 /sbin/init
```

 > The image is quite large (4 GB) and it will take some time to download.

2. Open a login shell in the running container.

 ```
$ docker exec -it travis-debug bash -l
```

3. Install all packages required to build Cyclone.

 ```
$ add-apt-repository ppa:ubuntu-toolchain-r/test
$ echo "deb http://apt.llvm.org/trusty/ llvm-toolchain-trusty-6.0 main" >> /etc/apt/sources.list.d/llvm-toolchain-6.0.list
$ wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
$ apt-get update
$ apt-get install gcc-8 g++-8 clang-6.0 bison
```

4. Switch to the *travis* user.

 ```
su - travis
```

5. Install The [Conan C/C++ Package Manager](https://conan.io).

 ```
$ pip install conan --upgrade --user
$ conan user
```

6. Add the required repositories.

 ```
$ conan remote add atolab https://api.bintray.com/conan/atolab/public-conan
$ conan remote add bincrafters https://api.bintray.com/conan/bincrafters/public-conan
```

7. Clone the git repository.

 ```
$ git clone https://github.com/eclipse-cyclonedds/cyclonedds.git
```

8. Build the project and run the tests.

 > By default CMake and Conan will choose *gcc*. To switch to *clang*, set the
 > CC and CXX environment variables to clang and clang++ respectively.

 ```
$ export CC=gcc-8
$ export CXX=g++-8
$ cd cyclonedds
$ mkdir build
$ conan install ..
$ cmake -DBUILD_TESTING=on ../src
$ cmake --build .
$ ctest -T test
```
