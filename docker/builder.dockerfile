FROM ubuntu:18.04
WORKDIR /root/compile/
RUN apt-get update && apt-get install -y build-essential libev-dev libgoogle-perftools-dev libhiredis-dev libicu-dev \
 libcurl4-openssl-dev libboost-dev libluajit-5.1-dev libpth-dev libjansson-dev libgoogle-glog-dev git curl autoconf \
 libtool shtool
COPY ./grpc.sh .
RUN /bin/bash ./grpc.sh