/* MTA:SA's frame-rate fixes (multitheftauto/mtasa-blue,
 * CMultiplayerSA_FrameRateFixes.cpp, GPLv3), for gta_sa.exe 1.0 US; included
 * by crashfix.c, so they install, stand down and log like its hooks, each
 * switched by its own fixes option (docs/MTA_FRAMERATE_FIXES.md).
 *
 * Physics the game runs once per step is scaled by timestep / 1.6667 (stock
 * at 30 fps); effects it spawns once per frame wait for a 30 Hz tick of our
 * own. Where they differ from MTA's:
 *   - the tick leaves CTimer::m_FrameCounter alone (ProperShaders reads it);
 *     the water cannon's push reads our 30 Hz count instead.
 *   - each gate keeps the flags and every register.
 *   - the helicopter's two rates and the swinging chassis's two impulses are
 *     hooked at their constants: 0x6F4354 jumps into the middle of MTA's
 *     chassis hook.
 *   - the bike's clamp takes the `!idle` path too, which MTA's NOPs drop.
 *   - the stillness threshold scales the game's own 0.005, stock at 30 fps.
 *   - the aiming threshold is compared from memory, the last x87 op.
 *   - CWeapon::Update's shot timing is not taken: it shortens every weapon's
 *     firing state at high fps.
 *   - the climb's get-to-position timer (0x68131B) keeps the real timestep:
 *     with MTA's constant it ran out after 30 frames (0.1 s at 300 fps), and
 *     the climb aborted and pushed the ped back.
 *   - every swinging part's angle scales, not the chassis's alone, as
 *     FramerateVigilante's does; its dampings are in vigilante.c.
 */

