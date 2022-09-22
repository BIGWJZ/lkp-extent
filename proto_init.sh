#!/bin/bash

set -ex

tar -xzvf ./etc/protobuf-cpp-3.0.0.tar.gz
cd ./etc/protobuf-3.0.0
cp -r ./src/google /usr/include
./configure --prefix=/usr/local/protobuf
make
make install
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/protobuf/lib
export LIBRARY_PATH=$LIBRARY_PATH:/usr/local/protobuf/lib
export PATH=$PATH:/usr/local/protobuf/bin
ldconfig
protoc --version