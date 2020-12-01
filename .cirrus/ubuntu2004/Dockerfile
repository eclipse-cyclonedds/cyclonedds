FROM ubuntu:focal
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get install -y gnupg2 ca-certificates wget curl tzdata ccache
RUN ln -snf /usr/share/zoneinfo/$(wget -qO - https://ipapi.co/timezone) /etc/localtime
RUN apt-get update
RUN apt-get install -y bison cmake gcc-10 clang-10 python3 python3-pip \
                       openjdk-11-jdk-headless maven