/* ------------------------------------------------------------------------ */
/* The rows: key, option, what, site, bytes, stock (FJ: a jmp to cf_hook_key; */
/* FP: NOPs, a pointer to `to`, or a call to `to`)                           */
/* ------------------------------------------------------------------------ */
#define TS "\x5C\xCB\xB7\x00"                    /* &CTimer::ms_fTimeStep */
#define STILL "\x4C\x8B\x85\x00"                 /* &0.005f */
#define FRAMERATE(FJ, FP) \
    FJ(turndamping, "turnfps", "CPhysical::ApplyAirResistance: the turn speed's 0.99 per step", 0x544D29, 5, \
        "\xD9\x46\x50\xD8\x0D") \
    FP(climb1, "pedfps", KIND_PTR, "CTaskSimpleClimb: the pull to the hold at 30 fps speed (MTA #602)", 0x6811E9, 4, TS, &cf_k30) \
    FP(climb2, "pedfps", KIND_PTR, "CTaskSimpleClimb: the pull to the hold at 30 fps speed", 0x68128A, 4, TS, &cf_k30) \
    FJ(aimwalk, "pedfps", "CTaskSimpleUseGun::SetMoveAnim: the move threshold scaled", 0x61E4F2, 6, \
        "\xD8\x15\x1C\x8B\x85\x00") \
    FP(aimreset, "pedfps", KIND_NOP, "CTaskSimpleUseGun::SetMoveAnim: the move command kept", 0x61E5E4, 6, \
        "\x89\x58\x14\x89\x58\x18", NULL) \
    FP(boatcontrol, "vehiclefps", KIND_PTR, "CVehicle::ProcessBoatControl: its timestep a constant", 0x6DC23F, 4, TS, &cf_k30) \
    FJ(boatwater, "vehiclefps", "CVehicle::ApplyBoatWaterResistance scaled", 0x6D2771, 6, "\xD8\x0D\xDC\x1D\x87\x00") \
    FJ(heli1, "vehiclefps", "CHeli::ProcessFlyingCarStuff: the takeoff rate scaled", 0x6C4F29, 6, "\xD8\x05\xDC\x8C\x85\x00") \
    FJ(heli2, "vehiclefps", "CHeli::ProcessFlyingCarStuff: the RC takeoff rate scaled", 0x6C4F37, 6, "\xD8\x05\xD8\x9C\x85\x00") \
    FJ(chassis1, "vehiclefps", "CDoor::Process: a swinging chassis's impulse scaled", 0x6F42D5, 6, "\xD9\x05\x28\x23\x87\x00") \
    FJ(chassis2, "vehiclefps", "CDoor::Process: a driverless chassis's impulse scaled", 0x6F434E, 6, "\xD9\x05\x2C\x23\x87\x00") \
    FJ(doorangle, "vehiclefps", "CDoor::Process: a swinging part's angle by the timestep", 0x6F4422, 8, \
        "\xD9\x46\x14\x8B\xCB\xD8\x46\x0C") \
    FJ(carsleep, "vehiclefps", "CAutomobile::ProcessControl: still frames before sleep scaled", 0x6B1D34, 10, \
        "\xB3\x0A\x3A\xC3\x88\x96\xB8\x00\x00\x00") \
    FJ(bikesleep, "vehiclefps", "CBike::ProcessControl: still frames before sleep scaled", 0x6B997C, 8, \
        "\x3C\x0A\x88\x8E\xB8\x00\x00\x00") \
    FJ(bikeclamp, "vehiclefps", "CBike::ProcessControl: the still-frame clamp scaled", 0x6B99BD, 24, \
        "\x8A\x44\x24\x27\x84\xC0\x74\x09\x80\xBE\xB8\x00\x00\x00\x0A\x76\x07\xC6\x86\xB8\x00\x00\x00\x0A") \
    FP(carsettle, "vehiclefps", KIND_NOP, "CAutomobile::ProcessControl: no sleep on its first frame", 0x6B1AF5, 5, \
        "\xC6\x44\x24\x2B\x01", NULL) \
    FP(bikesettle, "vehiclefps", KIND_NOP, "CBike::ProcessControl: no sleep on its first frame", 0x6B9850, 5, \
        "\xC6\x44\x24\x27\x01", NULL) \
    FP(carfall, "vehiclefps", KIND_NOP, "CAutomobile: the still damping keeps the fall speed", 0x6B361C, 3, "\x89\x43\x08", NULL) \
    FP(bikefall, "vehiclefps", KIND_NOP, "CBike: the still damping keeps the fall speed", 0x6BC18F, 3, "\x89\x42\x08", NULL) \
    FP(still1, "vehiclefps", KIND_PTR, "CAutomobile: the stillness threshold scaled", 0x6B33F8, 4, STILL, &cf_still) \
    FP(still2, "vehiclefps", KIND_PTR, "CAutomobile: the stillness threshold scaled", 0x6B340E, 4, STILL, &cf_still) \
    FP(still3, "vehiclefps", KIND_PTR, "CAutomobile: the stillness threshold scaled", 0x6B3424, 4, STILL, &cf_still) \
    FP(still4, "vehiclefps", KIND_PTR, "CBike: the stillness threshold scaled", 0x6BC103, 4, STILL, &cf_still) \
    FP(still5, "vehiclefps", KIND_PTR, "CBike: the stillness threshold scaled", 0x6BC119, 4, STILL, &cf_still) \
    FP(still6, "vehiclefps", KIND_PTR, "CBike: the stillness threshold scaled", 0x6BC12B, 4, STILL, &cf_still) \
    FJ(tick, "effectsfps", "CTimer::Update: a 30 Hz tick for the effects", 0x561C5D, 14, \
        "\xA1\x4C\xCB\xB7\x00\x83\xC4\x04\x40\xA3\x4C\xCB\xB7\x00") \
    FJ(shake, "effectsfps", "CCamera::Process: the shake scaled", 0x52C723, 18, \
        "\xD9\x05\x30\xEC\xB6\x00\xD8\x05\x80\x8C\x85\x00\xD9\x1D\x30\xEC\xB6\x00") \
    FJ(movingfog, "effectsfps", "CClouds::MovingFog_Update scaled", 0x716BA6, 22, \
        "\xD8\x0C\xBD\x94\xE3\xC6\x00\xD8\x06\xD9\x1E\xD9\x44\x24\x18\xD8\x0C\xBD\x94\xE3\xC6\x00") \
    FJ(glass1, "effectsfps", "CFallingGlassPane::Update: the speed scaled", 0x71AABF, 6, "\xD9\x44\x24\x20\xD8\x06") \
    FJ(glass2, "effectsfps", "CFallingGlassPane::Update: the spin scaled", 0x71AAEA, 6, "\x8B\x08\x89\x4C\x24\x2C") \
    FJ(glass3, "effectsfps", "CFallingGlassPane::Update: the spin scaled", 0x71AB29, 6, "\x8B\x10\x89\x54\x24\x38") \
    FJ(breakobj, "effectsfps", "BreakObject_c::Update: its countdown on the tick", 0x59E420, 11, \
        "\x8B\x54\x07\x70\x8D\x44\x07\x70\x4A\x89\x10") \
    FJ(cannon, "effectsfps", "CWaterCannon::Update_OncePerFrame on the tick (its reach)", 0x72A29B, 5, "\x0F\xBF\x47\x04\x40") \
    FJ(cannonpush, "effectsfps", "CWaterCannon: peds pushed on the tick's count", 0x72A37B, 6, "\x8B\x0D\x4C\xCB\xB7\x00") \
    FJ(cannonfx, "effectsfps", "CWaterCannon::Render: its particles on the tick", 0x729437, 5, "\xEB\x07\x8D\xA4\x24") \
    FJ(money, "effectsfps", "CPlayerInfo::Process: the money counter on the tick", 0x5700F5, 6, "\x8B\x96\xBC\x00\x00\x00") \
    FJ(rocket, "effectsfps", "CProjectileInfo::Update: rocket smoke on the tick", 0x738C63, 5, "\x8B\x03\x83\xF8\x13") \
    FJ(wheeldirt, "effectsfps", "CVehicle::AddWheelDirtAndWater on the tick", 0x6D2D50, 6, "\x8B\x44\x24\x08\x85\xC0") \
    FJ(planesmoke, "effectsfps", "CPlane::PreRender: smoke trails on the tick", 0x6CA937, 6, "\x8A\x86\x00\x0A\x00\x00") \
    FJ(boatsplash1, "effectsfps", "cBuoyancy::AddSplashParticles on the tick", 0x6C34E0, 6, "\x90\xE9\x9D\xE7\xD3\xFF") \
    FJ(rain, "effectsfps", "CWeather::AddRain on the tick", 0x72AAA8, 6, "\xD9\x05\xF0\x12\xC8\x00") \
    FJ(planedamage, "effectsfps", "CPlane::ProcessFlyingCarStuff: damage particles on the tick", 0x6CBE4B, 6, \
        "\x8B\x15\x3C\xF0\xB6\x00") \
    FJ(wheelsand, "effectsfps", "CAutomobile::UpdateWheelMatrix: sand and water on the tick", 0x6AA78A, 5, \
        "\x68\xCD\xCC\x4C\x3D") \
    FJ(boatsplash2, "effectsfps", "CVehicle::DoBoatSplashes on the tick", 0x6DD130, 6, "\x81\xEC\x80\x00\x00\x00") \
    FJ(rainsplash, "effectsfps", "CVehicle::AddWaterSplashParticles on the tick", 0x6DDF60, 6, "\x81\xEC\xC4\x00\x00\x00") \
    FJ(planecontrol, "effectsfps", "CPlane::ProcessControl: damage particles on the tick", 0x6C939A, 5, "\x8D\x4C\x24\x3C\x51") \
    FJ(exhaust, "effectsfps", "CVehicle::AddExhaustParticles on the tick", 0x6DE240, 6, "\x64\xA1\x00\x00\x00\x00") \
    FJ(swimfx, "effectsfps", "CTaskSimpleSwim::ProcessEffects on the tick", 0x68AD3B, 6, "\x8B\x47\x14\x83\xC0\x10") \
    FJ(bubbles, "effectsfps", "CTaskSimpleSwim::ProcessEffects: bubbles on the tick", 0x68AC31, 7, \
        "\x8B\xCF\xBE\x05\x00\x00\x00") \
    FP(push1, "pushfps", KIND_CALL, "CPhysical::ApplyCollision: a ped's push on a parked car capped", 0x54BDB2, 5, \
        "\xE8\xC9\xC8\xFF\xFF", cf_apply_collision) \
    FP(push2, "pushfps", KIND_CALL, "CPhysical::ApplyCollision: a ped's push capped", 0x54BF78, 5, "\xE8\x03\xC7\xFF\xFF", \
        cf_apply_collision) \
    FP(push3, "pushfps", KIND_CALL, "CPhysical::ApplyCollision: a ped's push capped", 0x54C23A, 5, "\xE8\x41\xC4\xFF\xFF", \
        cf_apply_collision) \
    FP(push4, "pushfps", KIND_CALL, "CPhysical::ApplyCollision: a ped's push capped", 0x54C435, 5, "\xE8\x46\xC2\xFF\xFF", \
        cf_apply_collision) \
    FP(push5, "pushfps", KIND_CALL, "CPhysical::ApplyCollision: a ped's push capped", 0x54D17E, 5, "\xE8\xFD\xB4\xFF\xFF", \
        cf_apply_collision) \
    FP(push6, "pushfps", KIND_CALL, "CPhysical::ApplyCollision: a ped's push capped", 0x54D27E, 5, "\xE8\xFD\xB3\xFF\xFF", \
        cf_apply_collision) \
    FP(push7, "pushfps", KIND_CALL, "CPhysical::ApplyCollision: a ped's push capped", 0x54D3FE, 5, "\xE8\x7D\xB2\xFF\xFF", \
        cf_apply_collision) \
    FP(push8, "pushfps", KIND_CALL, "CPhysical::ApplyCollision: a ped's push capped", 0x54D4D2, 5, "\xE8\xA9\xB1\xFF\xFF", \
        cf_apply_collision) \
    FP(pushalt1, "pushfps", KIND_CALL, "CPhysical::ApplyCollision (the other one): a ped's push capped", 0x54C9FA, 5, \
        "\xE8\x51\x83\xFF\xFF", cf_apply_collision_alt) \
    FP(pushalt2, "pushfps", KIND_CALL, "CPhysical::ApplyCollision (the other one): a ped's push capped", 0x54CAC2, 5, \
        "\xE8\x89\x82\xFF\xFF", cf_apply_collision_alt)

