#!/bin/bash
mkdir grpc
pushd grpc
git clone -b $(curl -L https://grpc.io/release) https://github.com/grpc/grpc
pushd grpc
git submodule update --init
make
make install
pushd third_party
pushd protobuf
make install
popd
popd
popd
popd