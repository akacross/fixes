/*
 * fixes.lua's position guard (guard.bin, from fixes_test.lua --guard-bytes,
 * built at 0x20000000, the player pointer at 0x20000F00) run on fake entities
 * under Wine: a good position stays; a NaN, inf or off-map one takes the last
 * good one for the player's ped or vehicle, else the RenderWare frame's, else
 * 0, 0, 0; bad speeds are zeroed; every register and the flags come back.
 *
 *     lib/fixes/test/position_guard_test.sh
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#define BASE 0x20000000u

static int passed, failed;
static void ok(int cond, const char *what)
{
    if (cond) passed++; else { failed++; printf("  FAIL: %s\n", what); }
}

/* CPlaceable/CEntity/CPhysical fields the guard reads: m_placement.pos at 4,
 * m_matrix at 0x14 (its position at +0x30), m_pRwObject at 0x18 (its frame at
 * +4, the frame's position at +0x40), move and turn speed at 0x44-0x5B. */
typedef struct { uint8_t b[0x7A4]; } Entity;   /* sizeof(CPed): m_pVehicle at 0x58C */
typedef struct { uint8_t b[0x48]; } Matrix;
typedef struct { uint8_t b[0x60]; } Frame;
typedef struct { uint8_t b[0x08]; } RwObject;

static float *fat(void *p, int off) { return (float *)((uint8_t *)p + off); }
static void set3(void *p, int off, float x, float y, float z) { fat(p, off)[0] = x; fat(p, off)[1] = y; fat(p, off)[2] = z; }
static int is3(void *p, int off, float x, float y, float z)
{
    return fat(p, off)[0] == x && fat(p, off)[1] == y && fat(p, off)[2] == z;
}
static int speedsZero(Entity *e) { for (int i = 0; i < 6; i++) if (fat(e, 0x44)[i] != 0) return 0; return 1; }

uint32_t regsIn[9], regsOut[9];
Entity *callee;
/* ecx = the entity; the other registers and the flags hold known values, all
 * of which must come back unchanged (ecx too). */
static void call(Entity *e)
{
    callee = e;
    __asm__ volatile(
        "pushal\n\t"
        "movl _callee, %%ecx\n\t"
        "movl $0x11111111, %%eax\n\t"
        "movl $0x22222222, %%ebx\n\t"
        "movl $0x33333333, %%edx\n\t"
        "movl $0x44444444, %%esi\n\t"
        "movl $0x55555555, %%edi\n\t"
        "pushl %%ebp\n\t"
        "movl $0x66666666, %%ebp\n\t"
        "stc\n\t"
        "pushfl\n\t"
        "popl _regsIn+32\n\t"
        "movl %%eax, _regsIn\n\t"
        "movl %%ebx, _regsIn+4\n\t"
        "movl %%ecx, _regsIn+8\n\t"
        "movl %%edx, _regsIn+12\n\t"
        "movl %%esi, _regsIn+16\n\t"
        "movl %%edi, _regsIn+20\n\t"
        "movl %%ebp, _regsIn+24\n\t"
        "movl %%esp, _regsIn+28\n\t"
        "pushl _regsIn+32\n\t"
        "popfl\n\t"
        "movl $0x20000040, %%eax\n\t"
        "pushl %%eax\n\t"
        "movl _regsIn, %%eax\n\t"
        "call *(%%esp)\n\t"
        "pushfl\n\t"
        "popl _regsOut+32\n\t"
        "addl $4, %%esp\n\t"
        "movl %%eax, _regsOut\n\t"
        "movl %%ebx, _regsOut+4\n\t"
        "movl %%ecx, _regsOut+8\n\t"
        "movl %%edx, _regsOut+12\n\t"
        "movl %%esi, _regsOut+16\n\t"
        "movl %%edi, _regsOut+20\n\t"
        "movl %%ebp, _regsOut+24\n\t"
        "movl %%esp, _regsOut+28\n\t"
        "popl %%ebp\n\t"
        "popal\n\t"
        : : : "memory", "cc");
}
static int regsKept(void) { return memcmp(regsIn, regsOut, sizeof regsIn) == 0; }

static uint32_t counter(int i) { return ((volatile uint32_t *)BASE)[i]; }

