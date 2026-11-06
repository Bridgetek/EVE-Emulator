// BT817 GUI benchmark on the EVE emulator, 800x480.
//
// Usage:
//   BT817Benchmark [--seconds N] [--interactive] [--capture DIR] [--csv FILE]
//
//   default        runs the six scenes in a window, N seconds each (default 6),
//                  then prints a results table and writes benchmark_results.csv
//   --interactive  stays on a scene until PREV / NEXT is clicked; close the
//                  window to finish
//   --capture DIR  headless: no window, saves one 800x480 BMP per scene to DIR
//
// The emulator verifies rendering and measures what the host and the display
// list cost per frame. Frame rate and underrun must be confirmed on real BT817
// hardware; see README.md.

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "bt8xxemu.h"
#include "eve_cmd.h"
#include "eve_hal.h"
#include "scenes.h"

using namespace bench;
using Clock = std::chrono::steady_clock;

namespace
{
constexpr uint32_t TAG_NEXT = 250;
constexpr uint32_t TAG_PREV = 251;
constexpr uint32_t FULL_FRAME_16BPP = SCREEN_W * SCREEN_H * 2; // 768,000 bytes

struct Options
{
    double seconds = 6.0;
    bool interactive = false;
    std::filesystem::path captureDir;
    std::filesystem::path csvPath = "benchmark_results.csv";
};

struct SceneResult
{
    std::string name;
    uint64_t updates = 0;
    double elapsed = 0;
    uint32_t displayFrames = 0;
    uint64_t hostBytes = 0;
    uint32_t maxDlBytes = 0;
    uint64_t sumDlBytes = 0;
    uint32_t underruns = 0;
    bool fault = false;
    std::string faultText;

