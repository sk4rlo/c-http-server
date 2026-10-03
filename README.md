# Minimal HTTP Server in C

This is a small learning HTTP server written in C. It uses Unix sockets to
listen on addresses and port configured with environmental variables (with default valuses `127.0.0.1:8080`), parse simple HTTP request lines, serve static files
from a fixed directory, build HTTP responses, log requests, and clean up
resources.

It is not intended to be a production web server.

## Features

- IPv4 TCP server
- Single-threaded blocking accept loop
- Supports `GET`
- Serves files from `files/`
- Returns basic `200`, `400`, `404`, `405`, and `500` responses
- Sends `Content-Length`, `Content-Type`, and `Connection: close`
- Logs method, path, status code, and response time

- Docker compatibility
## Build

```sh
make
```

This creates:

```text
server.out
```

## Run

```sh
./server.out
```

The server listens on:

```text
http://127.0.0.1:8080
```

## Try It

Root request:

```sh
curl http://127.0.0.1:8080/
```

Static file:

```sh
curl http://127.0.0.1:8080/test.html
```

Missing file:

```sh
curl -v http://127.0.0.1:8080/missing.html
```

Unsupported method:

```sh
curl -v -X POST http://127.0.0.1:8080/test.html
```
## Run with Docker

Build the image (a multi-stage build: the server is compiled in a `gcc`
image and only the binary and `files/` are copied into a slim Debian image):

```sh
docker build -t http-server .
```

Run it and publish the container port on the host:

```sh
docker run --rm -p 8080:8080 http-server
```

```sh
curl http://localhost:8080/healthz
curl http://localhost:8080/hostname    # returns the container ID
```

Inside the container the server listens on `0.0.0.0` so that it is reachable
from outside the container. The image sets the defaults:

```text
LISTEN_ADDR=0.0.0.0
PORT=8080
```

Override them with `-e`. The container side of `-p` must match `PORT`:

```sh
docker run --rm -e PORT=9090 -p 9090:9090 http-server
```

Invalid values (for example `PORT=abc`) make the server exit with an error
message, visible with `docker logs`.

### Multiple containers on a Docker network

On a user-defined network containers can reach each other by name, and no
`-p` is needed for container-to-container traffic:

```sh
docker network create lab-net
docker run -d --rm --network lab-net --name web1 http-server
docker run -d --rm --network lab-net --name web2 http-server
```

Each container answers `/hostname` with its own ID:

```sh
docker run --rm --network lab-net curlimages/curl -s http://web1:8080/hostname
docker run --rm --network lab-net curlimages/curl -s http://web2:8080/hostname
```

Clean up (`-t 1` shortens the wait: the server runs as PID 1 and does not
handle `SIGTERM`, so Docker would otherwise wait 10 seconds before killing it):

```sh
docker stop -t 1 web1 web2
docker network rm lab-net
```

## Project Layout

```text
Makefile
Dockerfile
server.c        socket setup, accept loop, request handling, file serving
http.c          HTTP request parsing and response building
http.h          public HTTP structs and function declarations
files/          static files served by the server
```

## Limitations

This server is intentionally minimal. It does not implement full HTTP behavior.
Known limitations include:

- one request per connection
- no concurrency
- request reading assumes the request fits in one buffer
- basic path traversal protection only
- basic content type handling
- no URL decoding
- limited client disconnect handling

The goal is to understand the path from sockets to HTTP parsing, file I/O,
response writing, logging, and cleanup.
