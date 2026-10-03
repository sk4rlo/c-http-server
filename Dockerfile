# STEP 1: source code compilation
FROM gcc:15-trixie AS source
WORKDIR /app
COPY server.c http.c http.h Makefile .
RUN make

# STEP 2: final light image creation
FROM debian:trixie-slim
ENV LISTEN_ADDR=0.0.0.0 PORT=8080
WORKDIR /app
COPY --from=source /app/server.out .
COPY /files /app/files
CMD ["./server.out"]