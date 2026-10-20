# EVE Emulator Release Package

This repository provides the binary release and public header files for the
BT8XX/EVE emulator, together with example projects that show how to launch and
drive the emulator from end-user applications.

The emulator itself is distributed here as prebuilt binaries. Application code
can link against the provided import library/header from C or C++, or call the
DLL from C# through the included P/Invoke wrapper examples.

## Repository Layout

```text
bin/
  bt8xxemu.dll      Runtime DLL for the emulator
  btflashemu.dll    Runtime library for flash emulation
  zlib.dll          Runtime compression dependency

include/
  bt8xxemu.h        Public emulator API header
  bt8xxemu_inttypes.h
                    Internal integer type definitions used by the public header

lib/
  bt8xxemu.lib      Windows import library for C/C++ projects

examples/
  CPP/
    BT817/
      Flash/        C++ example that attaches a Flash image to a BT817
    BT820/
      Flash/        C++ example using BT820 Flash addressing and scanout
      SDCard/       C++ example mounting a folder as a virtual BT820 SD card
  CSharp/
    BT81X/          C# WinForms example for BT81X devices
    BT82X/          C# WinForms example for BT82X devices
    common/         Shared C# wrapper sources used by the sample projects
```

## Supported EVE Devices

The emulator package targets the EVE family including:

- FT80X
- FT81x
- BT815/6
- BT817/8
- BT820

The C# wrappers expose the matching emulator modes through the native enum in the shared wrapper code.

## Specifying Flash or SD Card Images

Storage devices must be configured before launching the emulator:

- Flash image: initialize `BT8XXEMU_FlashParameters`, set `DataFilePath`
  or `Data`, create the Flash object, and assign it to the emulator parameters'
  `Flash` field. `btflashemu.dll` is the default flash library. To choose a
  specific chip, list available devices with `BT8XXEMU_Flash_list` and set
  `DeviceType` to a returned part or family name before creating the Flash
  object.
- SD card image (BT820): set `SDCardFilePath` before calling
  `BT8XXEMU_run`, or use `BT8XXEMU_insertSDCardImage` or
  `BT8XXEMU_insertSDCardFolder` while the emulator is running.

On Windows, file paths in the native API use `eve_tchar_t`, which is
`wchar_t`. Flash writes are temporary by default; set `Persistent` in
`BT8XXEMU_FlashParameters` when changes should be written back to the file.

## Using the Emulator in a C or C++ Project

1. Add `include/` to your compiler include directories.
2. Add `lib/` to your linker library directories.
3. Link your application with `bt8xxemu.lib`.
4. Deploy the runtime DLLs from `bin/` next to your executable, or make sure
   they are available on the process `PATH`:
   - `bt8xxemu.dll`
   - `btflashemu.dll` when attaching Flash
   - `zlib.dll`

Include the public API with:

```cpp
#include "bt8xxemu.h"
```

The current public header defines `BT8XXEMU_VERSION_API` as `19`. The bundled
C# examples still use API version `16`, which the current DLL supports for
compatibility; their P/Invoke wrapper does not expose the new API 19 controls.

## Major Native APIs

The public declarations are in `include/bt8xxemu.h`. Pass
`BT8XXEMU_VERSION_API` to initialization and creation functions so the DLL can
validate the structure layout used by the application.

