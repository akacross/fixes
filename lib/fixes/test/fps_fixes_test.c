/*
 * fixes.lua's frame-rate stubs (fps.bin, from fixes_test.lua --fps-bytes,
 * built at 0x20000000) run under Wine with real x87 maths. The game globals
 * they read (CTimer::ms_fTimeStep, gHandlingDataMgr.fWheelFriction and the
 * swim code's -0.1 and +0.01) are built at +F00h of the block, and the swim stub's jmp
 * back into the game goes to a ret at +F80h. Built with SSE maths, so the
 * compiler keeps nothing on the x87 stack the asm resets.
 *
 *     lib/fixes/test/fps_fixes_test.sh
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#define BASE 0x20000000u
#define TIMESTEP (*(volatile float *)0x20000F00)
#define FRICTION (*(volatile float *)0x20000F04)
#define MINUS_01 (*(volatile float *)0x20000F08)
#define LIFT_001 (*(volatile float *)0x20000F0C)

static int passed, failed;
static void ok(int cond, const char *what)
{
    if (cond) passed++; else { failed++; printf("  FAIL: %s\n", what); }
}
static int close_to(float a, float b) { return fabsf(a - b) <= 1e-5f * fmaxf(1.0f, fabsf(b)); }

uint32_t stub;
float in0, out0, out1;
float frame[16];                       /* the swim stub's [esp+..] */
float st0, st1, depth;

/* call a stub that takes st0 (or nothing) and leaves st0 */
static float call1(uint32_t at, float x, int takes)
{
    stub = at; in0 = x;
    if (takes)
        __asm__ volatile("finit\n\tflds _in0\n\tcall *_stub\n\tfstps _out0\n\t" ::: "memory");
    else
        __asm__ volatile("finit\n\tcall *_stub\n\tfstps _out0\n\t" ::: "memory");
    return out0;
}

uint32_t esp_in, esp_out;

