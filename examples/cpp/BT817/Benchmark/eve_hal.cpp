#include "eve_hal.h"

#include <algorithm>
#include <chrono>
#include <thread>

namespace bench
{
EveHal::EveHal(BT8XXEMU_Emulator *emulator) : emulator_(emulator) {}

// ---- Transport: replace these four with a board SPI driver on hardware ----
void EveHal::select() { BT8XXEMU_chipSelect(emulator_, 1); }
void EveHal::deselect()
{
    writing_ = false;
    BT8XXEMU_chipSelect(emulator_, 0);
}
uint8_t EveHal::transfer(uint8_t value)
{
    if (writing_)
        ++bytes_;
    return BT8XXEMU_transfer(emulator_, value);
}
bool EveHal::running() const { return BT8XXEMU_isRunning(emulator_) != 0; }
// ---------------------------------------------------------------------------

void EveHal::beginWrite(uint32_t address)
{
    select();
    writing_ = true;
    transfer(static_cast<uint8_t>(0x80 | ((address >> 16) & 0x3F)));
    transfer(static_cast<uint8_t>(address >> 8));
    transfer(static_cast<uint8_t>(address));
}

void EveHal::beginRead(uint32_t address)
{
    select();
    transfer(static_cast<uint8_t>((address >> 16) & 0x3F));
    transfer(static_cast<uint8_t>(address >> 8));
    transfer(static_cast<uint8_t>(address));
    transfer(0); // dummy byte
}

uint8_t EveHal::rd8(uint32_t address)
{
    beginRead(address);
    const uint8_t value = transfer(0);
    deselect();
    return value;
}

uint16_t EveHal::rd16(uint32_t address)
{
    beginRead(address);
    const uint16_t low = transfer(0);
    const uint16_t high = transfer(0);
    deselect();
    return static_cast<uint16_t>(low | (high << 8));
}

uint32_t EveHal::rd32(uint32_t address)
{
    beginRead(address);
    uint32_t value = 0;
    for (int i = 0; i < 4; ++i)
        value |= static_cast<uint32_t>(transfer(0)) << (8 * i);
    deselect();
    return value;
}

void EveHal::wr8(uint32_t address, uint8_t value)
{
    beginWrite(address);
    transfer(value);
    deselect();
}

void EveHal::wr16(uint32_t address, uint16_t value)
{
    beginWrite(address);
    transfer(static_cast<uint8_t>(value));
    transfer(static_cast<uint8_t>(value >> 8));
    deselect();
}

void EveHal::wr32(uint32_t address, uint32_t value)
{
    beginWrite(address);
    for (int i = 0; i < 4; ++i)
        transfer(static_cast<uint8_t>(value >> (8 * i)));
    deselect();
}

void EveHal::wrMem(uint32_t address, const uint8_t *data, size_t size)
{
    beginWrite(address);
    for (size_t i = 0; i < size; ++i)
        transfer(data[i]);
    deselect();
}

void EveHal::recoverCoprocessor()
{
    wr8(reg::REG_CPURESET, 1);
    wr16(reg::REG_CMD_READ, 0);
    wr16(reg::REG_CMD_WRITE, 0);
    wr16(reg::REG_CMD_DL, 0);
    wr8(reg::REG_CPURESET, 0);
}

bool EveHal::faulted()
{
    return rd16(reg::REG_CMD_READ) == reg::CMD_FIFO_FAULT;
}

std::string EveHal::faultReport()
{
    std::string text;
    for (uint32_t i = 0; i < 128; ++i)
    {
        const char c = static_cast<char>(rd8(reg::RAM_ERR_REPORT + i));
        if (c == 0)
            break;
        text.push_back(c);
    }
    return text;
}

bool EveHal::sendCommands(const std::vector<uint32_t> &words)
{
    size_t index = 0;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(3000);
    while (index < words.size())
    {
        if (!running())
            return false;
        const uint16_t space = rd16(reg::REG_CMDB_SPACE) & reg::CMD_FIFO_MASK;
        if (space == reg::CMD_FIFO_FAULT || faulted())
            return false;
        const size_t freeWords = space / 4;
        if (freeWords == 0)
        {
            if (std::chrono::steady_clock::now() > deadline)
                return false;
            std::this_thread::yield();
            continue;
        }
        const size_t count = std::min(freeWords, words.size() - index);
        beginWrite(reg::REG_CMDB_WRITE);
        for (size_t i = 0; i < count; ++i)
        {
            const uint32_t w = words[index + i];
            transfer(static_cast<uint8_t>(w));
            transfer(static_cast<uint8_t>(w >> 8));
            transfer(static_cast<uint8_t>(w >> 16));
            transfer(static_cast<uint8_t>(w >> 24));
        }
        deselect();
        index += count;
    }
    return true;
}

bool EveHal::waitIdle(unsigned timeoutMs)
{
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (running() && std::chrono::steady_clock::now() < deadline)
    {
        const uint16_t readPointer = rd16(reg::REG_CMD_READ);
        if (readPointer == reg::CMD_FIFO_FAULT)
            return false;
        if (readPointer == rd16(reg::REG_CMD_WRITE))
            return true;
        std::this_thread::yield();
    }
    return false;
}
} // namespace bench
