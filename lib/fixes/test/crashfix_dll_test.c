/* lib/fixes/native/crashfix.c against gta_sa.exe 1.0 US's own code, under
 * Wine: every hook goes in on the stock bytes and comes out leaving them as
 * they were, and the game's functions are called through the hooks with the
 * bad data each one is for, and with good data. fixes.lua's fog routine
 * (fog.bin beside the exe, from fixes_test.lua --fog-bytes) runs against the
 * game's own RwD3D9SetRenderState.
 *
 *     ./crashfix_dll_test.sh [path/to/gta_sa.exe]
 *
 * crashfix.c is built into this DLL. A copy of gta_sa.exe whose entry point
 * only loads it and calls run(), and which imports only KERNEL32, gives the
 * game's sections their own addresses (Wine has its heaps there before an exe
 * of ours could reserve them); none of the game's code runs but what is
 * called here.
 */
#include "../native/crashfix.c"
#include <stdio.h>
#define COBJMACROS
#include <d3d9.h>

static int passed, failed;
static void ok(int cond, const char *what)
{
    if (cond) passed++; else { failed++; printf("  FAIL: %s\n", what); }
}

static uint8_t *file;
static size_t file_size;

typedef struct { uint32_t va, size, raw, raw_size; } Section;
static Section sections[16];
static int nsections;

static const uint8_t *file_at(uint32_t va)
{
    for (int i = 0; i < nsections; i++)
        if (va >= sections[i].va && va < sections[i].va + sections[i].raw_size)
            return file + sections[i].raw + (va - sections[i].va);
    return NULL;
}

static int read_game(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    file = malloc(file_size);
    if (fread(file, 1, file_size, f) != file_size) return 0;
    fclose(f);
    if (file_size != 14383616) { printf("not gta_sa.exe 1.0 US (%u bytes)\n", (unsigned)file_size); return 0; }
    uint32_t pe = *(uint32_t *)(file + 0x3C);
    uint16_t n = *(uint16_t *)(file + pe + 6), opt = *(uint16_t *)(file + pe + 20);
    uint32_t base = *(uint32_t *)(file + pe + 52);
    for (int i = 0; i < n && i < 16; i++) {
        const uint8_t *s = file + pe + 24 + opt + 40 * i;
        sections[i] = (Section){base + *(uint32_t *)(s + 12), *(uint32_t *)(s + 8), *(uint32_t *)(s + 20), *(uint32_t *)(s + 16)};
    }
    nsections = n;
    return GetModuleHandleA(NULL) == (HMODULE)base;
}

static int find(const char *key)
{
    for (int i = 0; i < CrashFix_Count(); i++)
        if (!strcmp(CrashFix_Key(i), key)) return i;
    printf("  no hook %s\n", key);
    return -1;
}

static uint32_t averted(const char *key) { return CrashFix_Averted(find(key)); }

static uint32_t hook_of(const char *key) { return (uint32_t)hooks[find(key)].hook; }

/* What a mod puts at 0x70BDA4: nonzero leaves the function early */
static uint16_t shadow_seen;
static char __cdecl shadow_replacement(void) { shadow_seen = cf_shadow_model; return 1; }

/* A hook's way back into the game, made a ret so it returns here */
static uint8_t patched;
static void ret_at(uint32_t va) { patched = *(uint8_t *)va; *(uint8_t *)va = 0xC3; }
static void unret(uint32_t va) { *(uint8_t *)va = patched; }

/* Call a hook with x on the x87 stack (takes 1), nothing (0) and take st0
 * after, or neither (-1); the status word after it in fpu_sw. Not inlined:
 * the x87 stack is empty around a call. */
float fpu_in, fpu_out;
uint32_t fpu_target;
uint16_t fpu_sw;
__attribute__((noinline)) static float call_fpu(uint32_t entry, float x, int takes)
{
    fpu_target = entry, fpu_in = x;
    if (takes > 0)
        __asm__ volatile("finit\n\tflds _fpu_in\n\tcall *_fpu_target\n\tfnstsw _fpu_sw\n\tfstps _fpu_out\n" ::: "memory", "eax", "ecx", "edx");
    else if (takes == 0)
        __asm__ volatile("finit\n\tcall *_fpu_target\n\tfnstsw _fpu_sw\n\tfstps _fpu_out\n" ::: "memory", "eax", "ecx", "edx");
    else
        __asm__ volatile("finit\n\tcall *_fpu_target\n" ::: "memory", "eax", "ecx", "edx");
    return fpu_out;
}

/* Call a hook as code deep in a function reaches it: eax set, x on the x87
 * stack (takes 1) or not, the 100h bytes above its return address from
 * site_stack and back (site_stack[i] is the hook's [esp+4+4i]); after it eax
 * in site_ret, the flags in site_flags, the status word in fpu_sw and st0 in
 * fpu_out if it left one (left 1). */
uint32_t site_eax, site_ret, site_flags, site_stack[64];
int site_takes, site_left;
typedef uint8_t (__attribute__((thiscall)) *FallFn)(void *task, void *ped);
static uint8_t fall_ped[0x600] = {[0x46C] = 1};         /* standing */

/* Frames until the game's own CTaskSimpleFall::ProcessPed ends a 500 ms down
 * time at `fps`, or 0 after 4000 */
static int fall_frames(float fps)
{
    uint8_t task[0x20] = {0};
    task[8] = 1;                                        /* the fall anim finished */
    *(uint32_t *)(task + 0x1C) = 500;
    *(float *)0xB7CB5C = 50.0f / fps;
    for (int n = 1; n <= 4000; n++) {
        ++*(uint32_t *)0xB7CB4C;
        if (((FallFn)0x67FAF0)(task, fall_ped)) return n;
    }
    return 0;
}

__attribute__((noinline)) static void call_site(uint32_t entry, uint32_t eax, float x, int takes, int left)
{
    fpu_target = entry, fpu_in = x, site_eax = eax, site_takes = takes, site_left = left;
    __asm__ volatile(
        "finit\n\t"
        "cmpl $0, _site_takes\n\t"
        "je 1f\n\t"
        "flds _fpu_in\n"
        "1:\n\t"
        "sub $0x100, %%esp\n\t"
        "xor %%ecx, %%ecx\n"
        "2:\n\t"
        "mov _site_stack(,%%ecx,4), %%eax\n\t"
        "mov %%eax, (%%esp,%%ecx,4)\n\t"
        "inc %%ecx\n\t"
        "cmp $64, %%ecx\n\t"
        "jb 2b\n\t"
        "mov _site_eax, %%eax\n\t"
        "call *_fpu_target\n\t"
        "mov %%eax, _site_ret\n\t"
        "pushfl\n\t"
        "popl _site_flags\n\t"
        "fnstsw _fpu_sw\n\t"
        "xor %%ecx, %%ecx\n"
        "3:\n\t"
        "mov (%%esp,%%ecx,4), %%eax\n\t"
        "mov %%eax, _site_stack(,%%ecx,4)\n\t"
        "inc %%ecx\n\t"
        "cmp $64, %%ecx\n\t"
        "jb 3b\n\t"
        "add $0x100, %%esp\n\t"
        "cmpl $0, _site_left\n\t"
        "je 4f\n\t"
        "fstps _fpu_out\n"
        "4:\n"
        ::: "memory", "eax", "ecx", "edx");
}

/* Call a hook with ebp, esi and edi from reg_ebp, reg_esi and reg_edi, x on
 * the x87 stack (takes 1) or not; st0 after in fpu_out if it left one (left
 * 1), the status word before that in fpu_sw. -> eax */
uint32_t reg_ebp, reg_esi, reg_edi, reg_eax;
__attribute__((noinline)) static uint32_t call_regs(uint32_t entry, float x, int takes, int left)
{
    fpu_target = entry, fpu_in = x, site_takes = takes, site_left = left;
    __asm__ volatile(
        "finit\n\t"
        "push %%ebp\n\t"
        "push %%esi\n\t"
        "push %%edi\n\t"
        "push %%ebx\n\t"
        "cmpl $0, _site_takes\n\t"
        "je 1f\n\t"
        "flds _fpu_in\n"
        "1:\n\t"
        "mov _reg_ebp, %%ebp\n\t"
        "mov _reg_esi, %%esi\n\t"
        "mov _reg_edi, %%edi\n\t"
        "call *_fpu_target\n\t"
        "mov %%eax, _reg_eax\n\t"
        "fnstsw _fpu_sw\n\t"
        "pop %%ebx\n\t"
        "pop %%edi\n\t"
        "pop %%esi\n\t"
        "pop %%ebp\n\t"
        "cmpl $0, _site_left\n\t"
        "je 2f\n\t"
        "fstps _fpu_out\n"
        "2:\n"
        ::: "memory", "eax", "ecx", "edx");
    return reg_eax;
}

/* A thiscall function of four arguments, x the second */
typedef uint32_t (__attribute__((thiscall)) *Thiscall4)(void *self, uint32_t a, uint32_t b, uint32_t c, uint32_t d);
static uint32_t call_thiscall4(uint32_t entry, uint32_t x) { return ((Thiscall4)entry)(NULL, 0, x, 0, 0); }

/* The texture dictionary the game keeps: RwObject type 6, the list at +8; a
 * texture links at +8 and is named at +10h. */
typedef struct Link { struct Link *next, *prev; } Link;
typedef struct { uint8_t type, pad[7]; Link textures; } Dictionary;
typedef struct { void *raster; Dictionary *dict; Link link; char name[32], mask[32]; uint32_t filter, refs; } Texture;

static void dictionary(Dictionary *d) { memset(d, 0, sizeof *d); d->type = 6; d->textures.next = d->textures.prev = &d->textures; }

typedef Texture *(__cdecl *FindNamedFn)(void *dict, const char *name);
typedef void *(__cdecl *ForAllFn)(void *dict, void *(__cdecl *cb)(Texture *, void *), void *data);
typedef void *(__cdecl *AddFn)(void *dict, Texture *tex);
typedef Texture *(__cdecl *FindTextureCBFn)(const char *name);
typedef uint32_t (__attribute__((thiscall)) *GetSoundFn)(void *self, uint32_t sound, uint32_t slot, uint32_t *size, uint16_t *rate);
typedef uint8_t (__attribute__((thiscall)) *IsLoadedFn)(void *self, uint32_t bank, uint32_t slot);
typedef void (__attribute__((thiscall)) *EnableFn)(void *prim, uint32_t on);
typedef void (__attribute__((thiscall)) *TranslationFn)(void *node, float *out, float weight);
typedef void *(__attribute__((thiscall)) *UpdateRwFn)(void *m, void *rw);
typedef void (__cdecl *SetAlphaFn)(void *clump, int alpha);
typedef int (__cdecl *GetAlphaFn)(void *clump);
typedef void *(__cdecl *MatMulFn)(void *dst, void *a, void *b);
typedef uint32_t (__cdecl *InstFn)(uint32_t type, void *mem, void *src, uint32_t n, uint32_t stride);
typedef void (__cdecl *FallOutFn)(void *assoc, void *task);

static int walked;
static void *__cdecl count_texture(Texture *t, void *data) { (void)t; walked++; return data; }

/* The sky's vertical gradient from `top` to `bottom` (0xRRGGBB) over a
 * 64x256 backbuffer, through skydither or with the stock fixed-function
 * draw; -> the backbuffer's pixels, NULL without a device */
#define SKY_W 64
#define SKY_H 256
static IDirect3DDevice9 *sky_device;

static IDirect3DDevice9 *sky_open(void)
{
    HMODULE d3d9 = LoadLibraryA("d3d9.dll");
    IDirect3D9 *(WINAPI *create)(UINT) = d3d9 ? (void *)GetProcAddress(d3d9, "Direct3DCreate9") : NULL;
    IDirect3D9 *d3d = create ? create(D3D_SDK_VERSION) : NULL;
    if (!d3d) return NULL;
    HWND window = CreateWindowA("STATIC", "sky", WS_POPUP, 0, 0, SKY_W, SKY_H, NULL, NULL, NULL, NULL);
    D3DPRESENT_PARAMETERS pp = {SKY_W, SKY_H, D3DFMT_X8R8G8B8, 1};
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD, pp.hDeviceWindow = window, pp.Windowed = TRUE;
    IDirect3DDevice9 *device = NULL;
    if (FAILED(IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
                                       D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &device))
        && FAILED(IDirect3D9_CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window,
                                          D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device))) return NULL;
    return device;
}

