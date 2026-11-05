"""Display an animation from a virtual BT820 SD-card folder."""

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


def display_sdcard(emulator):
    emulator.submit_commands([gpu.CMD_DLSTART, gpu.CLEAR(1, 1, 1)])
    attach_result = emulator.submit_commands(
        [gpu.CMD_SDATTACH, gpu.OPT_4BIT | gpu.OPT_IS_SD, 0], result_index=2)
    if attach_result != 0:
        raise RuntimeError("CMD_SDATTACH failed: " + str(attach_result))
    name = b"bicycle.anim.reloc\0"
    name += b"\0" * (-len(name) % 4)
    source = [gpu.CMD_FSSOURCE]
    source.extend(int.from_bytes(name[i:i + 4], "little") for i in range(0, len(name), 4))
    source.append(0)
    source_result = emulator.submit_commands(source, result_index=len(source) - 1)
    if source_result != 0:
        raise RuntimeError("CMD_FSSOURCE failed: " + str(source_result))
    emulator.submit_commands([gpu.CMD_LOADASSET, 0, gpu.OPT_FS])
    emulator.submit_commands([gpu.CMD_ANIMFRAME, (300 << 16) | 400, 0, 1,
                              gpu.DISPLAY(), gpu.CMD_SWAP])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="check DLL and SD asset without opening a window")
    args = parser.parse_args()
    folder = Path(__file__).with_name("sd")
    if not (folder / "bicycle.anim.reloc").is_file():
        parser.error("SD-card asset is missing")
    with Emulator(gpu.MODE) as emulator:
        print(emulator.version())
        if args.check:
            print("BT820 SD-card asset and emulator DLL found")
            return
        emulator.start()
        size = emulator.mount_sd_folder(folder, read_only=False)
        print("Mounted SD-card folder:", size, "bytes")
        initialize_display(emulator)
        display_sdcard(emulator)
        print("Close the BT820 window or press Ctrl+C to exit.")
        while emulator.is_running():
            time.sleep(0.05)


if __name__ == "__main__":
    main()
