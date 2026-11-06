#pragma once

// Minimal BT817 host abstraction for the benchmark.
//
// All scene code talks to EVE only through EveHal, so porting the benchmark to
// real hardware (for example an IDM2040-7A or ME817EV board) means replacing
// the four transport functions in eve_hal.cpp (chip select and byte transfer)
// with the board's SPI driver. Everything else stays the same.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "bt8xxemu.h"

namespace bench
{
namespace reg
{
constexpr uint32_t RAM_G = 0x000000;
constexpr uint32_t RAM_G_SIZE = 0x100000; // 1 MiB on BT817
constexpr uint32_t RAM_DL = 0x300000;
constexpr uint32_t RAM_DL_SIZE = 0x2000;  // 8 KiB display list
constexpr uint32_t RAM_ERR_REPORT = 0x309800;

constexpr uint32_t REG_ID = 0x302000;
constexpr uint32_t REG_FRAMES = 0x302004;
constexpr uint32_t REG_CPURESET = 0x302020;
constexpr uint32_t REG_HCYCLE = 0x30202C;
constexpr uint32_t REG_HOFFSET = 0x302030;
constexpr uint32_t REG_HSIZE = 0x302034;
constexpr uint32_t REG_HSYNC0 = 0x302038;
constexpr uint32_t REG_HSYNC1 = 0x30203C;
constexpr uint32_t REG_VCYCLE = 0x302040;
constexpr uint32_t REG_VOFFSET = 0x302044;
constexpr uint32_t REG_VSIZE = 0x302048;
constexpr uint32_t REG_VSYNC0 = 0x30204C;
constexpr uint32_t REG_VSYNC1 = 0x302050;
constexpr uint32_t REG_DLSWAP = 0x302054;
constexpr uint32_t REG_DITHER = 0x302060;
constexpr uint32_t REG_SWIZZLE = 0x302064;
constexpr uint32_t REG_CSPREAD = 0x302068;
constexpr uint32_t REG_PCLK_POL = 0x30206C;
constexpr uint32_t REG_PCLK = 0x302070;
constexpr uint32_t REG_GPIO = 0x302094;
constexpr uint32_t REG_PWM_DUTY = 0x3020D4;
constexpr uint32_t REG_CMD_READ = 0x3020F8;
constexpr uint32_t REG_CMD_WRITE = 0x3020FC;
constexpr uint32_t REG_CMD_DL = 0x302100;
constexpr uint32_t REG_TOUCH_TAG = 0x30212C;
constexpr uint32_t REG_CMDB_SPACE = 0x302574;
constexpr uint32_t REG_CMDB_WRITE = 0x302578;
constexpr uint32_t REG_FLASH_STATUS = 0x3025F0;
constexpr uint32_t REG_UNDERRUN = 0x30260C;

constexpr uint16_t CMD_FIFO_MASK = 0x0FFF;
constexpr uint16_t CMD_FIFO_FAULT = 0x0FFF;
} // namespace reg

class EveHal
{
public:
    explicit EveHal(BT8XXEMU_Emulator *emulator);

    uint8_t rd8(uint32_t address);
    uint16_t rd16(uint32_t address);
    uint32_t rd32(uint32_t address);
    void wr8(uint32_t address, uint8_t value);
    void wr16(uint32_t address, uint16_t value);
    void wr32(uint32_t address, uint32_t value);
    void wrMem(uint32_t address, const uint8_t *data, size_t size);

    // Streams coprocessor words through REG_CMDB_WRITE, waiting for FIFO space.
    bool sendCommands(const std::vector<uint32_t> &words);
    // Waits until the coprocessor has consumed every command.
    bool waitIdle(unsigned timeoutMs = 3000);
    // True when the coprocessor reported a fault (REG_CMD_READ == 0xFFF).
    bool faulted();
    // The coprocessor's fault text from RAM_ERR_REPORT, if any.
    std::string faultReport();

    bool running() const;

    // Resets the coprocessor after a fault (as the BT817 programming guide describes).
    void recoverCoprocessor();

    // Host-to-EVE SPI bytes in write transactions (address and data) since the
    // last reset. Status polling reads are not counted, because a real host can
    // replace them with the INT_N interrupt.
    uint64_t bytesSent() const { return bytes_; }
    void resetBytesSent() { bytes_ = 0; }

private:
    void select();
    void deselect();
    uint8_t transfer(uint8_t value);
    void beginWrite(uint32_t address);
    void beginRead(uint32_t address);

    BT8XXEMU_Emulator *emulator_;
    uint64_t bytes_ = 0;
    bool writing_ = false;
};
} // namespace bench
