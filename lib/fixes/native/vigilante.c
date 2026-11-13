/* FramerateVigilante's frame-rate fixes that MTA's lack (Junior_Djjr's,
 * FramerateVigilante.cpp, MIT), for gta_sa.exe 1.0 US; included by crashfix.c
 * after framerate.c, so its SCALED and cf_k30 are here, each switched by its
 * own fixes option. Where they differ from FramerateVigilante's:
 *   - each damping is pow(rate, timestep / 1.6667), not a linear step.
 *   - the burnout's whole turn is scaled: its ApplyTurnForce passes the
 *     wheel's contact point as the force, which twists a car about its
 *     centre of mass every frame even with no steering, and FramerateVigilante
 *     scaled only the steering part.
 *   - the third dive-pitch damping is the fmul at 0x68A7C0 (its 0x68A7BD is
 *     the fld before it, and its code goes on inside the fmul).
 *   - the siren keeps a press time per vehicle: SA-MP runs it for other
 *     players' cars with their pads.
 *   - not taken: the rifle's aim-walk step (samp.dll gives ControlGunMove its
 *     own timestep, pedfps' aimwalk does the rest), the on-rails wheel spin
 *     (SA-MP has no simple cars), the fps limits.
 */

/* ------------------------------------------------------------------------ */
/* The rows: key, option, what, site, bytes, stock                          */
/* ------------------------------------------------------------------------ */
void cf_burnout_turn(void);

#define VIGILANTE(FJ, FP) \
    FJ(doordamp1, "vehiclefps", "CDoor::Process: a boat's part settles per 30 fps step", 0x6F4391, 6, \
        "\xD9\x05\x10\x23\x87\x00") \
    FJ(doordamp2, "vehiclefps", "CDoor::Process: the fire truck's ladder settles per 30 fps step", 0x6F43A1, 6, \
        "\xD9\x05\x14\x23\x87\x00") \
    FJ(doordamp3, "vehiclefps", "CDoor::Process: a bonnet settles per 30 fps step", 0x6F43CA, 6, "\xD9\x05\x18\x23\x87\x00") \
    FJ(doordamp4, "vehiclefps", "CDoor::Process: a door or boot settles per 30 fps step", 0x6F43D2, 6, \
        "\xD9\x05\x0C\x23\x87\x00") \
    FP(burnout, "vehiclefps", KIND_CALL, "CAutomobile::ProcessCarWheelPair: the burnout's turn, all of it, per 30 fps step", \
        0x6A5073, 5, "\xE8\xD8\xD9\xE9\xFF", cf_burnout_turn) \
    FJ(swimpitch1, "swimfps", "CTaskSimpleSwim::ProcessSwimmingResistance: the dive pitch settles per 30 fps step", \
        0x68A6BD, 6, "\xD8\x0D\xF0\x8E\x85\x00") \
    FJ(swimpitch2, "swimfps", "CTaskSimpleSwim::ProcessSwimmingResistance: the dive pitch settles per 30 fps step", \
        0x68A735, 6, "\xD8\x0D\xF0\x8E\x85\x00") \
    FJ(swimpitch3, "swimfps", "CTaskSimpleSwim::ProcessSwimmingResistance: the dive pitch settles per 30 fps step", \
        0x68A7C0, 6, "\xD8\x0D\xF0\x8E\x85\x00") \
    FP(swimsurface, "swimfps", KIND_PTR, "CTaskSimpleSwim::ProcessSwimmingResistance: the pull to the surface as at 30 fps", \
        0x68A7E8, 4, TS, &cf_k30) \
    FJ(siren, "sirenfps", "CVehicle::ProcessSirenAndHorn: a horn tap under 150 ms switches the siren", 0x6E0961, 7, \
        "\x90\x90\xE9\xD8\x2F\xD2\xFF")

VIGILANTE(FDECLARE_J, FDECLARE_P)

/* A damping the game applies once a frame, tuned at 30 fps, as the same
 * damping per second */
float cf_damping(float rate)
{
    float t = TIMESTEP;
    if (!(t > 0.0001f)) return rate;
    return powf(rate, t / cf_k30);
}

/* ------------------------------------------------------------------------ */
/* The siren                                                                */
/* ------------------------------------------------------------------------ */
/* ProcessSirenAndHorn calls a release after at most 3 of pad 0's last 5 horn
 * frames a tap, which switches the siren: 100 ms at 30 fps, 12 ms at 240.
 * Here a press under 150 ms is a tap, one held longer the horn. */
