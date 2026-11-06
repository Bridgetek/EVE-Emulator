# EVE Emulator Release Notes

## Version 5.3.36 - November 6, 2026

### Timing and SD Card Emulation

- `REG_CLOCK` now advances continuously instead of once per frame, reducing
  coprocessor timeout warnings.
- Improved internal EVE SD card emulation accuracy for reset and status-block
  behavior.
- Tested virtual FAT32 against all firmware patches, including stricter
  firmware handling of end-of-chain markers.

### Dependencies

- Updated to zlib 1.4. The runtime DLL is now named `libz.dll` instead of
  `zlib.dll`.

## Version 5.3.35 - October 15, 2026

### Performance

- Added Vulkan accelerated rendering engine backend. Automatically enabled on 
  dedicated GPUs (nVidia/AMD) and integrated AMD Radeon.
- When hardware acceleration is enabled, the software renderer handles frames
  while Vulkan shaders compile, so initialization does not block.

### Flash Emulation

- Added additional NOR flash modules to support all firmware variants
- Added NAND flash modules
- Consolidated the flash emulator libraries into `btflashemu.dll`.
- Added an API to list and select a specific flash device for emulation.

### Rendering

- Added callback for render targets on BT820
- Fixed `REG_ROTATE` behavior and bitmap transforms when rotation is active.
- Added `BT8XXEMU_EmulatorEnableWindowRotate` to automatically reverse rotation
  in the windowed output. This does not affect the Graphics callback.

## Version 5.1.26 - September 25, 2026

### Fixes

- Fixed bitmap and ASTC RAM address masking so that addresses correctly match
  the configured RAM size.

### Performance

- Improved the performance of hardware-matching line, rectangle, and point
  rendering.
- Added the `BT8XXEMU_MATCH_OPTIMIZED` option. This optimization is enabled by
  default.

### Dependencies

- Removed the dependency on the third-party ASTC library. Rendering already
  uses the faster hardware-matching ASTC decoder.
