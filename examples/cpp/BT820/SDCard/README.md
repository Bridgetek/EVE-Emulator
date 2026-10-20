# BT820 C++ SD Card Example

This Windows x64 example shows how to expose a host folder as a virtual BT820
SD card with the public API in `bt8xxemu.h`. It launches BT820 without Flash,
mounts the bundled `sd` directory read-only with
`BT8XXEMU_insertSDCardFolder`, configures the same 1280x720 display used by the
BT820 Flash example, loads `bicycle.anim.reloc`, and displays animation frame 1.

## SD Card and EVE Command Sequence

The application resolves `sd` relative to the executable and mounts it with:

```cpp
const ptrdiff_t size =
    BT8XXEMU_insertSDCardFolder(emulator, sdFolder.c_str(), 0, 1);
```

`minimumSize` is zero, so the emulator calculates the smallest suitable FAT32
image. The final argument enables read-only behavior. The folder remains on the
host; the emulator creates the virtual filesystem in memory.

After mounting, `eve_load_sdcard()` sends this BT820 command flow through the
16 KiB coprocessor FIFO:

```text
CLEAR(1, 1, 1)
CMD_SDATTACH(OPT_4BIT | OPT_IS_SD, 0)
CMD_FSSOURCE("bicycle.anim.reloc", 0)
CMD_LOADASSET(0, OPT_FS, "")
CMD_ANIMFRAME(600, 600, 0, 1)
```

The example checks that `CMD_SDATTACH` and `CMD_FSSOURCE` return zero, then
finishes the display list with `DISPLAY()` and `CMD_SWAP`. When the emulator
window closes, it calls `BT8XXEMU_ejectSDCard` before destroying the emulator.

## Requirements

- Windows x64
- Visual Studio 2022 with the Desktop development with C++ workload
- CMake 3.15 or newer when using the CMake build

## Build with CMake

From the repository root:

```powershell
cmake -S examples/CPP/BT820/SDCard -B examples/CPP/BT820/SDCard/build -G "Visual Studio 17 2022" -A x64
cmake --build examples/CPP/BT820/SDCard/build --config Debug
```

Run:

```powershell
examples/CPP/BT820/SDCard/build/bin/Debug/BT820SDCardExample.exe
```

Use `--config Release` to create a Release build.

## Build with Visual Studio

Open `VisualStudio/SDCardExample.sln`, select `Debug|x64` or `Release|x64`,
and build the solution. The Debug executable is written to:

```text
VisualStudio/bin/Debug/SDCardExample.exe
```

## Runtime Files

Both build methods copy these items beside the executable:

```text
bt8xxemu.dll
zlib.dll
sd/
  bicycle.anim.reloc
```

`btflashemu.dll` is not required because this example does not attach a Flash
device. The executable-relative folder lookup allows the program to be launched
from any working directory.
