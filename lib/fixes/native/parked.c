/* Parked cars' collision (fixes.lua's parkedcol option), for gta_sa.exe 1.0
 * US; included by crashfix.c after drawcount.c.
 *
 * A car never leaves the moving list, so CPhysical::ProcessCollision
 * (0x54DFB0) runs for every parked car every frame. With its move and turn
 * speeds zero (an abandoned car on fake physics has them reset each frame)
 * the game's own path checks nothing: it applies the zero speeds,
 * reorthogonalises the matrix, sets the flags below, measures the distance it
 * did not move and re-links the car in the sector lists (RemoveAndAdd) where
 * it already is. For such a car this sets the flags and returns. Everything
 * else, and any car the stock path would treat otherwise, takes the game's
 * path. Integers only: the x87 state is the game's.
 *
 * With parkedcreep on, a car whose speeds are all under 1e-6 after 10 still
 * frames (the stock fake-physics count) has them zeroed and is skipped too.
 */
#define PARKED(FJ) \
    FJ(parkedcol, "parkedcol", "CPhysical::ProcessCollision: a parked car's no-op skipped", 0x54DFB0, 8, \
        "\x6A\xFF\x64\xA1\x00\x00\x00\x00")

PARKED(FDECLARE_J)

static volatile LONG cf_parked_skips, cf_parked_creeps;
static int cf_parked_creep;                     /* option parkedcreep */
/* abandoned or wrecked cars by why they took the game's path: 0 skipped,
 * 1 no collision or simple collision off, 2 flagged, 3 a car park vehicle,
 * 4 moving or turning, 5 a friction speed */
static uint32_t cf_parked_why[6];
/* the moving ones (4) by their still count (+0xB8: 0, 1-9, 10-99, 100+) and
 * by what fails the still test at 30 fps scale: 0 smoothed speed (+0x74)
 * over 0.005, 1 smoothed turn (+0x80) over 0.0015, 2 moved (+0xD4) over
 * 0.005, 3 damage (+0xD8) above 0 */
static uint32_t cf_parked_count[4], cf_parked_fail[4];
/* the moving ones by what moves: 0 x or y, 1 z alone, 2 turn alone; and by
 * the largest component: 3 under 1e-6, 4 under 1e-4, 5 more */
static uint32_t cf_parked_what[6];
/* the moving ones by how their count changed since their last call: 0 the
 * same, 1 up one, 2 up more, 3 down (not to 0), 4 to 0 */
static uint32_t cf_parked_step[5];
static uint8_t cf_parked_last[4096];
static uint32_t cf_parked_last_frame[4096];
uint8_t __cdecl cf_sleep_frames(void);
static int is_vehicle(const uint8_t *p);

static void parked_note(const uint8_t *e)
{
    uint8_t n = e[0xB8], asleep = cf_sleep_frames();
    cf_parked_count[n == 0 ? 0 : n < asleep ? 1 : n == asleep ? 2 : 3]++;
    if (is_vehicle(e)) {
        uint32_t i = (uint32_t)(e - **(const uint8_t ***)0xB74494) / 0xA18, frame = *(const uint32_t *)0xB7CB4C;
        if (i < 4096) {
            if (cf_parked_last_frame[i] + 1 == frame) {
                uint8_t was = cf_parked_last[i];
                cf_parked_step[n == was ? 0 : n == (uint8_t)(was + 1) ? 1 : n > was ? 2 : n ? 3 : 4]++;
            }
            if (cf_parked_last_frame[i] != frame) cf_parked_last[i] = n, cf_parked_last_frame[i] = frame;
        }
    }
    const uint32_t *v = (const uint32_t *)(e + 0x44);
    float big = 0;
    for (int i = 0; i < 6; i++) {
        float a = ((const float *)v)[i];
        if (a < 0) a = -a;
        if (a > big) big = a;
    }
    if ((v[0] | v[1]) & 0x7FFFFFFF) cf_parked_what[0]++;
    else if (v[2] & 0x7FFFFFFF) cf_parked_what[1]++;
    else cf_parked_what[2]++;
    cf_parked_what[big < 1e-6f ? 3 : big < 1e-4f ? 4 : 5]++;
    const float *f = (const float *)(e + 0x74), *q = (const float *)(e + 0x80);
    if (f[0] * f[0] + f[1] * f[1] + f[2] * f[2] > 0.005f * 0.005f) cf_parked_fail[0]++;
    if (q[0] * q[0] + q[1] * q[1] + q[2] * q[2] > 0.0015f * 0.0015f) cf_parked_fail[1]++;
    if (*(const float *)(e + 0xD4) > 0.005f) cf_parked_fail[2]++;
    if (*(const float *)(e + 0xD8) > 0.0f) cf_parked_fail[3]++;
}

