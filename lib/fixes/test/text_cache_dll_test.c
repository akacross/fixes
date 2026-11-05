/* lib/fixes/text_cache.dll against the real d3dx9_25.dll, under Wine.
 *   test.exe check WORKDIR MOONLOADER_DIR          exports, correctness, fixes.lua's declarations
 *   test.exe stock|dll|asi WORKDIR MOONLOADER_DIR [FRAMES]   DrawText time per frame
 * WORKDIR holds d3dx9_25.dll (or Wine's builtin is used), lua51.dll, text_cache.dll and (for asi)
 * TextCacheFix.asi. */
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <d3dx9core.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef HRESULT (WINAPI *CreateFontA_t)(IDirect3DDevice9 *, INT, UINT, UINT, UINT, BOOL, DWORD, DWORD, DWORD, DWORD, LPCSTR, ID3DXFont **);
typedef HRESULT (WINAPI *CreateSprite_t)(IDirect3DDevice9 *, ID3DXSprite **);
typedef struct { uint32_t installed, hits, recorded, entries, mismatches, disabled; } Stats;
typedef int (__cdecl *Install_t)(void *, void *, uintptr_t *);
typedef int (__cdecl *Uninstall_t)(void);
typedef void (__cdecl *Stats_t)(Stats *);
typedef void lua_State;

static int passed, failed;
static void ok(int cond, const char *what) { if (cond) passed++; else { failed++; printf("  FAIL: %s\n", what); } }

static IDirect3DDevice9 *dev;
static ID3DXFont *font, *label;
static ID3DXSprite *sprite;
static double qpcFreq;
/* int64 ticks: CreateDevice drops the x87 FPU to single precision, too coarse for absolute seconds */
static LONGLONG now(void) { LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart; }
static int cmp(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }

static const char *chat[15] = {
    "[12:01:44] Cross_Lynch says: anyone at the bank? need a ride to the hospital",
    "[12:01:45] (( John_Doe: brb 5 mins, phone call ))",
    "[12:01:47] * Maria_Lopez takes out her phone and dials a number.",
    "[12:01:50] Admin: Server restart in 15 minutes, please find a safe spot.",
    "[12:01:52] Officer Smith (radio): 10-4, en route to Grove Street.",
    "[12:01:53] You have received $1,250 from Cross_Lynch.",
    "[12:01:55] [News] Live from Los Santos: traffic is heavy on the highway.",
    "[12:01:58] Tony_Montana shouts: Get out of my car!",
    "[12:02:00] (( OOC: does anyone know the command for the gps? ))",
    "[12:02:03] [Faction] Sgt. Brown: all units report to the station.",
    "[12:02:05] You bought a Sprunk for $5.",
    "[12:02:07] * Cross_Lynch puts the phone away.",
    "[12:02:09] Anna_Kim whispers: meet me behind the pier at 9.",
    "[12:02:11] [Hint] Type /help to see the list of commands.",
    "[12:02:13] Payday! You received $2,400 (tax: $240).",
};
static char tags[40][32], labels[10][48], live[5][64];

/* One SA-MP-like frame: chat with shadows, moving nametags with shadows, live
 * strings, then labels through the font's own sprite. Returns the DrawText time. */