#define FDECLARE_J(k, option, name, va, len, stock) void cf_hook_##k(void); volatile LONG cf_n_##k;
#define FDECLARE_P(k, option, kind, name, va, len, stock, to) volatile LONG cf_n_##k;
FRAMERATE(FDECLARE_J, FDECLARE_P)

/* ------------------------------------------------------------------------ */
/* Constants and the tick                                                   */
/* ------------------------------------------------------------------------ */
#define TIMESTEP (*(const float *)0xB7CB5C)     /* CTimer::ms_fTimeStep */
#define GAME_MS (*(const uint32_t *)0xB7CB84)   /* CTimer::m_snTimeInMilliseconds */

float cf_k30 = 50.0f / 30.0f;           /* the timestep at 30 fps */
float cf_still = 0.005f;                /* the stillness threshold, scaled */
volatile int cf_frame_due = 1;          /* a 30 Hz frame: the effects run (1 without the tick) */
uint32_t cf_frame30;                    /* 30 Hz frames counted */
static uint32_t frame30_at;

/* From CTimer::Update, once a frame */
void cf_tick(void)
{
    uint32_t since = GAME_MS - frame30_at;
    if (since < 33) {
        cf_frame_due = 0;
        return;
    }
    frame30_at = since >= 66 ? GAME_MS : frame30_at + 33;
    cf_frame_due = 1;
    cf_frame30++;
}