/* -> 1: the entity is a car the stock path would leave where it is */
int __cdecl cf_parked(uint8_t *e)
{
    uint32_t flags = *(uint32_t *)(e + 0x40);                       /* physicalFlags */
    int status = e[0x36] >> 3;
    if ((e[0x36] & 7) != 2 || (status != 4 && status != 5)) return 0;  /* a vehicle, abandoned or wrecked */
    if (!(e[0x1C] & 1) || (flags & 0x20000)) return cf_parked_why[1]++, 0;   /* uses collision, simple collision on */
    if (flags & 0x880) return cf_parked_why[2]++, 0;                 /* even if stationary; bDisableZ */
    if (e[0x42C] & 2) return cf_parked_why[3]++, 0;                  /* a car park vehicle */
    uint32_t *speed = (uint32_t *)(e + 0x44);
    if (cf_parked_creep && e[0xB8] >= 10) {
        uint32_t any = 0, tiny = 1;
        for (int i = 0; i < 12; i++) any |= speed[i] & 0x7FFFFFFF, tiny &= (speed[i] & 0x7FFFFFFF) < 0x358637BD;   /* 1e-6 */
        if (any && tiny) {
            for (int i = 0; i < 12; i++) speed[i] = 0;
            cf_parked_creeps++;
        }
    }
    for (int i = 0; i < 12; i++) {                                   /* move, turn, friction move and turn speeds */
        uint32_t f = ((const uint32_t *)(e + 0x44))[i];
        if (f & 0x7FFFFFFF) {
            if (i < 6) parked_note(e);
            return cf_parked_why[i < 6 ? 4 : 5]++, 0;
        }
    }
    cf_parked_why[0]++;
    *(uint32_t *)(e + 0xD4) = 0;                                     /* m_fMovingSpeed */
    *(uint32_t *)(e + 0x40) = flags & ~0x9800u;                      /* processing shift, skip line col, even if stationary */
    *(uint32_t *)(e + 0x1C) = (*(uint32_t *)(e + 0x1C) & ~0x10u) | 0x20u;   /* not stuck, in a safe position */
    cf_parked_skips++;
    return 1;
}

/* How many times a parked car's collision was skipped since the DLL loaded */
__declspec(dllexport) uint32_t __cdecl CrashFix_ParkedSkips(void) { return (uint32_t)cf_parked_skips; }

/* Option parkedcreep: also skip a still car whose speeds are all under 1e-6 */
__declspec(dllexport) void __cdecl CrashFix_SetParkedCreep(int on) { cf_parked_creep = on != 0; }

/* How many of the skips had a creep zeroed since the DLL loaded */
__declspec(dllexport) uint32_t __cdecl CrashFix_ParkedCreeps(void) { return (uint32_t)cf_parked_creeps; }

/* Since the DLL loaded, abandoned or wrecked cars' calls by why (as cf_parked_why) */
__declspec(dllexport) uint32_t __cdecl CrashFix_ParkedWhy(int i)
{
    if (i >= 0 && i < 6) return cf_parked_why[i];
    if (i >= 6 && i < 10) return cf_parked_count[i - 6];
    if (i >= 10 && i < 14) return cf_parked_fail[i - 10];
    if (i >= 14 && i < 20) return cf_parked_what[i - 14];
    if (i >= 20 && i < 25) return cf_parked_step[i - 20];
    return 0;
}

__asm__(
".intel_syntax noprefix\n"
"HOOK parkedcol\n"
"    push ecx\n"
"    push ecx\n"
"    call _cf_parked\n"
"    add esp, 4\n"
"    pop ecx\n"
"    test eax, eax\n"
"    jz 1f\n"
"    ret\n"
"1:\n"
"    push -1\n"
"    mov eax, dword ptr fs:[0]\n"
"    GO 0x54DFB8\n"
".att_syntax\n"
);