static double frame(int fr, double *whole)
{
    for (int i = 0; i < 5; ++i)
        snprintf(live[i], sizeof(live[i]), "FPS %d  time %d.%03d  ping %d", 60 + (fr % 7), fr / 60, (fr & 0x7FFFFFFF) % 1000, 40 + i);
    LONGLONG t0 = now();
    IDirect3DDevice9_Clear(dev, 0, NULL, D3DCLEAR_TARGET, 0xFF202020, 1.0f, 0);
    IDirect3DDevice9_BeginScene(dev);
    ID3DXSprite_Begin(sprite, D3DXSPRITE_ALPHABLEND);
    LONGLONG d0 = now();
    for (int i = 0; i < 15; ++i) {
        RECT rc = {11, 11 + 18 * i, 11, 11 + 18 * i};
        ID3DXFont_DrawTextA(font, sprite, chat[i], -1, &rc, DT_NOCLIP, 0xFF000000);
        RECT rc2 = {10, 10 + 18 * i, 10, 10 + 18 * i};
        ID3DXFont_DrawTextA(font, sprite, chat[i], -1, &rc2, DT_NOCLIP, 0xFFFFFFFF);
    }
    for (int i = 0; i < 40; ++i) {
        int x = 100 + (i % 8) * 80 + ((fr & 0xFFFF) * 3 + i) % 40, y = 320 + (i / 8) * 40 + ((fr & 0xFFFF) + i) % 20;
        RECT rc = {x + 1, y + 1, x + 1, y + 1};
        ID3DXFont_DrawTextA(font, sprite, tags[i], -1, &rc, DT_NOCLIP, 0xFF000000);
        RECT rc2 = {x, y, x, y};
        ID3DXFont_DrawTextA(font, sprite, tags[i], -1, &rc2, DT_NOCLIP, 0xFF3399FF);
    }
    for (int i = 0; i < 5; ++i) {
        RECT rc = {600, 10 + 16 * i, 600, 10 + 16 * i};
        ID3DXFont_DrawTextA(font, sprite, live[i], -1, &rc, DT_NOCLIP, 0xFFFFFF00);
    }
    LONGLONG d1 = now();
    ID3DXSprite_End(sprite);
    LONGLONG d2 = now();
    for (int i = 0; i < 10; ++i) {
        RECT rc = {300, 200 + 16 * i, 300, 200 + 16 * i};
        ID3DXFont_DrawTextA(label, NULL, labels[i], -1, &rc, DT_NOCLIP, 0xFF00FF00);
    }
    LONGLONG d3 = now();
    IDirect3DDevice9_EndScene(dev);
    IDirect3DDevice9_Present(dev, NULL, NULL, NULL, NULL);
    if (whole) *whole = (double)(now() - t0) / qpcFreq;
    return (double)((d1 - d0) + (d3 - d2)) / qpcFreq;
}

static void setSlot(void **vtbl, int i, void *v)
{
    DWORD old;
    VirtualProtect(&vtbl[i], sizeof(void *), PAGE_READWRITE, &old);
    vtbl[i] = v;
    VirtualProtect(&vtbl[i], sizeof(void *), old, &old);
}

static int inModule(HMODULE m, void *p)
{
    HMODULE owner = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &owner);
    return owner == m;
}

