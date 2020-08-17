FROM ubuntu:18.04

RUN apt-get update && \
apt-get install -y --no-install-recommends \
    g++ \
    make \
    cmake \
    wget \
    git \
    python3 \
    python3-distutils \
    python3-dev \
    vim \
    libboost-program-options-dev \
    libboost-system-dev \
    libboost-thread-dev \
    libboost-test-dev \
    libboost-serialization-dev \
    ca-certificates \
    libeigen3-dev \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

COPY . /opt/QueryBlazer
WORKDIR /opt/QueryBlazer/third_party
RUN git submodule update --init --recursive

RUN cd sentencepiece && mkdir build && cd build \
    && cmake .. -DCMAKE_CXX_FLAGS=-O2 && make -j4

RUN cd kenlm && mkdir build && cd build \
    && cmake .. -DCMAKE_CXX_FLAGS=-O2 -DKENLM_MAX_ORDER=8 && make -j4

RUN ./install_openfst.sh

WORKDIR /opt/QueryBlazer
RUN mkdir build && cd build \
    && cmake .. -DCMAKE_CXX_FLAGS=-O2 && make -j4

