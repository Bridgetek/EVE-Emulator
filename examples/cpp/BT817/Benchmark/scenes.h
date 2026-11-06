#pragma once

// Six 800x480 GUI scenes that stress what differs between a display-list
// renderer (BT817) and a framebuffer plus 2D-engine controller (for example
// LT7689): rotation and scaling, page transitions, scrolling lists with
// images, dense text, long polylines and many static pages.

#include <memory>
#include <string>
#include <vector>

#include "eve_cmd.h"
#include "eve_hal.h"

namespace bench
{
constexpr int SCREEN_W = 800;
constexpr int SCREEN_H = 480;
constexpr int HUD_H = 40;         // top status bar
constexpr int FONT_BIG = 14;      // handle for ROM font 34 (CMD_ROMFONT)
constexpr int FONT_MED = 13;      // handle for ROM font 32
constexpr int HANDLE_NEEDLE = 2;  // L8 needle bitmap in RAM_G
constexpr int HANDLE_ASTC = 3;    // ASTC image read straight from flash

// RAM_G layout used by the benchmark (1 MiB available on BT817)
constexpr uint32_t ADDR_NEEDLE = 0x00000;    // 16 x 200 L8 = 3.2 KB
constexpr uint32_t ADDR_LIST_DIAL = 0x40000; // cached display lists
constexpr uint32_t ADDR_LIST_PAGE_A = 0x48000;
constexpr uint32_t ADDR_LIST_PAGE_B = 0x50000;
constexpr uint32_t ADDR_LIST_GRID = 0x58000;
constexpr uint32_t FLASH_ASTC_SOURCE = 0x800000 | 128; // flash-817-default.bin
constexpr uint32_t FLASH_ASTC_W = 288;
constexpr uint32_t FLASH_ASTC_H = 164;

class Scene
{
public:
    virtual ~Scene() = default;
    virtual std::string name() const = 0;
    // What the scene stresses, printed in the console and README.
    virtual std::string stress() const = 0;
    // One-time setup: upload assets to RAM_G and build cached lists.
    virtual bool setup(EveHal &hal) { (void)hal; return true; }
    virtual uint32_t clearColor() const { return 0x0A0E14; }
    // Appends this frame's commands (after CMD_DLSTART and CLEAR).
    // t is seconds since the scene started.
    virtual void draw(Cmd &cmd, double t) = 0;
};

std::vector<std::unique_ptr<Scene>> makeScenes();

// Helpers shared by scenes and the HUD
uint32_t V(double x, double y); // VERTEX2F in 1/16 pixel (VERTEX_FORMAT 4)
} // namespace bench
