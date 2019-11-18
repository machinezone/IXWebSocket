FROM alpine as build

RUN apk add --no-cache gcc g++ musl-dev linux-headers cmake openssl-dev 
RUN apk add --no-cache make
RUN apk add --no-cache zlib-dev

RUN addgroup -S app && adduser -S -G app app 
RUN chown -R app:app /opt
RUN chown -R app:app /usr/local

# There is a bug in CMake where we cannot build from the root top folder
# So we build from /opt
COPY --chown=app:app . /opt
WORKDIR /opt

USER app
RUN [ "make", "ws_install" ]

FROM alpine as runtime

RUN apk add --no-cache libstdc++
RUN apk add --no-cache strace

RUN addgroup -S app && adduser -S -G app app 
COPY --chown=app:app --from=build /usr/local/bin/ws /usr/local/bin/ws
RUN chmod +x /usr/local/bin/ws
RUN ldd /usr/local/bin/ws

# Now run in usermode
USER app
WORKDIR /home/app

ENTRYPOINT ["ws"]
EXPOSE 8008
CMD ["--help"]
