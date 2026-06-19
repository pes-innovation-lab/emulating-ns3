FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG BUILD_PROF="optimized"
ARG VERSION="ns-3.48"

RUN apt -y update && apt -y upgrade && \
 apt -y install curl \
 git \
 g++ \
 cmake \
 ninja-build \
 ccache \
 python3 \
 python3-dev \
 python3-pip \
 gsl-bin \
 libgsl-dev \
 libgslcblas0 \
 libboost-all-dev \
 libgtk-3-dev \
 libfl-dev \
 libxml2 \
 libxml2-dev \
 libopenmpi-dev \
 openmpi-bin \
 openmpi-common \
 openmpi-doc \
 libsqlite3-dev \
 sqlite3 \
 libeigen3-dev \
 qtbase5-dev \
 qtchooser \
 qt5-qmake \
 qtbase5-dev-tools \
 python3.12-venv \
 net-tools \
 tcpdump \
 iputils-ping \
 iptables \
 iproute2

# TODO: update to allinone once released
RUN mkdir -p /app && cd /tmp && \
    curl -O https://www.nsnam.org/releases/${VERSION}.tar.bz2 && \
    tar -xaf ${VERSION}.tar.bz2 && \
    mv ${VERSION} /app/ns-3

# Copy OSPF module
COPY ospf /app/ns-3/src/ospf

RUN python3 -m venv /app/pyenv
RUN cd /app/ns-3 && . /app/pyenv/bin/activate && \
    ./ns3 configure --build-profile=${BUILD_PROF} --enable-examples --enable-tests && \
    ./ns3 build
WORKDIR /app/ns-3
CMD [ "tail", "-f", "/dev/null" ]