| API | Purpose |
| --- | --- |
| `BT8XXEMU_version` | Returns emulator version information for logging or diagnostics. |
| `BT8XXEMU_defaults` | Initializes `BT8XXEMU_EmulatorParameters` for a selected EVE device. Always call this before changing individual fields. |
| `BT8XXEMU_run` | Creates and starts an emulator. When `Main` is not supplied, it returns after startup and the calling thread acts as the MCU thread. |
| `BT8XXEMU_isRunning` | Reports whether the emulator is still running. It becomes false after the output window is closed or the emulator is stopped. |
| `BT8XXEMU_stop` | Stops a running emulator. It is safe to call more than once. |
| `BT8XXEMU_destroy` | Stops the emulator if necessary and releases the emulator object. Call it before the process exits. |
| `BT8XXEMU_setFlag` | Enables or disables one emulator flag at runtime and returns its resulting value. Only PWM, HSF, and window-rotation flags can be changed at runtime. |
| `BT8XXEMU_Flash_defaults` | Initializes `BT8XXEMU_FlashParameters` with the default Flash library and capacity. |
| `BT8XXEMU_Flash_list` | Lists the chips available in a Flash library so an application can select a part or family through `BT8XXEMU_FlashParameters.DeviceType`. |
| `BT8XXEMU_Flash_create` | Creates a Flash device from a file, memory buffer, or empty Flash configuration. |
| `BT8XXEMU_Flash_destroy` | Releases a Flash object after every emulator that uses it has been destroyed. |
| `BT8XXEMU_chipSelect` | Asserts or releases the EVE SPI chip-select signal. |
| `BT8XXEMU_transfer` | Exchanges one byte over the emulated EVE SPI bus. Use it between chip-select assertion and release. |
| `BT8XXEMU_touchSetXY` / `BT8XXEMU_touchResetXY` | Supplies or clears touch input when using driverless graphics output. The built-in window handles mouse input automatically. |
| `BT8XXEMU_insertSDCardImage` / `BT8XXEMU_insertSDCardFolder` / `BT8XXEMU_ejectSDCard` | Manages BT820 SD-card media at runtime. |

## Rendering Options

When hardware acceleration is enabled, the software renderer handles frames
while Vulkan shaders compile, so emulator initialization remains responsive.
`BT8XXEMU_rendererDevices` lists available GPU renderers, and
`BT8XXEMU_getRenderer` reports the renderer selected for an emulator.

Set `BT8XXEMU_EmulatorEnableWindowRotate` in the emulator flags to reverse the
rotation of each frame in the default output window. This does not change the
buffer passed to the `Graphics` callback. To enable it after startup, call
`BT8XXEMU_setFlag(emulator, BT8XXEMU_EmulatorEnableWindowRotate, 1)`.
`BT8XXEMU_setWindowRotate` can also select a fixed window rotation at runtime.

## Running BT8XXEMU without Flash

Flash is optional. `BT8XXEMU_defaults` initializes the emulator parameters with
`Flash` set to null, so an application that does not need external Flash can
skip every `BT8XXEMU_Flash_*` call. The shorter sequence is:

1. Initialize `BT8XXEMU_EmulatorParameters` with `BT8XXEMU_defaults`.
2. Configure callbacks or flags if needed.
3. Call `BT8XXEMU_run` and check that it returned an emulator object.
4. Drive the EVE device over the emulated SPI bus or poll
   `BT8XXEMU_isRunning` while the window is open.
5. Call `BT8XXEMU_stop`, then `BT8XXEMU_destroy`.

```cpp
#include <windows.h>

#include "bt8xxemu.h"

int main()
{
    BT8XXEMU_EmulatorParameters parameters{};
    BT8XXEMU_defaults(BT8XXEMU_VERSION_API,
                      &parameters,
                      BT8XXEMU_EmulatorBT817);

    // parameters.Flash remains null: no Flash device will be attached.
    BT8XXEMU_Emulator *emulator = nullptr;
    BT8XXEMU_run(BT8XXEMU_VERSION_API, &emulator, &parameters);
    if (!emulator)
        return 1;

    while (BT8XXEMU_isRunning(emulator))
        Sleep(50);

    BT8XXEMU_stop(emulator);
    BT8XXEMU_destroy(emulator);
    return 0;
}
```

Choose the emulator mode that matches the target EVE device. No Flash object
needs to be created, attached, or destroyed in this configuration.

## Emulator and Flash Call Sequence

The order is important because the emulator parameters retain a pointer to the
Flash object.

1. Initialize `BT8XXEMU_FlashParameters` with
   `BT8XXEMU_Flash_defaults`.
2. Set `DataFilePath`, `SizeBytes`, and any persistence or logging options.
3. Create the Flash object with `BT8XXEMU_Flash_create` and check that the
   returned pointer is not null.
4. Initialize `BT8XXEMU_EmulatorParameters` with `BT8XXEMU_defaults`, passing
   the required `BT8XXEMU_EmulatorMode` such as
   `BT8XXEMU_EmulatorBT817`.
5. Assign the Flash object to `emulatorParameters.Flash` and configure any
   callbacks or emulator flags.
