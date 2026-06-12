FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    g++ \
    cmake \
    ninja-build \
    python3 \
    python3-pip \
    git \
    wget \
    tcpdump \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /usr/src

RUN wget https://nsnam.org && \
    tar xfj ns-allinone-3.41.tar.bz2 && \
    rm ns-allinone-3.41.tar.bz2

WORKDIR /usr/src/ns-allinone-3.41/ns-3.41

RUN ./ns3 configure --build-profile=optimized && ./ns3 build
CMD ["sleep", "infinity"]