/* The still frames a vehicle needs before it sleeps: GTA's 10 at 30 fps,
 * the same time at any other (10..240); the stillness threshold with it. */
uint8_t __cdecl cf_sleep_frames(void)
{
    float t = TIMESTEP;
    if (!(t > 0.0001f)) return 10;
    cf_still = *(const float *)0x858B4C * (t / cf_k30);
    float n = roundf(10.0f * (cf_k30 / t));
    return n < 10.0f ? 10 : n > 240.0f ? 240 : (uint8_t)n;
}

/* CPhysical::ApplyAirResistance's turn damping: 0.99 per 30 fps step */
void cf_turn_damping(float *turn)
{
    float f = powf(*(const float *)0x862CD0, TIMESTEP / cf_k30);
    turn[0] *= f, turn[1] *= f, turn[2] *= f;
}

/* ------------------------------------------------------------------------ */
/* A ped pushing a parked car (pushfps)                                     */
/* ------------------------------------------------------------------------ */
/* MTA's governor: CPhysical::ApplyCollision adds an impulse on every contact
 * frame, so at high fps a ped walking into an unoccupied car shoves it away.
 * The push is held to what momentum allows, tilt and lift are kept out. */
#define F(p, off) (*(float *)((uint8_t *)(p) + (off)))
#define TYPE(p) (*((uint8_t *)(p) + 0x36) & 7)  /* 2 vehicle, 3 ped */

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static void govern_push(uint8_t *vehicle, uint8_t *ped, const float *v0, const float *w0, const float *pedV, float *v, float *w)
{
    if (*(void **)(vehicle + 0x460)) return;                 /* a driver */
    w[0] = w0[0], w[1] = w0[1];
    v[2] = v0[2];
    float dx = v[0] - v0[0], dy = v[1] - v0[1], d2 = dx * dx + dy * dy;
    if (d2 <= 0.000001f) return;
    float d = sqrtf(d2), px = dx / d, py = dy / d;
    float pedSpeed = pedV[0] * px + pedV[1] * py;
    if (pedSpeed <= 0.0f) {
        memcpy(v, v0, 12);
        memcpy(w, w0, 12);
        return;
    }
    float ratio = clampf(TIMESTEP, 0.001f, 1.0f);
    float pedMass = F(ped, 0x8C) * 10.0f, mass = F(vehicle, 0x8C);
    float maxPush = pedSpeed * (pedMass / (pedMass + mass));
    float fx = 0.0f, fy = 1.0f, rx = 1.0f, ry = 0.0f;
    uint8_t *m = *(uint8_t **)(vehicle + 0x14);
    if (m) {
        fx = F(m, 0x10), fy = F(m, 0x14), rx = F(m, 0x00), ry = F(m, 0x04);
    } else {
        float h = F(vehicle, 0x10);
        fx = -sinf(h), fy = cosf(h), rx = cosf(h), ry = sinf(h);
    }
    float len = sqrtf(fx * fx + fy * fy);
    if (len > 0.0001f) fx /= len, fy /= len;
    len = sqrtf(rx * rx + ry * ry);
    if (len > 0.0001f) rx /= len, ry /= len;
    float f0 = v0[0] * fx + v0[1] * fy, l0 = v0[0] * rx + v0[1] * ry;
    float f1 = v[0] * fx + v[1] * fy, l1 = v[0] * rx + v[1] * ry;
    float df = f1 - f0;
    f1 = f0 + df * ratio;
    if (fabsf(f0) < 0.001f && fabsf(f1) < 0.008f && fabsf(df) > 0.001f) f1 = df > 0.0f ? 0.008f : -0.008f;
    f1 = clampf(f1, fminf(f0, -maxPush), fmaxf(f0, maxPush));
    l1 = l0 + (l1 - l0) * ratio;
    float maxSide = maxPush * 0.10f;
    l1 = clampf(l1, fminf(l0, -maxSide), fmaxf(l0, maxSide));
    v[0] = f1 * fx + l1 * rx;
    v[1] = f1 * fy + l1 * ry;
    w[2] = w0[2] + (w[2] - w0[2]) * ratio;
    float turnMass = F(vehicle, 0x90) > 0.0f ? F(vehicle, 0x90) : mass * 2.5f;
    float maxTurn = (2.2f * pedMass * pedSpeed) / (turnMass + pedMass * 2.2f * 2.2f);
    w[2] = clampf(w[2], fminf(w0[2], -maxTurn), fmaxf(w0[2], maxTurn));
}

