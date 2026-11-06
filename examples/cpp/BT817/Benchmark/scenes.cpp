#include "scenes.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace bench
{
namespace
{
constexpr double PI = 3.14159265358979323846;

int contentTop() { return HUD_H + 4; }

double easeInOut(double x)
{
    x = std::clamp(x, 0.0, 1.0);
    return x < 0.5 ? 4 * x * x * x : 1 - std::pow(-2 * x + 2, 3) / 2;
}

double easeOut(double x)
{
    x = std::clamp(x, 0.0, 1.0);
    return 1 - std::pow(1 - x, 3);
}

std::string format(const char *f, int a)
{
    char buf[96];
    std::snprintf(buf, sizeof(buf), f, a);
    return buf;
}

bool runList(EveHal &hal, const Cmd &cmd)
{
    return hal.sendCommands(cmd.words) && hal.waitIdle();
}

// Rounded rectangle: RECTS with LINE_WIDTH gives the corner radius.
void roundRect(Cmd &c, double x0, double y0, double x1, double y1, double r)
{
    c.w(dl::LINE_WIDTH(static_cast<uint32_t>(r * 16)));
    c.w(dl::BEGIN(prim::RECTS));
    c.w(V(x0 + r, y0 + r));
    c.w(V(x1 - r, y1 - r));
    c.w(dl::END());
}

// ---------------------------------------------------------------------------
// 1. Needle gauge: rotated, anti-aliased bitmap over a cached dial
// ---------------------------------------------------------------------------
class GaugeScene : public Scene
{
    static constexpr int CX = 260, CY = 266, R = 200;
    static constexpr int NW = 16, NH = 200, PIVOT_Y = 172, BOX = 200;

    static double angleOf(double v) { return (-135.0 + v * 270.0 / 240.0) * PI / 180.0; }

public:
    std::string name() const override { return "Needle gauge"; }
    std::string stress() const override
    {
        return "Bilinear bitmap rotation each frame, cached dial (CMD_NEWLIST), "
               "ROM font 34 readout, two built-in CMD_GAUGE widgets.";
    }

    bool setup(EveHal &hal) override
    {
        // Needle as an 8-bit alpha (L8) bitmap with anti-aliased edges.
        std::vector<uint8_t> px(NW * NH, 0);
        for (int y = 0; y < NH; ++y)
        {
            double hw;
            if (y < 4)
                hw = 0;
            else if (y <= PIVOT_Y)
                hw = 0.8 + (5.5 - 0.8) * (y - 4) / double(PIVOT_Y - 4);
            else if (y < 196)
                hw = 5.5 - 1.5 * (y - PIVOT_Y) / double(196 - PIVOT_Y);
            else
                hw = 0;
            for (int x = 0; x < NW; ++x)
            {
                const double d = std::fabs(x + 0.5 - NW / 2.0);
                const double cov = std::clamp(hw - d + 0.5, 0.0, 1.0);
                px[y * NW + x] = static_cast<uint8_t>(cov * 255 + 0.5);
            }
        }
        hal.wrMem(ADDR_NEEDLE, px.data(), px.size());

        // Static dial compiled once into RAM_G.
        Cmd c;
        c.newlist(ADDR_LIST_DIAL);
        c.w(dl::BEGIN(prim::POINTS));
        c.w(dl::COLOR_RGB(0x3A4656));
        c.w(dl::POINT_SIZE(R * 16));
        c.w(V(CX, CY));
        c.w(dl::COLOR_RGB(0x0E131B));
        c.w(dl::POINT_SIZE((R - 6) * 16));
        c.w(V(CX, CY));
        c.w(dl::END());

        c.w(dl::BEGIN(prim::LINES));
        for (int pass = 0; pass < 3; ++pass)
        {
            // pass 0: minor ticks, 1: major ticks, 2: red zone
            if (pass == 0)
            {
                c.w(dl::COLOR_RGB(0x8090A0));
                c.w(dl::LINE_WIDTH(16));
            }
            else if (pass == 1)
            {
                c.w(dl::COLOR_RGB(0xFFFFFF));
                c.w(dl::LINE_WIDTH(40));
            }
            else
            {
                c.w(dl::COLOR_RGB(0xFF3B30));
                c.w(dl::LINE_WIDTH(40));
            }
            for (int v = 0; v <= 240; v += 2)
            {
                const bool major = v % 20 == 0;
                const bool red = v >= 200;
                if ((pass == 0 && (major || red)) || (pass == 1 && (!major || red)) ||
                    (pass == 2 && !red))
                    continue;
                const double a = angleOf(v);
                const double r0 = major ? R - 40 : R - 26;
                const double r1 = R - 14;
                c.w(V(CX + r0 * std::sin(a), CY - r0 * std::cos(a)));
                c.w(V(CX + r1 * std::sin(a), CY - r1 * std::cos(a)));
            }
        }
        c.w(dl::END());

        c.w(dl::COLOR_RGB(0xD8E0EA));
        for (int v = 0; v <= 240; v += 20)
        {
            const double a = angleOf(v);
            const double r = R - 62;
            c.number(static_cast<int>(CX + r * std::sin(a)),
                     static_cast<int>(CY - r * std::cos(a)), 28, opt::CENTER, v);
        }
        c.w(dl::COLOR_RGB(0x8090A0));
        c.text(CX, CY + 152, 28, opt::CENTER, "km/h");
        c.endlist();
        return runList(hal, c);
    }

    void draw(Cmd &c, double t) override
    {
        const double v = 120 + 115 * std::sin(t * 1.3) * std::cos(t * 0.37);
        const double a = angleOf(v);

        c.calllist(ADDR_LIST_DIAL);

        // Needle: rotate about its pivot with the coprocessor matrix helpers.
        c.w(dl::SAVE_CONTEXT());
        c.w(dl::BITMAP_HANDLE(HANDLE_NEEDLE));
        c.w(dl::BITMAP_SOURCE(ADDR_NEEDLE));
        c.w(dl::BITMAP_LAYOUT(fmt::L8, NW, NH));
        c.w(dl::BITMAP_LAYOUT_H(0, 0));
        c.w(dl::BITMAP_SIZE(BILINEAR, BORDER, BORDER, 2 * BOX, 2 * BOX));
        c.w(dl::BITMAP_SIZE_H(0, 0));
        c.loadidentity();
        c.translate(BOX * 65536, BOX * 65536);
        c.rotate(static_cast<uint32_t>(static_cast<int64_t>(std::lround(a / (2 * PI) * 65536)) & 0xFFFF));
        c.translate(-(NW / 2) * 65536, -PIVOT_Y * 65536);
        c.setmatrix();
        c.w(dl::COLOR_RGB(0xFF5A36));
        c.w(dl::BEGIN(prim::BITMAPS));
        c.w(V(CX - BOX, CY - BOX));
        c.w(dl::END());
        c.w(dl::RESTORE_CONTEXT());

        // Hub
        c.w(dl::BEGIN(prim::POINTS));
        c.w(dl::COLOR_RGB(0x2A323C));
        c.w(dl::POINT_SIZE(18 * 16));
        c.w(V(CX, CY));
        c.w(dl::COLOR_RGB(0x5A6676));
        c.w(dl::POINT_SIZE(8 * 16));
        c.w(V(CX, CY));
        c.w(dl::END());

        c.w(dl::COLOR_RGB(0xFFFFFF));
        c.number(CX, CY + 98, FONT_BIG, opt::CENTER, static_cast<int>(v));

        // Two built-in coprocessor gauges on the right.
        const int rpm = static_cast<int>(4000 + 3600 * std::sin(t * 2.1));
        const int temp = static_cast<int>(50 + 40 * std::sin(t * 0.5));
        c.fgcolor(0x2E6BD8);
        c.bgcolor(0x1A2230);
        c.w(dl::COLOR_RGB(0xFFFFFF));
        c.gauge(640, 160, 104, opt::FLAT, 8, 5, static_cast<uint16_t>(rpm), 8000);
        c.gauge(640, 372, 84, opt::FLAT, 5, 4, static_cast<uint16_t>(temp), 100);
        c.w(dl::COLOR_RGB(0xA8B4C4));
        c.text(640, 232, 26, opt::CENTER, "rpm");
        c.text(640, 424, 26, opt::CENTER, "motor C");
        c.number(640, 206, 29, opt::CENTER, rpm);
        c.number(640, 402, 28, opt::CENTER, temp);
    }
};

// ---------------------------------------------------------------------------
// 2. Page transitions: two cached pages slid with VERTEX_TRANSLATE_X
// ---------------------------------------------------------------------------
class TransitionScene : public Scene
{
    static void pageA(Cmd &c)
    {
        c.w(dl::COLOR_RGB(0xFFFFFF));
        c.text(32, 76, 31, opt::CENTERY, "Settings");
        c.w(dl::COLOR_RGB(0x9FB4CC));
        c.text(34, 116, 27, opt::CENTERY, "Page A  -  cached with CMD_NEWLIST");

        c.w(dl::COLOR_RGB(0x16263A));
        roundRect(c, 24, 144, 380, 460, 14);
        const char *labels[] = {"Auto brightness", "Bluetooth", "Eco mode",
                                "Key sound", "Night theme"};
        c.fgcolor(0x3A7BD5);
        c.bgcolor(0x2C3A4C);
        for (int i = 0; i < 5; ++i)
        {
            const int y = 176 + i * 58;
            c.w(dl::COLOR_RGB(0xE6ECF2));
            c.text(44, y, 28, opt::CENTERY, labels[i]);
            c.w(dl::COLOR_RGB(0xFFFFFF));
            c.toggle(296, y - 8, 52, 26, opt::FLAT, i % 2 == 0, "\xFF");
        }

        const char *buttons[] = {"Display", "Network", "Sound",
                                 "Power", "Service", "About"};
        const uint32_t colors[] = {0x2E6BD8, 0x1F9D6B, 0x8A4FD8,
                                   0xD8742E, 0x2E9BD8, 0x5A6676};
        for (int i = 0; i < 6; ++i)
        {
            const int col = i % 2, row = i / 2;
            c.fgcolor(colors[i]);
            c.w(dl::COLOR_RGB(0xFFFFFF));
            c.button(404 + col * 192, 150 + row * 104, 176, 88, 28, 0, buttons[i]);
        }
    }

    static void pageB(Cmd &c)
    {
        c.w(dl::COLOR_RGB(0xFFFFFF));
        c.text(32, 76, 31, opt::CENTERY, "Trip computer");
        c.w(dl::COLOR_RGB(0xC9A8E0));
        c.text(34, 116, 27, opt::CENTERY, "Page B  -  cached with CMD_NEWLIST");

        const char *names[] = {"Range", "Avg Wh/km", "Trip", "Ride time"};
        const int values[] = {84, 31, 126, 52};
        const char *units[] = {"km", "", "km", "min"};
        for (int i = 0; i < 4; ++i)
        {
            const int x = 24 + (i % 2) * 190, y = 148 + (i / 2) * 120;
            c.w(dl::COLOR_RGB(0x2A1C3A));
            roundRect(c, x, y, x + 176, y + 108, 12);
            c.w(dl::COLOR_RGB(0xB8A6CC));
            c.text(x + 14, y + 20, 27, opt::CENTERY, names[i]);
            c.w(dl::COLOR_RGB(0xFFFFFF));
            c.number(x + 14, y + 66, 31, opt::CENTERY, values[i]);
            c.text(x + 162, y + 76, 27, opt::RIGHTX, units[i]);
        }

        c.fgcolor(0xB04FD8);
        c.bgcolor(0x2A1C3A);
        const char *bars[] = {"Battery", "Regen", "Tyre front", "Tyre rear"};
        const int vals[] = {72, 35, 88, 84};
        for (int i = 0; i < 4; ++i)
        {
            const int y = 168 + i * 72;
            c.w(dl::COLOR_RGB(0xE6E0EE));
            c.text(420, y, 28, opt::CENTERY, bars[i]);
            c.w(dl::COLOR_RGB(0x6E3C9A));
            c.progress(420, y + 22, 340, 14, opt::FLAT, vals[i], 100);
        }
        c.w(dl::COLOR_RGB(0xFFFFFF));
        c.slider(30, 410, 340, 14, opt::FLAT, 60, 100);
    }

    // Background gradient, clipped to the part of the page that is on screen.
    static void background(Cmd &c, double dx, uint32_t top, uint32_t bottom)
    {
        const int x0 = std::max(0, static_cast<int>(std::floor(dx)));
        const int x1 = std::min(SCREEN_W, static_cast<int>(std::ceil(dx + SCREEN_W)));
        if (x1 <= x0)
            return;
        c.w(dl::SAVE_CONTEXT());
        c.w(dl::SCISSOR_XY(x0, contentTop()));
        c.w(dl::SCISSOR_SIZE(x1 - x0, SCREEN_H - contentTop()));
        c.gradient(static_cast<int>(dx), contentTop(), top,
                   static_cast<int>(dx + SCREEN_W * 0.3), SCREEN_H, bottom);
        c.w(dl::RESTORE_CONTEXT());
    }

    static void page(Cmd &c, double dx, bool b)
    {
        if (dx <= -SCREEN_W || dx >= SCREEN_W)
            return;
        if (b)
            background(c, dx, 0x3A1752, 0x0C0614);
        else
            background(c, dx, 0x0D3460, 0x050C16);
        c.w(dl::VERTEX_TRANSLATE_X(static_cast<int32_t>(std::lround(dx * 16))));
        c.calllist(b ? ADDR_LIST_PAGE_B : ADDR_LIST_PAGE_A);
        c.w(dl::VERTEX_TRANSLATE_X(0));
    }

public:
    std::string name() const override { return "Page transitions"; }
    std::string stress() const override
    {
        return "Two full pages compiled once into RAM_G; each frame the host only "
               "sends the slide offset (VERTEX_TRANSLATE_X) or a fade alpha.";
    }

    bool setup(EveHal &hal) override
    {
        Cmd c;
        c.newlist(ADDR_LIST_PAGE_A);
        pageA(c);
        c.endlist();
        c.newlist(ADDR_LIST_PAGE_B);
        pageB(c);
        c.endlist();
        return runList(hal, c);
    }

    void draw(Cmd &c, double t) override
    {
        // 4 s cycle: hold A, slide to B, hold B, fade through black to A
        const double p = std::fmod(t, 4.0);
        if (p < 1.2)
            page(c, 0, false);
        else if (p < 1.8)
        {
            const double e = easeInOut((p - 1.2) / 0.6);
            page(c, -SCREEN_W * e, false);
            page(c, SCREEN_W * (1 - e), true);
        }
        else if (p < 3.0)
            page(c, 0, true);
        else if (p < 3.6)
        {
            const double k = (p - 3.0) / 0.6;
            page(c, 0, k >= 0.5);
            const double alpha = k < 0.5 ? k * 2 : (1 - k) * 2;
            c.w(dl::COLOR_RGB(0x000000));
            c.w(dl::COLOR_A(static_cast<uint32_t>(alpha * 255)));
            c.w(dl::BEGIN(prim::RECTS));
            c.w(V(0, contentTop()));
            c.w(V(SCREEN_W, SCREEN_H));
            c.w(dl::END());
            c.w(dl::COLOR_A(255));
        }
        else
            page(c, 0, false);
    }
};

// ---------------------------------------------------------------------------
// 3. Kinetic list: 50 rows, ASTC icons from flash scaled on the fly
// ---------------------------------------------------------------------------
class ListScene : public Scene
{
    static constexpr int ROWS = 50, ROW_H = 72, ICON_W = 72, ICON_H = 41;

public:
    std::string name() const override { return "Kinetic list"; }
    std::string stress() const override
    {
        return "50-row list with fling scrolling; each visible row has an ASTC icon "
               "read directly from flash and scaled 0.25x bilinear.";
    }

    void draw(Cmd &c, double t) override
    {
        const int top = contentTop();
        const int viewH = SCREEN_H - top;
        const double maxScroll = ROWS * ROW_H - viewH;
        const double p = std::fmod(t, 6.0);
        const double scroll =
            p < 3.0 ? maxScroll * easeOut(p / 2.6) : maxScroll * (1 - easeOut((p - 3.0) / 2.6));

        const int first = std::max(0, static_cast<int>(scroll / ROW_H));
        const int last = std::min(ROWS - 1, static_cast<int>((scroll + viewH) / ROW_H));
        auto rowY = [&](int i) { return top + i * ROW_H - scroll; };

        c.w(dl::SAVE_CONTEXT());
        c.w(dl::SCISSOR_XY(0, top));
        c.w(dl::SCISSOR_SIZE(SCREEN_W, viewH));

        // Pass 1: row backgrounds (one BEGIN for all rows)
        c.w(dl::BEGIN(prim::RECTS));
        for (int i = first; i <= last; ++i)
        {
            c.w(dl::COLOR_RGB(i % 2 ? 0x141B26 : 0x1A2331));
            c.w(V(0, rowY(i)));
            c.w(V(780, rowY(i) + ROW_H - 2));
        }
        c.w(dl::END());

        // Pass 2: icons straight from flash (ASTC 4x4), scaled down 4x
        c.w(dl::BITMAP_HANDLE(HANDLE_ASTC));
        c.setbitmap(FLASH_ASTC_SOURCE, fmt::ASTC_4x4, FLASH_ASTC_W, FLASH_ASTC_H);
        c.w(dl::BITMAP_SIZE(BILINEAR, BORDER, BORDER, ICON_W, ICON_H));
        c.w(dl::BITMAP_TRANSFORM_A(1024));
        c.w(dl::BITMAP_TRANSFORM_E(1024));
        c.w(dl::COLOR_RGB(0xFFFFFF));
        c.w(dl::BEGIN(prim::BITMAPS));
        for (int i = first; i <= last; ++i)
            c.w(V(14, rowY(i) + (ROW_H - ICON_H) / 2.0));
        c.w(dl::END());
        c.w(dl::BITMAP_TRANSFORM_A(256));
        c.w(dl::BITMAP_TRANSFORM_E(256));

        // Pass 3: text
        for (int i = first; i <= last; ++i)
        {
            const int y = static_cast<int>(rowY(i));
            c.w(dl::COLOR_RGB(0xF0F4F8));
            c.text(104, y + 22, 28, opt::CENTERY, format("Node %02d  -  motor controller", i + 1));
            c.w(dl::COLOR_RGB(0x8C9AAE));
            c.text(104, y + 50, 26, opt::CENTERY, format("CAN 0x18FF%02X   status OK   fw 2.4.1", 0x40 + i));
            c.w(dl::COLOR_RGB(0x6CD49A));
            c.number(760, y + 36, 29, opt::CENTERY | opt::RIGHTX,
                     static_cast<int>(400 + 300 * std::sin(t * 2 + i)));
        }

        // Scrollbar
        const double barH = viewH * double(viewH) / (ROWS * ROW_H);
        const double barY = top + (viewH - barH) * (scroll / maxScroll);
        c.w(dl::COLOR_RGB(0x5A6676));
        roundRect(c, 786, barY, 796, barY + barH, 5);
        c.w(dl::RESTORE_CONTEXT());
    }
};

// ---------------------------------------------------------------------------
// 4. Dense text: two columns, live values, scrolling ticker
// ---------------------------------------------------------------------------
class TextScene : public Scene
{
public:
    std::string name() const override { return "Dense text"; }
    std::string stress() const override
    {
        return "About 1,000 anti-aliased glyphs per frame, re-sent every frame "
               "(no caching): live values, ticker, two columns.";
    }

    uint32_t clearColor() const override { return 0xF2F0EA; }

    void draw(Cmd &c, double t) override
    {
        static const char *lines[] = {
            "Service report - unit 4471, firmware 2.4.1",
            "Inverter self test passed at power-on. Phase",
            "currents balanced within 1.8 percent. DC link",
            "ripple 0.6 V peak to peak at 48 V nominal.",
            "Brake light circuit: OK. Turn signals: OK.",
            "Headlamp low and high beam: OK, 0.21 A each.",
            "Throttle sensor sweep 0.62 V to 4.31 V, no",
            "dead band detected. Regen level set to 2.",
            "Battery pack 16S2P, 32 Ah, balance delta 9 mV.",
            "Last fault: none in 1,284 km. Next service at",
            "the 3,000 km mark or in 92 days, whichever is",
            "first. Tyre pressure front 2.0, rear 2.25 bar.",
            "Bluetooth link to rider phone: paired, RSSI -58.",
            "Navigation tiles cached: 12 of 12 for Bandung.",
            "Ambient 31 C, controller 47 C, motor 63 C.",
        };
        const int top = contentTop();

        c.w(dl::COLOR_RGB(0x1C2430));
        c.text(20, top + 30, 31, opt::CENTERY, "Unit 4471");

        // Clock (ROM font 32)
        const int secs = static_cast<int>(t) % 60;
        char clock[16];
        std::snprintf(clock, sizeof(clock), "12:%02d", secs);
        c.w(dl::COLOR_RGB(0x2C5F99));
        c.text(780, top + 34, FONT_MED, opt::RIGHTX | opt::CENTERY, clock);

        // Ticker clipped to a strip
        const std::string ticker =
            "RANGE 84 km  *  SOC 72%  *  ODO 1,284 km  *  NEXT SERVICE 92 days  *  "
            "AMBIENT 31 C  *  RANGE 84 km  *  SOC 72%  *  ODO 1,284 km";
        c.w(dl::SAVE_CONTEXT());
        c.w(dl::SCISSOR_XY(0, top + 70));
        c.w(dl::SCISSOR_SIZE(SCREEN_W, 26));
        c.w(dl::COLOR_RGB(0x1C2430));
        c.w(dl::BEGIN(prim::RECTS));
        c.w(V(0, top + 70));
        c.w(V(SCREEN_W, top + 96));
        c.w(dl::END());
        c.w(dl::COLOR_RGB(0xFFD27A));
        const int tickerX = 800 - static_cast<int>(std::fmod(t * 120, 1500));
        c.text(tickerX, top + 83, 27, opt::CENTERY, ticker);
        c.w(dl::RESTORE_CONTEXT());

        // Left column: 15 lines of body text
        c.w(dl::COLOR_RGB(0x222A35));
        for (int i = 0; i < 15; ++i)
            c.text(20, top + 112 + i * 21, 27, 0, lines[i]);

        // Right column: live cell voltages
        c.w(dl::COLOR_RGB(0x1C2430));
        c.text(560, top + 108, 28, 0, "Cell voltages (mV)");
        for (int i = 0; i < 14; ++i)
        {
            const int y = top + 138 + i * 21;
            const int mv = 3900 + static_cast<int>(20 * std::sin(t * 3 + i * 0.7));
            c.w(dl::COLOR_RGB(0x5B6673));
            c.text(560, y, 27, 0, format("Cell %02d", i + 1));
            c.w(dl::COLOR_RGB(mv > 3910 ? 0x1F7A4D : 0x9C3B1B));
            c.number(780, y, 27, opt::RIGHTX, mv);
        }
    }
};

// ---------------------------------------------------------------------------
// 5. History chart: LINE_STRIP with up to 1,500 points
// ---------------------------------------------------------------------------
class ChartScene : public Scene
{
    static constexpr int PX0 = 50, PX1 = 780, PY0 = 96, PY1 = 460;

public:
    std::string name() const override { return "History chart"; }
    std::string stress() const override
    {
        return "Anti-aliased LINE_STRIP cycling 250 / 500 / 1000 / 1500 points "
               "over a cached grid; shows how close the 8 KB display list gets.";
    }

    bool setup(EveHal &hal) override
    {
        Cmd c;
        c.newlist(ADDR_LIST_GRID);
        c.w(dl::COLOR_RGB(0x243040));
        c.w(dl::LINE_WIDTH(16));
        c.w(dl::BEGIN(prim::LINES));
        for (int i = 0; i <= 10; ++i)
        {
            const double x = PX0 + (PX1 - PX0) * i / 10.0;
            c.w(V(x, PY0));
            c.w(V(x, PY1));
        }
        for (int i = 0; i <= 8; ++i)
        {
            const double y = PY0 + (PY1 - PY0) * i / 8.0;
            c.w(V(PX0, y));
            c.w(V(PX1, y));
        }
        c.w(dl::END());
        c.w(dl::COLOR_RGB(0x8090A0));
        for (int i = 0; i <= 8; i += 2)
            c.number(PX0 - 8, PY0 + (PY1 - PY0) * i / 8, 26, opt::RIGHTX | opt::CENTERY,
                     100 - i * 25 / 2);
        c.endlist();
        return runList(hal, c);
    }

    void draw(Cmd &c, double t) override
    {
        static const int counts[] = {250, 500, 1000, 1500};
        const int n = counts[static_cast<int>(t / 1.5) % 4];

        c.calllist(ADDR_LIST_GRID);
        c.w(dl::COLOR_RGB(0xFFFFFF));
        c.text(PX0, contentTop() + 24, 29, opt::CENTERY, format("Motor current, %d samples", n));

        c.w(dl::SAVE_CONTEXT());
        c.w(dl::SCISSOR_XY(PX0, PY0));
        c.w(dl::SCISSOR_SIZE(PX1 - PX0 + 1, PY1 - PY0 + 1));
        c.w(dl::COLOR_RGB(0x40C8FF));
        c.w(dl::LINE_WIDTH(24));
        c.w(dl::BEGIN(prim::LINE_STRIP));
        const double mid = (PY0 + PY1) / 2.0, amp = (PY1 - PY0) * 0.42;
        for (int i = 0; i < n; ++i)
        {
            const double u = i / double(n - 1);
            const double phase = u * 6 * PI + t * 2;
            double y = 0.55 * std::sin(phase) + 0.25 * std::sin(phase * 3.7 + 1) +
                       0.12 * std::sin(phase * 11.3 + t * 5);
            // deterministic "noise"
            const uint32_t h = (static_cast<uint32_t>(i) * 2654435761u) ^
                               static_cast<uint32_t>(t * 20);
            y += 0.08 * ((h >> 16 & 0xFF) / 255.0 - 0.5);
            c.w(V(PX0 + (PX1 - PX0) * u, mid - amp * y));
        }
        c.w(dl::END());
        c.w(dl::RESTORE_CONTEXT());
    }
};

// ---------------------------------------------------------------------------
// 6. Static pages: 30 pages flipped every 0.5 s
// ---------------------------------------------------------------------------
class PagesScene : public Scene
{
public:
    std::string name() const override { return "30 static pages"; }
    std::string stress() const override
    {
        return "Page flips every 0.5 s; the image is ASTC in flash scaled 2x, so a "
               "flip costs the host a few hundred bytes, not a 768 KB frame.";
    }

    void draw(Cmd &c, double t) override
    {
        const int pageNo = static_cast<int>(t / 0.5) % 30;
        const int top = contentTop();
        const double hue = pageNo / 30.0 * 2 * PI;
        const auto ch = [&](double off) {
            return static_cast<uint32_t>(150 + 105 * (0.5 + 0.5 * std::sin(hue + off)));
        };
        const uint32_t tint = (ch(0) << 16) | (ch(2.1) << 8) | ch(4.2);

        // Image: ASTC from flash, scaled 2x to 576 x 328
        c.w(dl::SAVE_CONTEXT());
        c.w(dl::BITMAP_HANDLE(HANDLE_ASTC));
        c.setbitmap(FLASH_ASTC_SOURCE, fmt::ASTC_4x4, FLASH_ASTC_W, FLASH_ASTC_H);
        c.w(dl::BITMAP_SIZE(BILINEAR, BORDER, BORDER, 576 & 511, 328));
        c.w(dl::BITMAP_SIZE_H(576 >> 9, 0));
        c.w(dl::BITMAP_TRANSFORM_A(128));
        c.w(dl::BITMAP_TRANSFORM_E(128));
        c.w(dl::COLOR_RGB(tint));
        c.w(dl::BEGIN(prim::BITMAPS));
        c.w(V(20, top + 54));
        c.w(dl::END());
        c.w(dl::RESTORE_CONTEXT());

        c.w(dl::COLOR_RGB(0xFFFFFF));
        c.text(20, top + 26, 30, opt::CENTERY, format("Page %02d / 30", pageNo + 1));
        c.w(dl::COLOR_RGB(0x8C9AAE));
        c.text(20, top + 404, 26, opt::CENTERY,
               "Image: ASTC 4x4 in QSPI flash, drawn without copying to RAM_G");

        const int x = 612;
        c.w(dl::COLOR_RGB(0x1A2331));
        roundRect(c, x - 8, top + 54, 792, top + 382, 10);
        const char *rows[] = {
            "30 x 800x480 screens",
            "",
            "RGB565     23.0 MB",
            "ASTC 4x4   11.5 MB",
            "ASTC 8x8    2.9 MB",
            "",
            "LT7689 16 MB SDRAM",
            "~21 frames at 16 bpp",
            "",
            "Host bytes per flip:",
            "see HUD (host B/upd)",
        };
        for (int i = 0; i < 11; ++i)
        {
            c.w(dl::COLOR_RGB(i == 0 || i == 6 || i == 9 ? 0xFFD27A : 0xE6ECF2));
            c.text(x + 4, top + 74 + i * 28, 26, opt::CENTERY, rows[i]);
        }
    }
};
} // namespace

uint32_t V(double x, double y)
{
    return dl::VERTEX2F(static_cast<int>(std::lround(x * 16)),
                        static_cast<int>(std::lround(y * 16)));
}

std::vector<std::unique_ptr<Scene>> makeScenes()
{
    std::vector<std::unique_ptr<Scene>> scenes;
    scenes.emplace_back(new GaugeScene());
    scenes.emplace_back(new TransitionScene());
    scenes.emplace_back(new ListScene());
    scenes.emplace_back(new TextScene());
    scenes.emplace_back(new ChartScene());
    scenes.emplace_back(new PagesScene());
    return scenes;
}
} // namespace bench
