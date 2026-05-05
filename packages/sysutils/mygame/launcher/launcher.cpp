// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2024-present ROCKNIX (https://github.com/ROCKNIX)
//
// SDL2 launcher menu — post-game system controller.
// Provides Bluetooth, Wi-Fi, USB gadget, and game launch controls.
// All system actions are delegated to /usr/bin/actions.sh.

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

// ── Types ─────────────────────────────────────────────────────────────────────

struct Color { uint8_t r, g, b, a; };

struct MenuItem {
    std::string label;
    std::function<void()> action;
};

struct BtDevice {
    std::string name;
    std::string mac;
    bool paired    = false;
    bool connected = false;
};

struct WifiNet {
    std::string ssid;
    bool connected = false;
};

// ── Palette ───────────────────────────────────────────────────────────────────

static Color BTN_IDLE      = {30,  30,  50,  255};
static Color BTN_HOVER     = {70,  70,  160, 255};
static Color BTN_PRESS     = {110, 110, 200, 255};
static Color BTN_TOGGLE_ON  = {40,  160, 70,  255};
static Color BTN_TOGGLE_OFF = {160, 40,  40,  255};
static Color BADGE_BT      = {100, 149, 237, 255};
static Color BADGE_USB     = {200, 130, 30,  255};

static Color COL_BG        = {15,  15,  25,  255};
static Color COL_TEXT      = {220, 220, 220, 255};
static Color COL_DIM       = {140, 140, 140, 255};
static Color COL_HEADER    = {200, 200, 255, 255};
static Color COL_BORDER    = {60,  60,  100, 255};

// ── SDL helpers ───────────────────────────────────────────────────────────────

static void setCol(SDL_Renderer* r, Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

static void fillRect(SDL_Renderer* r, SDL_Rect rect, Color c) {
    setCol(r, c);
    SDL_RenderFillRect(r, &rect);
}

static void drawRect(SDL_Renderer* r, SDL_Rect rect, Color c) {
    setCol(r, c);
    SDL_RenderDrawRect(r, &rect);
}

// Rounded-rectangle fill (approximated with horizontal spans)
static void fillRR(SDL_Renderer* r, SDL_Rect rect, int radius, Color c) {
    if (radius <= 0) { fillRect(r, rect, c); return; }
    setCol(r, c);
    for (int y = 0; y < rect.h; y++) {
        int iy = rect.y + y;
        int xOffset = 0;
        if (y < radius) {
            double d = radius - y;
            xOffset = (int)(radius - SDL_sqrt((double)(radius*radius) - d*d));
        } else if (y >= rect.h - radius) {
            double d = y - (rect.h - radius - 1);
            xOffset = (int)(radius - SDL_sqrt((double)(radius*radius) - d*d));
        }
        SDL_RenderDrawLine(r, rect.x + xOffset, iy, rect.x + rect.w - 1 - xOffset, iy);
    }
}

static SDL_Texture* makeText(SDL_Renderer* r, TTF_Font* font, const char* text,
                             Color c, int* w, int* h) {
    SDL_Color col = {c.r, c.g, c.b, c.a};
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text, col);
    if (!surf) return nullptr;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    if (w) *w = surf->w;
    if (h) *h = surf->h;
    SDL_FreeSurface(surf);
    return tex;
}