static uint32_t *sky_draw(uint32_t top, uint32_t bottom, int dithered)
{
    static uint32_t pixels[SKY_W * SKY_H];
    IDirect3DDevice9 *dev = sky_device;
    SkyVertex *v = (SkyVertex *)SKY_VERTICES;
    uint16_t *ix = (uint16_t *)SKY_INDICES;
    const float corner[4][2] = {{-1, 1}, {1, 1}, {-1, -1}, {1, -1}};
    for (int i = 0; i < 4; i++)
        v[i] = (SkyVertex){corner[i][0], corner[i][1], 0.5f, 0, 0, 0, 0xFF000000 | (i < 2 ? top : bottom), 0, 0};
    static const uint16_t quad[6] = {0, 2, 1, 1, 2, 3};          /* as SetUpOneSkyPoly */
    memcpy(ix, quad, sizeof quad);
    SKY_NVERTICES = 4, SKY_NINDICES = 6;
    IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(dev, D3DRS_LIGHTING, FALSE);
    IDirect3DDevice9_Clear(dev, 0, NULL, D3DCLEAR_TARGET, 0, 1, 0);
    IDirect3DDevice9_BeginScene(dev);
    if (dithered) ((void (*)(void))hook_of("skydither"))();
    else {
        IDirect3DDevice9_SetFVF(dev, SKY_FVF);
        IDirect3DDevice9_DrawIndexedPrimitiveUP(dev, D3DPT_TRIANGLELIST, 0, 4, 2, ix, D3DFMT_INDEX16, v, sizeof *v);
        SKY_NVERTICES = 0, SKY_NINDICES = 0;
    }
    IDirect3DDevice9_EndScene(dev);
    IDirect3DSurface9 *back = NULL, *copy = NULL;
    D3DLOCKED_RECT lr;
    if (FAILED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &back))
        || FAILED(IDirect3DDevice9_CreateOffscreenPlainSurface(dev, SKY_W, SKY_H, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM,
                                                               &copy, NULL))
        || FAILED(IDirect3DDevice9_GetRenderTargetData(dev, back, copy))
        || FAILED(IDirect3DSurface9_LockRect(copy, &lr, NULL, D3DLOCK_READONLY))) return NULL;
    for (int y = 0; y < SKY_H; y++) memcpy(pixels + y * SKY_W, (uint8_t *)lr.pBits + y * lr.Pitch, SKY_W * 4);
    IDirect3DSurface9_UnlockRect(copy);
    IDirect3DSurface9_Release(copy);
    IDirect3DSurface9_Release(back);
    return pixels;
}

/* -> how far the rows' means stray from the gradient, at most and the most
 * pixels of a row off by more than 1 from it; one channel, shift 16/8/0 */
static void sky_measure(const uint32_t *p, int top, int bottom, int shift, double *mean_err, int *off)
{
    *mean_err = 0, *off = 0;
    for (int y = 0; y < SKY_H; y++) {
        double want = top + (bottom - top) * ((double)y / SKY_H), sum = 0;   /* D3D9 samples at whole pixels */
        int n = 0;
        for (int x = 0; x < SKY_W; x++) {
            int c = (p[y * SKY_W + x] >> shift) & 0xFF;
            sum += c;
            if (fabs(c - want) > 1.0) n++;
        }
        if (fabs(sum / SKY_W - want) > *mean_err) *mean_err = fabs(sum / SKY_W - want);
        if (n > *off) *off = n;
    }
}

/* cTransmission::CalculateDriveAcceleration over a 4 s launch, at a frame
 * time of `ts`: the speed it reaches (each step's drive added as it is) */
typedef float (__attribute__((thiscall)) *DriveFn)(void *t, const float *gas, uint8_t *gear, float *count, float *v,
                                                   float *a6, float *a7, uint8_t wheels, uint8_t cheat);
static float launch(float ts)
{
    static uint8_t tr[0x68];
    memset(tr, 0, sizeof tr);
    tr[0x4A] = 4;                                   /* the Savanna: 4 gears, 160 km/h, 24, inertia 5 */
    *(float *)(tr + 0x50) = 24.0f * 0.0004f * 0.25f;
    *(float *)(tr + 0x54) = 5.0f;
    *(float *)(tr + 0x58) = 160.0f / 180.0f;
    *(float *)(tr + 0x60) = -0.2f;
    ((void (__attribute__((thiscall)) *)(void *))0x6D0460)(tr);   /* InitGearRatios */
    *(float *)0xB7CB5C = ts;
    float gas = 1.0f, count = 0.0f, v = 0.0f, a6 = 0.0f, a7 = 0.0f;
    uint8_t gear = 1;
    for (float t = 0; t < 200.0f; t += ts) v += ((DriveFn)0x6D05E0)(tr, &gas, &gear, &count, &v, &a6, &a7, 4, 0);
    return v;
}

/* SA-MixSets' rows */
static int skid_calls;
static float skid_point[2][3], skid_from[2][3];
static int32_t skid_wheel[2];
static void __attribute__((thiscall)) skid_recorder(void *veh, int32_t state, uint32_t colour, float a, float b,
                                                     void *colpoint, float *from, int32_t id, int32_t wheel,
                                                     int32_t skid, void *blood, uint32_t flags)
{
    (void)veh; (void)state; (void)colour; (void)a; (void)b; (void)id; (void)skid; (void)blood; (void)flags;
    if (skid_calls < 2) {
        memcpy(skid_point[skid_calls], colpoint, 12);
        memcpy(skid_from[skid_calls], from, 12);
        skid_wheel[skid_calls] = wheel;
    }
    skid_calls++;
}

static void mixsets_test(void)
{
    printf("== SA-MixSets: the wake's light, the second rear wheel's skidmark\n");
    float *water = (float *)0xB7C508, *mult = (float *)0x8D390C, keep[4];
    uint32_t *frame = (uint32_t *)0xB7CB4C;
    memcpy(keep, water, sizeof keep);
    water[0] = 90, water[1] = 170, water[2] = 170, water[3] = 240;      /* timecyc's noon water */
    (*frame)++;
    cf_boat_foam();
    ok(mult[0] == 0.4f && mult[1] == 1.0f && mult[2] == 0.2f && mult[3] == 1.0f && mult[4] == 0.4f,
       "boatfoam: day water, the stock wake");
    water[0] = 67, water[1] = 67, water[2] = 62, water[3] = 240;        /* 22:00 */
    (*frame)++;
    cf_boat_foam();
    ok(mult[1] > 0.5f && mult[1] < 0.6f && fabsf(mult[0] - 0.4f * mult[1]) < 1e-6f && mult[2] == 0.2f,
       "boatfoam: night water, about half");
    cf_boat_foam_stock();
    ok(mult[0] == 0.4f && mult[1] == 1.0f && mult[3] == 1.0f && mult[4] == 0.4f, "boatfoam: put back when it goes");
    memcpy(water, keep, sizeof keep);

    uint8_t keepcode[5], jmp[5] = {0xE9};
    memcpy(keepcode, (void *)0x6DE880, 5);
    *(uint32_t *)(jmp + 1) = (uint32_t)skid_recorder - (0x6DE880 + 5);
    write_code(0x6DE880, jmp, 5);
    static uint8_t veh[0x400], handling[0x100];
    static float m[18];
    memset(veh, 0, sizeof veh);
    memset(handling, 0, sizeof handling);
    memset(m, 0, sizeof m);
    m[0] = 0.6f, m[1] = 0.8f;                                           /* right */
    *(float **)(veh + 0x14) = m;
    *(void **)(veh + 0x384) = handling;
    float point[11] = {10, 20, 30}, from[3] = {1, 2, 3};
    skid_calls = 0;
    cf_double_skid(veh, 0, 0, 0, 0, point, from, 7, 1, 0, NULL, 0);
    ok(skid_calls == 1, "doubleskid: a car without double rear wheels, one skidmark");
    *(uint32_t *)(handling + 0xCC) = 0x20000000;
    skid_calls = 0;
    cf_double_skid(veh, 0, 0, 0, 0, point, from, 7, 2, 0, NULL, 0);
    ok(skid_calls == 1, "doubleskid: a front wheel, one");
    skid_calls = 0;
    cf_double_skid(veh, 0, 0, 0, 0, point, from, 7, 1, 0, NULL, 0);
    ok(skid_calls == 2 && skid_wheel[1] == 1 + 69696969, "doubleskid: a rear wheel of a truck, two");
    ok(fabsf(skid_point[1][0] - (10 + 0.6f * 0.45f)) < 1e-5f && fabsf(skid_from[1][1] - (2 + 0.8f * 0.45f)) < 1e-5f,
       "doubleskid: the second 0.45 along the car's right");
    skid_calls = 0;
    cf_double_skid(veh, 0, 0, 0, 0, point, from, 7, 3, 0, NULL, 0);
    ok(skid_calls == 2 && fabsf(skid_point[1][0] - (10 - 0.6f * 0.45f)) < 1e-5f, "doubleskid: the right side's to the right");
    write_code(0x6DE880, keepcode, 5);
}

/* The stream timer's stand-ins for the game's functions */
static int stream_calls;
static uint32_t __cdecl stream_game(void *buffer, int32_t id)
{
    (void)buffer;
    stream_calls++;
    Sleep(id == 20005 ? 4 : 1);
    return 0xAB000001;                                                  /* al 1: converted */
}

typedef uint32_t (__cdecl *StreamFn)(void *buffer, int32_t id);
static void __cdecl stream_drain(uint32_t priority)
{
    (void)priority;
    ((StreamFn)0x40C6B0)(NULL, 25001);
    ((StreamFn)0x408CB0)(NULL, 300);
}

/* fn(NULL, id) with esi, edi and ebx set: -> they come back */
static int stream_keeps(uint32_t fn, int32_t id, uint32_t *eax)
{
    uint32_t esi, edi, ebx;
    __asm__ volatile("mov $0x51515151, %%esi\n mov $0xD1D1D1D1, %%edi\n mov $0xB1B1B1B1, %%ebx\n"
                     "push %%ecx\n push $0\n call *%%eax\n add $8, %%esp"
                     : "=a"(*eax), "=S"(esi), "=D"(edi), "=b"(ebx) : "a"(fn), "c"(id) : "edx", "memory", "cc");
    return esi == 0x51515151 && edi == 0xD1D1D1D1 && ebx == 0xB1B1B1B1;
}

static void stream_test(const char *exe)
{
    printf("== the stream timer (streamtime): the three functions timed, by id and type, into stream.log\n");
    static const char *const keys[3] = {"streamconvert", "streamfinish", "streamall"};
    int was[3];
    for (int i = 0; i < 3; i++) {
        was[i] = CrashFix_Installed(find(keys[i]));
        CrashFix_Install(find(keys[i]), NULL);
    }
    const Hook *convert = &hooks[find("streamconvert")], *txd = &hooks[find("txdconvert")];
    ok(convert->va == 0x40C6B0 && convert->va + convert->len <= txd->va && CrashFix_Installed(find("txdconvert"))
       && CrashFix_Installed(find("streamconvert")), "the entry's hook ends before txdconvert's site; both in");
    ok(cf_stream_type(0) == ST_DFF && cf_stream_type(19999) == ST_DFF && cf_stream_type(20000) == ST_TXD
       && cf_stream_type(24999) == ST_TXD && cf_stream_type(25000) == ST_COL && cf_stream_type(25254) == ST_COL
       && cf_stream_type(25255) == ST_OTHER && cf_stream_type(25575) == ST_IFP && cf_stream_type(25754) == ST_IFP
       && cf_stream_type(25755) == ST_OTHER && cf_stream_type(-1) == ST_OTHER, "an id's type, by the game's ranges");

    /* the game's bodies past the hooks: back to the entry's stack, then the stand-ins */
    uint8_t keep[3][9], body[9];
    memcpy(keep[0], (void *)0x40C6B7, 8);
    memcpy(keep[1], (void *)0x408CB8, 9);
    memcpy(keep[2], (void *)0x40A45E, 5);
    memcpy(body, "\x83\xC4\x20\xE9", 4);                                 /* add esp, 0x20; jmp */
    *(uint32_t *)(body + 4) = (uint32_t)stream_game - (0x40C6B7 + 8);
    write_code(0x40C6B7, body, 8);
    memcpy(body, "\x5E\x83\xC4\x08\xE9", 5);                             /* pop esi; add esp, 8; jmp */
    *(uint32_t *)(body + 5) = (uint32_t)stream_game - (0x408CB8 + 9);
    write_code(0x408CB8, body, 9);
    body[0] = 0xE9;
    *(uint32_t *)(body + 1) = (uint32_t)stream_drain - (0x40A45E + 5);
    write_code(0x40A45E, body, 5);

    char log[MAX_PATH];
    strcpy(log, exe);
    strcpy(strrchr(log, '\\') + 1, "stream.log");
    DeleteFileA(log);
    uint32_t *sectors = (uint32_t *)(0x8E4CC0 + 20 * 100 + 0xC), keep_sectors = *sectors;
    *sectors = 48;
    double ms0 = CrashFix_StreamStat(0), n0 = CrashFix_StreamStat(1), all0 = CrashFix_StreamStat(2), alls0 = CrashFix_StreamStat(3);
    ok(CrashFix_StreamStart(log) == 1 && CrashFix_StreamStart(log) == 1, "started; a second start keeps the first");
    CrashFix_StreamResetWorst();
    ok(CrashFix_StreamStat(5) == -1, "nothing yet: no worst file");

    stream_calls = 0;
    uint32_t r1, r2, r3;
    int kept = stream_keeps(0x40C6B0, 100, &r1) & stream_keeps(0x40C6B0, 20005, &r2) & stream_keeps(0x408CB0, 300, &r3);
    ok(stream_calls == 3 && r1 == 0xAB000001 && r2 == r1 && r3 == r1, "through the hooks: the game's functions run, eax comes back whole");
    ok(kept, "esi, edi and ebx come back");
    double ms = CrashFix_StreamStat(0) - ms0;
    ok(CrashFix_StreamStat(1) - n0 == 3 && ms >= 4.0 && ms < 500.0, "three files, their time summed");
    printf("  3 files: %.3f ms, the longest %.3f ms (id %d)\n", ms, CrashFix_StreamStat(4), (int)CrashFix_StreamStat(5));
    ok(CrashFix_StreamStat(5) == 20005 && CrashFix_StreamStat(4) >= 3.0 && CrashFix_StreamStat(4) < ms, "the longest: the 4 ms texture dictionary");
    CrashFix_StreamResetWorst();
    ok(CrashFix_StreamStat(5) == -1 && CrashFix_StreamStat(4) == 0, "the longest starts again when asked");

    ((void (__cdecl *)(uint32_t))0x40EA10)(1);
    double all = CrashFix_StreamStat(2) - all0;
    ok(stream_calls == 5 && CrashFix_StreamStat(3) - alls0 == 1 && CrashFix_StreamStat(1) - n0 == 5 && all >= 1.5 && all < 500.0,
       "a drain: counted once, timed whole, its two files counted too");
    ok(CrashFix_StreamStat(0) - ms0 <= ms + all, "the drain's files are no longer than the drain");

    const char *sum = CrashFix_StreamSummary();
    ok(strstr(sum, "convert dff: 1 in ") && strstr(sum, "convert txd: 1 in ") && strstr(sum, "convert col: 1 in ")
       && strstr(sum, "finish dff: 2 in ") && strstr(sum, "loadall: 1 in ") && strstr(sum, "txd 20005: 1 load(s), ")
       && strstr(sum, "dff 100: 1 load(s), "), "the summary: by kind and type, the drains, the models");
    ok(strstr(sum, "txd 20005") < strstr(sum, "dff 100: "), "the model that took longest first");
    ok(CrashFix_StreamStop() == 1, "stopped");
    ((StreamFn)0x40C6B0)(NULL, 101);
    CrashFix_StreamFlush();
    ok(CrashFix_StreamStat(1) - n0 == 6, "stopped: still counted");

    static char text[16384];
    FILE *f = fopen(log, "rb");
    size_t n = f ? fread(text, 1, sizeof text - 1, f) : 0;
    if (f) fclose(f);
    text[n] = 0;
    const char *head = strstr(text, " fixes streamtime: ");
    const char *dff = head ? strstr(head, " convert dff 100 ") : NULL;
    const char *tex = dff ? strstr(dff, " convert txd 20005 ") : NULL;
    const char *fin = tex ? strstr(tex, " finish dff 300 ") : NULL;
    const char *col = fin ? strstr(fin, " convert col 25001 ") : NULL;
    const char *drain = col ? strstr(col, " loadall ") : NULL;
    ok(head && text[0] == '=' && text[1] == '=', "stream.log: a header");
    ok(dff && tex && fin && col, "a line for each file, in order, with its kind, type and id");
    ok(dff && strstr(dff, " ms 96 KB\r\n") && strstr(dff, " ms 96 KB\r\n") < tex, "and its size in the IMG");
    ok(dff && tex && !memchr(dff, 'l', tex - dff), "outside a drain: not marked");
    ok(col && drain && strstr(col, " KB in loadall\r\n") && strstr(col, " KB in loadall\r\n") < drain, "inside one: marked");
    ok(drain && strstr(drain, " ms, 2 file(s) ") && strstr(drain, " ms, from crashfix_dll_test.dll+0x"),
       "the drain's line: its time, its files, who called it");
    ok(drain && strstr(drain, "-- summary at +") && strstr(drain, "convert txd: 1 in "), "the summary at the stop");
    ok(!strstr(text, " convert dff 101 "), "nothing written after the stop");
    if (!drain) printf("%s\n", text);

    *sectors = keep_sectors;
    write_code(0x40C6B7, keep[0], 8);
    write_code(0x408CB8, keep[1], 9);
    write_code(0x40A45E, keep[2], 5);
    int out = 1;
    for (int i = 0; i < 3; i++) {
        const Hook *h = &hooks[find(keys[i])];
        out &= CrashFix_Uninstall(find(keys[i])) == 1 && !memcmp((void *)h->va, file_at(h->va), 16);
        if (was[i]) CrashFix_Install(find(keys[i]), NULL);
    }
    ok(out, "out again: the three entries stock");
}

