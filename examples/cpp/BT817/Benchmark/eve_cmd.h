#pragma once

// Display list encoders and a coprocessor command builder for BT817.
// The builder only collects 32-bit words; EveHal::sendCommands sends them.

#include <cstdint>
#include <algorithm>
#include <string>
#include <vector>

namespace bench
{
// ---- Constants ------------------------------------------------------------
namespace opt
{
constexpr uint16_t CENTERX = 0x0200;
constexpr uint16_t CENTERY = 0x0400;
constexpr uint16_t CENTER = 0x0600;
constexpr uint16_t RIGHTX = 0x0800;
constexpr uint16_t FLAT = 0x0100;
constexpr uint16_t NOBACK = 0x1000;
constexpr uint16_t NOTICKS = 0x2000;
constexpr uint16_t NOPOINTER = 0x4000;
constexpr uint16_t SIGNED = 0x0100;
} // namespace opt

namespace prim
{
constexpr uint32_t BITMAPS = 1;
constexpr uint32_t POINTS = 2;
constexpr uint32_t LINES = 3;
constexpr uint32_t LINE_STRIP = 4;
constexpr uint32_t EDGE_STRIP_B = 8;
constexpr uint32_t RECTS = 9;
} // namespace prim

namespace fmt
{
constexpr uint32_t L8 = 3;
constexpr uint32_t RGB565 = 7;
constexpr uint32_t ASTC_4x4 = 0x93B0;
} // namespace fmt

constexpr uint32_t NEAREST = 0;
constexpr uint32_t BILINEAR = 1;
constexpr uint32_t BORDER = 0;
constexpr uint32_t REPEAT = 1;
constexpr uint32_t BLEND_ZERO = 0;
constexpr uint32_t BLEND_ONE = 1;
constexpr uint32_t BLEND_SRC_ALPHA = 2;
constexpr uint32_t BLEND_ONE_MINUS_SRC_ALPHA = 4;

// ---- Display list encoders -------------------------------------------------
namespace dl
{
inline uint32_t DISPLAY() { return 0; }
inline uint32_t VERTEX2F(int x, int y)
{
    return (1u << 30) | ((static_cast<uint32_t>(x) & 32767u) << 15) |
           (static_cast<uint32_t>(y) & 32767u);
}
inline uint32_t VERTEX2II(uint32_t x, uint32_t y, uint32_t handle, uint32_t cell)
{
    return (2u << 30) | ((x & 511u) << 21) | ((y & 511u) << 12) |
           ((handle & 31u) << 7) | (cell & 127u);
}
inline uint32_t BITMAP_SOURCE(uint32_t a) { return (1u << 24) | (a & 0xFFFFFFu); }
inline uint32_t CLEAR_COLOR_RGB(uint32_t rgb) { return (2u << 24) | (rgb & 0xFFFFFFu); }
inline uint32_t TAG(uint32_t t) { return (3u << 24) | (t & 0xFFu); }
inline uint32_t COLOR_RGB(uint32_t rgb) { return (4u << 24) | (rgb & 0xFFFFFFu); }
inline uint32_t BITMAP_HANDLE(uint32_t h) { return (5u << 24) | (h & 31u); }
inline uint32_t BITMAP_LAYOUT(uint32_t format, uint32_t stride, uint32_t height)
{
    return (7u << 24) | ((format & 31u) << 19) | ((stride & 1023u) << 9) |
           (height & 511u);
}
inline uint32_t BITMAP_SIZE(uint32_t filter, uint32_t wrapx, uint32_t wrapy,
                            uint32_t w, uint32_t h)
{
    return (8u << 24) | ((filter & 1u) << 20) | ((wrapx & 1u) << 19) |
           ((wrapy & 1u) << 18) | ((w & 511u) << 9) | (h & 511u);
}
inline uint32_t BLEND_FUNC(uint32_t src, uint32_t dst)
{
    return (11u << 24) | ((src & 7u) << 3) | (dst & 7u);
}
inline uint32_t POINT_SIZE(uint32_t s) { return (13u << 24) | (s & 8191u); }
inline uint32_t LINE_WIDTH(uint32_t w) { return (14u << 24) | (w & 4095u); }
inline uint32_t CLEAR_COLOR_A(uint32_t a) { return (15u << 24) | (a & 255u); }
inline uint32_t COLOR_A(uint32_t a) { return (16u << 24) | (a & 255u); }
inline uint32_t TAG_MASK(uint32_t m) { return (20u << 24) | (m & 1u); }
// 8.8 fixed point (p = 0)
inline uint32_t BITMAP_TRANSFORM_A(int32_t v) { return (21u << 24) | (static_cast<uint32_t>(v) & 0x1FFFFu); }
inline uint32_t BITMAP_TRANSFORM_B(int32_t v) { return (22u << 24) | (static_cast<uint32_t>(v) & 0x1FFFFu); }
inline uint32_t BITMAP_TRANSFORM_C(int32_t v) { return (23u << 24) | (static_cast<uint32_t>(v) & 0xFFFFFFu); }
inline uint32_t BITMAP_TRANSFORM_D(int32_t v) { return (24u << 24) | (static_cast<uint32_t>(v) & 0x1FFFFu); }
inline uint32_t BITMAP_TRANSFORM_E(int32_t v) { return (25u << 24) | (static_cast<uint32_t>(v) & 0x1FFFFu); }
inline uint32_t BITMAP_TRANSFORM_F(int32_t v) { return (26u << 24) | (static_cast<uint32_t>(v) & 0xFFFFFFu); }
inline uint32_t SCISSOR_XY(uint32_t x, uint32_t y)
{
    return (27u << 24) | ((x & 2047u) << 11) | (y & 2047u);
}
inline uint32_t SCISSOR_SIZE(uint32_t w, uint32_t h)
{
    return (28u << 24) | ((w & 4095u) << 12) | (h & 4095u);
}
inline uint32_t BEGIN(uint32_t p) { return (31u << 24) | (p & 15u); }
inline uint32_t END() { return 33u << 24; }
inline uint32_t SAVE_CONTEXT() { return 34u << 24; }
inline uint32_t RESTORE_CONTEXT() { return 35u << 24; }
inline uint32_t CLEAR(bool c, bool s, bool t)
{
    return (38u << 24) | ((c ? 1u : 0u) << 2) | ((s ? 1u : 0u) << 1) | (t ? 1u : 0u);
}
inline uint32_t VERTEX_FORMAT(uint32_t f) { return (39u << 24) | (f & 7u); }
inline uint32_t BITMAP_LAYOUT_H(uint32_t stride, uint32_t height)
{
    return (40u << 24) | ((stride & 3u) << 2) | (height & 3u);
}
inline uint32_t BITMAP_SIZE_H(uint32_t w, uint32_t h)
{
    return (41u << 24) | ((w & 3u) << 2) | (h & 3u);
}
inline uint32_t VERTEX_TRANSLATE_X(int32_t x) { return (43u << 24) | (static_cast<uint32_t>(x) & 131071u); }
inline uint32_t VERTEX_TRANSLATE_Y(int32_t y) { return (44u << 24) | (static_cast<uint32_t>(y) & 131071u); }
} // namespace dl

// ---- Coprocessor command builder -----------------------------------------
class Cmd
{
public:
    std::vector<uint32_t> words;