    double updateFps() const { return elapsed > 0 ? updates / elapsed : 0; }
    double displayFps() const { return elapsed > 0 ? displayFrames / elapsed : 0; }
    double hostPerUpdate() const { return updates ? double(hostBytes) / updates : 0; }
    double avgDl() const { return updates ? double(sumDlBytes) / updates : 0; }
};

// ---- Headless capture through the Graphics callback ------------------------
struct Capture
{
    std::mutex mutex;
    std::atomic<int> countdown{0}; // complete frames to skip before copying
    std::atomic<bool> done{false};
    std::vector<uint32_t> pixels;
    uint32_t width = 0, height = 0;
};

Capture g_capture;

int graphicsCallback(BT8XXEMU_Emulator *, void *, int output, const argb8888 *buffer,
                     uint32_t hsize, uint32_t vsize, BT8XXEMU_FrameFlags flags)
{
    if (!output || !buffer || !(flags & BT8XXEMU_FrameBufferComplete))
        return 1;
    int n = g_capture.countdown.load();
    if (n <= 0)
        return 1;
    if (n > 1)
    {
        g_capture.countdown.store(n - 1);
        return 1;
    }
    std::lock_guard<std::mutex> lock(g_capture.mutex);
    g_capture.pixels.assign(buffer, buffer + size_t(hsize) * vsize);
    g_capture.width = hsize;
    g_capture.height = vsize;
    g_capture.countdown.store(0);
    g_capture.done.store(true);
    return 1;
}

bool writeBmp(const std::filesystem::path &path, const std::vector<uint32_t> &px,
              uint32_t w, uint32_t h)
{
    std::ofstream f(path, std::ios::binary);
    if (!f)
        return false;
    const uint32_t row = (w * 3 + 3) & ~3u;
    const uint32_t dataSize = row * h;
    auto u16 = [&](uint16_t v) { f.put(char(v & 0xFF)).put(char(v >> 8)); };
    auto u32 = [&](uint32_t v) { u16(uint16_t(v)); u16(uint16_t(v >> 16)); };
    f.put('B').put('M');
    u32(54 + dataSize); u32(0); u32(54);
    u32(40); u32(w); u32(h); u16(1); u16(24); u32(0); u32(dataSize);
    u32(2835); u32(2835); u32(0); u32(0);
    std::vector<char> line(row, 0);
    for (int y = int(h) - 1; y >= 0; --y)
    {
        for (uint32_t x = 0; x < w; ++x)
        {
            const uint32_t p = px[size_t(y) * w + x];
            line[x * 3 + 0] = char(p & 0xFF);
            line[x * 3 + 1] = char((p >> 8) & 0xFF);
            line[x * 3 + 2] = char((p >> 16) & 0xFF);
        }
        f.write(line.data(), row);
    }
    return bool(f);
}

// ---- Board bring-up: 800x480 panel timing as in the BT817 Flash example ----
bool waitForBoot(EveHal &hal)
{
    const auto deadline = Clock::now() + std::chrono::seconds(3);
    while (Clock::now() < deadline && hal.running())
    {
        if (hal.rd8(reg::REG_ID) == 0x7C && hal.rd8(reg::REG_CPURESET) == 0)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

bool initializeDisplay(EveHal &hal)
{
    if (!waitForBoot(hal))
        return false;
    hal.wr8(reg::REG_PCLK, 0);
    hal.wr16(reg::REG_HSIZE, SCREEN_W);
    hal.wr16(reg::REG_HCYCLE, 928);
    hal.wr16(reg::REG_HOFFSET, 88);
    hal.wr16(reg::REG_HSYNC0, 0);
    hal.wr16(reg::REG_HSYNC1, 48);
    hal.wr16(reg::REG_VSIZE, SCREEN_H);
    hal.wr16(reg::REG_VCYCLE, 525);
    hal.wr16(reg::REG_VOFFSET, 32);
    hal.wr16(reg::REG_VSYNC0, 0);
    hal.wr16(reg::REG_VSYNC1, 3);
    hal.wr8(reg::REG_PCLK_POL, 1);
    hal.wr8(reg::REG_SWIZZLE, 0);
    hal.wr8(reg::REG_CSPREAD, 0);
    hal.wr8(reg::REG_DITHER, 1);
    hal.wr32(reg::RAM_DL, dl::CLEAR_COLOR_RGB(0));
    hal.wr32(reg::RAM_DL + 4, dl::CLEAR(true, true, true));
    hal.wr32(reg::RAM_DL + 8, dl::DISPLAY());
    hal.wr8(reg::REG_DLSWAP, 2);
    hal.wr8(reg::REG_GPIO, static_cast<uint8_t>(hal.rd8(reg::REG_GPIO) | 0x80));
    hal.wr8(reg::REG_PCLK, 2);
    hal.wr8(reg::REG_PWM_DUTY, 127);
    return true;
}

bool enableFlashFullMode(EveHal &hal)
{
    auto run = [&](std::vector<uint32_t> words, uint8_t expected) {
        return hal.sendCommands(words) && hal.waitIdle() &&
               hal.rd8(reg::REG_FLASH_STATUS) == expected;
    };
    return run({0xFFFFFF48}, 1) &&    // CMD_FLASHDETACH -> DETACHED
           run({0xFFFFFF49}, 2) &&    // CMD_FLASHATTACH -> BASIC
           run({0xFFFFFF4A, 0}, 3);   // CMD_FLASHFAST   -> FULL
}

// ---- HUD ---------------------------------------------------------------------
void drawHud(Cmd &c, size_t index, size_t count, const std::string &name,
             const SceneResult &live, uint32_t lastDl, uint32_t lastHost,
             bool interactive)
{
    c.w(dl::SAVE_CONTEXT());
    c.w(dl::COLOR_A(255));
    c.w(dl::VERTEX_TRANSLATE_X(0));
    c.w(dl::COLOR_RGB(0x0F141B));
    c.w(dl::BEGIN(prim::RECTS));
    c.w(V(0, 0));
    c.w(V(SCREEN_W, HUD_H));
    c.w(dl::END());
    c.w(dl::COLOR_RGB(0x2C5F99));
    c.w(dl::BEGIN(prim::RECTS));
    c.w(V(0, HUD_H));
    c.w(V(SCREEN_W, HUD_H + 2));
    c.w(dl::END());

    char title[64];
    std::snprintf(title, sizeof(title), "%zu/%zu %s", index + 1, count, name.c_str());
    c.w(dl::COLOR_RGB(0xFFFFFF));
    c.text(10, HUD_H / 2, 28, opt::CENTERY, title);

    char stats[128];
    std::snprintf(stats, sizeof(stats),
                  "DL %uB (%u%%)  host %uB/upd  %.0f upd/s  %.0f fps  UR %u", lastDl,
                  lastDl * 100 / reg::RAM_DL_SIZE, lastHost, live.updateFps(),
                  live.displayFps(), live.underruns);
    c.w(dl::COLOR_RGB(lastDl > reg::RAM_DL_SIZE * 9 / 10 ? 0xFF8A65 : 0x9FB4CC));
    c.text(interactive ? 228 : 300, HUD_H / 2, 26, opt::CENTERY, stats);

    if (interactive)
    {
        c.fgcolor(0x2C5F99);
        c.w(dl::COLOR_RGB(0xFFFFFF));
        c.w(dl::TAG(TAG_PREV));
        c.button(642, 5, 72, 30, 27, opt::FLAT, "PREV");
        c.w(dl::TAG(TAG_NEXT));
        c.button(722, 5, 72, 30, 27, opt::FLAT, "NEXT");
        c.w(dl::TAG(0));
    }
    c.w(dl::RESTORE_CONTEXT());
}

bool parseArgs(int argc, char **argv, Options &o)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--seconds" && i + 1 < argc)
            o.seconds = std::max(1.0, std::atof(argv[++i]));
        else if (a == "--interactive")
            o.interactive = true;
        else if (a == "--capture" && i + 1 < argc)
            o.captureDir = argv[++i];
        else if (a == "--csv" && i + 1 < argc)
            o.csvPath = argv[++i];
        else
        {
            std::cerr << "Usage: BT817Benchmark [--seconds N] [--interactive] "
                         "[--capture DIR] [--csv FILE]\n";
            return false;
        }
    }
    if (o.interactive && !o.captureDir.empty())
    {
        std::cerr << "--interactive needs the window; it cannot be combined with --capture.\n";
        return false;
    }
    return true;
}

std::filesystem::path executableDir(const char *argv0)
{
#ifdef _WIN32
    std::vector<wchar_t> buf(32768);
    const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    if (n > 0 && n < buf.size())
        return std::filesystem::path(buf.data()).parent_path();
#endif
    return std::filesystem::absolute(argv0).parent_path();
}

std::string fileSafe(std::string s)
{
    for (char &ch : s)
        if (!std::isalnum(static_cast<unsigned char>(ch)))
            ch = '_';
    return s;
}
} // namespace