static void gear_test(void)
{
    printf("== the gearbox's engine inertia per 30 fps frame (gearstep, gearinertia, gearsmooth1, 2)\n");
    const char *keys[] = {"gearstep", "gearinertia", "gearsmooth1", "gearsmooth2"};
    float keep = *(float *)0xB7CB5C;
    int was[4];
    for (int i = 0; i < 4; i++) { was[i] = CrashFix_Installed(find(keys[i])); CrashFix_Uninstall(find(keys[i])); }
    float s30 = launch(50.0f / 30.0f), s850 = launch(50.0f / 850.0f);
    ok(s850 > s30 * 1.03f, "stock: 850 fps reaches a higher speed than 30 fps");
    printf("  stock: %.4f at 30 fps, %.4f at 850 fps\n", s30, s850);
    int in = 1;
    for (int i = 0; i < 4; i++) in &= CrashFix_Install(find(keys[i]), NULL) == 1;
    ok(in, "the four rows in");
    float f30 = launch(50.0f / 30.0f), f850 = launch(50.0f / 850.0f), f144 = launch(50.0f / 144.0f);
    printf("  fixed: %.4f at 30 fps, %.4f at 144, %.4f at 850\n", f30, f144, f850);
    ok(fabsf(f30 - s30) < s30 * 0.001f, "30 fps as before");
    ok(fabsf(f850 - f30) < f30 * 0.015f && fabsf(f144 - f30) < f30 * 0.015f, "144 and 850 fps within 1.5% of 30");
    for (int i = 0; i < 4; i++) if (!was[i]) CrashFix_Uninstall(find(keys[i]));
    *(float *)0xB7CB5C = keep;
}

/* CVehicle::ProcessWheel for a free-rolling wheel sliding sideways 0.02 a step
 * (1 m/s) at a frame time of `ts`: the wheel state it leaves */
typedef void (__attribute__((thiscall)) *WheelFn)(void *veh, float *fwd, float *right, float *speed, float *point,
                                                  int32_t wheels, float thrust, float brake, float adhesion,
                                                  int8_t id, float *wspeed, int32_t *state, uint16_t status);
static float slide_brake, slide_along;
static int32_t slide(float ts, float *sideways)
{
    static uint8_t veh[0x1000], handling[0x100];
    static float matrix[18];
    memset(veh, 0, sizeof veh);
    memset(handling, 0, sizeof handling);
    memset(matrix, 0, sizeof matrix);
    matrix[0] = matrix[5] = matrix[10] = 1.0f;
    *(float **)(veh + 0x14) = matrix;
    *(float *)(veh + 0x8C) = 1500.0f;               /* mass, turn mass */
    *(float *)(veh + 0x90) = 2500.0f;
    *(void **)(veh + 0x384) = handling;
    *(float *)(handling + 0xA4) = 0.8f;             /* traction loss */
    *(uint8_t *)0xC1CDAC = 0;                       /* bAlreadySkidding */
    *(float *)0xB7CB5C = ts;
    float fwd[3] = {0, 1, 0}, right[3] = {1, 0, 0}, speed[3] = {0.02f, 0, 0}, point[3] = {1, 1, 0}, wspeed = 0;
    int32_t state = 0;
    if (slide_brake > 0) speed[0] = 0, speed[1] = slide_along;    /* a braked wheel creeping along */
    ((WheelFn)0x6D6C00)(veh, fwd, right, speed, point, 4, 0.0f, slide_brake * ts, 0.004f, 0, &wspeed, &state, 0);
    if (sideways) *sideways = *(float *)(veh + 0x44);
    return state;
}

static void wheel_test(void)
{
    printf("== a tyre skids on a 30 fps frame's sideways slip (wheelskid)\n");
    float keep = *(float *)0xB7CB5C, v30s, v850s, v30f, v850f;
    int was = CrashFix_Installed(find("wheelskid"));
    CrashFix_Uninstall(find("wheelskid"));
    ok(slide(50.0f / 30.0f, &v30s) == 0, "stock, 30 fps: 1 m/s sideways is held, no skid");
    ok(slide(50.0f / 850.0f, &v850s) == 2, "stock, 850 fps: the same slip skids");
    ok(CrashFix_Install(find("wheelskid"), NULL) == 1, "wheelskid in");
    ok(slide(50.0f / 30.0f, &v30f) == 0 && v30f == v30s, "fixed, 30 fps: as before");
    ok(slide(50.0f / 850.0f, &v850f) == 0, "fixed, 850 fps: no skid");
    {
        float adh = 0.004f * 50.0f / 850.0f;
        ok(fabsf(v850s + 0.8f * adh) < adh * 1e-3f, "stock: the push is 0.8 of the frame's adhesion (the traction loss)");
        ok(fabsf(v850f + adh) < adh * 1e-3f, "fixed: the frame's whole adhesion, no more");
    }
    ok(slide(50.0f / 850.0f, NULL) == 0 && slide(50.0f / 30.0f, NULL) == 0, "again");
    /* a car on its brake, a wheel creeping 0.002 a step (0.1 m/s): held at 30 fps */
    slide_brake = 0.012f, slide_along = 0.002f;
    CrashFix_Uninstall(find("wheelskid"));
    int stock30 = slide(50.0f / 30.0f, NULL), stock850 = slide(50.0f / 850.0f, NULL);
    CrashFix_Install(find("wheelskid"), NULL);
    int fixed30 = slide(50.0f / 30.0f, NULL), fixed850 = slide(50.0f / 850.0f, NULL);
    printf("  braked and creeping: stock %d at 30 fps, %d at 850; fixed %d, %d\n", stock30, stock850, fixed30, fixed850);
    ok(stock30 == 0 && stock850 != 0, "stock: the creep skids at 850 fps only");
    ok(fixed30 == 0 && fixed850 == 0, "fixed: held at both");
    slide_brake = 0.0004f, slide_along = 0.02f;            /* braking hard while moving: the brake's push */
    ok(slide(50.0f / 850.0f, NULL) == slide(50.0f / 30.0f, NULL), "a wheel braking at its brake limit: the same state at both");
    slide_brake = 0;
    if (!was) CrashFix_Uninstall(find("wheelskid"));
    *(float *)0xB7CB5C = keep;
}

static void sky_test(void)
{
    int k = find("skydither");
    uint32_t found;
    int r = CrashFix_Install(k, &found);
    ok(r >= 1 && *(uint8_t *)0x7154A8 == 0xE9
       && 0x7154AD + *(uint32_t *)0x7154A9 == (uint32_t)hooks[k].hook, "the tail jump to RenderOutGeometryBuffer is ours");
    void **device = &SKY_DEVICE, *was = *device;
    *device = NULL;
    SKY_NINDICES = 0;
    ((void (*)(void))hook_of("skydither"))();
    ok(sky_made == 0, "no sky in the buffer: the stock flush, no shaders made");

    sky_device = sky_open();
    if (!sky_device) { printf("  skipped: no D3D9 device under this Wine\n"); CrashFix_Uninstall(k); return; }
    float keep_view[16], keep_proj[16], identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    int32_t keep_size[2];
    memcpy(keep_view, (void *)0xC9BC80, 64), memcpy(keep_proj, (void *)0x8E2458, 64);
    memcpy(keep_size, (void *)0xC17044, 8);
    memcpy((void *)0xC9BC80, identity, 64), memcpy((void *)0x8E2458, identity, 64);
    *(int32_t *)0xC17044 = SKY_W, *(int32_t *)0xC17048 = SKY_H;
    *(uint32_t *)0xC9A5EC = 0;                                    /* no render states queued */
    *device = sky_device;

    const int top[3] = {40, 90, 170}, bottom[3] = {52, 120, 210};
    const uint32_t t = top[0] << 16 | top[1] << 8 | top[2], b = bottom[0] << 16 | bottom[1] << 8 | bottom[2];
    uint32_t *stock = sky_draw(t, b, 0);
    double stock_err[3] = {0};
    int stock_off[3] = {0};
    if (stock) for (int c = 0; c < 3; c++) sky_measure(stock, top[c], bottom[c], 16 - 8 * c, &stock_err[c], &stock_off[c]);
    uint32_t *sky = stock ? sky_draw(t, b, 1) : NULL;
    ok(sky != NULL && sky_made == 1, "the shaders made on the first sky with a device");
    if (sky) {
        double err[3];
        int off[3], mixed = 0;
        for (int c = 0; c < 3; c++) sky_measure(sky, top[c], bottom[c], 16 - 8 * c, &err[c], &off[c]);
        for (int y = 0; y < SKY_H && !mixed; y++)
            for (int x = 1; x < SKY_W; x++)
                if ((sky[y * SKY_W + x] >> 16 & 0xFF) != (sky[y * SKY_W] >> 16 & 0xFF)) { mixed = 1; break; }
        printf("  rows' means off the gradient by at most: stock %.2f %.2f %.2f, dithered %.2f %.2f %.2f\n",
               stock_err[0], stock_err[1], stock_err[2], err[0], err[1], err[2]);
        ok(stock_err[0] > 0.3 && stock_err[2] > 0.3, "stock: each row one value, the steps show");
        ok(err[0] < 0.2 && err[1] < 0.2 && err[2] < 0.2, "dithered: each row's mean follows the gradient");
        if (off[0] || off[1] || off[2]) printf("  pixels off by more than 1 in a row: %d %d %d\n", off[0], off[1], off[2]);
        ok(off[0] == 0 && off[1] == 0 && off[2] == 0, "and every pixel is within 1 of it");
        ok(mixed, "a row mixes two values");
        ok(SKY_NVERTICES == 0 && SKY_NINDICES == 0, "the buffer emptied, as the stock flush does");
        ok(*(void **)0x8E2448 == NULL && *(void **)0x8E244C == NULL && *(uint32_t *)0x8E2440 == SKY_FVF,
           "RwD3D9's shader and FVF caches match the device after");
    }

    *device = was;
    memcpy((void *)0xC9BC80, keep_view, 64), memcpy((void *)0x8E2458, keep_proj, 64);
    memcpy((void *)0xC17044, keep_size, 8);
    ok(CrashFix_Uninstall(k) == 1 && !memcmp((void *)0x7154A8, "\xE9\xA3\xCC\x00\x00", 5), "out again: the stock jump");
}

