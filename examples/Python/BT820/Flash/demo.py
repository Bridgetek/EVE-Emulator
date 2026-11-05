"""Display the bundled BT820 RGB8 image from an emulated Flash device."""

import argparse
import sys
import time
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from common.bt8xxemu import Emulator
from common.eve_gpu_defs import BT820 as gpu


def initialize_display(emulator):
    emulator.wait_for_boot(timeout=5)
    for address, value in (
        (gpu.REG_FREQUENCY, 72000000), (gpu.REG_SO_MODE, 3),
        (gpu.REG_SO_EN, 1), (gpu.REG_DISP, 1), (gpu.REG_PWM_DUTY, 128),
        (gpu.LVDSPLL_CFG, 0x00300870), (gpu.LVDS_EN, 0),
        (gpu.LVDS_EN, 7), (gpu.I2S_CTL, 0),
        (gpu.I2S_CFG, 0x400), (gpu.REG_I2S_EN, 1),
        (gpu.REG_I2S_FREQ, 0x3CF0),
    ):
        emulator.write32(address, value)
    for address, value in (
        (gpu.REG_HSIZE, 1280), (gpu.REG_VSIZE, 720),
        (gpu.REG_HCYCLE, 960), (gpu.REG_HOFFSET, 65296),
        (gpu.REG_HSYNC0, 0), (gpu.REG_HSYNC1, 65456),
        (gpu.REG_VCYCLE, 625), (gpu.REG_VOFFSET, 65464),
        (gpu.REG_VSYNC0, 0), (gpu.REG_VSYNC1, 65512),
    ):
        emulator.write16(address, value)
    for address, value in (
        (gpu.REG_SC0_SIZE, 2), (gpu.REG_SC0_PTR0, 0x07ADD000),
        (gpu.REG_SC0_PTR1, 0x0783A000), (gpu.REG_RE_DEST, 0xFFFF00FF),
        (gpu.REG_RE_FORMAT, gpu.RGB8), (gpu.REG_RE_W, 1280),
        (gpu.REG_RE_H, 720), (gpu.REG_SO_SOURCE, 0xFFFF00FF),
        (gpu.REG_SO_FORMAT, gpu.RGB8),
    ):
        emulator.write32(address, value)
    emulator.write8(gpu.REG_PCLK_POL, 0)
    if emulator.read16(gpu.REG_HSIZE) != 1280 or emulator.read16(gpu.REG_VSIZE) != 720:
        raise RuntimeError("BT820 display timing did not initialize")


def display_flash(emulator):
    for command, status in ((gpu.CMD_FLASHDETACH, 1), (gpu.CMD_FLASHATTACH, 2)):
        emulator.submit_commands([command])
        if emulator.read8(gpu.REG_FLASH_STATUS) != status:
            raise RuntimeError("BT820 Flash attach failed")
    emulator.submit_commands([gpu.CMD_FLASHFAST, 0])
    if emulator.read8(gpu.REG_FLASH_STATUS) != 3:
        raise RuntimeError("BT820 Flash did not enter FULL mode")
    emulator.submit_commands([gpu.CMD_DLSTART, gpu.CLEAR_COLOR_RGB(0, 0, 0),
                              gpu.CLEAR(1, 1, 1)])
    emulator.submit_commands([
        gpu.CMD_FLASHREAD, 0, 4096, 140832, gpu.BITMAP_HANDLE(0),
        gpu.CMD_SETBITMAP, 0, (288 << 16) | gpu.RGB8, 163,
        gpu.BEGIN(gpu.BITMAPS), gpu.VERTEX2F(2592, 2528), gpu.END(),
    ])
    emulator.submit_commands([gpu.DISPLAY(), gpu.CMD_SWAP])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="check DLL and Flash image without opening a window")
    args = parser.parse_args()
    image = Path(__file__).with_name("flash-820-default.bin")
    with Emulator(gpu.MODE) as emulator:
        emulator.attach_flash(image)
        print(emulator.version())
        if args.check:
            print("BT820 Flash image and emulator DLL loaded")
            return
        emulator.start()
        initialize_display(emulator)
        display_flash(emulator)
        print("Close the BT820 window or press Ctrl+C to exit.")
        while emulator.is_running():
            time.sleep(0.05)


if __name__ == "__main__":
    main()
