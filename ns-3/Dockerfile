FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG BUILD_PROF="optimized"

RUN apt -y update && apt -y upgrade && \
 apt -y install make curl build-essential g++ python3 virtualenv apt-utils && \
 apt -y install qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools && \
 apt -y install gir1.2-goocanvas-2.0 python3-gi python3-gi-cairo python3-pygraphviz gir1.2-gtk-3.0 ipython3 && \
 apt -y install autoconf cvs bzr unrar && \
 apt -y install gdb valgrind && \
 apt -y install uncrustify && \
 apt -y install doxygen graphviz imagemagick && \
 apt -y install texlive texlive-extra-utils texlive-latex-extra texlive-font-utils dvipng latexmk && \
 apt -y install python3-sphinx dia && \
 apt -y install gsl-bin libgsl-dev libgslcblas0 && \
 apt -y install tcpdump && \
 apt -y install sqlite sqlite3 libsqlite3-dev && \
 apt -y install libxml2 libxml2-dev && \
 apt -y install cmake libc6-dev libc6-dev-i386 automake python3-pip && \
 pip3 install cxxfilt && \
 apt -y install libgtk-3-dev && \
 apt -y install vtun lxc uml-utilities libboost-all-dev && \
 apt -y install gfortran unzip
# TODO: update to allinone once released
RUN mkdir -p /app && cd /tmp && mkdir ns-3-download && cd ns-3-download && \
    curl -O https://www.nsnam.org/releases/ns-3.48.tar.bz2 && \
    tar -xaf ns-3.48.tar.bz2 && \
    mv ns-3.48 /app/ns-3 && \
    virtualenv --python=/usr/bin/python3 /app/pyenv
RUN cd /app/ns-3 && . /app/pyenv/bin/activate && \
    ./ns3 configure --build-profile=${BUILD_PROF} --enable-examples --enable-tests && \
    ./ns3 build
WORKDIR /app/ns-3
CMD [ "tail", "-f", "/dev/null" ]