int main(int argc, char **argv)
{
    Options opts;
    if (!parseArgs(argc, argv, opts))
        return 2;
    const bool headless = !opts.captureDir.empty();
    if (headless)
        std::filesystem::create_directories(opts.captureDir);

    // Flash image with the ASTC test picture, shared with the Flash example.
    const std::filesystem::path flashPath = executableDir(argv[0]) / "flash-817-default.bin";
    if (!std::filesystem::is_regular_file(flashPath))
    {
        std::cerr << "Flash image was not found: " << flashPath.string() << '\n';
        return 1;
    }
    BT8XXEMU_FlashParameters flashParams{};
    BT8XXEMU_Flash_defaults(BT8XXEMU_VERSION_API, &flashParams);
    const auto native = flashPath.native();
    if (native.size() >= sizeof(flashParams.DataFilePath) / sizeof(flashParams.DataFilePath[0]))
    {
        std::cerr << "The flash image path is too long.\n";
        return 1;
    }
    std::copy(native.begin(), native.end(), flashParams.DataFilePath);
    flashParams.DataFilePath[native.size()] = 0;
    flashParams.SizeBytes = 8 * 1024 * 1024;
    BT8XXEMU_Flash *flash = BT8XXEMU_Flash_create(BT8XXEMU_VERSION_API, &flashParams);
    if (!flash)
    {
        std::cerr << "Unable to create the flash emulator.\n";
        return 1;
    }

    BT8XXEMU_EmulatorParameters params{};
    BT8XXEMU_defaults(BT8XXEMU_VERSION_API, &params, BT8XXEMU_EmulatorBT817);
    params.Flash = flash;
    // Every frame must be rendered completely for a fair measurement.
    params.Flags &= ~BT8XXEMU_EmulatorEnableDynamicDegrade;
    params.Flags &= ~BT8XXEMU_EmulatorEnableAudio;
    if (headless)
        params.Graphics = graphicsCallback;

    std::cout << BT8XXEMU_version() << '\n';
    BT8XXEMU_Emulator *emulator = nullptr;
    BT8XXEMU_run(BT8XXEMU_VERSION_API, &emulator, &params);
    if (!emulator)
    {
        std::cerr << "Unable to launch the EVE emulator.\n";
        BT8XXEMU_Flash_destroy(flash);
        return 1;
    }

    EveHal hal(emulator);
    int exitCode = 0;
    std::vector<SceneResult> results;
    auto scenes = makeScenes();

    if (!initializeDisplay(hal))
    {
        std::cerr << "The BT817 display did not initialize.\n";
        exitCode = 1;
    }
    else if (!enableFlashFullMode(hal))
    {
        std::cerr << "The flash did not enter FULL mode (ASTC scenes need it).\n";
        exitCode = 1;
    }
    else
    {
        for (auto &s : scenes)
        {
            if (!s->setup(hal))
            {
                std::cerr << "Setup failed for scene '" << s->name()
                          << "': " << hal.faultReport() << '\n';
                hal.recoverCoprocessor();
            }
        }

        results.resize(scenes.size());
        for (size_t i = 0; i < scenes.size(); ++i)
            results[i].name = scenes[i]->name();

        size_t index = 0;
        bool quit = false;
        uint32_t lastTag = 0;
        Cmd cmd;
        while (!quit && hal.running())
        {
            Scene &scene = *scenes[index];
            SceneResult live;
            live.name = scene.name();
            std::cout << "\n[" << index + 1 << "/" << scenes.size() << "] " << scene.name()
                      << "\n    " << scene.stress() << '\n';

            const auto start = Clock::now();
            const uint32_t frames0 = hal.rd32(reg::REG_FRAMES);
            const uint32_t underrun0 = hal.rd32(reg::REG_UNDERRUN);
            uint32_t lastFrame = frames0, lastDl = 0, lastHost = 0;
            bool captured = !headless;
            int step = 0; // 0: stay, +1 next, -1 previous

            while (hal.running())
            {
                const double t = std::chrono::duration<double>(Clock::now() - start).count();
                if (!opts.interactive && t >= opts.seconds)
                {
                    step = 1;
                    break;
                }

                // Pace updates to the display: one update per scanned-out frame.
                const auto waitUntil = Clock::now() + std::chrono::milliseconds(100);
                while (hal.rd32(reg::REG_FRAMES) == lastFrame && Clock::now() < waitUntil)
                    std::this_thread::yield();
                lastFrame = hal.rd32(reg::REG_FRAMES);

                cmd.clear();
                cmd.dlstart();
                cmd.w(dl::CLEAR_COLOR_RGB(scene.clearColor()));
                cmd.w(dl::CLEAR(true, true, true));
                cmd.w(dl::VERTEX_FORMAT(4));
                cmd.romfont(FONT_BIG, 34);
                cmd.romfont(FONT_MED, 32);
                scene.draw(cmd, t);
                drawHud(cmd, index, scenes.size(), scene.name(), live, lastDl, lastHost,
                        opts.interactive);
                cmd.w(dl::DISPLAY());
                cmd.swap();

                hal.resetBytesSent();
                if (!hal.sendCommands(cmd.words) || !hal.waitIdle())
                {
                    live.fault = true;
                    live.faultText = hal.faultReport();
                    std::cerr << "    Coprocessor fault: " << live.faultText << '\n';
                    hal.recoverCoprocessor();
                    step = 1;
                    break;
                }
                lastHost = static_cast<uint32_t>(hal.bytesSent());
                lastDl = hal.rd16(reg::REG_CMD_DL);

                live.updates++;
                live.hostBytes += lastHost;
                live.sumDlBytes += lastDl;
                live.maxDlBytes = std::max(live.maxDlBytes, lastDl);
                live.elapsed = std::chrono::duration<double>(Clock::now() - start).count();
                live.displayFrames = hal.rd32(reg::REG_FRAMES) - frames0;
                live.underruns = hal.rd32(reg::REG_UNDERRUN) - underrun0;

                if (!captured && t >= opts.seconds * 0.5)
                {
                    g_capture.done.store(false);
                    g_capture.countdown.store(3);
                    const auto deadline = Clock::now() + std::chrono::seconds(5);
                    while (!g_capture.done.load() && Clock::now() < deadline && hal.running())
                        std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    captured = true;
                    if (g_capture.done.load())
                    {
                        char file[96];
                        std::snprintf(file, sizeof(file), "scene%zu_%s.bmp", index + 1,
                                      fileSafe(scene.name()).c_str());
                        std::lock_guard<std::mutex> lock(g_capture.mutex);
                        const auto out = opts.captureDir / file;
                        if (writeBmp(out, g_capture.pixels, g_capture.width, g_capture.height))
                            std::cout << "    captured " << out.string() << '\n';
                    }
                    else
                        std::cerr << "    capture timed out\n";
                }

                if (opts.interactive)
                {
                    const uint32_t tag = hal.rd8(reg::REG_TOUCH_TAG);
                    if (tag != lastTag && (tag == TAG_NEXT || tag == TAG_PREV))
                    {
                        step = tag == TAG_NEXT ? 1 : -1;
                        lastTag = tag;
                        break;
                    }
                    lastTag = tag;
                }
            }

            // Keep the run with the most updates for each scene.
            if (live.updates >= results[index].updates || live.fault)
                results[index] = live;

            if (!hal.running())
                break;
            if (!opts.interactive && index + 1 == scenes.size())
                quit = true;
            else
                index = (index + scenes.size() + step) % scenes.size();
        }
    }

    // ---- Report ---------------------------------------------------------------
    if (!results.empty())
    {
        std::printf("\n%-18s %8s %8s %12s %10s %8s %6s\n", "scene", "upd/s", "fps",
                    "host B/upd", "max DL B", "DL %", "UR");
        for (const auto &r : results)
            std::printf("%-18s %8.1f %8.1f %12.0f %10u %7u%% %6u%s\n", r.name.c_str(),
                        r.updateFps(), r.displayFps(), r.hostPerUpdate(), r.maxDlBytes,
                        r.maxDlBytes * 100 / reg::RAM_DL_SIZE, r.underruns,
                        r.fault ? "  FAULT" : "");
        std::printf("\nReference: one full 800x480 16bpp frame is %u bytes "
                    "(framebuffer controller, CPU-drawn full redraw).\n",
                    FULL_FRAME_16BPP);

        std::ofstream csv(opts.csvPath);
        csv << "scene,updates,update_fps,display_fps,host_bytes_per_update,"
               "avg_dl_bytes,max_dl_bytes,max_dl_percent,underruns,"
               "fullframe_16bpp_bytes,host_vs_fullframe_percent,fault\n";
        for (const auto &r : results)
        {
            char line[512];
            std::snprintf(line, sizeof(line),
                          "\"%s\",%llu,%.2f,%.2f,%.0f,%.0f,%u,%.1f,%u,%u,%.3f,\"%s\"\n",
                          r.name.c_str(), static_cast<unsigned long long>(r.updates),
                          r.updateFps(), r.displayFps(), r.hostPerUpdate(), r.avgDl(),
                          r.maxDlBytes, r.maxDlBytes * 100.0 / reg::RAM_DL_SIZE,
                          r.underruns, FULL_FRAME_16BPP,
                          r.hostPerUpdate() * 100.0 / FULL_FRAME_16BPP,
                          r.fault ? r.faultText.c_str() : "");
            csv << line;
        }
        std::cout << "Results written to " << opts.csvPath.string() << '\n';
    }

    BT8XXEMU_stop(emulator);
    BT8XXEMU_destroy(emulator);
    BT8XXEMU_Flash_destroy(flash);
    return exitCode;
}
