FROM ubuntu:18.04 as build
WORKDIR /root/compile/
RUN apt-get update
RUN apt-get install -y build-essential libev-dev libgoogle-perftools-dev libhiredis-dev libicu-dev \
 libcurl4-openssl-dev libboost-dev libluajit-5.1-dev libpth-dev libjansson-dev libgoogle-glog-dev git curl autoconf \
 libtool shtool
COPY . .
RUN /bin/bash ./grpc.sh
RUN make -j2 && make install

#FROM ubuntu:18.04
#RUN apt-get update
#RUN apt-get install -y libev4 libgoogle-perftools4 libhiredis0.13 libicu60 libcurl4 libluajit-5.1 \
# libjansson4 libgoogle-glog0v5
#WORKDIR /app/
#COPY --from=build /root/compile/bin/ .
#ENV DOCKER=1
#EXPOSE 9722/tcp
#VOLUME /app/data/
#ENTRYPOINT ["./fserv"]


