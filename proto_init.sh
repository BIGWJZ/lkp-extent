#!/bin/bash

set -ex

ROOT_DIR=$(pwd)
tar -xzvf ./etc/protobuf-cpp-3.0.0.tar.gz -C ./etc
cd ./etc/protobuf-3.0.0
cp -r ./src/google /usr/include
./configure --prefix=/usr/local/protobuf
make
make install
cd $ROOT_DIR
