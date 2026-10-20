# BT820 C++ Flash Example

This Windows x64 example attaches a Flash image to the BT820 emulator through
the public C API in `bt8xxemu.h`. It configures the BT82X 1280x720 render
target and scanout, switches Flash to FULL mode, copies 140,832 bytes from
Flash offset 4096 to RAM_G address 0 with `CMD_FLASHREAD`, and displays the
288x163 RGB8 bitmap stored in `flash-820-default.bin`. The application runs
until the emulator window closes.

## BT820 SPI Differences

The low-level helpers intentionally differ from the BT817 example:

- `wr8`, `wr16`, and `wr32` send a four-byte address and set bit 31 for writes.
- `rd8`, `rd16`, and `rd32` send a four-byte read address, wait for the nonzero
  read-ready token, and then receive the little-endian value.
- The coprocessor command FIFO is 16 KiB and uses mask `0x3FFF`.
- `CMD_FLASHREAD(0, 4096, 140832)` copies the RGB8 pixels into RAM_G before
  `CMD_SETBITMAP(0, RGB8, 288, 163)` selects them for bitmap handle 0.
- The bitmap is drawn with `VERTEX2F(2592, 2528)` as requested by the example.

The bundled Flash image includes the BT82X 4096-byte Flash bootstrap blob
required for `CMD_FLASHFAST`, followed by the RGB8 image payload.

## Requirements

- Windows x64
- Visual Studio 2022 with the Desktop development with C++ workload
- CMake 3.15 or newer when using the CMake build

## Build with CMake

From the repository root:

```powershell
cmake -S examples/CPP/BT820/Flash -B examples/CPP/BT820/Flash/build -G "Visual Studio 17 2022" -A x64
cmake --build examples/CPP/BT820/Flash/build --config Debug
```

Run:

```powershell
examples/CPP/BT820/Flash/build/bin/Debug/BT820FlashExample.exe
```

Use `--config Release` to create a Release build.

## Build with Visual Studio

Open `VisualStudio/FlashExample.sln`, select `Debug|x64` or `Release|x64`, and
build the solution. The Debug executable is written to:

```text
VisualStudio/bin/Debug/FlashExample.exe
```

## Runtime Files

Both build methods copy these files beside the executable:

```text
bt8xxemu.dll
btflashemu.dll
zlib.dll
flash-820-default.bin
```

The program resolves the Flash image relative to its executable, so it can be
launched from any working directory. On exit it destroys the emulator before
destroying the attached Flash object.