#define HORN_HISTORY ((const uint8_t *)0xB73569)        /* CPad::Pads[0].bHornHistory[5] */
#define HORN_NOW (*(const uint8_t *)0xB7356E)           /* its iCurrHornHistory */
#define HORN_TAP 150

typedef struct { const void *vehicle; uint32_t down_at; } HornPress;
static HornPress horn_press[16];
static int horn_next;

/* -> 0 neither, 1 the horn, 2 the siren switched */
int __cdecl cf_siren_horn(const void *vehicle)
{
    uint8_t now = HORN_NOW;
    if (now > 4) return 0;
    int held = HORN_HISTORY[now] != 0, was = HORN_HISTORY[(now + 4) % 5] != 0;
    HornPress *p = NULL;
    for (int i = 0; i < 16 && !p; i++) if (horn_press[i].vehicle == vehicle) p = &horn_press[i];
    if (held && !was) {
        if (!p) p = &horn_press[horn_next], horn_next = (horn_next + 1) % 16, p->vehicle = vehicle;
        p->down_at = GAME_MS;
        return 0;
    }
    if (!p) return held;
    uint32_t since = GAME_MS - p->down_at;
    if (held) return since >= HORN_TAP;
    if (!was) return 0;
    p->vehicle = NULL;
    return since < HORN_TAP ? 2 : 0;
}

/* ------------------------------------------------------------------------ */
/* The hook bodies                                                          */
/* ------------------------------------------------------------------------ */
__asm__(
".intel_syntax noprefix\n"
/* st0 = cf_damping(the rate at `rate`) */
".macro DAMPING rate\n"
"    pushad\n"
"    push dword ptr ds:\\rate\n"
"    call _cf_damping\n"
"    add esp, 4\n"
"    popad\n"
".endm\n"

/* CDoor::Process: each part's damping, loaded for the fmul [esi+14h] after */
"HOOK doordamp1\n"
"    DAMPING 0x872310\n"
"    GO 0x6F4397\n"
"HOOK doordamp2\n"
"    DAMPING 0x872314\n"
"    GO 0x6F43A7\n"
"HOOK doordamp3\n"
"    DAMPING 0x872318\n"
"    GO 0x6F43D0\n"
"HOOK doordamp4\n"
"    DAMPING 0x87230C\n"
"    GO 0x6F43D8\n"

/* CAutomobile::ProcessCarWheelPair: min(1, 3000 / turn mass) times
 * -0.002 * turn mass */
/* CAutomobile::ProcessCarWheelPair's call of CPhysical::ApplyTurnForce(force,
 * point) in a burnout: the turn is point x force, so the force (on the stack
 * past the return address) scales all of it */
".globl _cf_burnout_turn\n"
"_cf_burnout_turn:\n"
"    fld dword ptr [esp+4]\n"
"    SCALED\n"
"    fstp dword ptr [esp+4]\n"
"    fld dword ptr [esp+8]\n"
"    SCALED\n"
"    fstp dword ptr [esp+8]\n"
"    fld dword ptr [esp+12]\n"
"    SCALED\n"
"    fstp dword ptr [esp+12]\n"
"    GO 0x542A50\n"

/* CTaskSimpleSwim::ProcessSwimmingResistance: st0 is the pitch rate at
 * [edi+34h], times 0.95 */
".macro PITCH back\n"
"    fstp st(0)\n"
"    DAMPING 0x858EF0\n"
"    fmul dword ptr [edi+0x34]\n"
"    GO \\back\n"
".endm\n"
"HOOK swimpitch1\n"
"    PITCH 0x68A6C3\n"
"HOOK swimpitch2\n"
"    PITCH 0x68A73B\n"
"HOOK swimpitch3\n"
"    PITCH 0x68A7C6\n"

/* CVehicle::ProcessSirenAndHorn (the vehicle in esi, edi and esi pushed):
 * the game's own ways out */
"HOOK siren\n"
"    push esi\n"
"    call _cf_siren_horn\n"
"    add esp, 4\n"
"    cmp eax, 1\n"
"    je 1f\n"
"    ja 2f\n"
"    GO 0x6E09F7\n"                     /* neither: the horn off */
"1:  GO 0x6E09E8\n"                     /* the horn on */
"2:  GO 0x6E0999\n"                     /* the siren switched, the horn off */
".att_syntax\n");
