# BT817 C++ Flash Example

This Windows x64 example attaches a Flash image to the BT817 emulator through
the public C API in `bt8xxemu.h`. It uses `BT8XXEMU_Flash_create`, initializes
an 800x480 display over the emulated SPI bus, switches Flash to FULL mode, and
displays the 288x164 ASTC image stored at offset 4096 in the Flash file. The
application keeps running until the emulator window is closed.

## Requirements

- Windows x64
- Visual Studio 2022 with the Desktop development with C++ workload
- CMake 3.15 or newer when using the CMake build

The example uses the emulator header, import library, and runtime DLLs from
this repository. The small `eve_flash.cpp` module contains only the register,
coprocessor FIFO, and display-list operations needed to port `eve_load_flash()`;
no EVE-MCU-Dev installation is required.

## Build with CMake

From the repository root:

```powershell
cmake -S examples/CPP/BT817/Flash -B examples/CPP/BT817/Flash/build -G "Visual Studio 17 2022" -A x64
cmake --build examples/CPP/BT817/Flash/build --config Debug
```

Run:

```powershell
examples/CPP/BT817/Flash/build/bin/Debug/BT817FlashExample.exe
```

Use `--config Release` to create a Release build.

## Build with Visual Studio

Open `VisualStudio/FlashExample.sln`, select either `Debug|x64` or
`Release|x64`, and build the solution. The Debug executable is written to:

```text
VisualStudio/bin/Debug/FlashExample.exe
```

## Runtime Files

Both build methods copy these files beside the executable:

```text
bt8xxemu.dll
btflashemu.dll
zlib.dll
flash-817-default.bin
```

The program resolves `flash-817-default.bin` relative to its executable, so it
can be launched from any working directory. Closing the emulator window stops
the application and releases both the emulator and Flash objects. A successful
run shows the Ducati ASTC image from Flash on a black background.