int main(void)
{
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    strcpy(strrchr(path, '\\') + 1, "fps.bin");
    FILE *f = fopen(path, "rb");
    if (!f) { printf("no fps.bin beside the exe\n"); return 2; }
    uint8_t *block = VirtualAlloc((void *)BASE, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    size_t n = fread(block, 1, 0x10000, f);
    fclose(f);
    if (block != (uint8_t *)BASE || n < 0x40) { printf("block not loaded\n"); return 2; }
    block[0xF80] = 0xC3;                                   /* where the swim stub jmps back to */
    FRICTION = 0.9f;
    MINUS_01 = -0.1f;
    LIFT_001 = 0.01f;

    /* the stubs, found by the game-address instruction each starts with */
    uint32_t wheel = 0, swim = 0, dive = 0, flt = 0, lift = 0;
    for (uint32_t i = 0x10; i + 6 < n; i++) {
        uint8_t *p = block + i;
        if (!wheel && !memcmp(p, "\xD9\x05\x04\x0F\x00\x20", 6)) wheel = BASE + i;
        else if (!dive && !memcmp(p, "\xD8\x0D\x08\x0F\x00\x20", 6)) dive = BASE + i;
        else if (!lift && !memcmp(p, "\xD9\x05\x0C\x0F\x00\x20", 6)) lift = BASE + i;
        else if (!memcmp(p, "\xD9\x05\x00\x0F\x00\x20", 6)) { if (!swim) swim = BASE + i; else if (!flt) flt = BASE + i; }
    }
    ok(wheel && swim && dive && flt && lift, "all five stubs found");

    printf("== wheelfps: the coasting brake per step scales with the timestep\n");
    TIMESTEP = 50.0f / 30.0f;
    ok(close_to(call1(wheel, 0, 0), 0.9f), "30 fps: 0.9, stock");
    TIMESTEP = 50.0f / 60.0f;
    ok(close_to(call1(wheel, 0, 0), 0.45f), "60 fps: 0.45, twice the steps, the same slow-down a second");
    TIMESTEP = 50.0f / 144.0f;
    ok(close_to(call1(wheel, 0, 0), 0.9f * 30.0f / 144.0f), "144 fps");
    FRICTION = 0.5f;
    TIMESTEP = 50.0f / 60.0f;
    ok(close_to(call1(wheel, 0, 0), 0.25f), "a value handling.cfg or another mod set is scaled, not replaced");
    FRICTION = 0.9f;

    printf("== dive: the vertical branch's -0.1 times timestep / 1.6667\n");
    TIMESTEP = 50.0f / 60.0f;
    ok(close_to(call1(dive, 2.0f, 1), 2.0f * -0.1f * 0.5f), "60 fps: x * -0.1 * 0.5");
    TIMESTEP = 50.0f / 30.0f;
    ok(close_to(call1(dive, 2.0f, 1), -0.2f), "30 fps: stock");

    printf("== lift: the underwater state's +0.01 times timestep / 1.6667, added\n");
    TIMESTEP = 50.0f / 30.0f;
    ok(close_to(call1(lift, 0.05f, 1), 0.06f), "30 fps: z + 0.01, stock");
    TIMESTEP = 50.0f / 60.0f;
    ok(close_to(call1(lift, 0.05f, 1), 0.055f), "60 fps: z + 0.005, which the swim stub's k doubles back");

    printf("== floatfps: the buoyancy timestep becomes t * (t / 1.5 + 1)\n");
    TIMESTEP = 50.0f / 30.0f;
    ok(close_to(call1(flt, 3.0f, 1), 3.0f * 50.0f / 30.0f), "30 fps: times the timestep, stock");
    TIMESTEP = 50.0f / 60.0f;
    ok(close_to(call1(flt, 3.0f, 1), 3.0f * 0.5f * (0.5f / 1.5f + 1.0f)), "60 fps: t 0.5");

    printf("== swimfps: x, y and z times 1.6667 / timestep, the x87 stack as the game leaves it\n");
    {
        float x = 2.0f, y = 3.0f, z = 4.0f, blend = 0.25f, pw = 0.75f;
        TIMESTEP = 50.0f / 60.0f;                          /* k = 2 */
        memset(frame, 0, sizeof frame);
        /* esp = &frame[1] before the call, so after its push [esp + 4n] is
         * frame[n]: y at [esp+14h], z at [esp+18h], the results at 1Ch, 20h */
        frame[5] = y; frame[6] = z;
        in0 = x * blend; st0 = blend; st1 = pw; stub = swim;
        __asm__ volatile(
            "finit\n\t"
            "flds _st1\n\tflds _st0\n\tflds _in0\n\t"     /* pow, blend, x*blend */
            "movl %%esp, _esp_in\n\t"
            "movl %%esp, %%eax\n\t"
            "leal _frame+4, %%esp\n\t"                     /* [esp] after the call = frame[0] */
            "call *_stub\n\t"
            "movl %%eax, %%esp\n\t"
            "fstps _out0\n\t"                              /* blend*k*z */
            "fstps _out1\n\t"                              /* pow, untouched */
            "fstps _depth\n\t"
            "movl %%esp, _esp_out\n\t"
            ::: "eax", "memory");
        ok(close_to(frame[7], x * blend * 2.0f), "[esp+1Ch]: x * blend * k");
        ok(close_to(frame[8], y * blend * 2.0f), "[esp+20h]: y * blend * k");
        ok(close_to(out0, z * blend * 2.0f), "st0: z * blend * k, as the game's next fld expects");
        ok(close_to(out1, pw), "st1: the pow under it, untouched");
        ok(isnan(depth), "nothing else left on the x87 stack");
        ok(esp_in == esp_out, "esp back");
        TIMESTEP = 50.0f / 30.0f;
        memset(frame, 0, sizeof frame);
        frame[5] = y; frame[6] = z; in0 = x * blend;
        __asm__ volatile(
            "finit\n\tflds _st1\n\tflds _st0\n\tflds _in0\n\t"
            "movl %%esp, %%eax\n\tleal _frame+4, %%esp\n\tcall *_stub\n\tmovl %%eax, %%esp\n\t"
            "fstps _out0\n\tfstps _out1\n\t" ::: "eax", "memory");
        ok(close_to(frame[7], x * blend) && close_to(frame[8], y * blend) && close_to(out0, z * blend), "30 fps: stock");
    }

    printf("\nfps fixes: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
