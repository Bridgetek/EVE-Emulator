"""Windows x64 ctypes binding for the bundled BT8XX emulator DLL."""

import ctypes
import os
import sys
import time
from pathlib import Path

from common.eve_gpu_defs import BT820 as BT820_DEFS, for_mode


API_VERSION = 19
BT820 = BT820_DEFS.MODE


class EmulatorParameters(ctypes.Structure):
    _fields_ = [
        ("Main", ctypes.c_void_p),
        ("Flags", ctypes.c_int),
        ("Mode", ctypes.c_int),
        ("MousePressure", ctypes.c_uint32),
        ("ExternalFrequency", ctypes.c_uint32),
        ("ReduceGraphicsThreads", ctypes.c_uint32),
        ("RendererDeviceUuid", ctypes.c_uint8 * 16),
        ("MCUSleep", ctypes.c_void_p),
        ("MCUWake", ctypes.c_void_p),
        ("RomFilePath", ctypes.c_wchar * 260),
        ("OtpFilePath", ctypes.c_wchar * 260),
        ("CoprocessorRomFilePath", ctypes.c_wchar * 260),
        ("Graphics", ctypes.c_void_p),
        ("RenderTarget", ctypes.c_void_p),
        ("Interrupt", ctypes.c_void_p),
        ("Log", ctypes.c_void_p),
        ("Close", ctypes.c_void_p),
        ("UserContext", ctypes.c_void_p),
        ("Flash", ctypes.c_void_p),
        ("RamGSizeBytes", ctypes.c_ssize_t),
        ("SDCardFilePath", ctypes.c_wchar * 260),
    ]


class FlashParameters(ctypes.Structure):
    _fields_ = [
        ("LibraryName", ctypes.c_wchar * 26),
        ("DeviceType", ctypes.c_wchar * 26),
        ("SizeBytes", ctypes.c_ssize_t),
        ("DataFilePath", ctypes.c_wchar * 260),
        ("StatusFilePath", ctypes.c_wchar * 260),
        ("Persistent", ctypes.c_int),
        ("StdOut", ctypes.c_int),
        ("Data", ctypes.c_void_p),
        ("DataSizeBytes", ctypes.c_ssize_t),
        ("Log", ctypes.c_void_p),
        ("UserContext", ctypes.c_void_p),
    ]


def _bind(dll):
    dll.BT8XXEMU_version.argtypes = []
    dll.BT8XXEMU_version.restype = ctypes.c_char_p
    dll.BT8XXEMU_defaults.argtypes = [
        ctypes.c_uint32, ctypes.POINTER(EmulatorParameters), ctypes.c_int,
    ]
    dll.BT8XXEMU_defaults.restype = None
    dll.BT8XXEMU_run.argtypes = [
        ctypes.c_uint32, ctypes.POINTER(ctypes.c_void_p),
        ctypes.POINTER(EmulatorParameters),
    ]
    dll.BT8XXEMU_run.restype = None
    dll.BT8XXEMU_isRunning.argtypes = [ctypes.c_void_p]
    dll.BT8XXEMU_isRunning.restype = ctypes.c_int
    dll.BT8XXEMU_stop.argtypes = [ctypes.c_void_p]
    dll.BT8XXEMU_stop.restype = None
    dll.BT8XXEMU_destroy.argtypes = [ctypes.c_void_p]
    dll.BT8XXEMU_destroy.restype = None
    dll.BT8XXEMU_chipSelect.argtypes = [ctypes.c_void_p, ctypes.c_int]
    dll.BT8XXEMU_chipSelect.restype = None
    dll.BT8XXEMU_transfer.argtypes = [ctypes.c_void_p, ctypes.c_uint8]
    dll.BT8XXEMU_transfer.restype = ctypes.c_uint8
    dll.BT8XXEMU_Flash_defaults.argtypes = [ctypes.c_uint32, ctypes.POINTER(FlashParameters)]
    dll.BT8XXEMU_Flash_defaults.restype = None
    dll.BT8XXEMU_Flash_create.argtypes = [ctypes.c_uint32, ctypes.POINTER(FlashParameters)]
    dll.BT8XXEMU_Flash_create.restype = ctypes.c_void_p
    dll.BT8XXEMU_Flash_destroy.argtypes = [ctypes.c_void_p]
    dll.BT8XXEMU_Flash_destroy.restype = None
    dll.BT8XXEMU_insertSDCardFolder.argtypes = [
        ctypes.c_void_p, ctypes.c_wchar_p, ctypes.c_ssize_t, ctypes.c_int,
    ]
    dll.BT8XXEMU_insertSDCardFolder.restype = ctypes.c_ssize_t
    dll.BT8XXEMU_ejectSDCard.argtypes = [ctypes.c_void_p]
    dll.BT8XXEMU_ejectSDCard.restype = None


