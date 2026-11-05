# BT820 Python SD Card Example

This Windows x64 example mirrors the C++ BT820 SD-card example. It mounts the
bundled `sd` folder as a virtual SD card, configures the 1280x720 display,
loads `bicycle.anim.reloc`, and draws animation frame 1. No Flash device is
attached.

Use 64-bit Python 3.8 or newer. From the repository root:

```powershell
python examples/Python/BT820/SDCard/demo.py --check
python examples/Python/BT820/SDCard/demo.py
```

Close the emulator window or press Ctrl+C to exit. The shared binding ejects
the SD card before stopping the emulator. Register and command names come
from `examples/Python/common/eve_gpu_defs.py`.
