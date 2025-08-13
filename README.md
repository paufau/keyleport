# Keyleport

Remote keyboard/mouse input teleportation between machines over network.

## Sponsor & Support

This project is fully developed and maintained thanks to the generous support of its sponsors and contributors.

<a href='https://ko-fi.com/Y8Y315L7NK' target='_blank'><img height='36' style='border:0px;height:36px;' src='https://storage.ko-fi.com/cdn/kofi2.png?v=6' border='0' alt='Buy Me a Coffee at ko-fi.com' /></a>

## What it does

- Captures keyboard and mouse on the sender and transmits events over network to a receiving device, then emulates input on the receiver.
- Uses UDP for high-rate mouse move/scroll; uses TCP for key presses.

## Features Support

| Can do / OS      | MacOS | Windows | Linux |
| ---------------- | ----- | ------- | ----- |
| Send TCP/UDP     | ✅    | ✅      | ❌    |
| Receive TCP/UDP  | ✅    | ✅      | ❌    |
| Discover Servers | ✅    | ✅      | ❌    |
| Capture Input    | ✅    | ❌      | ❌    |
| Emulate Input    | ❌    | ✅      | ❌    |
| Display UI       | ❌    | ❌      | ❌    |

## High-level architecture

- `src/flows`

  - Contains possible work modes

- `src/keyboard/`

  - Logic for input handling and emulation

- `src/networking`

  - Part responsible for network connection, data transfer and running servers discovery

- `src/utils/`
  - Platform helpers

## Build

- macOS/Linux: requires CMake; SDL3 detected via pkg-config. A helper script is included:

```sh
./scripts/compile.sh
```

- Windows: CMake project links `user32` and `ws2_32`; build with your preferred generator (e.g., Visual Studio). A CI workflow for Windows exists.

## Run

Start a receiver on the target machine:

```sh
./build/keyleport --mode receiver --port 8080
```

Start a sender on the source machine, pointing to the receiver IP:

```sh
./build/keyleport --mode sender --ip 192.168.x.y --port 8080
```

Transport selection (in sender):

- UDP for mouse Move/Scroll
- TCP for keyboard and mouse button Down/Up

## License

MIT License — see `LICENSE` (© [Pavel Pakseev](https://www.linkedin.com/in/pavel-pakseev/)).

### Future Features

- Key mapping (eg: windows switches language like mac os)
- Local events emulation (in key-mapping mode only. so it's like using macros)
- Optimized binary data transferring
- Secured data transferring
- Possibly network events batching