class Emulator:
    """Own a native emulator handle and its DLL search path."""

    def __init__(self, mode, bin_dir=None):
        if sys.platform != "win32" or ctypes.sizeof(ctypes.c_void_p) != 8:
            raise RuntimeError("EVE emulator requires 64-bit Python on Windows")
        if ctypes.sizeof(EmulatorParameters) != 2208 or ctypes.sizeof(FlashParameters) != 1192:
            raise RuntimeError("ctypes layout does not match the x64 API 19 header")

        self.mode = mode
        self.gpu = for_mode(mode)
        directory = Path(bin_dir) if bin_dir else Path(__file__).resolve().parents[3] / "bin"
        self._bin_dir = directory
        self._flash_dll = None
        self._dll_directory = os.add_dll_directory(str(directory))
        try:
            self.dll = ctypes.CDLL(str(directory / "bt8xxemu.dll"))
            _bind(self.dll)
            self.parameters = EmulatorParameters()
            self.dll.BT8XXEMU_defaults(API_VERSION, ctypes.byref(self.parameters), mode)
        except BaseException:
            self._dll_directory.close()
            raise
        self.handle = ctypes.c_void_p()
        self.flash = ctypes.c_void_p()
        self._sd_mounted = False

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()

    def version(self):
        return self.dll.BT8XXEMU_version().decode("utf-8", errors="replace")

    def start(self):
        if self.handle.value:
            raise RuntimeError("Emulator is already started")
        self.dll.BT8XXEMU_run(
            API_VERSION, ctypes.byref(self.handle), ctypes.byref(self.parameters),
        )
        if not self.handle.value:
            raise RuntimeError("Unable to start EVE emulator")

    def attach_flash(self, image_path, size_bytes=8 * 1024 * 1024):
        if self.handle.value or self.flash.value:
            raise RuntimeError("Attach one Flash device before starting the emulator")
        image_path = Path(image_path).resolve()
        if not image_path.is_file() or len(str(image_path)) >= 260:
            raise ValueError("Flash image is missing or its path is too long")
        params = FlashParameters()
        self.dll.BT8XXEMU_Flash_defaults(API_VERSION, ctypes.byref(params))
        params.DataFilePath = str(image_path)
        params.SizeBytes = size_bytes
        # The emulator loads this library by name, so keep its absolute-path load alive.
        self._flash_dll = ctypes.CDLL(str(self._bin_dir / "btflashemu.dll"))
        handle = self.dll.BT8XXEMU_Flash_create(API_VERSION, ctypes.byref(params))
        if not handle:
            raise RuntimeError("Unable to create Flash emulator")
        self.flash = ctypes.c_void_p(handle)
        self.parameters.Flash = self.flash.value

    def mount_sd_folder(self, folder, minimum_size=64 * 1024 * 1024, read_only=False):
        if not self.handle.value or self._sd_mounted:
            raise RuntimeError("Start the emulator before mounting one SD-card folder")
        folder = Path(folder).resolve()
        if not folder.is_dir():
            raise ValueError("SD-card folder does not exist")
        size = self.dll.BT8XXEMU_insertSDCardFolder(
            self.handle, str(folder), minimum_size, int(read_only),
        )
        if size <= 0:
            raise RuntimeError("Unable to mount SD-card folder")
        self._sd_mounted = True
        return size

    def is_running(self):
        return bool(self.handle.value and self.dll.BT8XXEMU_isRunning(self.handle))

    def close(self):
        if self.handle.value:
            try:
                if self._sd_mounted:
                    self.dll.BT8XXEMU_ejectSDCard(self.handle)
                    self._sd_mounted = False
                self.dll.BT8XXEMU_stop(self.handle)
            finally:
                self.dll.BT8XXEMU_destroy(self.handle)
                self.handle = ctypes.c_void_p()
        if self.flash.value:
            self.dll.BT8XXEMU_Flash_destroy(self.flash)
            self.flash = ctypes.c_void_p()
        if self._dll_directory is not None:
            self._dll_directory.close()
            self._dll_directory = None

    def _transfer(self, address, write, values):
        if not self.handle.value:
            raise RuntimeError("Start the emulator before transferring SPI data")
        self.dll.BT8XXEMU_chipSelect(self.handle, 1)
        try:
            if self.mode == BT820:
                header = (
                    ((address >> 24) & 0x7F) | (0x80 if write else 0),
                    address >> 16, address >> 8, address,
                )
            else:
                header = (
                    ((address >> 16) & 0x3F) | (0x80 if write else 0),
                    address >> 8, address,
                )
            for value in header:
                self.dll.BT8XXEMU_transfer(self.handle, value & 0xFF)
            if not write:
                if self.mode == BT820:
                    for _ in range(100000):
                        if self.dll.BT8XXEMU_transfer(self.handle, 0):
                            break
                    else:
                        raise TimeoutError("BT820 SPI read-ready token was not received")
                else:
                    self.dll.BT8XXEMU_transfer(self.handle, 0)
            return [self.dll.BT8XXEMU_transfer(self.handle, value) for value in values]
        finally:
            self.dll.BT8XXEMU_chipSelect(self.handle, 0)

    def write8(self, address, value):
        self._transfer(address, True, value.to_bytes(1, "little"))

    def write16(self, address, value):
        data = value.to_bytes(2, "little")
        self._transfer(address, True, data + (b"\0\0" if self.mode == BT820 else b""))

    def write32(self, address, value):
        self._transfer(address, True, value.to_bytes(4, "little"))

    def read8(self, address):
        return self._transfer(address, False, [0])[0]

    def read16(self, address):
        return int.from_bytes(self._transfer(address, False, [0] * 2), "little")

    def read32(self, address):
        return int.from_bytes(self._transfer(address, False, [0] * 4), "little")

    def wait_for_boot(self, timeout=3):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline and self.is_running():
            if self.mode == BT820:
                ready = self.read32(self.gpu.REG_ID) == 0x7C
            else:
                ready = (self.read8(self.gpu.REG_ID) == 0x7C
                         and self.read8(self.gpu.REG_CPURESET) == 0)
            if ready:
                return
            time.sleep(0.01)
        raise TimeoutError("EVE emulator did not finish booting")

    def submit_commands(self, words, result_index=None, timeout=5):
        if self.mode == BT820:
            read_pointer, write_pointer = self.read32, self.write32
        else:
            read_pointer, write_pointer = self.read16, self.write16
        ram_cmd = self.gpu.RAM_CMD
        read_reg = self.gpu.REG_CMD_READ
        write_reg = self.gpu.REG_CMD_WRITE
        mask = self.gpu.EVE_CMD_FIFO_MASK
        if len(words) * 4 > mask + 1 - 4:
            raise ValueError("Command batch is too large for the FIFO")
        start = read_pointer(write_reg) & mask

        def wait_for(pointer):
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline and self.is_running():
                current = read_pointer(read_reg) & mask
                if current & 0x3:
                    raise RuntimeError("EVE coprocessor command FIFO fault")
                if current == pointer:
                    return
                time.sleep(0.001)
            raise TimeoutError("EVE coprocessor command FIFO did not drain")

        wait_for(start)
        end = start
        for word in words:
            self.write32(ram_cmd + end, word)
            end = (end + 4) & mask
        write_pointer(write_reg, end)
        wait_for(end)
        if result_index is not None:
            return self.read32(ram_cmd + ((start + result_index * 4) & mask))