static void drawTextAt(SDL_Renderer* r, TTF_Font* font, const char* text,
                       int x, int y, Color c) {
    int w, h;
    SDL_Texture* tex = makeText(r, font, text, c, &w, &h);
    if (!tex) return;
    SDL_Rect dst = {x, y, w, h};
    SDL_RenderCopy(r, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

static void drawTextC(SDL_Renderer* r, TTF_Font* font, const char* text,
                      SDL_Rect bounds, Color c) {
    int w, h;
    SDL_Texture* tex = makeText(r, font, text, c, &w, &h);
    if (!tex) return;
    SDL_Rect dst = {bounds.x + (bounds.w - w) / 2,
                    bounds.y + (bounds.h - h) / 2,
                    w, h};
    SDL_RenderCopy(r, tex, nullptr, &dst);
    SDL_DestroyTexture(tex);
}

// ── Shell helpers ─────────────────────────────────────────────────────────────

static void runAction(const char* action, const char* arg = nullptr) {
    char cmd[512];
    if (arg)
        snprintf(cmd, sizeof(cmd), "/usr/bin/actions.sh %s \"%s\"", action, arg);
    else
        snprintf(cmd, sizeof(cmd), "/usr/bin/actions.sh %s", action);
    fprintf(stderr, "[launcher] $ %s\n", cmd);
    system(cmd);
}

static std::vector<std::string> runLines(const char* action,
                                         const char* arg = nullptr) {
    char cmd[512];
    if (arg)
        snprintf(cmd, sizeof(cmd), "/usr/bin/actions.sh %s \"%s\" 2>/dev/null",
                 action, arg);
    else
        snprintf(cmd, sizeof(cmd), "/usr/bin/actions.sh %s 2>/dev/null", action);

    std::vector<std::string> lines;
    FILE* f = popen(cmd, "r");
    if (!f) return lines;
    char buf[256];
    while (fgets(buf, sizeof(buf), f)) {
        std::string line(buf);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();
        if (!line.empty())
            lines.push_back(line);
    }
    pclose(f);
    return lines;
}

// Parse "Device XX:XX:XX:XX:XX:XX  Some Name" lines from bluetoothctl
static std::vector<BtDevice> parseBtDevices(const std::vector<std::string>& lines) {
    std::vector<BtDevice> devices;
    for (const auto& line : lines) {
        // Lines look like: "Device AA:BB:CC:DD:EE:FF My Device"
        const char* p = line.c_str();
        if (strncmp(p, "Device ", 7) != 0) continue;
        p += 7;
        BtDevice d;
        // MAC is next 17 chars
        if (strlen(p) < 18) continue;
        d.mac  = std::string(p, 17);
        d.name = strlen(p) > 18 ? std::string(p + 18) : d.mac;
        devices.push_back(d);
    }
    return devices;
}

// Parse iwctl get-networks output; connected network starts with ">"
static std::vector<WifiNet> parseWifiNets(const std::vector<std::string>& lines) {
    std::vector<WifiNet> nets;
    // Skip header lines (contain "---" or "Available networks")
    bool inTable = false;
    for (const auto& line : lines) {
        if (line.find("---") != std::string::npos) { inTable = true; continue; }
        if (!inTable) continue;
        WifiNet n;
        n.connected = (!line.empty() && line[0] == '>');
        // SSID is the first non-whitespace token after optional '>'
        const char* p = line.c_str();
        if (*p == '>') p++;
        while (*p == ' ') p++;
        // Read until double-space or end
        const char* end = p;
        while (*end && !(*end == ' ' && *(end+1) == ' ')) end++;
        n.ssid = std::string(p, end - p);
        if (!n.ssid.empty())
            nets.push_back(n);
    }
    return nets;
}

// ── App ───────────────────────────────────────────────────────────────────────

struct App {
    SDL_Window*   win    = nullptr;
    SDL_Renderer* ren    = nullptr;
    TTF_Font*     font   = nullptr;
    TTF_Font*     fontSm = nullptr;
    bool          alive  = true;

    bool init();
    void quit();

    // Returns false when user presses Back/B at the top level.
    bool runMenu(const std::string& title, std::vector<MenuItem>& items,
                 const std::string& badge = "");

    void drawFrame(const std::string& title, const std::vector<MenuItem>& items,
                   int sel, const std::string& badge);

    void mainMenu();
    void settingsMenu();
    void btMenu();
    void wifiMenu();
    void usbMenu();
};

// ── Rendering ─────────────────────────────────────────────────────────────────

static constexpr int LW = 640;  // logical width
static constexpr int LH = 480;  // logical height
static constexpr int ITEM_H   = 44;
static constexpr int ITEM_PAD = 8;
static constexpr int MARGIN   = 60;

void App::drawFrame(const std::string& title,
                    const std::vector<MenuItem>& items,
                    int sel,
                    const std::string& badge) {
    // Background
    fillRect(ren, {0, 0, LW, LH}, COL_BG);

    // Header bar
    SDL_Rect hdr = {0, 0, LW, 52};
    fillRect(ren, hdr, {25, 25, 50, 255});
    drawRect(ren, hdr, COL_BORDER);
    drawTextC(ren, font, title.c_str(), hdr, COL_HEADER);

    // Badge (top-right label, e.g. "BT  Bluetooth")
    if (!badge.empty()) {
        int bw, bh;
        SDL_Texture* btex = makeText(ren, fontSm, badge.c_str(), BADGE_BT, &bw, &bh);
        if (btex) {
            SDL_Rect bdst = {LW - bw - 12, (52 - bh) / 2, bw, bh};
            SDL_RenderCopy(ren, btex, nullptr, &bdst);
            SDL_DestroyTexture(btex);
        }
    }

    // Menu items
    int startY = 62;
    for (int i = 0; i < (int)items.size(); i++) {
        SDL_Rect btn = {MARGIN, startY + i * (ITEM_H + ITEM_PAD),
                        LW - 2 * MARGIN, ITEM_H};

        bool hover = (i == sel);
        Color bg = hover ? BTN_HOVER : BTN_IDLE;
        fillRR(ren, btn, 8, bg);
        if (hover)
            drawRect(ren, btn, {180, 180, 255, 200});

        drawTextC(ren, font, items[i].label.c_str(), btn, COL_TEXT);
    }

    // Hint bar
    SDL_Rect hint = {0, LH - 28, LW, 28};
    fillRect(ren, hint, {20, 20, 40, 255});
    drawTextC(ren, fontSm,
              "D-Pad/Arrows: Navigate  A/Enter: Select  B/Esc: Back",
              hint, COL_DIM);

    SDL_RenderPresent(ren);
}

// ── Generic menu runner ───────────────────────────────────────────────────────

bool App::runMenu(const std::string& title, std::vector<MenuItem>& items,
                  const std::string& badge) {
    int sel = 0;
    SDL_GameController* gc = nullptr;
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_IsGameController(i)) { gc = SDL_GameControllerOpen(i); break; }

    bool pressed = false;

    while (alive) {
        // Rebuild items list can change between loops (e.g. after scan)
        drawFrame(title, items, sel, badge);

        SDL_Event ev;
        SDL_WaitEventTimeout(&ev, 100);

        auto moveUp = [&] { sel = (sel - 1 + (int)items.size()) % (int)items.size(); };
        auto moveDn = [&] { sel = (sel + 1) % (int)items.size(); };

        switch (ev.type) {
            case SDL_QUIT:
                alive = false;
                return false;

            case SDL_KEYDOWN:
                pressed = true;
                switch (ev.key.keysym.sym) {
                    case SDLK_UP:   case SDLK_w: moveUp(); break;
                    case SDLK_DOWN: case SDLK_s: moveDn(); break;
                    case SDLK_RETURN: case SDLK_KP_ENTER:
                        if (!items.empty()) items[sel].action();
                        break;
                    case SDLK_ESCAPE: case SDLK_BACKSPACE:
                        if (gc) SDL_GameControllerClose(gc);
                        return false;
                    default: break;
                }
                break;

            case SDL_CONTROLLERBUTTONDOWN:
                pressed = true;
                switch (ev.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_UP:   moveUp(); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: moveDn(); break;
                    case SDL_CONTROLLER_BUTTON_B:  // physical A (East) on Nintendo-layout pads
                    case SDL_CONTROLLER_BUTTON_START:
                        if (!items.empty()) items[sel].action();
                        break;
                    case SDL_CONTROLLER_BUTTON_A:  // physical B (South) on Nintendo-layout pads
                    case SDL_CONTROLLER_BUTTON_BACK:
                        if (gc) SDL_GameControllerClose(gc);
                        return false;
                    default: break;
                }
                break;

            case SDL_CONTROLLERDEVICEADDED:
                if (!gc) gc = SDL_GameControllerOpen(ev.cdevice.which);
                break;

            default: break;
        }
        (void)pressed;
    }

    if (gc) SDL_GameControllerClose(gc);
    return false;
}

// ── Menus ─────────────────────────────────────────────────────────────────────

void App::mainMenu() {
    std::vector<MenuItem> items = {
        {"Launch Game",   [&]{ runAction("launch"); }},
        {"Settings",      [&]{ settingsMenu(); }},
        {"Update System", [&]{ runAction("update"); }},
        {"Shutdown",      [&]{ runAction("shutdown"); alive = false; }},
    };
    runMenu("LAUNCHER", items);
}

void App::settingsMenu() {
    std::vector<MenuItem> items = {
        {"BT  Bluetooth", [&]{ btMenu();   }},
        {"WiFi  Wi-Fi",   [&]{ wifiMenu(); }},
        {"USB  USB Mode", [&]{ usbMenu();  }},
        {"<  Back",       [&]{ /* handled by runMenu returning false */ }},
    };
    // "<  Back" triggers a return from this menu
    while (alive) {
        std::vector<MenuItem> loop = {
            {"BT  Bluetooth", [&]{ btMenu();   }},
            {"WiFi  Wi-Fi",   [&]{ wifiMenu(); }},
            {"USB  USB Mode", [&]{ usbMenu();  }},
        };
        if (!runMenu("SETTINGS", loop))
            return;
    }
}

void App::btMenu() {
    bool btOn = false;
    std::vector<BtDevice> devices;
    std::string status = "Ready";

    auto refresh = [&](std::vector<MenuItem>& out) {
        out.clear();
        // Toggle
        std::string togLabel = btOn ? "BT ON" : "BT OFF";
        out.push_back({togLabel, [&]{
            if (btOn) { runAction("bt_off");  btOn = false; status = "Powered off"; }
            else      { runAction("bt_on");   btOn = true;  status = "Powered on";  }
        }});

        if (btOn) {
            out.push_back({"Scan for Devices", [&]{
                status = "Scanning...";
                auto lines = runLines("bt_scan");
                devices = parseBtDevices(lines);
                status = "Found " + std::to_string(devices.size()) + " device(s)";
            }});

            for (auto& d : devices) {
                std::string lbl = d.name + (d.connected ? "  Connected" :
                                            d.paired    ? "  Paired"    : "  Not paired");
                std::string mac = d.mac;
                bool conn = d.connected;
                out.push_back({lbl, [&, mac, conn]{
                    if (conn) {
                        runAction("bt_disconnect", mac.c_str());
                        status = "Disconnected: " + mac;
                        for (auto& dev : devices)
                            if (dev.mac == mac) dev.connected = false;
                    } else if (d.paired) {
                        runAction("bt_connect", mac.c_str());
                        status = "Connected: " + mac;
                        for (auto& dev : devices)
                            if (dev.mac == mac) dev.connected = true;
                    } else {
                        runAction("bt_pair", mac.c_str());
                        status = "Paired: " + mac;
                        for (auto& dev : devices)
                            if (dev.mac == mac) dev.paired = true;
                    }
                }});
            }

            if (!devices.empty()) {
                out.push_back({"Forget All Paired", [&]{
                    runAction("bt_forget_all");
                    devices.clear();
                    status = "All devices forgotten";
                }});
            }
        } else {
            out.push_back({"Enable BT first", [&]{}});
        }
    };

    while (alive) {
        std::vector<MenuItem> items;
        refresh(items);
        if (!runMenu("BLUETOOTH", items, "BT  Bluetooth"))
            return;
    }
}

void App::wifiMenu() {
    bool wifiOn = false;
    std::vector<WifiNet> nets;
    std::string status = "Ready";

    while (alive) {
        std::vector<MenuItem> items;
        items.push_back({wifiOn ? "WiFi ON" : "WiFi OFF", [&]{
            if (wifiOn) { runAction("wifi_off"); wifiOn = false; status = "Disabled"; }
            else        { runAction("wifi_on");  wifiOn = true;  status = "Ready";    }
        }});

        if (wifiOn) {
            items.push_back({"Scan Networks", [&]{
                auto lines = runLines("wifi_scan");
                nets = parseWifiNets(lines);
                status = "Found " + std::to_string(nets.size()) + " network(s)";
            }});

            for (auto& n : nets) {
                std::string lbl = n.ssid + (n.connected ? "  Connected  " : "");
                std::string ssid = n.ssid;
                bool conn = n.connected;
                items.push_back({lbl, [&, ssid, conn]{
                    if (conn) {
                        runAction("wifi_disconnect");
                        status = "Disconnected";
                        for (auto& net : nets)
                            if (net.ssid == ssid) net.connected = false;
                    } else {
                        runAction("wifi_connect", ssid.c_str());
                        status = "Connected  ";
                        for (auto& net : nets) net.connected = false;
                        for (auto& net : nets)
                            if (net.ssid == ssid) net.connected = true;
                    }
                }});
            }

            if (!nets.empty()) {
                items.push_back({"Forget Network...", [&]{
                    if (!nets.empty()) {
                        runAction("wifi_forget", nets[0].ssid.c_str());
                        nets.erase(nets.begin());
                    }
                }});
            }
        } else {
            items.push_back({"Enable Wi-Fi first", [&]{}});
        }

        if (!runMenu("WIFI", items, "WiFi  Wi-Fi"))
            return;
    }
}

void App::usbMenu() {
    while (alive) {
        std::vector<MenuItem> items = {
            {"Switch to ECM", [&]{ runAction("usb_ecm"); }},
            {"Switch to MTP", [&]{ runAction("usb_mtp"); }},
        };
        if (!runMenu("USB MODE", items, "USB  USB Mode"))
            return;
    }
}

// ── Init / quit ───────────────────────────────────────────────────────────────

bool App::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        fprintf(stderr, "[launcher] SDL_Init: %s\n", SDL_GetError());
        return false;
    }

    SDL_DisplayMode mode = {};
    if (SDL_GetCurrentDisplayMode(0, &mode) != 0)
        fprintf(stderr, "[launcher] SDL_GetCurrentDisplayMode: %s\n", SDL_GetError());

    // Honour env-var overrides (set by start_mygame.sh via sdl_resolution)
    const char* envW = getenv("SDL_LAUNCHER_W");
    const char* envH = getenv("SDL_LAUNCHER_H");
    if (envW && atoi(envW) > 0) mode.w = atoi(envW);
    if (envH && atoi(envH) > 0) mode.h = atoi(envH);

    win = SDL_CreateWindow("Launcher",
                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           mode.w ? mode.w : LW, mode.h ? mode.h : LH,
                           SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_SHOWN);
    if (!win) {
        fprintf(stderr, "[launcher] SDL_CreateWindow: %s\n", SDL_GetError());
        return false;
    }

    ren = SDL_CreateRenderer(win, -1,
                             SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        fprintf(stderr, "[launcher] SDL_CreateRenderer: %s\n", SDL_GetError());
        return false;
    }

    SDL_RenderSetLogicalSize(ren, LW, LH);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    if (TTF_Init() != 0) {
        fprintf(stderr, "[launcher] TTF_Init: %s\n", TTF_GetError());
        return false;
    }

    static const char* fontPaths[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/liberation/LiberationSans-Bold.ttf",
        nullptr,
    };
    for (int i = 0; fontPaths[i]; i++) {
        font = TTF_OpenFont(fontPaths[i], 22);
        if (font) {
            fontSm = TTF_OpenFont(fontPaths[i], 14);
            break;
        }
    }
    if (!font) {
        fprintf(stderr, "[launcher] Could not open any font\n");
        return false;
    }

    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_IsGameController(i)) SDL_GameControllerOpen(i);

    return true;
}

void App::quit() {
    if (fontSm) { TTF_CloseFont(fontSm); fontSm = nullptr; }
    if (font)   { TTF_CloseFont(font);   font   = nullptr; }
    TTF_Quit();
    if (ren) { SDL_DestroyRenderer(ren); ren = nullptr; }
    if (win) { SDL_DestroyWindow(win);   win = nullptr; }
    SDL_Quit();
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main(int /*argc*/, char** /*argv*/) {
    App app;
    if (!app.init()) return 1;
    app.mainMenu();
    app.quit();
    return 0;
}