/* ------------------------------------------------------------------------ */
/* faults.c: a fault in the game's own code, written down with its callers   */
/* ------------------------------------------------------------------------ */
/* guarded_call(fn, ecx) -> 0 when fn returned, 1 when it faulted (recover_fault
 * puts esp back and lands on guarded_landing) */
int __cdecl guarded_call(uint32_t fn, uint32_t ecx);
extern char guarded_landing[];
uint32_t guard_esp;
__asm__(
".intel_syntax noprefix\n"
".globl _guarded_call\n"
"_guarded_call:\n"
"    push ebx\n"
"    push esi\n"
"    push edi\n"
"    push ebp\n"
"    mov dword ptr [_guard_esp], esp\n"
"    mov ecx, dword ptr [esp+24]\n"
"    mov eax, dword ptr [esp+20]\n"
"    call eax\n"
"    xor eax, eax\n"
"    jmp 1f\n"
".globl _guarded_landing\n"
"_guarded_landing:\n"
"    mov eax, 1\n"
"1:  pop ebp\n"
"    pop edi\n"
"    pop esi\n"
"    pop ebx\n"
"    ret\n"
".att_syntax\n");
static volatile LONG guarding;

static LONG CALLBACK recover_fault(EXCEPTION_POINTERS *e)
{
    if (e->ExceptionRecord->ExceptionCode == 0xE0000001) return EXCEPTION_CONTINUE_EXECUTION;   /* raised below */
    if (!guarding || e->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) return EXCEPTION_CONTINUE_SEARCH;
    guarding = 0;
    e->ContextRecord->Esp = guard_esp;
    e->ContextRecord->Eip = (DWORD)(uintptr_t)guarded_landing;
    return EXCEPTION_CONTINUE_EXECUTION;
}

static void faults_test(const char *exe)
{
    char log[MAX_PATH];
    strcpy(log, exe);
    strcpy(strrchr(log, '\\') + 1, "faults.log");
    DeleteFileA(log);
    ok(CrashFix_FaultsStart(log) == 1, "started");
    ok(CrashFix_FaultsStart(log) == 1, "a second start keeps the first (one handler)");
    void *recover = AddVectoredExceptionHandler(0, recover_fault);   /* after ours */

    /* CAtomicModelInfo::CreateInstance (0x4C4530) on a model with a bad atomic:
     * with no RenderWare engine RpAtomicClone faults one call in, in
     * RpAtomicCreate (0x749C5D, reading address 0). AddRef gets a texture
     * dictionary pool of one. */
    static uint8_t model[0x40], objects[16], flags[4];
    static uint32_t pool[2];
    volatile uint32_t *txd = (uint32_t *)0xC8800C, *engine = (uint32_t *)0xC97B24, *plugin = (uint32_t *)0xC924AC;
    uint32_t keep_txd = *txd, keep_engine = *engine, keep_plugin = *plugin;
    pool[0] = (uint32_t)(uintptr_t)objects, pool[1] = (uint32_t)(uintptr_t)flags;
    *txd = (uint32_t)(uintptr_t)pool, *engine = 0, *plugin = 0;
    *(uint32_t *)(model + 0x1C) = 0xDEAD0000;
    uint32_t seen = CrashFix_FaultsSeen();
    guarding = 1;
    int first = guarded_call(0x4C4530, (uint32_t)(uintptr_t)model);
    guarding = 1;
    int second = guarded_call(0x4C4530, (uint32_t)(uintptr_t)model);
    RaiseException(0xE0000001, 0, 0, NULL);
    *txd = keep_txd, *engine = keep_engine, *plugin = keep_plugin;
    ok(first == 1 && second == 1, "the game's code faulted, twice");
    ok(CrashFix_FaultsSeen() - seen == 2, "both seen; a software exception is not a crash");
    CrashFix_FaultsFlush();

    static char text[16384];
    FILE *f = fopen(log, "rb");
    size_t n = f ? fread(text, 1, sizeof text - 1, f) : 0;
    if (f) fclose(f);
    text[n] = 0;
    const char *at = strstr(text, "fault 1: access violation reading 0x00000000 at gta_sa.exe+0x349C5D (0x749C5D)");
    const char *clone = at ? strstr(at, "gta_sa.exe+0x349E66 (0x749E66)") : NULL;
    const char *create = clone ? strstr(clone, "gta_sa.exe+0xC454F (0x4C454F)") : NULL;
    const char *ours_ = create ? strstr(create, "crashfix_dll_test.dll+") : NULL;
    ok(at != NULL, "the fault: what it read, where, as module+offset");
    ok(strstr(text, "== ") == text && strstr(text, "  eax=") != NULL, "a header, then the registers");
    ok(clone && create, "called from RpAtomicClone, called from CAtomicModelInfo::CreateInstance, in order");
    ok(ours_ != NULL, "and from the test's call");
    ok(strstr(text, "fault 1 again 1 time(s) at gta_sa.exe+0x349C5D (0x749C5D), 2 in all") != NULL,
       "the same address again is counted, not copied");
    ok(strstr(text, "fault 2") == NULL, "one record");
    if (!at || !create || !strstr(text, " again ")) printf("%s", text);

    ok(CrashFix_FaultsStop() == 1, "stopped");
    seen = CrashFix_FaultsSeen();
    *txd = (uint32_t)(uintptr_t)pool, *engine = 0, *plugin = 0;
    guarding = 1;
    guarded_call(0x4C4530, (uint32_t)(uintptr_t)model);
    *txd = keep_txd, *engine = keep_engine, *plugin = keep_plugin;
    ok(CrashFix_FaultsSeen() == seen, "stopped: not watched");
    RemoveVectoredExceptionHandler(recover);
}

