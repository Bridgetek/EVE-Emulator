"""Display the bundled BT817 ASTC image from an emulated Flash device."""

import argparse
import sys
import time
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from common.bt8xxemu import Emulator
from common.eve_gpu_defs import BT817 as gpu


def initialize_display(emulator):
    emulator.wait_for_boot()
    emulator.write8(gpu.REG_PCLK, 0)
    for address, value in (
        (gpu.REG_HSIZE, 800), (gpu.REG_HCYCLE, 928), (gpu.REG_HOFFSET, 88),
        (gpu.REG_HSYNC0, 0), (gpu.REG_HSYNC1, 48), (gpu.REG_VSIZE, 480),
        (gpu.REG_VCYCLE, 525), (gpu.REG_VOFFSET, 32), (gpu.REG_VSYNC0, 0),
        (gpu.REG_VSYNC1, 3),
    ):
        emulator.write16(address, value)
    for address, value in (
        (gpu.REG_PCLK_POL, 1), (gpu.REG_SWIZZLE, 0),
        (gpu.REG_CSPREAD, 0), (gpu.REG_DITHER, 1),
    ):
        emulator.write8(address, value)
    emulator.write32(gpu.RAM_DL, gpu.CLEAR_COLOR_RGB(0, 0, 0))
    emulator.write32(gpu.RAM_DL + 4, gpu.CLEAR(1, 1, 1))
    emulator.write32(gpu.RAM_DL + 8, gpu.DISPLAY())
    emulator.write8(gpu.REG_DLSWAP, 2)
    emulator.write8(gpu.REG_GPIO, emulator.read8(gpu.REG_GPIO) | 0x80)
    emulator.write8(gpu.REG_PCLK, 2)
    emulator.write8(gpu.REG_PWM_DUTY, 127)


def display_flash(emulator):
    for command, status in ((gpu.CMD_FLASHDETACH, 1), (gpu.CMD_FLASHATTACH, 2)):
        emulator.submit_commands([command])
        if emulator.read8(gpu.REG_FLASH_STATUS) != status:
            raise RuntimeError("BT817 Flash attach failed")
    emulator.submit_commands([gpu.CMD_FLASHFAST, 0])
    if emulator.read8(gpu.REG_FLASH_STATUS) != 3:
        raise RuntimeError("BT817 Flash did not enter FULL mode")
    emulator.submit_commands([
        gpu.CMD_DLSTART, gpu.CLEAR_COLOR_RGB(0, 0, 0), gpu.CLEAR(1, 1, 1),
        gpu.CMD_SETBITMAP, 0x800000 | 128,
        (288 << 16) | gpu.COMPRESSED_RGBA_ASTC_4x4_KHR, 164,
        gpu.BEGIN(gpu.BITMAPS), gpu.VERTEX2F(3000, 3000),
        gpu.END(), gpu.DISPLAY(), gpu.CMD_SWAP,
    ])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="check DLL and Flash image without opening a window")
    args = parser.parse_args()
    image = Path(__file__).with_name("flash-817-default.bin")
    with Emulator(gpu.MODE) as emulator:
        emulator.attach_flash(image)
        print(emulator.version())
        if args.check:
            print("BT817 Flash image and emulator DLL loaded")
            return
        emulator.start()
        initialize_display(emulator)
        display_flash(emulator)
        print("Close the BT817 window or press Ctrl+C to exit.")
        while emulator.is_running():
            time.sleep(0.05)


if __name__ == "__main__":
    main()
