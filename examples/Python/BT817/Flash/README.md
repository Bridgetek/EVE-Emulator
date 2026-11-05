# BT817 Python Flash Example

This Windows x64 example mirrors the C++ BT817 Flash example. It creates an
8 MiB Flash device from `flash-817-default.bin`, configures an 800x480 display,
switches Flash to FULL mode, and displays the bundled ASTC image.

Use 64-bit Python 3.8 or newer. From the repository root:

```powershell
python examples/Python/BT817/Flash/demo.py --check
python examples/Python/BT817/Flash/demo.py
```

Close the emulator window or press Ctrl+C to exit. The example uses the
shared binding and BT817 GPU definitions in `examples/Python/common/` and
needs no Python packages.
