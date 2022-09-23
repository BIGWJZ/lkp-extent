#!/bin/bash
set -ex

ROOT_DIR=$(pwd)
echo $ROOT_DIR

if [ ! -d /usr/local/protobuf/lib ] || [ ! -d /usr/local/protobuf/bin ];then
    echo "protobuf not found, exit"
    exit 0
fi

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/protobuf/lib
export LIBRARY_PATH=$LIBRARY_PATH:/usr/local/protobuf/lib
export PATH=$PATH:/usr/local/protobuf/bin
ldconfig
protoc --version

git submodule init
git submodule update

#make and install muduo
cd $ROOT_DIR/src/muduo
./build.sh
./build.sh install

cp -r $ROOT_DIR/src/build/release-install-cpp11/include/muduo /usr/include
cp $ROOT_DIR/src/build/release-install-cpp11/lib/* /usr/lib

mkdir -p /usr/include/muduo/net/protorpc/
cp  $ROOT_DIR/src/muduo/muduo/net/protorpc/google-inl.h /usr/include/muduo/net/protorpc/
#make lkp-extent
cd $ROOT_DIR/src
make

#install lkp-extent
mkdir -p /usr/local/bin
ln -sf $ROOT_DIR/bin/lkp-ctl /usr/local/bin/lkp-ctl

mkdir -p $ROOT_DIR/log
mkdir -p $ROOT_DIR/results/local
mkdir -p $ROOT_DIR/results/remote
mkdir -p $ROOT_DIR/testcases

set +ex

echo "lkp-extent is ready!"
echo "Use 'lkp-ctl init' and 'lkp-ctl start server/client' to start lkp-extent"


