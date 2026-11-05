# BT820 Python Flash Example

This Windows x64 example mirrors the C++ BT820 Flash example. It creates an
8 MiB Flash device from `flash-820-default.bin`, configures the 1280x720
display and swapchain, reads the RGB8 bitmap from Flash into RAM_G, and draws
it through the coprocessor command FIFO.

Use 64-bit Python 3.8 or newer. From the repository root:

```powershell
python examples/Python/BT820/Flash/demo.py --check
python examples/Python/BT820/Flash/demo.py
```

Close the emulator window or press Ctrl+C to exit. The example uses the
shared binding and BT820 GPU definitions in `examples/Python/common/`; its
display setup is in `demo.py`.