__declspec(dllexport) int __cdecl run(void)
{
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, sizeof path);
    if (!read_game(path)) { printf("not run from gta_sa.exe at 0x400000\n"); return 2; }

    printf("== every hook goes in on the stock bytes and comes out leaving them\n");
    int all_in = 1, all_out = 1;
    for (int i = 0; i < CrashFix_Count(); i++) {
        if (!CrashFix_Address(i)) continue;          /* the SA-MP hook */
        uint32_t found;
        int r = CrashFix_Install(i, &found);
        uint8_t want[MAX_LEN];
        ours(&hooks[i], want);
        if (r != 1 || memcmp((void *)CrashFix_Address(i), want, hooks[i].len)) {
            all_in = 0;
            printf("  %s: %d\n", CrashFix_Key(i), r);
        }
    }
    ok(all_in, "each one hooked");
    ok(CrashFix_Install(find("misc1"), NULL) == 2, "a second install: already ours");
    for (int i = 0; i < CrashFix_Count(); i++) {
        if (!CrashFix_Address(i)) continue;
        int len = hooks[i].len;
        if (CrashFix_Uninstall(i) != 1 || memcmp((void *)hooks[i].va, file_at(hooks[i].va), len)) {
            all_out = 0;
            printf("  %s not restored\n", CrashFix_Key(i));
        }
    }
    ok(all_out, "each one back to the exe's bytes");

    printf("== shadow leaves the call at 0x70BDA4 to whoever replaces it (ProperShaders)\n");
    {
        int k = find("shadow");
        ok(CrashFix_Install(k, NULL) == 1, "shadow in");
        const uint8_t *entry = (const uint8_t *)0x70BDA0;
        ok(entry[0] == 0xEB && 0x70BDA2 + (int8_t)entry[1] == 0x70BD9B, "the entry hops back to the jump");
        ok(!memcmp(entry + 4, file_at(0x70BDA4), 5), "the call is as the exe has it");
        uint8_t was[5], call[5] = {0xE8};
        memcpy(was, entry + 4, 5);
        *(uint32_t *)(call + 1) = (uint32_t)shadow_replacement - 0x70BDA9;
        write_code(0x70BDA4, call, 5);
        uint8_t vehicle[0x100] = {0};
        *(uint16_t *)(vehicle + 0x22) = 411;
        shadow_seen = 0;
        ((void (__cdecl *)(void *, int))0x70BDA0)(vehicle, 1);
        ok(shadow_seen == 411, "the replaced call runs, with the vehicle already known");
        ok(*(uint8_t *)0x70BD9B == 0xE9, "our jump is intact");
        write_code(0x70BDA4, was, 5);
        cf_shadow_model = 0;
        ok(CrashFix_Uninstall(k) == 1 && !memcmp((void *)0x70BD9B, file_at(0x70BD9B), 9), "out again, stock");
    }

    printf("== ms_modelInfoPtrs moved (as SA-MP R5 does): the sites are still stock\n");
    {
        static void *infos[30000];
        uint32_t moved = (uint32_t)infos, stockRef = *(uint32_t *)0x403DA7;
        *(uint32_t *)0x403DA7 = moved;
        *(uint32_t *)(0x156650D + 3) = moved;
        ok(CrashFix_Install(find("misc50"), NULL) == 1, "misc50 over the moved reference");
        ok(CrashFix_Uninstall(find("misc50")) == 1 && *(uint32_t *)(0x156650D + 3) == moved,
           "and out again, the moved reference kept");
        *(uint32_t *)(0x156658A + 3) = moved + 4;
        uint32_t found = 0;
        ok(CrashFix_Install(find("misc51"), &found) == 0, "a reference to somewhere else: other bytes");
        memcpy((void *)0x156658A, file_at(0x156658A), 7);
        memcpy((void *)0x156650D, file_at(0x156650D), 7);
        *(uint32_t *)0x403DA7 = stockRef;
    }

    printf("== another mod's jmp or call through a pointer (SA-MP's way) is its hook\n");
    {
        uint8_t *site = (uint8_t *)0x4D4610;        /* blendanim; SA-MP keeps its pointer at 0x4D4609 */
        site[0] = 0xFF, site[1] = 0x25;
        *(uint32_t *)(site + 2) = 0x4D4609;
        *(uint32_t *)0x4D4609 = 0x401000;
        uint32_t found = 0;
        ok(CrashFix_Install(find("blendanim"), &found) == 3 && found == 0x401000, "jmp [0x4D4609]: owned, its target named");
        site[1] = 0x15;
        found = 0;
        ok(CrashFix_Install(find("blendanim"), &found) == 3 && found == 0x401000, "call [0x4D4609]: the same");
        *(uint32_t *)(site + 2) = 0x10;
        found = 0;
        ok(CrashFix_Install(find("blendanim"), &found) == 0 && found == 0x001015FF, "a pointer that can't be read: other bytes");
        ok(!CrashFix_Installed(find("blendanim")), "and nothing written");
        memcpy((void *)0x4D4609, file_at(0x4D4609), 13);
    }

    for (int i = 0; i < CrashFix_Count(); i++)
        if (CrashFix_Address(i)) CrashFix_Install(i, NULL);

    printf("== texture dictionaries (misc33, 35, 36, 37)\n");
    {
        FindNamedFn findNamed = (FindNamedFn)0x7F39F0;
        ForAllFn forAll = (ForAllFn)0x7F3730;
        AddFn add = (AddFn)0x7F3980;
        static Dictionary d, other;
        static Texture wheel, body;
        dictionary(&d);
        dictionary(&other);
        memset(&wheel, 0, sizeof wheel);
        memset(&body, 0, sizeof body);
        strcpy(wheel.name, "wheel");
        strcpy(body.name, "body");
        add(&d, &wheel);
        add(&d, &body);
        ok(wheel.dict == &d && d.textures.next == &body.link, "AddTexture through misc37 links them in");
        ok(findNamed(&d, "WHEEL") == &wheel, "FindNamedTexture finds one, any case");
        ok(findNamed(&d, "door") == NULL, "and not one it lacks");
        ok(findNamed(NULL, "wheel") == NULL && averted("misc33") == 1, "a null dictionary: nothing (misc33)");
        ok(findNamed(&d, NULL) == NULL && averted("misc33") == 2, "a null name: nothing");
        walked = 0;
        ok(forAll(&d, count_texture, &d) == &d && walked == 2, "ForAllTextures walks both");

        uint8_t *pages = VirtualAlloc(NULL, 0x2000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        DWORD old;
        VirtualProtect(pages + 0x1000, 0x1000, PAGE_NOACCESS, &old);
        Link *edge = (Link *)(pages + 0x1000 - 8);  /* its name, at +8, is in the dead page */
        edge->next = &d.textures;
        edge->prev = &body.link;
        body.link.next = edge;
        ok(findNamed(&d, "door") == NULL && averted("misc36") == 1, "an entry whose name can't be read ends the search (misc36)");
        body.link.next = (Link *)(pages + 0x1000);
        walked = 0;
        ok(forAll(&d, count_texture, &d) == &d && walked == 1 && averted("misc35") == 1,
           "an entry that can't be read ends the walk after the one before it (misc35)");
        body.link.next = &d.textures;
        static Texture spare;
        memset(&spare, 0, sizeof spare);
        other.textures.next = (Link *)0x1234;
        add(&other, &spare);
        ok(averted("misc37") == 1 && other.textures.next == (Link *)0x1234, "a list head that points nowhere is left (misc37)");

        printf("== CVehicleModelInfo::FindTextureCB (misc39)\n");
        static uint8_t engine[0x40];
        static Dictionary vehicles, current;
        static Texture remapped, paint;
        dictionary(&vehicles);
        dictionary(&current);
        memset(&remapped, 0, sizeof remapped);
        memset(&paint, 0, sizeof paint);
        strcpy(remapped.name, "#emapbody");
        strcpy(paint.name, "vehiclegrunge256");
        add(&current, &remapped);
        add(&vehicles, &paint);
        *(void **)0xC97B24 = engine;                /* RwEngineInstance */
        *(uint32_t *)0xC97B4C = 0;                  /* the texture plugin's offset */
        *(void **)(engine + 0x10) = &current;       /* RwTexDictionaryGetCurrent */
        *(void **)0xB4E688 = &vehicles;             /* the vehicle TXD */
        FindTextureCBFn cb = (FindTextureCBFn)0x4C7510;
        ok(cb("vehiclegrunge256") == &paint, "from the vehicle TXD");
        ok(cb("remapbody") == &remapped, "a remap found by its # name");
        char longName[64];
        memset(longName, 'x', sizeof longName - 1);
        longName[sizeof longName - 1] = 0;
        memcpy(longName, "remap", 5);
        ok(cb(longName) == NULL && averted("misc39") == 1, "a remap name past 31 characters: nothing, no overrun");
        uint32_t before = averted("misc33");
        static uint8_t notADictionary[0x20];
        ok(findNamed(notADictionary, "#emapbody") == &remapped && averted("misc33") == before + 1,
           "something not a dictionary: the current one is searched");
    }

    printf("== CAEMP3BankLoader (misc29, 43, 44, 45)\n");
    {
        static uint8_t loader[0x700], slots[2][0x12D4], lookups[2][12];
        memset(loader, 0, sizeof loader);
        *(void **)(loader + 0) = slots;
        *(void **)(loader + 4) = lookups;
        *(uint16_t *)(loader + 0x0C) = 2;           /* slots */
        *(uint16_t *)(loader + 0x0E) = 1;           /* banks */
        loader[0x14] = 1;
        *(uint32_t *)(loader + 0x1C) = 0x5000;      /* the buffer */
        uint8_t *s = slots[0];
        *(uint32_t *)(s + 0) = 0x100;               /* where the slot starts */
        *(uint32_t *)(s + 4) = 2000;                /* its size */
        *(uint16_t *)(s + 0x10) = 0;                /* bank */
        *(uint16_t *)(s + 0x12) = 2;                /* sounds */
        *(uint32_t *)(s + 0x14 + 12) = 600;         /* sound 1's offset */
        *(uint16_t *)(s + 0x14 + 12 + 8) = 44100;
        *(uint32_t *)(lookups[0] + 8) = 1000;       /* the bank's size */
        GetSoundFn get = (GetSoundFn)0x4E0280;
        IsLoadedFn loaded = (IsLoadedFn)0x4E0220;
        uint32_t size = 0;
        uint16_t rate = 0;
        ok(get(loader, 1, 0, &size, &rate) == 0x5000 + 0x100 + 600 && size == 400 && rate == 44100, "a sound in range");
        ok(get(loader, 1, 7, &size, &rate) == 0 && averted("misc43") == 1, "a slot past the count: no buffer (misc43)");
        ok(get(loader, 1, 0xFFFF, &size, &rate) == 0 && averted("misc43") == 2, "slot -1: no buffer");
        *(uint16_t *)(s + 0x12) = 900;
        ok(get(loader, 1, 0, &size, &rate) == 0 && averted("misc45") == 1, "900 sounds in a slot: no buffer (misc45)");
        *(uint16_t *)(s + 0x12) = 2;
        *(uint32_t *)(lookups[0] + 8) = 5000;
        ok(get(loader, 1, 0, &size, &rate) == 0 && averted("misc44") == 1, "a sound past the slot's buffer: none (misc44)");
        ok(loaded(loader, 0, 0) == 1, "IsSoundBankLoaded: bank 0 in slot 0");
        ok(loaded(loader, 0, 5) == 0 && averted("misc29") == 1, "slot 5 of 2: not loaded, not read (misc29)");
        ok(loaded(loader, 0, 0xFFFF) == 0 && averted("misc29") == 2, "slot -1: not loaded");
    }

    printf("== clump alpha, FxPrim_c::Enable, matrices, vertex instancing\n");
    {
        *(uint32_t *)0x8D6094 = 0x10;               /* ms_clumpPluginOffset */
        static uint8_t clump[0x40];
        ((SetAlphaFn)0x732B00)(clump, 77);
        ok(((GetAlphaFn)0x732B20)(clump) == 77, "an alpha set is read back");
        ((SetAlphaFn)0x732B00)(NULL, 5);
        ok(((GetAlphaFn)0x732B20)(NULL) == 255 && averted("getalpha") == 1 && averted("setalpha") == 1,
           "a null clump: opaque, nothing written");
        static uint8_t prim[0x20];
        ((EnableFn)0x4A9F50)(prim, 1);
        ((EnableFn)0x4A9F50)(NULL, 1);
        ok(prim[0x0C] == 1 && averted("fxprim") == 1, "FxPrim_c::Enable sets the flag, and on null does nothing");
        static float m[18] __attribute__((aligned(16))), rw[16] __attribute__((aligned(16)));
        UpdateRwFn update = (UpdateRwFn)0x59AD70;
        m[0] = 2.5f;
        update(m, rw);
        ok(rw[0] == 2.5f, "UpdateRwMatrix copies");
        update(m, NULL);
        update(m, (uint8_t *)rw + 2);
        ok(averted("matrix") == 2, "into null or an unaligned matrix: nothing");
        ok(((MatMulFn)0x7F18B0)(rw, m, NULL) == rw && averted("matmul") == 1, "RwMatrixMultiply by null: dst back, untouched");
        uint32_t typeSize = *(uint32_t *)(0x874EF8 + 4 * 2);
        float src[12] = {0};
        ok(((InstFn)0x752AD0)(2, NULL, src, 4, 12) == typeSize && averted("vbv3d") == 1, "V3d into no buffer: the type's size");
        ok(((InstFn)0x753B60)(2, NULL, src, 4, 12) == typeSize && averted("vbmorph") == 1, "V3dMorph: the same");
        ok(((InstFn)0x752320)(2, (void *)0x20, src, 4, 12) == typeSize && averted("vbweights") == 1, "Weights at 0x20: the same");
    }

    printf("== CAnimBlendNode::GetCurrentTranslation (translation)\n");
    {
        static uint8_t node[0x18], assoc[0x40], seq[0x0C];
        static float frames[2][8];
        *(float *)(assoc + 0x18) = 1.0f;            /* blend */
        *(void **)(node + 0x10) = seq;
        *(void **)(node + 0x14) = assoc;
        *(uint16_t *)(seq + 4) = 2;                 /* has translation */
        *(uint16_t *)(seq + 6) = 2;                 /* frames */
        *(void **)(seq + 8) = frames;
        frames[1][4] = 1.0f;                        /* frame 1's time */
        frames[1][5] = 3.0f;                        /* its translation */
        *(uint16_t *)(node + 8) = 1;
        *(uint16_t *)(node + 0x0A) = 0;
        float out[3] = {9, 9, 9};
        ((TranslationFn)0x4CFC50)(node, out, 1.0f);
        ok(averted("translation") == 0 && out[0] == out[0], "key frames 0..1 of 2: worked out");
        *(uint16_t *)(node + 8) = 1000;
        out[0] = out[1] = out[2] = 9;
        ((TranslationFn)0x4CFC50)(node, out, 1.0f);
        ok(averted("translation") == 1 && out[0] == 0 && out[1] == 0 && out[2] == 0, "key frame 1000 of 2: none, and back safely");
    }

    printf("== CTaskSimpleCarFallOut::FinishAnimFallOutCB (misc21)\n");
    {
        static uint8_t vehicles[3][0xA18], flags[3] = {0x00, 0x80, 0x00};
        static struct { void *objects; uint8_t *flags; int size, first; } pool;
        pool.objects = vehicles;
        pool.flags = flags;
        pool.size = 3;
        *(void **)0xB74494 = &pool;
        static uint32_t task[8];
        task[0] = 0x86EFD0;
        task[4] = (uint32_t)vehicles[2];
        ok(cf_fall_out_valid(NULL, task) == 1 && task[4] == (uint32_t)vehicles[2], "a live vehicle stays");
        task[4] = (uint32_t)vehicles[1];
        ok(cf_fall_out_valid(NULL, task) == 1 && task[4] == 0 && averted("misc21") == 1, "a freed slot is dropped");
        task[4] = (uint32_t)vehicles[0] + 4;
        ok(cf_fall_out_valid(NULL, task) == 1 && task[4] == 0, "and a pointer into one");
        FallOutFn cb = (FallOutFn)0x648EE0;
        task[2] = 0;
        cb(NULL, task);
        ok((task[2] & 0xFF) == 1, "the game's callback runs on a good task (finished set)");
        task[0] = 0x86EFD4;
        task[2] = 0;
        uint32_t n = averted("misc21");
        cb(NULL, task);
        ok(task[2] == 0 && averted("misc21") == n + 1, "another task: the callback does nothing");
    }

    printf("== CAnimBlendAssocGroup::CopyAnimation (copyanim, #1109)\n");
    {
        static AssocGroup groups[40];
        static StaticAssoc few[5], python[30];
        memset(groups, 0, sizeof groups);
        groups[0] = (AssocGroup){NULL, few, 5, 0, 0};
        groups[11] = (AssocGroup){NULL, python, 30, 200, 11};
        few[0].hierarchy = few;
        few[3].hierarchy = few;
        for (int i = 0; i < 30; i++) python[i].hierarchy = python;
        *(void **)0xB4EA34 = groups;
        AssocGroup *g = &groups[0];
        int id = 1;
        cf_copy_animation(&g, &id);
        ok(g == &groups[0] && id == 0, "an id with no hierarchy: the nearest one with");
        g = &groups[0];
        id = 224;
        cf_copy_animation(&g, &id);
        ok(g == &groups[11] && id == 224, "fire (224) from group 0: the Python group's");
        g = &groups[28];
        id = 224;
        cf_copy_animation(&g, &id);
        ok(g == &groups[28], "from the grenade group: left");
    }

    printf("== collision data, the train, lines of sight\n");
    {
        static uint8_t colModel[0x30], data[0x30];
        *(void **)(colModel + 0x2C) = data;
        *(uint32_t *)(data + 0x20) = 4;
        *(uint32_t *)(data + 0x24) = 8;
        *(void **)(data + 0x28) = data;
        *(void **)(data + 0x2C) = data;
        data[7] = 0x14;
        cf_check_col_data(colModel);
        ok(*(uint32_t *)(data + 0x20) == 4 && data[7] == 0x14, "shadow data that fits stays");
        *(uint32_t *)(data + 0x24) = 70000;
        cf_check_col_data(colModel);
        ok(*(uint32_t *)(data + 0x20) == 0 && *(uint32_t *)(data + 0x24) == 0 && data[7] == 0 && averted("colalloc") == 1,
           "70000 shadow vertices: no shadow");
        float d, len = 100.0f;
        d = 250.0f; cf_wrap_rail_distance(&d, &len);
        ok(d == 50.0f, "250 on a 100 track: 50");
        d = -30.0f; cf_wrap_rail_distance(&d, &len);
        ok(d == 70.0f, "-30: 70");
        d = -100.0f; cf_wrap_rail_distance(&d, &len);
        ok(d == 0.0f, "-100: 0");
        d = 1e12f; cf_wrap_rail_distance(&d, &len);
        ok(d >= 0.0f && d < 100.0f, "1e12, which adding 100 never changes: in range");
        len = 0.0f;
        d = 5.0f; cf_wrap_rail_distance(&d, &len);
        ok(d == 0.0f && averted("train") == 1, "no track length: 0, no endless loop");
        float a[3] = {10, 20, 30}, b[3] = {11, 21, 31};
        cf_limit_line(a, b, &cf_n_los);
        ok(a[0] == 10 && b[0] == 11 && averted("los") == 0, "a line in the world stays");
        b[0] = NAN;
        cf_limit_line(a, b, &cf_n_los);
        ok(b[0] == 10 && b[2] == 30.01f && averted("los") == 1, "a NaN end: next to the start");
        a[1] = 1e9f;
        b[2] = -2000000;
        cf_limit_line(a, b, &cf_n_los);
        ok(a[2] == 100.0f && b[2] == 100.01f, "both out: a short line at the origin");
        float c[3] = {-2080.44f, 296.46f, -10000.04f}, d2[3] = {-2079.0f, 300.0f, -9990.0f};
        LONG before = cf_n_los;
        cf_limit_line(c, d2, &cf_n_los);
        ok(c[2] == -10000.04f && d2[2] == -9990.0f && cf_n_los == before,
           "a line at z -10000 (Unity Roleplay's objects) stays, nothing counted");
    }

    printf("== a camera gone NaN (camnan)\n");
    {
        typedef void (__attribute__((thiscall)) *InitFn)(void *);
        static uint8_t cam[0x238], clean[0x238], ent[0x40];
        static float mat[16] = {1, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 100, 200, 10, 0};    /* forward (0, 2): not unit */
        ((InitFn)0x50E490)(cam);
        *(uint16_t *)(cam + 0x0C) = 18;
        cam[0x0A] = 0;
        memcpy(clean, cam, sizeof cam);
        cf_cam_check(cam);
        ok(!memcmp(cam, clean, sizeof cam) && averted("camnan") == 0, "a camera with no NaN: left");

        *(float **)(ent + 0x14) = mat;
        *(void **)(cam + 0x21C) = ent;
        *(float *)(cam + 0xA0) = NAN;                       /* the true beta */
        *(float *)(cam + 0x194) = INFINITY;                 /* the front */
        *(float *)(cam + 0x1D8) = -NAN;                     /* the target's history */
        cf_cam_check(cam);
        int finite = 1;
        for (int o = 0x2C; o < 0x208; o += 4) if (o < 0x110 || o >= 0x124 || o == 0x118 || o == 0x11C)
            if (!isfinite(*(float *)(cam + o))) finite = 0;
        const float *src = (const float *)(cam + 0x19C), *front = (const float *)(cam + 0x190);
        ok(finite && averted("camnan") == 1, "NaN and infinity: every float finite again");
        ok(*(uint16_t *)(cam + 0x0C) == 18 && cam[0x0A] == 1, "its mode kept, its statics to be reset");
        ok(src[0] == 100 && src[1] == 196 && src[2] == 11 && !memcmp(cam + 0x1A8, src, 12),
           "the source 4 behind the target and 1 above");
        ok(front[0] == 0 && fabsf(front[1] - 0.9701425f) < 1e-6f && fabsf(front[2] + 0.2425356f) < 1e-6f,
           "the front at the target");

        void **player = (void **)0xB7CD98, *was = *player;
        *(void **)(cam + 0x21C) = NULL;
        *player = NULL;
        *(float *)(cam + 0xA4) = NAN;
        cf_cam_check(cam);
        ok(averted("camnan") == 2 && isfinite(*(float *)(cam + 0xA4)), "no target and no player: zeroed only");
        *(uint32_t *)(cam + 0x21C) = 0x7FC00010;            /* a target with a NaN's bits */
        *(float *)(cam + 0xA4) = NAN;
        cf_cam_check(cam);
        ok(averted("camnan") == 3 && *(uint32_t *)(cam + 0x21C) == 0x7FC00010, "a pointer with a NaN's bits: left");
        *(void **)(cam + 0x21C) = NULL;
        *player = ent;
        mat[13] = NAN;                                      /* the ped nowhere: no source from it */
        *(float *)(cam + 0xA4) = NAN;
        *(float *)(cam + 0x1A0) = 5;
        cf_cam_check(cam);
        ok(averted("camnan") == 4 && src[1] == 5, "a target nowhere: the source left");
        mat[13] = 200;
        *(float *)(cam + 0xA4) = NAN;
        cf_cam_check(cam);
        ok(averted("camnan") == 5 && src[1] == 196, "no target: the player's ped");

        static int32_t got_ecx;
        static float got_st0;
        uint8_t keep[13], *go = (uint8_t *)0x526FC6, ret[13] = {0x89, 0x0D, 0, 0, 0, 0, 0xD9, 0x1D, 0, 0, 0, 0, 0xC3};
        *(int32_t **)(ret + 2) = &got_ecx, *(float **)(ret + 8) = &got_st0;    /* mov [got_ecx], ecx / fstp [got_st0] / ret */
        int32_t *frame = (int32_t *)0xB6FE34, frame_was = *frame;
        memcpy(keep, go, sizeof keep), memcpy(go, ret, sizeof ret);
        *frame = 7;
        *(float *)(cam + 0xA4) = NAN;
        __asm__ volatile("mov %0, %%ecx\n\tcall *%1" :: "r"(cam), "r"(hook_of("camnan")) : "eax", "ecx", "edx", "memory");
        memcpy(go, keep, sizeof keep);
        *frame = frame_was, *player = was;
        ok(averted("camnan") == 6 && got_ecx == (int32_t)cam && got_st0 == 7.0f,
           "through the hook: checked, ecx kept, the frame count loaded");
    }

    printf("== San Andreas Online's: shadow meshes over 2048 vertices (stencilveh, stencilobj)\n");
    {
        uint8_t keep[2] = {*(uint8_t *)0x70FC8D, *(uint8_t *)0x7102D8};
        *(uint8_t *)0x70FC8D = *(uint8_t *)0x7102D8 = 0xC3;
        memset(site_stack, 0, sizeof site_stack);
        call_site(hook_of("stencilveh"), 100, 1.5f, 1, 0);
        ok(averted("stencilveh") == 0 && *(float *)&site_stack[26] == 1.5f && !(site_flags & 0x40) && !(fpu_sw & 0x3800),
           "100 vertices: on, the float stored, the flags of test eax");
        call_site(hook_of("stencilveh"), 0, 1.5f, 1, 0);
        ok(averted("stencilveh") == 0 && (site_flags & 0x40), "none: ZF for the jle after");
        site_stack[26] = 0;
        call_site(hook_of("stencilveh"), 5000, 2.5f, 1, 0);
        ok(averted("stencilveh") == 1 && *(float *)&site_stack[26] == 2.5f && !(fpu_sw & 0x3800),
           "5000: the shadow skipped, the float stored first");
        *(uint8_t *)0x70FC8D = keep[0], *(uint8_t *)0x7102D8 = keep[1];

        keep[0] = *(uint8_t *)0x710477, keep[1] = *(uint8_t *)0x710AB2;
        *(uint8_t *)0x710477 = *(uint8_t *)0x710AB2 = 0xC3;
        *(float *)&site_stack[13] = 3.5f;
        call_site(hook_of("stencilobj"), 2048, 0, 0, 1);
        ok(averted("stencilobj") == 0 && fpu_out == 3.5f && !(site_flags & 0x40), "2048: on, its float loaded");
        call_site(hook_of("stencilobj"), 2049, 0, 0, 0);
        ok(averted("stencilobj") == 1 && !(fpu_sw & 0x3800), "2049: skipped, nothing on the x87 stack");
        call_site(hook_of("stencilobj"), 0xFFFFFFF0u, 0, 0, 0);
        ok(averted("stencilobj") == 2, "a count read as negative: skipped too");
        *(uint8_t *)0x710477 = keep[0], *(uint8_t *)0x710AB2 = keep[1];
    }

    printf("== San Andreas Online's: IKChain_c::Init with a bone the skin lacks (ikchain)\n");
    {
        typedef struct { int id, index, flags; void *frame; } Node;
        static Node nodes[8];
        static struct { int flags, count; void *matrices, *unaligned; Node *nodes; } hier = {0, 0, NULL, NULL, nodes};
        static const int arm[] = {21, 22, 23, 24, 25};
        for (int i = 0; i < 5; i++) nodes[i].id = arm[i];
        hier.count = 5;
        ok(cf_ik_bones_valid(&hier, 25, 21), "the right arm, hand to clavicle: all there");
        ok(cf_ik_bones_valid(&hier, 21, 21), "the pivot alone");
        ok(!cf_ik_bones_valid(NULL, 25, 21), "no hierarchy: refused");
        nodes[2].id = 99;
        ok(!cf_ik_bones_valid(&hier, 25, 21), "no 23 between: refused");
        nodes[2].id = 23, nodes[0].id = 98;
        ok(!cf_ik_bones_valid(&hier, 25, 21), "no pivot: refused");
        nodes[0].id = 21, nodes[4].id = 77;
        ok(!cf_ik_bones_valid(&hier, 77, 21), "an effector the bone table lacks: refused");
        nodes[4].id = 25;

        uint8_t keepGet[6], keepOn[2], keepOff = *(uint8_t *)0x6184A3;
        memcpy(keepGet, (void *)0x734A40, 6);
        memcpy(keepOn, (void *)0x6183B8, 2);
        *(uint8_t *)0x734A40 = 0xB8, *(void **)0x734A41 = &hier, *(uint8_t *)0x734A45 = 0xC3;   /* mov eax, &hier / ret */
        memcpy((void *)0x6183B8, "\x59\xC3", 2);                                              /* its argument, then back */
        *(uint8_t *)0x6184A3 = 0xC3;
        site_stack[7] = 25, site_stack[11] = 21;
        call_site(hook_of("ikchain"), 0x1234, 0, 0, 0);
        ok(averted("ikchain") == 0, "through the hook, all there: on to the game's lookup");
        nodes[1].id = 99;
        call_site(hook_of("ikchain"), 0x1234, 0, 0, 0);
        ok(averted("ikchain") == 1, "no 22: false returned, the stack as it was");
        memcpy((void *)0x734A40, keepGet, 6);
        memcpy((void *)0x6183B8, keepOn, 2);
        *(uint8_t *)0x6184A3 = keepOff;
    }

    printf("== San Andreas Online's: the vehicle glass list grows (glasslist)\n");
    {
        typedef uint8_t (__cdecl *InsertAtomicFn)(void *atomic, float distance);
        const InsertAtomicFn insertAtomic = (InsertAtomicFn)0x733D10;
        static AlphaLink links[20];
        AlphaList *list = ALPHA_LIST;
        list->usedHead.next = &list->usedTail, list->usedTail.prev = &list->usedHead;
        list->freeHead.next = &list->freeTail, list->freeTail.prev = &list->freeHead;
        for (int i = 19; i >= 0; i--) {
            links[i].prev = &list->freeHead, links[i].next = list->freeHead.next;
            links[i].next->prev = &links[i], list->freeHead.next = &links[i];
        }
        int all = 1;
        for (int i = 0; i < 20; i++) all &= insertAtomic((void *)(uintptr_t)(0x1000 + i), 100.0f - i) != 0;
        ok(all && cf_glass_used == 0 && averted("glass1") == 0, "20 parts: the game's own links");
        ok(insertAtomic((void *)0x2000, 50.5f) != 0 && cf_glass_used == 20 && averted("glass1") == 1,
           "the 21st: 20 links more, counted on its site's row");
        int n = 0, sorted = 1;
        float last = -1.0f;
        for (AlphaLink *l = list->usedHead.next; l != &list->usedTail; l = l->next, n++) {
            if (l->distance < last) sorted = 0;
            last = l->distance;
        }
        ok(n == 21 && sorted, "21 in the list, nearest first");
        for (int i = 0; i < 19; i++) insertAtomic((void *)0x3000, 10.0f + i);
        ok(insertAtomic((void *)0x4000, 1.0f) != 0 && cf_glass_used == 60 && averted("glass1") == 2, "the 41st: 40 more");
        static AlphaList other;
        other.usedHead.next = &other.usedTail, other.usedTail.prev = &other.usedHead;
        other.freeHead.next = &other.freeTail, other.freeTail.prev = &other.freeHead;
        static const struct { void *object, *render; float distance; } info = {NULL, NULL, 1.0f};
        ok(cf_insert_alpha(&other, NULL, &info) == NULL && cf_glass_used == 60, "another list full: the game's NULL");
        cf_glass_used = GLASS_POOL;
        for (int i = 0; i < 39; i++) insertAtomic((void *)0x5000, 5.0f);
        ok(insertAtomic((void *)0x6000, 5.0f) == 0 && averted("glass1") == 2,
           "the pool used up: the game draws that part at once, as before");
        cf_glass_used = GLASS_POOL - 5;
        ok(insertAtomic((void *)0x7000, 5.0f) != 0 && cf_glass_used == GLASS_POOL, "the last 5 links");
    }

    printf("== MTA's frame-rate fixes: the 30 Hz tick, sleep frames, turn damping\n");
    {
        float *ts = (float *)0xB7CB5C;
        uint32_t *ms = (uint32_t *)0xB7CB84;
        *ms = 1000; cf_tick();
        int first = cf_frame_due;
        *ms = 1010; cf_tick();
        int early = cf_frame_due;
        *ms = 1033; cf_tick();
        int due = cf_frame_due;
        *ms = 1050; cf_tick();
        int between = cf_frame_due;
        *ms = 1066; cf_tick();
        ok(first && !early && due && !between && cf_frame_due, "due every 33 ms, not in between");
        uint32_t n = cf_frame30;
        *ms = 5000; cf_tick();
        *ms = 5010; cf_tick();
        ok(cf_frame30 == n + 1 && !cf_frame_due, "a jump in time: one due frame, then on from there");
        *ts = 50.0f / 30.0f;
        ok(cf_sleep_frames() == 10 && fabsf(cf_still - 0.005f) < 1e-7f, "30 fps: 10 still frames, 0.005, stock");
        *ts = 50.0f / 60.0f;
        ok(cf_sleep_frames() == 20 && fabsf(cf_still - 0.0025f) < 1e-7f, "60 fps: 20, the threshold halved");
        *ts = 50.0f / 1000.0f;
        ok(cf_sleep_frames() == 240, "1000 fps: capped at 240");
        *ts = 3.0f;
        ok(cf_sleep_frames() == 10, "below 30 fps: never fewer than 10");
        float turn[3] = {1, 2, 3};
        *ts = 50.0f / 30.0f;
        cf_turn_damping(turn);
        ok(fabsf(turn[0] - 0.99f) < 1e-6f && fabsf(turn[2] - 2.97f) < 1e-5f, "30 fps: the turn speed times 0.99, stock");
        turn[0] = 1;
        *ts = 50.0f / 60.0f;
        cf_turn_damping(turn);
        ok(fabsf(turn[0] - sqrtf(0.99f)) < 1e-6f, "60 fps: times sqrt(0.99) a step, 0.99 every two");
    }

    printf("== MTA's frame-rate fixes: a ped's push on a parked car\n");
    {
        static uint8_t car[0x600], ped[0x600], matrix[0x48];
        float *ts = (float *)0xB7CB5C;
        car[0x36] = 2, ped[0x36] = 3;
        *(float *)(car + 0x8C) = 1000.0f, *(float *)(car + 0x90) = 2000.0f, *(float *)(ped + 0x8C) = 70.0f;
        *(void **)(car + 0x14) = matrix;
        *(float *)(matrix + 0x00) = 1.0f;                   /* right */
        *(float *)(matrix + 0x14) = 1.0f;                   /* forward */
        float v0[3] = {0}, w0[3] = {0}, pedV[3] = {0, 0.1f, 0}, v[3] = {0, 0.5f, 0.2f}, w[3] = {0.3f, 0.3f, 0};
        *ts = 0.4f;
        govern_push(car, ped, v0, w0, pedV, v, w);
        float most = 0.1f * (700.0f / 1700.0f);
        ok(fabsf(v[1] - most) < 1e-6f && v[0] == 0 && v[2] == 0, "forward no more than momentum gives, no lift");
        ok(w[0] == 0 && w[1] == 0, "no tilt");
        *(void **)(car + 0x460) = ped;
        float v2[3] = {0, 0.5f, 0.2f};
        govern_push(car, ped, v0, w0, pedV, v2, w);
        ok(v2[1] == 0.5f && v2[2] == 0.2f, "a car with a driver: left alone");
    }

    printf("== MTA's frame-rate fixes through their hooks\n");
    {
        float *ts = (float *)0xB7CB5C;
        float k = 50.0f / 30.0f;
        *ts = k / 2;
        ret_at(0x6C4F2F);
        ok(fabsf(call_fpu(hook_of("heli1"), 1.0f, 1) - (1.0f + *(float *)0x858CDC * 0.5f)) < 1e-6f,
           "heli1: the takeoff rate times timestep / 1.6667");
        unret(0x6C4F2F);
        ret_at(0x6D2777);
        ok(fabsf(call_fpu(hook_of("boatwater"), 2.0f, 1) - 2.0f * *(float *)0x871DDC * 0.5f) < 1e-6f,
           "boatwater: the resistance scaled");
        unret(0x6D2777);
        ret_at(0x6F42DB);
        ok(fabsf(call_fpu(hook_of("chassis1"), 0, 0) - *(float *)0x872328 * 0.5f) < 1e-7f, "chassis1: the impulse scaled");
        unret(0x6F42DB);
        ret_at(0x61E4F8);
        call_fpu(hook_of("aimwalk"), 0.07f, 1);
        int above = !(fpu_sw & 0x0100);
        float kept = call_fpu(hook_of("aimwalk"), 0.03f, 1);
        int below = (fpu_sw & 0x0100) != 0;
        unret(0x61E4F8);
        ok(above && below && kept == 0.03f, "aimwalk: 60 fps compares against 0.05, the amount left on the stack");
        float before = *(float *)0xB6EC30;
        ret_at(0x52C735);
        call_fpu(hook_of("shake"), 0, -1);
        unret(0x52C735);
        ok(fabsf(*(float *)0xB6EC30 - (before + *(float *)0x858C80 * 0.5f)) < 1e-5f, "shake: the phase steps scaled");
        cf_frame_due = 0;
        ok(call_thiscall4(hook_of("wheeldirt"), 0x1234) == 0, "a gate on a frame not due: returns 0, its 10h of arguments taken");
        cf_frame_due = 1;
        uint8_t keep[3];
        memcpy(keep, (void *)0x6D2D56, 3);
        memcpy((void *)0x6D2D56, "\xC2\x10\x00", 3);
        ok(call_thiscall4(hook_of("wheeldirt"), 0x1234) == 0x1234, "on a due frame: the game's code (its first argument read)");
        memcpy((void *)0x6D2D56, keep, 3);
        *ts = k;
    }

    printf("== a knocked-down ped's down time at any fps (fallstep)\n");
    {
        uint8_t *ped = fall_ped, task[0x20], task2[0x20];
        float *ts = (float *)0xB7CB5C;
        uint32_t *frames = (uint32_t *)0xB7CB4C;
        int was = CrashFix_Uninstall(find("fallstep")) == 1;
        int stock60 = fall_frames(60), stock600 = fall_frames(600), stock2000 = fall_frames(2000);
        ok(stock60 == 32 && stock600 == 500 && stock2000 == 0,
           "stock: 32 frames at 60 fps (16 ms each), 500 at 600 (1 ms), never at 2000 (0 ms)");
        ok(CrashFix_Install(find("fallstep"), NULL) == 1, "fallstep in");
        int ours60 = fall_frames(60), ours600 = fall_frames(600), ours2000 = fall_frames(2000);
        printf("   frames: stock %d %d %d, ours %d %d %d\n", stock60, stock600, stock2000, ours60, ours600, ours2000);
        ok(abs(ours60 - 30) <= 1 && abs(ours600 - 300) <= 1 && abs(ours2000 - 1000) <= 1,
           "ours: 30, 300 and 1000 frames (each within 1), 500 ms at each");
        *ts = 50.0f / 600.0f;
        memset(task, 0, sizeof task), memset(task2, 0, sizeof task2);
        task[8] = task2[8] = 1;
        *(uint32_t *)(task + 0x1C) = *(uint32_t *)(task2 + 0x1C) = 500;
        int same = 1;
        for (int n = 0; n < 50; n++) {
            ++*frames;
            ((FallFn)0x67FAF0)(task, ped);
            ((FallFn)0x67FAF0)(task2, ped);
            same &= *(uint32_t *)(task + 0x1C) == *(uint32_t *)(task2 + 0x1C);
        }
        ok(same, "two peds down the same frame: the same step off each");
        *ts = __builtin_nanf("");
        ++*frames;
        uint32_t before = *(uint32_t *)(task + 0x1C);
        ((FallFn)0x67FAF0)(task, ped);
        ok(*(uint32_t *)(task + 0x1C) == before, "a NaN timestep: nothing off");
        ok(CrashFix_Uninstall(find("fallstep")) == 1 && !memcmp((void *)0x67FB35, "\xD9\x05\x5C\xCB\xB7\x00", 6), "and out");
        if (was) CrashFix_Install(find("fallstep"), NULL);
        *ts = 50.0f / 30.0f;
    }

    printf("== timers by the carried step, the jump-out damping, the engine drain, the fuel\n");
    {
        float *ts = (float *)0xB7CB5C;
        uint32_t *frames = (uint32_t *)0xB7CB4C;
        static const char *calls[] = {"chokestep", "climbstep", "fightstep", "stancestep", "carburn", "heliburn", "rcburn",
                                      "bikeburn", "boatburn"};
        int called = 1;
        for (int i = 0; i < 9; i++) {
            const Hook *h = &hooks[find(calls[i])];
            const uint8_t *site = (const uint8_t *)h->va;
            called &= h->installed && site[0] == 0xE8 && h->va + 5 + *(const uint32_t *)(site + 1) == (uint32_t)cf_step_call
                      && site[5] == 0x90 && site[22] == 0x90;
        }
        ok(called, "the nine sites call cf_step_call, the rest NOPs");
        *ts = 50.0f / 600.0f;
        uint32_t va = hooks[find("carburn")].va, sum = 0;
        ret_at(va + 5);
        for (int n = 0; n < 600; n++) {
            ++*frames;
            call_site(va, 0, 0, 0, 0);
            sum += site_ret;
        }
        unret(va + 5);
        ok(sum >= 999 && sum <= 1000, "a burn timer at 600 fps: 1000 ms in 600 frames");

        /* the game's own CVehicle::ProcessCarAlarm: a 1000 ms alarm */
        typedef void (__attribute__((thiscall)) *AlarmFn)(void *vehicle);
        static uint8_t car[0x600];
        int alarm[2][3], fps[3] = {60, 600, 2000};
        int was = CrashFix_Uninstall(find("alarmstep")) == 1;
        for (int k = 0; k < 2; k++) {
            if (k) ok(CrashFix_Install(find("alarmstep"), NULL) == 1, "alarmstep in");
            for (int f = 0; f < 3; f++) {
                *ts = 50.0f / fps[f];
                *(uint16_t *)(car + 0x45C) = 1000;
                alarm[k][f] = 0;
                for (int n = 1; n <= 5000 && !alarm[k][f]; n++) {
                    ++*frames;
                    ((AlarmFn)0x6D21F0)(car);
                    if (!*(uint16_t *)(car + 0x45C)) alarm[k][f] = n;
                }
            }
        }
        printf("   alarm frames: stock %d %d %d, ours %d %d %d\n", alarm[0][0], alarm[0][1], alarm[0][2], alarm[1][0],
               alarm[1][1], alarm[1][2]);
        ok(alarm[0][0] == 63 && alarm[0][1] == 1000 && alarm[0][2] == 0, "stock: 63 frames at 60 fps, 1000 at 600, never at 2000");
        ok(abs(alarm[1][0] - 60) <= 1 && abs(alarm[1][1] - 600) <= 1 && abs(alarm[1][2] - 2000) <= 1,
           "ours: 60, 600 and 2000 frames (each within 1), 1000 ms at each");
        if (!was) CrashFix_Uninstall(find("alarmstep"));

        *ts = 50.0f / 300.0f;
        ++*frames;
        ret_at(0x737A0E);
        call_site(hook_of("fuelstep"), 0, 0, 0, 0);
        unret(0x737A0E);
        ok(site_ret == (uint32_t)-(int32_t)cf_ms_step(), "fuelstep: the step negated, for the timer's sub");
        cf_frame_due = 0;
        ret_at(0x737AFD);
        call_site(hook_of("fuelfx"), 0, 0, 0, 0);
        unret(0x737AFD);
        cf_frame_due = 1;
        ret_at(0x737A3D);
        call_site(hook_of("fuelfx"), 0, 0, 0, 0);
        unret(0x737A3D);
        ok(1, "fuelfx: past the fuel on a frame not due, to it on a due one at 64 ms");

        ret_at(0x6A7625);
        float health = call_fpu(hook_of("enginedrain"), 500.0f, 1);
        unret(0x6A7625);
        ok(fabsf(health - (500.0f - 2.0f * (*ts / cf_k30))) < 1e-3f, "enginedrain: 2 health scaled by timestep / 1.6667");

        static float veh[0x20];
        veh[0x50 / 4] = 1.0f;
        reg_esi = (uint32_t)veh;
        ret_at(0x6D2119);
        call_regs(hook_of("jumpout1"), 0, 0, 1);
        unret(0x6D2119);
        float turn_out = fpu_out, want = powf(0.9f, *ts / cf_k30);
        ok(fabsf(turn_out - want) < 1e-5f && fabsf(cf_jump_damp - want) < 1e-6f, "jumpout1: the turn speed by pow(0.9, timestep / 1.6667)");
        int ptrs = 1;
        for (int i = 2; i <= 6; i++) {
            char key[16];
            snprintf(key, sizeof key, "jumpout%d", i);
            ptrs &= *(float **)hooks[find(key)].va == &cf_jump_damp;
        }
        ok(ptrs, "jumpout2-6: the other five 0.9s read it");
        *ts = 50.0f / 30.0f;

        /* bikestill: both ways back are rets; what the bike's speeds hold after */
        static uint32_t bike[0x470 / 4];
        float *bturn = (float *)bike + 0x50 / 4, speed[3];
        uint8_t keep166 = *(uint8_t *)0x6BC166, keep192 = *(uint8_t *)0x6BC192;
        *(uint8_t *)0x6BC166 = *(uint8_t *)0x6BC192 = 0xC3;
        reg_esi = (uint32_t)bike, reg_edi = (uint32_t)speed;
        #define BIKESTILL(driver, tx, ty) (bike[0x460 / 4] = (driver), bturn[0] = (tx), bturn[1] = (ty), bturn[2] = 0.5f, \
            call_regs(hook_of("bikestill"), 0, 0, 0))
        float pass30 = 0.0005f, over30 = 0.002f;
        BIKESTILL(0, pass30, pass30);
        ok(bturn[0] == pass30 && bturn[1] == pass30 && bturn[2] == 0.0f, "bikestill: no driver, the yaw alone is zeroed, stock");
        BIKESTILL(1, pass30, -pass30);
        ok(bturn[0] == 0.0f && bturn[1] == 0.0f && bturn[2] == 0.0f, "a driver, at rest in roll and pitch: all three zeroed");
        BIKESTILL(1, pass30, -over30);
        ok(bturn[0] == pass30 && bturn[1] == -over30 && bturn[2] == 0.5f, "a driver, rolling: nothing is zeroed");
        *ts = 50.0f / 300.0f;
        BIKESTILL(1, pass30, 0.0f);
        ok(bturn[0] == pass30 && bturn[2] == 0.5f, "300 fps: the limit is a tenth, 0.0005 is rolling");
        BIKESTILL(1, 0.00005f, 0.00005f);
        ok(bturn[0] == 0.0f && bturn[1] == 0.0f && bturn[2] == 0.0f, "300 fps: 0.00005 is at rest");
        *(uint8_t *)0x6BC166 = keep166, *(uint8_t *)0x6BC192 = keep192;
        *ts = 50.0f / 30.0f;
    }

    printf("== FramerateVigilante's: swinging parts, the burnout, swimming\n");
    {
        float *ts = (float *)0xB7CB5C;
        float k = 50.0f / 30.0f;
        *ts = k;
        ok(cf_damping(0.97f) == 0.97f, "30 fps: a damping as stock");
        *ts = k / 2;
        ok(fabsf(cf_damping(0.97f) - sqrtf(0.97f)) < 1e-6f, "60 fps: its square root a frame, the same a second");
        *ts = 0;
        ok(cf_damping(0.97f) == 0.97f, "no timestep: as stock");
        *ts = k / 2;
        static float door[8];
        door[3] = 1.0f, door[5] = 0.2f;                    /* m_angle +0Ch, m_angVel +14h */
        reg_esi = (uint32_t)door;
        ret_at(0x6F442A);
        call_regs(hook_of("doorangle"), 0, 0, 1);
        unret(0x6F442A);
        ok(fabsf(fpu_out - 1.1f) < 1e-6f, "doorangle: a door (not a chassis) turns by its speed scaled");
        ret_at(0x6F4397);
        ok(fabsf(call_fpu(hook_of("doordamp1"), 0, 0) - sqrtf(*(float *)0x872310)) < 1e-6f, "doordamp1: a boat's 0.98");
        unret(0x6F4397);
        ret_at(0x6F43A7);
        ok(fabsf(call_fpu(hook_of("doordamp2"), 0, 0) - sqrtf(*(float *)0x872314)) < 1e-6f, "doordamp2: the ladder's 0.92");
        unret(0x6F43A7);
        ret_at(0x6F43D0);
        ok(fabsf(call_fpu(hook_of("doordamp3"), 0, 0) - sqrtf(*(float *)0x872318)) < 1e-6f, "doordamp3: a bonnet's 0.97");
        unret(0x6F43D0);
        ret_at(0x6F43D8);
        ok(fabsf(call_fpu(hook_of("doordamp4"), 0, 0) - sqrtf(*(float *)0x87230C)) < 1e-6f && ((fpu_sw >> 11) & 7) == 7,
           "doordamp4: any other part's 0.97, one value left");
        unret(0x6F43D8);
        {   /* the burnout's ApplyTurnForce(contact point, no steering): the twist about the
             * centre of mass, all of it scaled */
            typedef struct { float x, y, z; } V3;
            typedef void (__attribute__((thiscall)) *TurnFn)(void *self, V3 force, V3 point);
            static uint8_t car[0x100];
            static float m[18];
            memset(car, 0, sizeof car);
            memset(m, 0, sizeof m);
            m[0] = m[5] = m[10] = 1.0f;
            *(float **)(car + 0x14) = m;
            *(float *)(car + 0x90) = 2500.0f;                /* turn mass */
            *(float *)(car + 0xA8) = -0.6f;                  /* centre of mass y */
            V3 contact = {-0.8f, -1.3f, -0.5f}, none = {0, 0, 0};
            float keep = *(float *)0xB7CB5C;
            *(float *)0xB7CB5C = 50.0f / 850.0f;
            ((TurnFn)0x542A50)(car, contact, none);
            float stock = *(float *)(car + 0x58);
            memset(car + 0x50, 0, 12);
            ((TurnFn)(uintptr_t)hooks[find("burnout")].to)(car, contact, none);
            float fixed = *(float *)(car + 0x58);
            *(float *)0xB7CB5C = keep;
            ok(stock != 0, "burnout: with no steering the game still twists the car (the centre of mass)");
            ok(fabsf(fixed - stock * (50.0f / 850.0f) / (50.0f / 30.0f)) < fabsf(stock) * 1e-5f,
               "burnout: all of that twist scaled to a 30 fps step");
        }
        static float task[0x10];
        task[0x34 / 4] = 2.0f;                              /* m_fStateChanger, the pitch rate */
        reg_edi = (uint32_t)task;
        ret_at(0x68A6C3);
        call_regs(hook_of("swimpitch1"), 2.0f, 1, 1);
        int one = ((fpu_sw >> 11) & 7) == 7;
        unret(0x68A6C3);
        ok(one && fabsf(fpu_out - 2.0f * sqrtf(0.95f)) < 1e-5f, "swimpitch1: the rate times 0.95 per 30 fps step, one value left");
        int r = CrashFix_Install(find("swimsurface"), NULL);
        ok(r >= 1 && *(float **)0x68A7E8 == &cf_k30, "swimsurface: the pull's cap reads 1.6667, so it is 0.1667 at any fps");
        ok(CrashFix_Uninstall(find("swimsurface")) == 1 && *(uint32_t *)0x68A7E8 == 0xB7CB5C, "and back");
        if (r == 2) CrashFix_Install(find("swimsurface"), NULL);
        *ts = k;
    }

    printf("== FramerateVigilante's: a horn tap switches the siren (siren)\n");
    {
        uint8_t *history = (uint8_t *)0xB73569, *now = (uint8_t *)0xB7356E;
        uint32_t *ms = (uint32_t *)0xB7CB84;
        static uint8_t carA[0x600], carB[0x600];
        memset(history, 0, 5);
        /* one pad frame: the horn held or not at the game's time t */
#define PAD(held, t) (*now = (*now + 1) % 5, history[*now] = (held), *ms = (t))
        *now = 0;
        PAD(0, 1000);
        ok(cf_siren_horn(carA) == 0, "nothing held: neither");
        PAD(1, 1010);
        ok(cf_siren_horn(carA) == 0, "pressed: not the horn yet");
        PAD(1, 1060);
        ok(cf_siren_horn(carA) == 0, "held 50 ms: still not");
        PAD(0, 1110);
        ok(cf_siren_horn(carA) == 2, "let go after 100 ms: the siren switched");
        PAD(0, 1120);
        ok(cf_siren_horn(carA) == 0, "then nothing");
        PAD(1, 2000);
        cf_siren_horn(carA);
        PAD(1, 2149);
        int before = cf_siren_horn(carA);
        PAD(1, 2150);
        int at = cf_siren_horn(carA);
        PAD(0, 2400);
        ok(before == 0 && at == 1 && cf_siren_horn(carA) == 0, "held 150 ms: the horn; let go: no switch");
        PAD(1, 3000);
        cf_siren_horn(carA);
        *ms = 3100;
        cf_siren_horn(carB);
        PAD(0, 3120);
        int a = cf_siren_horn(carA);
        *ms = 3290;
        ok(a == 2 && cf_siren_horn(carB) == 0, "each car its own press: a tap for one, too long for the other");
        static const uint8_t ways[3][3] = {{0xB0, 0, 0xC3}, {0xB0, 1, 0xC3}, {0xB0, 2, 0xC3}};
        static const uint32_t to[3] = {0x6E09F7, 0x6E09E8, 0x6E0999};
        uint8_t keep[3][3];
        for (int i = 0; i < 3; i++) memcpy(keep[i], (void *)to[i], 3), memcpy((void *)to[i], ways[i], 3);
        reg_esi = (uint32_t)carA;
        PAD(1, 4000);
        int pressed = call_regs(hook_of("siren"), 0, 0, 0) & 0xFF;
        PAD(0, 4050);
        int tapped = call_regs(hook_of("siren"), 0, 0, 0) & 0xFF;
        PAD(1, 5000);
        call_regs(hook_of("siren"), 0, 0, 0);
        PAD(1, 5200);
        int horn = call_regs(hook_of("siren"), 0, 0, 0) & 0xFF;
        for (int i = 0; i < 3; i++) memcpy((void *)to[i], keep[i], 3);
        ok(pressed == 0 && tapped == 2 && horn == 1, "through the hook: the game's way out for each");
#undef PAD
    }

    printf("== MTA's: a ped's bone matrices (pedanim, pedskin)\n");
    {
        float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0.5f, -0.2f, 9.5f, 0};
        float good[16];
        memcpy(good, m, sizeof m);
        cf_ped_anim(m);
        ok(!memcmp(m, good, sizeof m) && averted("pedanim") == 0, "a bone 9.5 from its parent: left");
        m[5] = 2.5f;
        cf_ped_anim(m);
        int zero = 1;
        for (int i = 0; i < 15; i++) if (i != 3 && i != 7 && i != 11 && m[i] != 0) zero = 0;
        ok(zero && averted("pedanim") == 1, "a rotation of 2.5: zeroed");
        memcpy(m, good, sizeof m), m[13] = -10.5f;
        cf_ped_anim(m);
        ok(m[0] == 0 && averted("pedanim") == 2, "10.5 from its parent: zeroed");
        memcpy(m, good, sizeof m), m[2] = NAN;
        cf_ped_anim(m);
        ok(m[0] == 0 && averted("pedanim") == 3, "NaN: zeroed");
        memcpy(&site_stack[20], good, sizeof good);
        ((float *)&site_stack[20])[14] = INFINITY;
        uint8_t keep[3];
        memcpy(keep, (void *)0x7C5A61, 2);
        memcpy((void *)0x7C5A61, "\x58\xC3", 2);           /* pop the eax it pushed, ret */
        call_site(hook_of("pedanim"), 0x1234, 0, 0, 0);
        memcpy((void *)0x7C5A61, keep, 2);
        ok(averted("pedanim") == 4 && ((float *)&site_stack[20])[0] == 0 && site_ret == 0x1234,
           "through the hook: the matrix at [esp+54h], eax kept");

        uint32_t *offset = (uint32_t *)0xC978A4, was = *offset;
        static float bones[3][16];
        static struct { int32_t flags, count; float (*matrices)[16]; } hier = {0, 3, bones};
        static uint8_t atomic[0x80];
        for (int i = 0; i < 3; i++) memcpy(bones[i], good, sizeof good);
        bones[2][14] = -10000.0f;                           /* where Unity Roleplay keeps objects */
        *offset = 0x40, *(void **)(atomic + 0x40) = &hier;
        ok(cf_ped_skin_ok(atomic), "skin matrices in the world, z -10000 too: drawn");
        bones[1][12] = 200000.0f;
        ok(!cf_ped_skin_ok(atomic), "one 200000 out: not drawn");
        bones[1][12] = 0, bones[0][9] = NAN;
        ok(!cf_ped_skin_ok(atomic), "a NaN: not drawn");
        bones[0][9] = 0;
        *(void **)(atomic + 0x40) = NULL;
        ok(cf_ped_skin_ok(atomic), "no hierarchy: the game's to draw");
        *(void **)(atomic + 0x40) = &hier;
        memcpy(keep, (void *)0x7335B6, 3);
        memcpy((void *)0x7335B6, "\x5F\x5E\xC3", 3);       /* pop edi, esi; ret */
        site_stack[0] = (uint32_t)atomic;
        call_site(hook_of("pedskin"), 0, 0, 0, 0);
        int drawn = site_ret == 1 && averted("pedskin") == 0;
        bones[2][0] = INFINITY;
        call_site(hook_of("pedskin"), 0, 0, 0, 0);
        memcpy((void *)0x7335B6, keep, 3);
        ok(drawn && site_ret == (uint32_t)atomic && averted("pedskin") == 1,
           "through the hook: drawn on, or its atomic returned and counted");
        *offset = was;
    }

    printf("== the dithered sky (skydither) on a D3D9 device, through the game's RwD3D9\n");
    mixsets_test();
    gear_test();
    wheel_test();
    sky_test();

    printf("== fixes.lua's fog routine through RwD3D9SetRenderState\n");
    {
        char fog[MAX_PATH];
        strcpy(fog, path);
        strcpy(strrchr(fog, '\\') + 1, "fog.bin");
        FILE *f = fopen(fog, "rb");
        uint8_t *code = VirtualAlloc(NULL, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        size_t n = f ? fread(code, 1, 0x1000, f) : 0;
        if (f) fclose(f);
        ok(n == 121, "fog.bin read (121 bytes)");
        if (n == 121) {
            static const uint32_t states[4] = {0x1C, 0x22, 0x23, 0x8C}, values[4] = {1, 0x00ABCDEF, 3, 0};
            volatile uint32_t *shadow = (uint32_t *)0xC991D0, *queue = (uint32_t *)0xC98B40, *count = (uint32_t *)0xC9A5EC;
            for (int i = 0; i < 4; i++) shadow[states[i] * 2] = values[i], shadow[states[i] * 2 + 1] = 0;
            *count = 0;
            uint32_t esi, edi, ebx;
            __asm__ volatile("mov %%esi, %0\n mov %%edi, %1\n mov %%ebx, %2" : "=m"(esi), "=m"(edi), "=m"(ebx));
            ((void (*)(void))code)();
            uint32_t esi2, edi2, ebx2;
            __asm__ volatile("mov %%esi, %0\n mov %%edi, %1\n mov %%ebx, %2" : "=m"(esi2), "=m"(edi2), "=m"(ebx2));
            int kept = 1, queued = 1;
            for (int i = 0; i < 4; i++) {
                if (shadow[states[i] * 2] != values[i]) kept = 0;
                if (shadow[states[i] * 2 + 1] != 1 || queue[i] != states[i]) queued = 0;
            }
            ok(kept, "each state keeps the value the game holds");
            ok(queued && *count == 4, "and is queued for the next flush, in order");
            ok(esi == esi2 && edi == edi2 && ebx == ebx2, "the registers come back");
            ((void (*)(void))code)();
            ok(*count == 4, "again before a flush: already queued, not twice");
        }
    }

    printf("== faults.c: a fault in the game's code, with its callers, in faults.log\n");
    faults_test(path);

    stream_test(path);

    printf("crashfix: %d passed, %d failed\n", passed, failed);
    fflush(stdout);
    return failed != 0;
}