typedef int (__attribute__((thiscall)) *ApplyCollisionFn)(void *self, void *other, void *colPoint, float *a, float *b);
typedef int (__attribute__((thiscall)) *ApplyCollisionAltFn)(void *self, void *other, void *colPoint, float *a, float *v,
                                                               float *w);

/* In place of the call to CPhysical::ApplyCollision (0x548680, thiscall) */
int __attribute__((fastcall)) cf_apply_collision(uint8_t *self, void *edx, uint8_t *other, void *colPoint, float *a, float *b)
{
    (void)edx;
    float v0[3] = {0}, w0[3] = {0}, ov0[3] = {0}, ow0[3] = {0};
    if (self) memcpy(v0, self + 0x44, 12), memcpy(w0, self + 0x50, 12);
    if (other) memcpy(ov0, other + 0x44, 12), memcpy(ow0, other + 0x50, 12);
    int hit = ((ApplyCollisionFn)0x548680)(self, other, colPoint, a, b) & 0xFF;
    if (hit && self && other) {
        if (TYPE(self) == 2 && TYPE(other) == 3)
            govern_push(self, other, v0, w0, ov0, (float *)(self + 0x44), (float *)(self + 0x50));
        else if (TYPE(self) == 3 && TYPE(other) == 2)
            govern_push(other, self, ov0, ow0, v0, (float *)(other + 0x44), (float *)(other + 0x50));
    }
    return hit;
}

