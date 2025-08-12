# Keyleport

Remote keyboard/mouse input teleportation between machines over network.

## What it does

- Captures keyboard and mouse on the sender (macOS via SDL3; Windows stub for listener).
- Transmits events over the network using a JSON format.
- Replays events on the receiver (Windows via SendInput; macOS debug emitter prints).
- Uses UDP for high-rate mouse move/scroll; uses TCP for other events. Receiver listens on both simultaneously on the same port.

## High-level architecture

- `src/main.cpp`

  - Entry point; parses CLI, selects sender/receiver mode.
  - Receiver: decodes a single InputEvent per message and emits it.
  - Sender: encodes individual InputEvent into JSON and sends via UDP/TCP based on event type/action.

- `src/keyboard/`

  - `input_event.h` — InputEvent schema and enums:
    - Type: Key, Mouse; Action: Down, Up, Move, Scroll
    - Fields: `code`, `dx`, `dy` (relative only; no absolute x/y)
    - `InputEventJSONConverter` for single-event JSON encode/decode
  - `event_batch.*` — JSON batching utilities (kept for future use; not used by main now)
  - `macos/` — SDL3 listener (relative mode + cursor confinement); simple emitter (logs)
  - `windows/` — Emitter that uses SendInput; listener stub
  - `mapping/` — HID (SDL scancode) → Windows scan code mapping

- `src/networking/server/`

  - Interfaces: `server.h`, `sender.h`, `receiver.h`
  - `cxx/receiver.cpp` — Listens on TCP and UDP together; `select()` loop; UDP `recvfrom()` path; larger buffers; Windows UDP conn-reset guard
  - `cxx/sender.cpp` — TCP connect+send; UDP send with buffer/timeout and connect+send or sendto fallback

- `src/utils/`
  - CLI args (`utils/cli/args.h`) and platform helpers

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

## Sponsor & Support

<a href='https://ko-fi.com/Y8Y315L7NK' target='_blank'><img height='36' style='border:0px;height:36px;' src='https://storage.ko-fi.com/cdn/kofi2.png?v=6' border='0' alt='Buy Me a Coffee at ko-fi.com' /></a>