6. Call `BT8XXEMU_run` and check that it returned an emulator object.
7. Drive the device through `BT8XXEMU_chipSelect` and `BT8XXEMU_transfer`, or
   wait while the emulator window is open by polling `BT8XXEMU_isRunning`.
8. Call `BT8XXEMU_stop`, then `BT8XXEMU_destroy`.
9. Call `BT8XXEMU_Flash_destroy` last. The Flash object must outlive the
   emulator that references it.

The essential lifecycle is:

```cpp
#include <cwchar>
#include <iterator>
#include <windows.h>

#include "bt8xxemu.h"

int wmain()
{
    BT8XXEMU_FlashParameters flashParameters{};
    BT8XXEMU_Flash_defaults(BT8XXEMU_VERSION_API, &flashParameters);
    wcscpy_s(flashParameters.DataFilePath,
             std::size(flashParameters.DataFilePath),
             L"flash-817-default.bin");
    flashParameters.SizeBytes = 8 * 1024 * 1024;

    BT8XXEMU_Flash *flash =
        BT8XXEMU_Flash_create(BT8XXEMU_VERSION_API, &flashParameters);
    if (!flash)
        return 1;

    BT8XXEMU_EmulatorParameters emulatorParameters{};
    BT8XXEMU_defaults(BT8XXEMU_VERSION_API,
                      &emulatorParameters,
                      BT8XXEMU_EmulatorBT817);
    emulatorParameters.Flash = flash;

    BT8XXEMU_Emulator *emulator = nullptr;
    BT8XXEMU_run(BT8XXEMU_VERSION_API, &emulator, &emulatorParameters);
    if (!emulator)
    {
        BT8XXEMU_Flash_destroy(flash);
        return 1;
    }

    while (BT8XXEMU_isRunning(emulator))
        Sleep(50);

    BT8XXEMU_stop(emulator);
    BT8XXEMU_destroy(emulator);
    BT8XXEMU_Flash_destroy(flash);
    return 0;
}
```

Production code should resolve relative asset paths against the executable,
check path-buffer limits, and release already-created objects on every failure
path. The C++ example implements those checks.

## Running the BT817 C++ Flash Example

[`examples/CPP/BT817/Flash`](examples/CPP/BT817/Flash) is a minimal Windows x64
application that follows the sequence above. It resolves
`flash-817-default.bin` beside the executable, creates an 8 MiB Flash device,
attaches it to a BT817 emulator, switches Flash to FULL mode, and displays the
288x164 Ducati ASTC image stored at Flash offset 4096. The example includes
only the SPI, coprocessor FIFO, and display-list helpers needed to initialize
the display and render the bundled Flash image through this repository's public
emulator API.

The example provides two build entry points:

- CMake: `examples/CPP/BT817/Flash/CMakeLists.txt`
- Visual Studio 2022: `examples/CPP/BT817/Flash/VisualStudio/FlashExample.sln`

Build with CMake from the repository root:

```powershell
cmake -S examples/CPP/BT817/Flash -B examples/CPP/BT817/Flash/build -G "Visual Studio 17 2022" -A x64
cmake --build examples/CPP/BT817/Flash/build --config Debug
```

The Debug executables are generated at:

```text
examples/CPP/BT817/Flash/build/bin/Debug/BT817FlashExample.exe
examples/CPP/BT817/Flash/VisualStudio/bin/Debug/FlashExample.exe
```

Both builds copy `bt8xxemu.dll`, `btflashemu.dll`, `zlib.dll`, and
`flash-817-default.bin` beside the executable. See the
[example README](examples/CPP/BT817/Flash/README.md) for complete Debug and
Release instructions.

## Running the BT820 C++ Flash Example

[`examples/CPP/BT820/Flash`](examples/CPP/BT820/Flash) follows the same public
Flash/emulator lifecycle as the BT817 example but uses the fifth-generation
BT820 bus and display model. Its `wr8`, `wr16`, and `wr32` helpers send four
address bytes and set the write bit in the most significant address byte. Its
`rd8`, `rd16`, and `rd32` helpers send the four-byte read address, wait for the
BT820 read-ready token, and then receive the value. The BT820 command FIFO is
16 KiB, so command pointers use mask `0x3FFF` instead of the BT817 `0x0FFF`.
These transport and command helpers are private to `eve_flash.cpp`; the example
provides only the CMake and Visual Studio application projects, with no
standalone protocol header or test project.