/* In place of the call to the other CPhysical::ApplyCollision (0x544D50),
 * which writes the speeds to v and w */
int __attribute__((fastcall)) cf_apply_collision_alt(uint8_t *self, void *edx, uint8_t *other, void *colPoint, float *a,
                                                    float *v, float *w)
{
    (void)edx;
    float v0[3] = {0}, w0[3] = {0}, ov[3] = {0};
    if (v) memcpy(v0, v, 12);
    if (w) memcpy(w0, w, 12);
    if (other) memcpy(ov, other + 0x44, 12);
    int hit = ((ApplyCollisionAltFn)0x544D50)(self, other, colPoint, a, v, w) & 0xFF;
    if (hit && self && other && v && w && TYPE(self) == 2 && TYPE(other) == 3) govern_push(self, other, v0, w0, ov, v, w);
    return hit;
}
#undef F
#undef TYPE

/* ------------------------------------------------------------------------ */
/* The hook bodies: physics                                                 */
/* ------------------------------------------------------------------------ */
__asm__(
".intel_syntax noprefix\n"
".macro SCALED\n"                               /* st0 *= timestep / 1.6667 */
"    fmul dword ptr ds:0xB7CB5C\n"
"    fdiv dword ptr [_cf_k30]\n"
".endm\n"
".data\n"
"aim_threshold: .long 0\n"
".text\n"

/* CPhysical::ApplyAirResistance (this in esi): the turn speed at +50h */
"HOOK turndamping\n"
"    pushad\n"
"    lea eax, [esi+0x50]\n"
"    push eax\n"
"    call _cf_turn_damping\n"
"    add esp, 4\n"
"    popad\n"
"    GO 0x544D4D\n"

/* CTaskSimpleUseGun::SetMoveAnim: the move amount (st0) against 0.1 scaled */
"HOOK aimwalk\n"
"    fld dword ptr ds:0xB7CB5C\n"
"    fdiv dword ptr [_cf_k30]\n"
"    fmul dword ptr ds:0x858B1C\n"
"    fstp dword ptr [aim_threshold]\n"
"    fcom dword ptr [aim_threshold]\n"
"    GO 0x61E4F8\n"

/* CVehicle::ApplyBoatWaterResistance */
"HOOK boatwater\n"
"    fmul dword ptr ds:0x871DDC\n"
"    SCALED\n"
"    GO 0x6D2777\n"

/* CHeli::ProcessFlyingCarStuff: the takeoff rate, then the RC helis' */
"HOOK heli1\n"
"    fld dword ptr ds:0x858CDC\n"
"    SCALED\n"
"    faddp st(1), st\n"
"    GO 0x6C4F2F\n"
"HOOK heli2\n"
"    fld dword ptr ds:0x859CD8\n"
"    SCALED\n"
"    faddp st(1), st\n"
"    GO 0x6C4F3D\n"

/* CDoor::Process: the chassis impulse, with a driver and without; any
 * part's angle */
"HOOK chassis1\n"
"    fld dword ptr ds:0x872328\n"
"    SCALED\n"
"    GO 0x6F42DB\n"
"HOOK chassis2\n"
"    fld dword ptr ds:0x87232C\n"
"    SCALED\n"
"    GO 0x6F4354\n"
"HOOK doorangle\n"
"    fld dword ptr [esi+0x14]\n"
"    mov ecx, ebx\n"
"    SCALED\n"
"    fadd dword ptr [esi+0x0C]\n"
"    GO 0x6F442A\n"

/* still frames before a car or bike sleeps (the count in dl or cl, +B8h) */
"HOOK carsleep\n"
"    push ecx\n"
"    push edx\n"
"    call _cf_sleep_frames\n"
"    mov bl, al\n"
"    pop edx\n"
"    pop ecx\n"
"    mov al, dl\n"
"    cmp al, bl\n"
"    mov byte ptr [esi+0xB8], dl\n"
"    GO 0x6B1D3E\n"
"HOOK bikesleep\n"
"    push ecx\n"
"    push edx\n"
"    call _cf_sleep_frames\n"
"    pop edx\n"
"    pop ecx\n"
"    cmp cl, al\n"
"    mov byte ptr [esi+0xB8], cl\n"
"    GO 0x6B9984\n"
/* if (!idle || count > limit) count = limit */
"HOOK bikeclamp\n"
"    push ecx\n"
"    push edx\n"
"    call _cf_sleep_frames\n"
"    pop edx\n"
"    pop ecx\n"
"    cmp byte ptr [esp+0x27], 0\n"
"    je 1f\n"
"    cmp byte ptr [esi+0xB8], al\n"
"    jbe 2f\n"
"1:  mov byte ptr [esi+0xB8], al\n"
"2:  GO 0x6B99D5\n"

/* CTimer::Update: the frame counter as stock, then the 30 Hz tick */
"HOOK tick\n"
"    mov eax, dword ptr ds:0xB7CB4C\n"
"    add esp, 4\n"
"    inc eax\n"
"    mov dword ptr ds:0xB7CB4C, eax\n"
"    pushad\n"
"    call _cf_tick\n"
"    popad\n"
"    GO 0x561C6B\n"

/* CCamera::Process: the shake phase */
"HOOK shake\n"
"    fld dword ptr ds:0x858C80\n"
"    SCALED\n"
"    fadd dword ptr ds:0xB6EC30\n"
"    fstp dword ptr ds:0xB6EC30\n"
"    GO 0x52C735\n"

/* CClouds::MovingFog_Update: both steps of a patch */
"HOOK movingfog\n"
"    fmul dword ptr [edi*4+0xC6E394]\n"
"    SCALED\n"
"    fadd dword ptr [esi]\n"
"    fstp dword ptr [esi]\n"
"    fld dword ptr [esp+0x18]\n"
"    fmul dword ptr [edi*4+0xC6E394]\n"
"    SCALED\n"
"    GO 0x716BBC\n"

/* CFallingGlassPane::Update: the step at esp+20h, then the spins at eax */
"HOOK glass1\n"
"    fld dword ptr [esp+0x28]\n"
"    SCALED\n"
"    fstp dword ptr [esp+0x28]\n"
"    fld dword ptr [esp+0x24]\n"
"    SCALED\n"
"    fstp dword ptr [esp+0x24]\n"
"    fld dword ptr [esp+0x20]\n"
"    SCALED\n"
"    fadd dword ptr [esi]\n"
"    GO 0x71AAC5\n"
".macro SPIN\n"
"    fld dword ptr [eax]\n"
"    SCALED\n"
"    fstp dword ptr [eax]\n"
"    fld dword ptr [eax+4]\n"
"    SCALED\n"
"    fstp dword ptr [eax+4]\n"
"    fld dword ptr [eax+8]\n"
"    SCALED\n"
"    fstp dword ptr [eax+8]\n"
".endm\n"
"HOOK glass2\n"
"    SPIN\n"
"    mov ecx, dword ptr [eax]\n"
"    mov dword ptr [esp+0x2C], ecx\n"
"    GO 0x71AAF0\n"
"HOOK glass3\n"
"    SPIN\n"
"    mov edx, dword ptr [eax]\n"
"    mov dword ptr [esp+0x38], edx\n"
"    GO 0x71AB2F\n"
".att_syntax\n");

