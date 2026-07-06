# Minimal HTTP Server in C

This is a small learning HTTP server written in C. It uses Unix sockets to
listen on `127.0.0.1:8080`, parse simple HTTP request lines, serve static files
from a fixed directory, build HTTP responses, log requests, and clean up
resources.

It is not intended to be a production web server.

## Features

- IPv4 TCP server on `127.0.0.1:8080`
- Single-threaded blocking accept loop
- Supports `GET`
- Serves files from `files/`
- Returns basic `200`, `400`, `404`, `405`, and `500` responses
- Sends `Content-Length`, `Content-Type`, and `Connection: close`
- Logs method, path, status code, and response time

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

## Project Layout

```text
Makefile
server.c        socket setup, accept loop, request handling, file serving
http.c          HTTP request parsing and response building
http.h          public HTTP structs and function declarations
files/          static files served by the server
plan.md         learning milestone plan
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