int main(int argc, char **argv)
{
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    strcpy(strrchr(path, '\\') + 1, "guard.bin");
    FILE *f = fopen(path, "rb");
    if (!f) { printf("no guard.bin beside the exe\n"); return 2; }
    uint8_t *block = VirtualAlloc((void *)BASE, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (block != (uint8_t *)BASE) { printf("VirtualAlloc at 0x%08X failed\n", BASE); return 2; }
    size_t n = fread(block, 1, 0x10000, f);
    fclose(f);
    ok(n > 0x10 && n < 0x300, "guard.bin read, smaller than its 0x300 block");

    static Entity e;
    static Matrix m;
    static Frame fr;
    static RwObject rw;
    float nan = NAN, inf = INFINITY;

    printf("== a good position with a matrix is left alone\n");
    memset(&e, 0, sizeof e);
    *(Matrix **)(e.b + 0x14) = &m;
    set3(&m, 0x30, 1544.0f, -1675.0f, 13.5f);
    set3(&e, 0x44, 0.1f, -0.2f, 0.0f);
    call(&e);
    ok(is3(&m, 0x30, 1544.0f, -1675.0f, 13.5f), "position kept");
    ok(fat(&e, 0x44)[0] == 0.1f && fat(&e, 0x44)[1] == -0.2f, "speed kept");
    ok(counter(0) == 0 && counter(1) == 0, "nothing counted");
    ok(regsKept(), "every register and the flags kept");

    printf("== the limits themselves pass\n");
    set3(&m, 0x30, 20000.0f, -20000.0f, 1000000.0f);
    call(&e);
    ok(is3(&m, 0x30, 20000.0f, -20000.0f, 1000000.0f) && counter(0) == 0, "x, y 20000 and z 1000000 kept");
    set3(&m, 0x30, 0.0f, 0.0f, -1000000.0f);
    call(&e);
    ok(is3(&m, 0x30, 0.0f, 0.0f, -1000000.0f) && counter(0) == 0, "z -1000000 kept");
    set3(&m, 0x30, -2080.44f, 296.46f, -10000.04f);
    call(&e);
    ok(is3(&m, 0x30, -2080.44f, 296.46f, -10000.04f) && counter(0) == 0, "an object a server keeps at z -10000.04 kept (Unity Roleplay)");
    set3(&m, 0x30, 0.0f, 0.0f, -0.0f);
    call(&e);
    ok(counter(0) == 0, "z -0 kept");

    printf("== a NaN position takes the frame's\n");
    *(RwObject **)(e.b + 0x18) = &rw;
    *(Frame **)(rw.b + 4) = &fr;
    set3(&fr, 0x40, 100.0f, 200.0f, 10.0f);
    set3(&m, 0x30, nan, nan, nan);
    set3(&e, 0x44, 1.0f, 2.0f, 3.0f);
    call(&e);
    ok(is3(&m, 0x30, 100.0f, 200.0f, 10.0f), "the frame's position copied in");
    ok(speedsZero(&e), "speeds zeroed");
    ok(counter(0) == 1, "counted once");
    ok(regsKept(), "registers kept");
    {
        volatile uint32_t *sample = (volatile uint32_t *)(BASE + 0x28);
        float sx, sy, sz;
        memcpy(&sx, (void *)&sample[1], 4); memcpy(&sy, (void *)&sample[2], 4); memcpy(&sz, (void *)&sample[3], 4);
        ok(sample[0] == (uint32_t)(uintptr_t)&e && isnan(sx) && isnan(sy) && isnan(sz),
           "the sample: this entity and the bad x, y, z, before they were put back");
    }

    printf("== inf, past 20000 and below -1000000 are put back too\n");
    set3(&m, 0x30, 0.0f, inf, 0.0f); call(&e);
    ok(is3(&m, 0x30, 100.0f, 200.0f, 10.0f), "inf y");
    set3(&m, 0x30, 25000.0f, 0.0f, 0.0f); call(&e);
    ok(is3(&m, 0x30, 100.0f, 200.0f, 10.0f), "x 25000");
    set3(&m, 0x30, 0.0f, 0.0f, -2000000.0f); call(&e);
    ok(is3(&m, 0x30, 100.0f, 200.0f, 10.0f), "z -2000000");
    set3(&m, 0x30, 0.0f, 0.0f, 2147483647.0f); call(&e);
    ok(is3(&m, 0x30, 100.0f, 200.0f, 10.0f), "z 2147483647");
    ok(counter(0) == 5, "each counted");

    printf("== a bad frame too: 0, 0, 0\n");
    set3(&fr, 0x40, nan, 0.0f, 0.0f);
    set3(&m, 0x30, nan, 1.0f, 1.0f);
    call(&e);
    ok(is3(&m, 0x30, 0.0f, 0.0f, 0.0f), "zeroed");
    *(RwObject **)(e.b + 0x18) = NULL;
    set3(&m, 0x30, -inf, 1.0f, 1.0f);
    call(&e);
    ok(is3(&m, 0x30, 0.0f, 0.0f, 0.0f), "no RwObject: zeroed");
    *(RwObject **)(e.b + 0x18) = &rw;
    *(Frame **)(rw.b + 4) = NULL;
    set3(&m, 0x30, nan, 1.0f, 1.0f);
    call(&e);
    ok(is3(&m, 0x30, 0.0f, 0.0f, 0.0f), "no frame: zeroed");
    ok(regsKept(), "registers kept");

    printf("== no matrix: m_placement is checked\n");
    *(Matrix **)(e.b + 0x14) = NULL;
    *(RwObject **)(e.b + 0x18) = NULL;
    set3(&e, 0x04, 5.0f, 6.0f, 7.0f);
    call(&e);
    ok(is3(&e, 0x04, 5.0f, 6.0f, 7.0f), "a good placement kept");
    set3(&e, 0x04, nan, 6.0f, 7.0f);
    call(&e);
    ok(is3(&e, 0x04, 0.0f, 0.0f, 0.0f), "a NaN placement zeroed");

    printf("== bad speeds alone are zeroed\n");
    set3(&e, 0x04, 5.0f, 6.0f, 7.0f);
    uint32_t before = counter(0);
    set3(&e, 0x44, 1.0f, 1.0f, 1.0f);
    set3(&e, 0x50, 0.5f, nan, 0.5f);
    call(&e);
    ok(speedsZero(&e) && counter(1) == 1, "a NaN turn speed: all six zeroed, counted");
    ok(is3(&e, 0x04, 5.0f, 6.0f, 7.0f) && counter(0) == before, "the position untouched");
    set3(&e, 0x44, inf, 0.0f, 0.0f);
    call(&e);
    ok(speedsZero(&e) && counter(1) == 2, "an inf move speed");
    set3(&e, 0x44, 1e30f, -1e30f, 0.0f);
    call(&e);
    ok(fat(&e, 0x44)[0] == 1e30f && counter(1) == 2, "a huge but finite speed is left");
    ok(regsKept(), "registers kept");

    printf("== the player's ped and vehicle get their last good position back\n");
    {
        static Entity ped, car, other;
        static Matrix pm, cm, om;
        static Frame pf;
        static RwObject prw;
        Entity **player = (Entity **)(BASE + 0xF00);
        memset(&ped, 0, sizeof ped); memset(&car, 0, sizeof car); memset(&other, 0, sizeof other);
        *(Matrix **)(ped.b + 0x14) = &pm;
        *(Matrix **)(car.b + 0x14) = &cm;
        *(Matrix **)(other.b + 0x14) = &om;
        *(RwObject **)(ped.b + 0x18) = &prw;
        *(Frame **)(prw.b + 4) = &pf;
        set3(&pf, 0x40, nan, nan, 50.0f);                   /* the frame already holds the bad one */
        *(Entity **)(ped.b + 0x58C) = &car;
        *player = &ped;
        uint32_t base = counter(0);
        set3(&pm, 0x30, 1544.0f, -1675.0f, 13.5f);
        call(&ped);                                         /* moving: the good one is kept */
        set3(&pm, 0x30, nan, nan, 50.0f);                   /* /tppos id nan nan 50 */
        set3(&ped, 0x44, 1.0f, 1.0f, 1.0f);
        call(&ped);
        ok(is3(&pm, 0x30, 1544.0f, -1675.0f, 13.5f), "the ped stays where it was, not at 0, 0, 0");
        ok(speedsZero(&ped) && counter(0) == base + 1, "speeds zeroed, counted");
        set3(&pm, 0x30, 1600.0f, -1700.0f, 14.0f);
        call(&ped);
        set3(&pm, 0x30, 0.0f, 0.0f, 2147483647.0f);
        call(&ped);
        ok(is3(&pm, 0x30, 1600.0f, -1700.0f, 14.0f), "the latest good one, after a legal teleport");
        {
            static Entity respawned;
            static Matrix rm;
            static Frame rf;
            static RwObject rrw;
            memset(&respawned, 0, sizeof respawned);
            *(Matrix **)(respawned.b + 0x14) = &rm;
            *(RwObject **)(respawned.b + 0x18) = &rrw;
            *(Frame **)(rrw.b + 4) = &rf;
            set3(&rf, 0x40, nan, nan, nan);
            set3(&rm, 0x30, nan, nan, nan);
            *player = &respawned;                           /* a new ped, the saved one left behind */
            call(&respawned);
            ok(is3(&rm, 0x30, 1600.0f, -1700.0f, 14.0f),
               "a respawned ped gets the last good position, not 0, 0, 0");
            *player = &ped;
        }
        set3(&cm, 0x30, 2500.0f, 2500.0f, 10.0f);
        call(&car);
        set3(&cm, 0x30, inf, 0.0f, 0.0f);
        call(&car);
        ok(is3(&cm, 0x30, 2500.0f, 2500.0f, 10.0f), "the ped's vehicle too");
        set3(&om, 0x30, 10.0f, 10.0f, 10.0f);
        call(&other);
        set3(&om, 0x30, nan, 0.0f, 0.0f);
        call(&other);
        ok(is3(&om, 0x30, 0.0f, 0.0f, 0.0f), "anything else: no slot, no frame, 0, 0, 0");

        printf("== 0, 0, 0 does not replace a known teleport\n");
        set3(&pf, 0x40, nan, nan, nan);
        set3(&pm, 0x30, 1294.0f, -1403.0f, 53.0f);
        call(&ped);
        set3(&pm, 0x30, 0.0f, 0.0f, 0.0f);
        call(&ped);
        ok(is3(&pm, 0x30, 0.0f, 0.0f, 0.0f), "the ped may stand at the origin");
        set3(&pm, 0x30, nan, nan, nan);
        call(&ped);
        ok(is3(&pm, 0x30, 1294.0f, -1403.0f, 53.0f),
           "a bad position comes back to the teleport, not 0, 0, 0");

        printf("== an origin slot gives way to the frame's teleport\n");
        {
            static Entity born;
            static Matrix bm;
            static Frame bf;
            static RwObject brw;
            volatile uint32_t *slot = (volatile uint32_t *)(BASE + 8);
            slot[0] = slot[1] = slot[2] = slot[3] = 0;     /* no teleport stored */
            memset(&born, 0, sizeof born);
            *(Matrix **)(born.b + 0x14) = &bm;
            *(RwObject **)(born.b + 0x18) = &brw;
            *(Frame **)(brw.b + 4) = &bf;
            *player = &born;
            set3(&bm, 0x30, 0.0f, 0.0f, 0.0f);
            set3(&bf, 0x40, nan, nan, nan);
            call(&born);                                    /* the birthplace is the first thing stored */
            set3(&bf, 0x40, 1296.0f, -1413.0f, 54.0f);
            set3(&bm, 0x30, nan, nan, nan);
            call(&born);
            ok(is3(&bm, 0x30, 1296.0f, -1413.0f, 54.0f), "the frame's teleport, not the birthplace");
            set3(&bf, 0x40, nan, nan, nan);
            set3(&bm, 0x30, nan, nan, nan);
            call(&born);
            ok(is3(&bm, 0x30, 1296.0f, -1413.0f, 54.0f), "and that teleport is what the next bad one comes back to");
        }
        *player = NULL;
        set3(&pm, 0x30, 1.0f, 2.0f, 3.0f);
        call(&ped);
        ok(is3(&pm, 0x30, 1.0f, 2.0f, 3.0f), "no player: a good position is still kept");
        ok(regsKept(), "registers kept");
    }

    printf("\nposition guard: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