/* ------------------------------------------------------------------------ */
/* The hook bodies: effects on the 30 Hz tick                               */
/* ------------------------------------------------------------------------ */
/* Each gate: on a due frame the stolen bytes and on, else the game's own way
 * past the effect (9:). The flags and every register are kept. */
__asm__(
".intel_syntax noprefix\n"
".macro DUE\n"
"    pushfd\n"
"    cmp dword ptr [_cf_frame_due], 0\n"
"    je 9f\n"
"    popfd\n"
".endm\n"

"HOOK breakobj\n"
"    DUE\n"
"    mov edx, dword ptr [edi+eax+0x70]\n"
"    lea eax, [edi+eax+0x70]\n"
"    dec edx\n"
"    mov dword ptr [eax], edx\n"
"    GO 0x59E42B\n"
"9:  popfd\n"
"    GO 0x59E42B\n"

"HOOK cannon\n"
"    DUE\n"
"    movsx eax, word ptr [edi+4]\n"
"    inc eax\n"
"    GO 0x72A2A0\n"
"9:  popfd\n"
"    GO 0x72A2BB\n"

/* every fourth 30 Hz frame, as the game's counter at 30 fps */
"HOOK cannonpush\n"
"    DUE\n"
"    mov ecx, dword ptr [_cf_frame30]\n"
"    GO 0x72A381\n"
"9:  popfd\n"
"    GO 0x72A38E\n"

"HOOK cannonfx\n"
"    DUE\n"
"    GO 0x729440\n"
"9:  popfd\n"
"    GO 0x7294EE\n"

"HOOK money\n"
"    DUE\n"
"    mov edx, dword ptr [esi+0xBC]\n"
"    GO 0x5700FB\n"
"9:  popfd\n"
"    GO 0x57015B\n"

"HOOK rocket\n"
"    DUE\n"
"    mov eax, dword ptr [ebx]\n"
"    cmp eax, 0x13\n"
"    GO 0x738C68\n"
"9:  popfd\n"
"    GO 0x738F22\n"

/* the function's entry: on a skipped frame it returns 0 (thiscall, 10h) */
"HOOK wheeldirt\n"
"    DUE\n"
"    mov eax, dword ptr [esp+8]\n"
"    test eax, eax\n"
"    GO 0x6D2D56\n"
"9:  popfd\n"
"    xor eax, eax\n"
"    ret 0x10\n"

"HOOK planesmoke\n"
"    DUE\n"
"    mov al, byte ptr [esi+0xA00]\n"
"    GO 0x6CA93D\n"
"9:  popfd\n"
"    GO 0x6CAA93\n"

/* HOODLUM keeps the entry's sub esp,0E0h in a stub at 0x401C83 */
"HOOK boatsplash1\n"
"    DUE\n"
"    sub esp, 0xE0\n"
"    GO 0x6C34E6\n"
"9:  popfd\n"
"    ret 0x2C\n"

/* 0x72AF5E: the function's own add esp,84h / ret */
"HOOK rain\n"
"    DUE\n"
"    fld dword ptr ds:0xC812F0\n"
"    GO 0x72AAAE\n"
"9:  popfd\n"
"    GO 0x72AF5E\n"

"HOOK planedamage\n"
"    DUE\n"
"    mov edx, dword ptr ds:0xB6F03C\n"
"    GO 0x6CBE51\n"
"9:  popfd\n"
"    GO 0x6CC0D9\n"

"HOOK wheelsand\n"
"    DUE\n"
"    push 0x3D4CCCCD\n"
"    GO 0x6AA78F\n"
"9:  popfd\n"
"    GO 0x6AAAD0\n"

"HOOK boatsplash2\n"
"    DUE\n"
"    sub esp, 0x80\n"
"    GO 0x6DD136\n"
"9:  popfd\n"
"    ret 4\n"

"HOOK rainsplash\n"
"    DUE\n"
"    sub esp, 0xC4\n"
"    GO 0x6DDF66\n"
"9:  popfd\n"
"    ret\n"

"HOOK planecontrol\n"
"    DUE\n"
"    lea ecx, [esp+0x3C]\n"
"    push ecx\n"
"    GO 0x6C939F\n"
"9:  popfd\n"
"    GO 0x6C9463\n"

"HOOK exhaust\n"
"    DUE\n"
"    mov eax, dword ptr fs:0\n"
"    GO 0x6DE246\n"
"9:  popfd\n"
"    ret\n"

"HOOK swimfx\n"
"    DUE\n"
"    mov eax, dword ptr [edi+0x14]\n"
"    add eax, 0x10\n"
"    GO 0x68AD41\n"
"9:  popfd\n"
"    GO 0x68AFDB\n"

"HOOK bubbles\n"
"    DUE\n"
"    mov ecx, edi\n"
"    mov esi, 5\n"
"    GO 0x68AC38\n"
"9:  popfd\n"
"    GO 0x68AD36\n"
".att_syntax\n");