int main(int argc, char **argv)
{
    if (argc < 4) return 2;
    const char *mode = argv[1], *work = argv[2], *ml = argv[3];
    int frames = argc > 4 ? atoi(argv[4]) : 2000;
    char path[1024];
    LARGE_INTEGER f; QueryPerformanceFrequency(&f); qpcFreq = (double)f.QuadPart;
    for (int i = 0; i < 40; ++i) snprintf(tags[i], sizeof(tags[i]), "Player_%02d_Name (%d)", i, 100 + i);
    for (int i = 0; i < 10; ++i) snprintf(labels[i], sizeof(labels[i]), "House for sale #%d - $%d,000", i, 50 + i);

    snprintf(path, sizeof(path), "%s\\d3dx9_25.dll", work);
    HMODULE d3dx = GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES ? LoadLibraryA(path) : LoadLibraryA("d3dx9_25.dll");
    if (!d3dx) { fprintf(stderr, "d3dx9_25 load error %lu\n", GetLastError()); return 3; }
    CreateFontA_t createFont = (CreateFontA_t)GetProcAddress(d3dx, "D3DXCreateFontA");
    CreateSprite_t createSprite = (CreateSprite_t)GetProcAddress(d3dx, "D3DXCreateSprite");
    WNDCLASSA wc = {0}; wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "tc";
    RegisterClassA(&wc);
    HWND wnd = CreateWindowA("tc", "tc", WS_OVERLAPPEDWINDOW, 0, 0, 800, 600, NULL, NULL, wc.hInstance, NULL);
    IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS pp = {0};
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE; pp.hDeviceWindow = wnd;
    if (FAILED(IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, wnd,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev))) { fprintf(stderr, "CreateDevice failed\n"); return 4; }
    if (FAILED(createFont(dev, 16, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
            DEFAULT_PITCH, "Arial", &font)) ||
        FAILED(createFont(dev, 14, 0, FW_NORMAL, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
            DEFAULT_PITCH, "Tahoma", &label)) ||
        FAILED(createSprite(dev, &sprite))) { fprintf(stderr, "font/sprite failed\n"); return 5; }

    void **vtbl = *(void ***)font;
    static const int hooked[5] = {2, 14, 15, 16, 17};
    void *stock[18];
    for (int i = 0; i < 18; ++i) stock[i] = vtbl[i];

    snprintf(path, sizeof(path), "%s\\text_cache.dll", work);
    HMODULE dll = NULL;
    Install_t install = NULL; Uninstall_t uninstall = NULL; Stats_t stats = NULL;
    if (!strcmp(mode, "check") || !strcmp(mode, "dll")) {
        dll = LoadLibraryA(path);
        if (!dll) { fprintf(stderr, "text_cache.dll load error %lu\n", GetLastError()); return 6; }
        install = (Install_t)GetProcAddress(dll, "TextCache_Install");
        uninstall = (Uninstall_t)GetProcAddress(dll, "TextCache_Uninstall");
        stats = (Stats_t)GetProcAddress(dll, "TextCache_Stats");
        if (!install || !uninstall || !stats) { fprintf(stderr, "exports missing\n"); return 7; }
    }

    if (!strcmp(mode, "check")) {
        Stats st;
        uintptr_t foreign = 0;

        IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((uint8_t *)d3dx + ((IMAGE_DOS_HEADER *)d3dx)->e_lfanew);
        int microsoft = nt->FileHeader.TimeDateStamp == 0x423B781A;
        printf("   d3dx9_25 build 0x%08lX (%s)\n", nt->FileHeader.TimeDateStamp, microsoft ? "Microsoft's" : "another: Wine's builtin");
        if (!microsoft) {
            printf("\n== another build and no device: no font to find its vtable with\n");
            ok(install(d3dx, NULL, &foreign) == 0, "Install returns 0");
        }

        printf("\n== a slot outside d3dx9_25: stand down, naming the address\n");
        setSlot(vtbl, 14, (void *)0x12345678);
        ok(install(d3dx, dev, &foreign) == 2 && foreign == 0x12345678, "Install returns 2 and the foreign address");
        stats(&st);
        ok(!st.installed, "not installed");
        setSlot(vtbl, 14, stock[14]);

        printf("\n== install: the five slots point into text_cache.dll, the rest stock\n");
        ok(install(d3dx, dev, &foreign) == 1, "Install returns 1");
        int ours = 1, rest = 1;
        for (int k = 0; k < 5; ++k) if (!inModule(dll, vtbl[hooked[k]])) ours = 0;
        for (int i = 0; i < 18; ++i) {
            int h = 0; for (int k = 0; k < 5; ++k) if (hooked[k] == i) h = 1;
            if (!h && vtbl[i] != stock[i]) rest = 0;
        }
        ok(ours && rest, "Release, DrawTextA/W, OnLost/OnReset are ours; nothing else moved");
        ok(install(d3dx, dev, &foreign) == 1, "a second Install is a no-op");

        printf("\n== 600 frames: replayed, and every 256th hit matches the real draw\n");
        for (int fr = 0; fr < 600; ++fr) frame(fr, NULL);
        stats(&st);
        printf("   %u layouts, %u replayed, %u recorded, %u mismatches\n", st.entries, st.hits, st.recorded, st.mismatches);
        unsigned layouts = microsoft ? 65 : 55;     /* another build: the 10 labels with no sprite are drawn, not cached */
        ok(st.installed && st.hits > 256 * 100 && st.recorded >= layouts && st.entries >= layouts, "hits, recordings, entries");
        ok(st.mismatches == 0 && !st.disabled, "no mismatch with the real d3dx draw");

        printf("\n== uninstall: the stock slots go back; install works again\n");
        ok(uninstall() == 1, "Uninstall returns 1");
        int back = 1;
        for (int i = 0; i < 18; ++i) if (vtbl[i] != stock[i]) back = 0;
        ok(back, "all 18 slots stock");
        stats(&st);
        ok(!st.installed && st.entries == 0, "not installed, cache empty");
        ok(install(d3dx, dev, &foreign) == 1 && inModule(dll, vtbl[14]), "installed again");

        printf("\n== a hook on top of ours: uninstall leaves the chain alone\n");
        void *ourDraw = vtbl[14];
        setSlot(vtbl, 14, (void *)0x12345678);
        ok(uninstall() == 0 && vtbl[14] == (void *)0x12345678, "Uninstall returns 0, the other hook stays");
        setSlot(vtbl, 14, ourDraw);
        ok(uninstall() == 1 && vtbl[14] == stock[14], "with it gone, Uninstall restores");

        printf("\n== fixes.lua's declarations, from the game's lua51.dll\n");
        snprintf(path, sizeof(path), "%s\\lua51.dll", work);
        HMODULE lua = LoadLibraryA(path);
        lua_State *(*newstate)(void) = (void *)GetProcAddress(lua, "luaL_newstate");
        void (*openlibs)(lua_State *) = (void *)GetProcAddress(lua, "luaL_openlibs");
        int (*loadstring)(lua_State *, const char *) = (void *)GetProcAddress(lua, "luaL_loadstring");
        int (*pcall)(lua_State *, int, int, int) = (void *)GetProcAddress(lua, "lua_pcall");
        const char *(*tostr)(lua_State *, int, size_t *) = (void *)GetProcAddress(lua, "lua_tolstring");
        lua_State *L = newstate(); openlibs(L);
        char code[4096];
        snprintf(code, sizeof(code),
            "local ffi = require('ffi')\n"
            "local src = assert(io.open([[%s\\fixes.lua]])):read('*a')\n"
            "local decl = assert(src:match('pcall%%(ffi%%.cdef, %%[%%[(%%s*typedef struct { uint32_t installed.-)%%]%%]'))\n"
            "ffi.cdef(decl)\n"
            "local lib = ffi.load([[%s\\text_cache.dll]])\n"
            "local foreign = ffi.new('uintptr_t[1]')\n"
            "assert(lib.TextCache_Install(ffi.cast('void*', %lu), ffi.cast('void*', %lu), foreign) == 1, 'install')\n"
            "local st = ffi.new('FxTextCacheStats')\n"
            "lib.TextCache_Stats(st)\n"
            "assert(st.installed == 1, 'stats')\n"
            "assert(lib.TextCache_Uninstall() == 1, 'uninstall')\n"
            "lib.TextCache_Stats(st)\n"
            "assert(st.installed == 0, 'stats after')\n", ml, work, (unsigned long)(uintptr_t)d3dx, (unsigned long)(uintptr_t)dev);
        int err = loadstring(L, code) || pcall(L, 0, 0, 0);
        if (err) printf("   lua: %s\n", tostr(L, -1, NULL));
        ok(!err, "install, stats and uninstall through fixes.lua's cdef");

        printf("\ntext_cache.dll: %d passed, %d failed\n", passed, failed);
        fflush(stdout);
        ExitProcess(failed ? 1 : 0);
    }

    if (!strcmp(mode, "dll")) {
        uintptr_t foreign;
        if (install(d3dx, dev, &foreign) != 1) { fprintf(stderr, "Install failed\n"); return 8; }
    } else if (!strcmp(mode, "asi")) {
        snprintf(path, sizeof(path), "%s\\TextCacheFix.asi", work);
        if (!LoadLibraryA(path)) { fprintf(stderr, "TextCacheFix.asi load error %lu\n", GetLastError()); return 9; }
        for (int i = 0; i < 50 && vtbl[14] == stock[14]; ++i) Sleep(100);
        if (vtbl[14] == stock[14]) { fprintf(stderr, "TextCacheFix.asi did not hook\n"); return 10; }
    }
    double *draws = malloc(sizeof(double) * frames), *wholes = malloc(sizeof(double) * frames);
    for (int fr = -200; fr < frames; ++fr) {
        double whole, d = frame(fr, &whole);
        if (fr >= 0) { draws[fr] = d; wholes[fr] = whole; }
    }
    qsort(draws, frames, sizeof(double), cmp);
    qsort(wholes, frames, sizeof(double), cmp);
    printf("%-5s DrawText per frame: median %7.1f us  p95 %7.1f us  (125 calls)  whole frame median %7.1f us\n",
        mode, draws[frames / 2] * 1e6, draws[frames * 95 / 100] * 1e6, wholes[frames / 2] * 1e6);
    fflush(stdout);
    ExitProcess(0);
}