The example configures the 1280x720 swapchain and scanout registers used by the
BT82X C# sample. After Flash enters FULL mode, it uses
`CMD_FLASHREAD(0, 4096, 140832)` to copy the image into RAM_G, selects bitmap
handle 0, calls `CMD_SETBITMAP(0, RGB8, 288, 163)`, and draws it with
`VERTEX2F(2592, 2528)`. Its Flash file contains the BT82X bootstrap blob
required by `CMD_FLASHFAST` followed by the RGB8 image payload.

Build with CMake from the repository root:

```powershell
cmake -S examples/CPP/BT820/Flash -B examples/CPP/BT820/Flash/build -G "Visual Studio 17 2022" -A x64
cmake --build examples/CPP/BT820/Flash/build --config Debug
```

The two Debug output paths are:

```text
examples/CPP/BT820/Flash/build/bin/Debug/BT820FlashExample.exe
examples/CPP/BT820/Flash/VisualStudio/bin/Debug/FlashExample.exe
```

See the [BT820 example README](examples/CPP/BT820/Flash/README.md) for the
Release build, runtime-file, and execution instructions.

## Running the BT820 C++ SD Card Example

[`examples/CPP/BT820/SDCard`](examples/CPP/BT820/SDCard) launches BT820 without
a Flash object and mounts its bundled `sd` directory as a read-only virtual SD
card with `BT8XXEMU_insertSDCardFolder`. Passing zero for `minimumSize` lets the
emulator calculate the required FAT32 image size from the folder contents.

After mounting the folder, the example applies the same 1280x720 BT820 display
configuration as the Flash example. It clears the display, attaches the SD bus
with `CMD_SDATTACH(OPT_4BIT | OPT_IS_SD, 0)`, selects
`bicycle.anim.reloc` with `CMD_FSSOURCE`, loads it into RAM_G with
`CMD_LOADASSET(0, OPT_FS)`, and displays frame 1 with
`CMD_ANIMFRAME(600, 600, 0, 1)`. Both SD attach and file selection results are
checked before rendering continues.

Build with CMake from the repository root:

```powershell
cmake -S examples/CPP/BT820/SDCard -B examples/CPP/BT820/SDCard/build -G "Visual Studio 17 2022" -A x64
cmake --build examples/CPP/BT820/SDCard/build --config Debug
```

The two Debug output paths are:

```text
examples/CPP/BT820/SDCard/build/bin/Debug/BT820SDCardExample.exe
examples/CPP/BT820/SDCard/VisualStudio/bin/Debug/SDCardExample.exe
```

Both projects copy `bt8xxemu.dll`, `zlib.dll`, and the `sd` folder beside the
executable. When the emulator window closes, the example ejects the SD card
before destroying the emulator. See the
[SD Card example README](examples/CPP/BT820/SDCard/README.md) for complete
build and execution instructions.

## Running the C# Examples

The C# examples are Visual Studio/MSBuild projects targeting .NET Framework
3.5. Each project includes:

- `BT8XXEMUNative.cs`: raw P/Invoke declarations for `bt8xxemu.dll`
- `BT8XXEMU.cs`: managed helper/wrapper code
- `MainWindow.cs`: sample WinForms UI and emulator usage

To run an example:

1. Open one of the solution files in Visual Studio:
   - `examples/CSharp/BT81X/BT8XXEMUCSharpDemo.sln`
   - `examples/CSharp/BT82X/BT8XXEMUCSharpDemo.sln`
2. Build the solution.
3. Run the generated `BT8XXEMUCSharpDemo.exe`.

The projects contain post-build steps that copy the required runtime DLLs from
the repository `bin/` directory into the example output directory. The BT81X
example also copies `flash-817-default.bin` for its flash-emulation setup.

## Redistributing an Application

When distributing an application that uses the emulator, include the executable
or library you built along with the runtime files from `bin/` that it depends
on. Flash examples require `btflashemu.dll`; the BT820 SD Card example does not
attach Flash and therefore deploys only `bt8xxemu.dll` and `zlib.dll`.

## Notes

- This repository is intended for emulator release artifacts and integration
  examples, not for building the emulator library itself from source.
- Keep the DLLs, import library, and public headers from the same release
  together to avoid API or ABI mismatches.
