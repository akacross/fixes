/* SA-MixSets' fixes (Junior_Djjr's MixSets.cpp / ReadIni.cpp, MIT), for
 * gta_sa.exe 1.0 US; included by crashfix.c after vigilante.c, each switched
 * by its own fixes option. Its byte patches (smooth headlight and shadow
 * movement, the weapon icon, the scroll reload) are rows in fixes.lua.
 * Where they differ from MixSets':
 *   - the wake's brightness multiplier is 2.0, which MixSets reads from its
 *     ini with no default given: day water (timecyc) then reaches the stock
 *     brightness, night water about half of it.
 *   - the second wheel's offset is always 0.45 (no VehFuncs lookup).
 */
void __attribute__((thiscall)) cf_double_skid(void *veh, int32_t state, uint32_t colour, float a, float b,
                                              void *colpoint, float *from, int32_t id, int32_t wheel,
                                              int32_t skid, void *blood, uint32_t flags);

#define MIXSETS(FJ, FP) \
    FP(doubleskid1, "doubleskid", KIND_CALL, "CAutomobile::PreRender: the second rear wheel's skidmark and dust", \
        0x6AB136, 5, "\xE8\x45\x37\x03\x00", (const void *)cf_double_skid) \
    FP(doubleskid2, "doubleskid", KIND_CALL, "CAutomobile::PreRender: the second rear wheel's skidmark and dust", \
        0x6AB1FD, 5, "\xE8\x7E\x36\x03\x00", (const void *)cf_double_skid) \
    FP(doubleskid3, "doubleskid", KIND_CALL, "CAutomobile::PreRender: the second rear wheel's skidmark and dust", \
        0x6AB2B7, 5, "\xE8\xC4\x35\x03\x00", (const void *)cf_double_skid) \
    FJ(boatradio, "boatradio", "CVehicle: a boat's driver keeps the boat pose when the radio changes", 0x6DF4E7, 5, \
        "\x68\x00\x00\x80\x40") \
    FJ(boatfoam, "boatfoam", "CWaterLevel::RenderWakeSegment: a boat's wake as bright as the water's light", \
        0x6EA260, 6, "\x55\x8B\xEC\x83\xE4\xF8")

MIXSETS(FDECLARE_J, FDECLARE_P)

/* CVehicle::AddSingleWheelParticles, thiscall */
typedef void (__attribute__((thiscall)) *WheelParticlesFn)(void *veh, int32_t state, uint32_t colour, float a, float b,
                                                         void *colpoint, float *from, int32_t id, int32_t wheel,
                                                         int32_t skid, void *blood, uint32_t flags);
#define ADD_WHEEL_PARTICLES ((WheelParticlesFn)0x6DE880)

/* A rear wheel of a vehicle with double rear wheels (handling's model flag
 * 0x20000000) gets its pair's skidmark and dust 0.45 to the outside: the
 * contact point and the wheel's position moved along the car's right */
void __attribute__((thiscall)) cf_double_skid(void *veh, int32_t state, uint32_t colour, float a, float b,
                                              void *colpoint, float *from, int32_t id, int32_t wheel,
                                              int32_t skid, void *blood, uint32_t flags)
{
    ADD_WHEEL_PARTICLES(veh, state, colour, a, b, colpoint, from, id, wheel, skid, blood, flags);
    const uint8_t *handling = *(uint8_t **)((uint8_t *)veh + 0x384);
    const float *right = *(float **)((uint8_t *)veh + 0x14);
    if (!handling || !right || !colpoint || !from || !(*(const uint32_t *)(handling + 0xCC) & 0x20000000)) return;
    if (wheel != 1 && wheel != 3 && wheel != 5 && wheel != 6) return;
    float d = (wheel == 3 || wheel == 6) ? -0.45f : 0.45f;
    uint8_t point[0x2C];                            /* a CColPoint, m_vecPoint first */
    float pos[3];
    memcpy(point, colpoint, sizeof point);
    for (int i = 0; i < 3; i++) {
        ((float *)point)[i] += right[i] * d;
        pos[i] = from[i] + right[i] * d;
    }
    ADD_WHEEL_PARTICLES(veh, state, colour, a, b, point, pos, id, wheel + 69696969, skid, blood, flags);
}

/* CWaterLevel::ms_WakePointAlphaMults (0.4, 1, 0.2, 1, 0.4): the outer four
 * follow the water's colour, once a frame; the boatfoam row's removal puts
 * them back */
void cf_boat_foam(void)
{
    static uint32_t done = 0xFFFFFFFFu;
    uint32_t frame = *(const uint32_t *)0xB7CB4C;   /* CTimer::m_FrameCounter */
    if (frame == done) return;
    done = frame;
    const float *water = (const float *)0xB7C508;   /* CTimeCycle::m_CurrentColours' water RGBA */
    float k = (water[0] + water[1] + water[2] + water[3]) / 255.0f / 4.0f;
    k = (k * k + 0.1f) * 2.0f;
    if (k > 1.0f) k = 1.0f;
    float *mult = (float *)0x8D390C;
    mult[0] = mult[4] = 0.4f * k;
    mult[1] = mult[3] = k;
}

void cf_boat_foam_stock(void)
{
    float *mult = (float *)0x8D390C;
    mult[0] = mult[4] = 0.4f;
    mult[1] = mult[3] = 1.0f;
}

__asm__(
".intel_syntax noprefix\n"
/* the radio's change: ecx the driver's clump. In the boat pose (animation
 * 81, drive boat) nothing is blended (eax 0, as when the blend gives none) */
"HOOK boatradio\n"
"    push ecx\n"
"    push 81\n"
"    push ecx\n"
"    mov eax, 0x4D68B0\n"                       /* RpAnimBlendClumpGetAssociation */
"    call eax\n"
"    add esp, 8\n"
"    pop ecx\n"
"    test eax, eax\n"
"    jz boatradio_tune\n"
"    xor eax, eax\n"
"    GO 0x6DF4FC\n"
"boatradio_tune:\n"
"    push 0x40800000\n"
"    GO 0x6DF4EC\n"
/* RenderWakeSegment's start, the FPU empty */
"HOOK boatfoam\n"
"    pushad\n"
"    call _cf_boat_foam\n"
"    popad\n"
"    push ebp\n"
"    mov ebp, esp\n"
"    and esp, 0xFFFFFFF8\n"
"    GO 0x6EA266\n"
".att_syntax\n"
);