    void clear() { words.clear(); }
    Cmd &w(uint32_t v)
    {
        words.push_back(v);
        return *this;
    }
    static uint32_t xy(int x, int y)
    {
        return (static_cast<uint32_t>(static_cast<uint16_t>(x))) |
               (static_cast<uint32_t>(static_cast<uint16_t>(y)) << 16);
    }

    void str(const std::string &s)
    {
        const size_t n = s.size() + 1;
        const size_t padded = (n + 3) & ~size_t(3);
        std::vector<uint8_t> bytes(padded, 0);
        std::copy(s.begin(), s.end(), bytes.begin());
        for (size_t i = 0; i < padded; i += 4)
            w(bytes[i] | (bytes[i + 1] << 8) | (bytes[i + 2] << 16) |
              (static_cast<uint32_t>(bytes[i + 3]) << 24));
    }

    void dlstart() { w(0xFFFFFF00); }
    void swap() { w(0xFFFFFF01); }
    void bgcolor(uint32_t rgb) { w(0xFFFFFF09).w(rgb); }
    void fgcolor(uint32_t rgb) { w(0xFFFFFF0A).w(rgb); }
    void gradient(int x0, int y0, uint32_t rgb0, int x1, int y1, uint32_t rgb1)
    {
        w(0xFFFFFF0B).w(xy(x0, y0)).w(rgb0).w(xy(x1, y1)).w(rgb1);
    }
    void text(int x, int y, int font, uint16_t options, const std::string &s)
    {
        w(0xFFFFFF0C).w(xy(x, y)).w(xy(font, options));
        str(s);
    }
    void button(int x, int y, int bw, int bh, int font, uint16_t options,
                const std::string &s)
    {
        w(0xFFFFFF0D).w(xy(x, y)).w(xy(bw, bh)).w(xy(font, options));
        str(s);
    }
    void progress(int x, int y, int pw, int ph, uint16_t options, uint16_t val,
                  uint16_t range)
    {
        w(0xFFFFFF0F).w(xy(x, y)).w(xy(pw, ph)).w(xy(options, val)).w(range);
    }
    void slider(int x, int y, int sw, int sh, uint16_t options, uint16_t val,
                uint16_t range)
    {
        w(0xFFFFFF10).w(xy(x, y)).w(xy(sw, sh)).w(xy(options, val)).w(range);
    }
    void toggle(int x, int y, int tw, int font, uint16_t options, bool on,
                const std::string &labels)
    {
        w(0xFFFFFF12).w(xy(x, y)).w(xy(tw, font)).w(xy(options, on ? 65535 : 0));
        str(labels);
    }
    void gauge(int x, int y, int r, uint16_t options, uint16_t major,
               uint16_t minor, uint16_t val, uint16_t range)
    {
        w(0xFFFFFF13).w(xy(x, y)).w(xy(r, options)).w(xy(major, minor)).w(xy(val, range));
    }
    void number(int x, int y, int font, uint16_t options, int32_t n)
    {
        w(0xFFFFFF2E).w(xy(x, y)).w(xy(font, options)).w(static_cast<uint32_t>(n));
    }
    void loadidentity() { w(0xFFFFFF26); }
    void translate(int32_t tx, int32_t ty) // 16.16 fixed point
    {
        w(0xFFFFFF27).w(static_cast<uint32_t>(tx)).w(static_cast<uint32_t>(ty));
    }
    void scale(int32_t sx, int32_t sy) // 16.16 fixed point
    {
        w(0xFFFFFF28).w(static_cast<uint32_t>(sx)).w(static_cast<uint32_t>(sy));
    }
    void rotate(uint32_t a) { w(0xFFFFFF29).w(a); } // 65536 = full turn
    void setmatrix() { w(0xFFFFFF2A); }
    void romfont(uint32_t font, uint32_t slot) { w(0xFFFFFF3F).w(font).w(slot); }
    void setbitmap(uint32_t source, uint32_t format, uint32_t width, uint32_t height)
    {
        w(0xFFFFFF43).w(source).w(format | (width << 16)).w(height);
    }
    void newlist(uint32_t address) { w(0xFFFFFF68).w(address); }
    void endlist() { w(0xFFFFFF69); }
    void calllist(uint32_t address) { w(0xFFFFFF67).w(address); }
};
} // namespace bench
