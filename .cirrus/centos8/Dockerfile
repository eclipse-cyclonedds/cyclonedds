FROM centos:8
# EPEL: https://fedoraproject.org/wiki/EPEL
RUN dnf install -y epel-release && \
    dnf install -y 'dnf-command(config-manager)' && \
    dnf config-manager --set-enabled PowerTools && \
    dnf update -y
RUN dnf install -y bison cmake make gcc clang python3 python3-pip ccache \
                   java-11-openjdk-headless maven
