# libera-protocol

`libera-protocol` is the shared wire-format and session library for the Libera
network protocol.

The protocol is designed to support both:

- raw point streams for receivers with limited memory
- frame-aware streams with explicit boundary records for receivers that can
  ingest complete frames

The initial library contains the protocol types, binary record codec, point
encoder/decoder, and small sender/receiver stream helpers. UDP discovery and TCP
transport code will build on these primitives.

## Build

```bash
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Targets

- `libera-protocol-core`
- `libera-protocol::core`

## Status

This repository is at MVP scaffolding stage. The record codec and sender/receiver
helpers are intentionally small so Libera and Libera Link can integrate against
the same protocol types before the network transport is filled in.

