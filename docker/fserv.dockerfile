FROM local:builder as build
WORKDIR /root/compile/
COPY . .
ENV DEBUG=1
RUN make -j2 && make install

FROM ubuntu:18.04
RUN apt-get update && apt-get install -y libev4 libgoogle-perftools4 libhiredis0.13 libicu60 libcurl4 libluajit-5.1 \
 libjansson4 libgoogle-glog0v5 gdb
WORKDIR /app/
COPY --from=build ["/root/compile/bin/", "./"]
COPY --from=build ["/usr/local/lib/*.so.*", "/usr/lib/"]
ENV DOCKER=1
EXPOSE 9722/tcp
VOLUME /app/data/
CMD ["./fserv"]


