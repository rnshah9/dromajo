#------------------------------------------------------------------------------
# Copyright (C) 2018, Esperanto Technologies Inc.
# The copyright to the computer program(s) herein is the
# property of Esperanto Technologies, Inc. All Rights Reserved.
# The program(s) may be used and/or copied only with
# the written permission of Esperanto Technologies and
# in accordance with the terms and conditions stipulated in the
# agreement/contract under which the program(s) have been supplied.
#------------------------------------------------------------------------------


FROM centos:7.5.1804

LABEL Description="This image contains any build dependencies for the Maxion project"

RUN yum install -y \
    epel-release \
    centos-release-scl \
    http://opensource.wandisco.com/centos/7/git/x86_64/wandisco-git-release-7-2.noarch.rpm

RUN yum install -y \
    "@Development tools" \
    autoconf \
    automake \
    bison \
    devtoolset-7 \
    dtc \
    dpkg \
    expat-devel \
    flex \
    git-2.18.0 \
    libtool \
    lbzip2 \
    python36 \
    make \
    sudo \
    tmux \
    texinfo \
    wget \
    zlib-devel \
  && yum clean all

RUN update-alternatives --install /usr/bin/python3 python3 /usr/bin/python36 1

# Install Gosu
ENV GOSU_VERSION 1.10
RUN set -ex; \
        \
        dpkgArch="$(dpkg --print-architecture | awk -F- '{ print $NF }')"; \
        wget -O /usr/bin/gosu "https://github.com/tianon/gosu/releases/download/$GOSU_VERSION/gosu-$dpkgArch"; \
        wget -O /tmp/gosu.asc "https://github.com/tianon/gosu/releases/download/$GOSU_VERSION/gosu-$dpkgArch.asc"; \
        \
# verify the signature
        export GNUPGHOME="$(mktemp -d)"; \
        gpg --keyserver ha.pool.sks-keyservers.net --recv-keys B42F6819007F00F88E364FD4036A9C25BF357DD4; \
        gpg --batch --verify /tmp/gosu.asc /usr/bin/gosu; \
        rm -r "$GNUPGHOME" /tmp/gosu.asc; \
        \
        chmod +x /usr/bin/gosu; \
        # verify that the binary works
        gosu nobody true; \
        \
        yum clean all

# Install other required python packages
RUN cd /tmp/ \
  && curl https://bootstrap.pypa.io/get-pip.py -o get-pip.py \
  && python3 get-pip.py \
  && python3 -m pip install \
    junit-xml==1.8 \
    pexpect==4.6 \
    pytest==3.8.2 \
    boto3==1.9.25 \
    checksumdir==1.1.4 \
  # Python2 specific packages
  && pip install \
     awscli==1.16.34

ENV RISCV_TOOLS_SHA a0b0fce42126aa0f8d1b0ab50c1b1d9650c397fe
ENV ESPERANTO_DIR=/esperanto
ENV RISCV $ESPERANTO_DIR
ENV MAKEFLAGS "-j `nproc`"

RUN cd /tmp/ \
  && git clone --shallow-submodules https://github.com/riscv/riscv-tools.git \
  && cd riscv-tools \
  && git checkout $RISCV_TOOLS_SHA \
  # Using --depth=1 to not fetch history does not work :(
  && git submodule update --init --recursive \
        riscv-isa-sim \
        riscv-fesvr \
        riscv-pk \
        riscv-opcodes \
        riscv-gnu-toolchain \
  && cd /tmp/riscv-tools \
  # Use gcc-7 to compile the toolchain to avoid
  # compile issues with gcc-4.8
  &&  source scl_source enable devtoolset-7 \
  &&  source ./build.common \
  &&  build_project riscv-fesvr --prefix=$RISCV \
  &&  build_project riscv-isa-sim --prefix=$RISCV --with-fesvr=$RISCV --with-isa=rv64imafdc \
  &&  build_project riscv-isa-sim --prefix=$RISCV-debug --with-fesvr=$RISCV --with-isa=rv64imafdc --enable-commitlog \
  && cd riscv-gnu-toolchain \
  &&  mkdir build \
  &&  autoreconf -i \
  &&  cd build \
  &&  ../configure --prefix=$RISCV --with-arch=rv64imafdc \
  &&  make -j $(nproc) \
  && rm -rf /tmp/riscv-tools


ENV ELF2HEX_SHA 519ccaea326275695362ec8daf37440a71d3c869

# Build elf2hex
RUN cd /tmp \
  && git clone https://github.com/sifive/elf2hex.git \
  && cd elf2hex \
  && git checkout $ELF2HEX_SHA \
  && autoreconf -i \
  && PYTHON=python3.6 ./configure --target=riscv64-unknown-elf --prefix=$RISCV \
  && make -j $(nproc) \
  && make install \
  && rm -rf /tmp/elf2hex


ENV PATH $PATH:/esperanto/bin

ARG GIT_SHA
LABEL git_sha="$GIT_SHA"

COPY ./entrypoint.sh  /

ENTRYPOINT [ "/entrypoint.sh" ]
