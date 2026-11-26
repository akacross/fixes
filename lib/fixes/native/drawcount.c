/* Draw calls by what is being drawn (fixes.lua's drawcount option, a
 * diagnostic), for gta_sa.exe 1.0 US; included by crashfix.c after stream.c.
 *
 * The exe draws from some 250 sites through the device's vtable, so the count
 * is taken there: slots 81-84 of the IDirect3DDevice9 at 0xC97C28
 * (DrawPrimitive, DrawIndexedPrimitive and their UP forms). That also counts
 * SA-MP's and the overlays' draws on the same device. Each draw goes to the
 * category the game's thread is in: CRenderer::RenderRoads as a whole, an
 * entity's type inside RenderOneNonRoad and CVisibilityPlugins::RenderEntity
 * (the alpha lists), "other" anywhere else (sky, water, effects, shadows, the
 * HUD, SA-MP, the overlays). The scopes are timed as stream.c times; the
 * outer one of a nested pair has the time. Integers only.
 *
 * The frame's phases are timed the same way, each a wrapper around one of the
 * functions Idle (0x53E920) calls, at its entry or, where another mod holds
 * that (urp-client.dll does six), at the call of it: CGame::Process (and inside it
 * CWorld::Process and CStreaming::Update), CRenderer::ConstructRenderList and
 * PreRender, RenderScene, RenderEffects, Render2dStuff and RwCameraShowRaster
 * (the present, with whatever hooks it). What they leave of a frame is the
 * rest: the message loop, audio, SA-MP's own work, the menus and fonts.
 * Inside CWorld::Process, its passes over the moving entities are timed a
 * call each: UpdateAnim, ProcessControl and ProcessCollision by the entity's
 * type, ProcessShift. An object's are also summed by its type, attachment and
 * movement, and its collision by model.
 */
#define DRAWCOUNT(FJ, FP) \
    FJ(drawnonroad, "drawcount", "CRenderer::RenderOneNonRoad, counted", 0x553260, 5, "\x56\x8B\x74\x24\x08") \
    FJ(drawentity, "drawcount", "CVisibilityPlugins::RenderEntity, counted", 0x732B40, 5, "\x56\x8B\x74\x24\x08") \
    FJ(drawroads, "drawcount", "CRenderer::RenderRoads, counted", 0x553A10, 5, "\xA1\x24\x7B\xC9\x00") \
    FP(phprocess, "drawcount", KIND_CALL, "Idle's call of CGame::Process, timed", 0x53E981, 5, \
        "\xE8\x5A\xD5\xFF\xFF", cf_ph_process) \
    FJ(phworld, "drawcount", "CWorld::Process, timed", 0x5684A0, 7, "\x56\x8B\x35\xC8\xAC\xB9\x00") \
    FP(phstreaming, "drawcount", KIND_CALL, "CGame::Process' call of CStreaming::Update, timed", 0x53BF0B, 5, \
        "\xE8\x60\x27\xED\xFF", cf_ph_streaming) \
    FP(phlist, "drawcount", KIND_CALL, "Idle's call of CRenderer::ConstructRenderList, timed", 0x53E9F9, 5, \
        "\xE8\xE2\x6C\x01\x00", cf_ph_list) \
    FJ(phprerender, "drawcount", "CRenderer::PreRender, timed", 0x553910, 5, "\xA1\x40\x68\xB7\x00") \
    FP(phscene, "drawcount", KIND_CALL, "Idle's call of RenderScene, timed", 0x53EABF, 5, \
        "\xE8\x7C\xF4\xFF\xFF", cf_ph_scene) \
    FP(pheffects, "drawcount", KIND_CALL, "Idle's call of RenderEffects, timed", 0x53EAD3, 5, \
        "\xE8\x98\xF6\xFF\xFF", cf_ph_effects) \
    FP(phhud, "drawcount", KIND_CALL, "Idle's call of Render2dStuff, timed", 0x53EB12, 5, \
        "\xE8\x19\xF7\xFF\xFF", cf_ph_hud) \
    FJ(phpresent, "drawcount", "RwCameraShowRaster, timed", 0x7EE370, 8, "\x8B\x44\x24\x0C\x8B\x4C\x24\x08") \
    FP(wanim, "drawcount", KIND_CALL, "CWorld::Process: an entity's UpdateAnim, timed", 0x5685BB, 5, \
        "\xE8\x40\xD9\xFC\xFF", cf_w_anim_stub) \
    FP(wcontrol1, "drawcount", KIND_CALL, "CWorld::Process: an entity's ProcessControl, timed", 0x56863B, 7, \
        "\x8B\x06\x8B\xCE\xFF\x50\x28", cf_w_control_stub) \
    FP(wcontrol2, "drawcount", KIND_CALL, "CWorld::Process: a postponed entity's ProcessControl, timed", 0x5686ED, 7, \
        "\x8B\x16\x8B\xCE\xFF\x52\x28", cf_w_control_stub) \
    FP(wcollision1, "drawcount", KIND_CALL, "CWorld::Process: an entity's ProcessCollision, timed", 0x56876F, 7, \
        "\x8B\x16\x8B\xCE\xFF\x52\x2C", cf_w_collision_stub) \
    FP(wcollision2, "drawcount", KIND_CALL, "CWorld::Process: an entity's ProcessCollision again, timed", 0x5687CF, 7, \
        "\x8B\x16\x8B\xCE\xFF\x52\x2C", cf_w_collision_stub) \
    FP(wshift1, "drawcount", KIND_CALL, "CWorld::Process: an entity's ProcessShift, timed", 0x568894, 7, \
        "\x8B\x06\x8B\xCE\xFF\x50\x30", cf_w_shift_stub) \
    FP(wshift2, "drawcount", KIND_CALL, "CWorld::Process: an entity's second ProcessShift, timed", 0x568908, 7, \
        "\x8B\x06\x8B\xCE\xFF\x50\x30", cf_w_shift_stub)

void __cdecl cf_ph_process(void);
void __cdecl cf_ph_streaming(void);
void __cdecl cf_ph_list(void);
void __cdecl cf_ph_scene(void);
void __cdecl cf_ph_effects(void);
void __cdecl cf_ph_hud(void);
void cf_w_anim_stub(void);
void cf_w_control_stub(void);
void cf_w_collision_stub(void);
void cf_w_shift_stub(void);

DRAWCOUNT(FDECLARE_J, FDECLARE_P)

void __cdecl cf_draw_nonroad_orig(void *entity);
void __cdecl cf_draw_entity_orig(void *entity, uint32_t dist);
void __cdecl cf_draw_roads_orig(void);
static int readable(const void *p, size_t n);
static int write_code(uint32_t va, const uint8_t *bytes, size_t n);

enum { DC_OTHER, DC_ROADS, DC_BUILDINGS, DC_LODS, DC_VEHICLES, DC_PEDS, DC_OBJECTS, DC_DUMMIES, DC_CATS };
static const char *const draw_cat_name[DC_CATS] = {"other", "roads", "buildings", "lods", "vehicles", "peds", "objects",
                                                   "dummies"};
#define DRAW_DEVICE (*(void ****)0xC97C28)      /* RwD3D9's IDirect3DDevice9 */
#define DRAW_SLOT 81                            /* DrawPrimitive; the three after it follow */

typedef struct { uint32_t draws, scopes; uint64_t prims; int64_t ticks; } DrawSum;
static DrawSum draw_sum[DC_CATS];
static uint32_t draw_cat, draw_depth;
static void **draw_vtbl;                        /* the vtable while the slots are ours */
static void *draw_orig[4];

/* CEntity: the type in the low 3 bits of +0x36; a building with LOD children
 * (+0x34) is a LOD */
int cf_draw_category(const void *entity)
{
    const uint8_t *e = entity;
    if (!e) return DC_OTHER;
    switch (e[0x36] & 7) {
    case 1: return e[0x34] ? DC_LODS : DC_BUILDINGS;
    case 2: return DC_VEHICLES;
    case 3: return DC_PEDS;
    case 4: return DC_OBJECTS;
    case 5: return DC_DUMMIES;
    }
    return DC_OTHER;
}

typedef struct { uint32_t was; int64_t t0; } DrawScope;

static DrawScope draw_enter(int cat)
{
    DrawScope s = {draw_cat, 0};
    draw_cat = cat;
    if (!draw_depth++) draw_sum[cat].scopes++, s.t0 = stream_now();
    return s;
}

static void draw_leave(int cat, DrawScope s)
{
    if (!--draw_depth) draw_sum[cat].ticks += stream_now() - s.t0;
    draw_cat = s.was;
}

void __cdecl cf_draw_nonroad(void *entity)
{
    int cat = draw_depth ? (int)draw_cat : cf_draw_category(entity);    /* inside RenderEntity: its category */
    DrawScope s = draw_enter(cat);
    cf_draw_nonroad_orig(entity);
    draw_leave(cat, s);
}

void __cdecl cf_draw_entity(void *entity, uint32_t dist)
{
    int cat = cf_draw_category(entity);
    DrawScope s = draw_enter(cat);
    cf_draw_entity_orig(entity, dist);
    draw_leave(cat, s);
}

void __cdecl cf_draw_roads(void)
{
    DrawScope s = draw_enter(DC_ROADS);
    cf_draw_roads_orig();
    draw_leave(DC_ROADS, s);
}

/* The frame's phases; a dotted one is inside the one it names */
enum { PH_PROCESS, PH_WORLD, PH_STREAMING, PH_LIST, PH_PRERENDER, PH_SCENE, PH_EFFECTS, PH_HUD, PH_PRESENT,
       PH_W_ANIM, PH_W_CONTROL, PH_W_COLLISION = PH_W_CONTROL + 3, PH_W_SHIFT = PH_W_COLLISION + 3, PH_COUNT };
static const char *const draw_phase_name[PH_COUNT] = {"process", "process.world", "process.streaming", "list",
                                                      "prerender", "scene", "effects", "hud", "present",
                                                      "world.anim", "world.control.peds", "world.control.vehicles",
                                                      "world.control.objects", "world.collision.peds",
                                                      "world.collision.vehicles", "world.collision.objects", "world.shift"};
typedef struct { uint32_t calls, depth; int64_t ticks; } PhaseSum;
static PhaseSum draw_phase[PH_COUNT];

static int64_t phase_enter(int ph) { return draw_phase[ph].depth++ ? 0 : stream_now(); }

static void phase_leave(int ph, int64_t t0)
{
    PhaseSum *p = &draw_phase[ph];
    if (--p->depth) return;
    p->calls++;
    p->ticks += stream_now() - t0;
}

#define PHASE(name, ph) \
    void __cdecl cf_ph_##name##_orig(void); \
    void __cdecl cf_ph_##name(void) \
    { \
        int64_t t0 = phase_enter(ph); \
        cf_ph_##name##_orig(); \
        phase_leave(ph, t0); \
    }
PHASE(world, PH_WORLD)
PHASE(prerender, PH_PRERENDER)

/* Another mod holds these six's entries, so the calls of them are ours instead */
#define PHASECALL(name, ph, fn) \
    void __cdecl cf_ph_##name(void) \
    { \
        int64_t t0 = phase_enter(ph); \
        ((void (__cdecl *)(void))fn)(); \
        phase_leave(ph, t0); \
    }
PHASECALL(process, PH_PROCESS, 0x53BEE0)
PHASECALL(streaming, PH_STREAMING, 0x40E670)
PHASECALL(list, PH_LIST, 0x5556E0)
PHASECALL(scene, PH_SCENE, 0x53DF40)
PHASECALL(effects, PH_EFFECTS, 0x53E170)
PHASECALL(hud, PH_HUD, 0x53E230)

/* CWorld::Process' passes over the moving entities: a call of an entity's
 * virtual (the site's `mov reg, [esi]; mov ecx, esi; call [reg+slot]`), by
 * its type: 0 peds, 1 vehicles, 2 the rest */
static int world_type(const void *entity)
{
    int type = ((const uint8_t *)entity)[0x36] & 7;
    return type == 3 ? 0 : type == 2 ? 1 : 2;
}

typedef void (__thiscall *EntityFn)(void *entity);

/* A ped's or vehicle's ProcessControl and ProcessCollision by what it is
 * doing: how far from the player's ped (under 30 m, under 100 m, further),
 * moving or still, and a vehicle with a driver or a ped in a vehicle */
enum { WD_CONTROL, WD_COLLISION, WD_PASSES, WD_BUCKETS = 24 };
typedef struct { uint32_t calls; int64_t ticks; } WorldSum;
static WorldSum world_detail[WD_PASSES][WD_BUCKETS];
/* a vehicle's collision by its status (+0x36 bits 3-7) and whether the game
 * has it on fake physics (m_nFakePhysics, +0xB8, at 10: speeds reset each
 * frame, the parked state) */
static WorldSum world_status[32][2];
/* an object's update and collision by its type (+0x13C), attached to another
 * entity (+0xFC) or not, and moving or still; its collision also by model
 * (+0x22) */
enum { WO_CLASSES = 4, WO_MODELS = 20000, WO_TOP = 8 };
static WorldSum world_object[WD_PASSES][WO_CLASSES][2][2];
static WorldSum world_object_model[WO_MODELS];
static uint32_t world_detail_frame;             /* the frame counter at the last summary */
static char world_text[8192];

static const float *world_position(const void *entity)
{
    const void *matrix = *(void *const *)((const uint8_t *)entity + 0x14);
    return matrix ? (const float *)((const uint8_t *)matrix + 0x30) : (const float *)((const uint8_t *)entity + 4);
}

/* -> the bucket, -1 for what is neither a ped nor a vehicle */
int cf_world_bucket(const void *entity)
{
    const uint8_t *e = entity;
    int type = e[0x36] & 7, vehicle = type == 2;
    if (type != 2 && type != 3) return -1;
    int band = 0;
    const void *player = *(void *const *)0xB6F5F0;                      /* the player's ped */
    if (player && player != entity) {
        const float *a = world_position(entity), *b = world_position(player);
        float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2], d2 = dx * dx + dy * dy + dz * dz;
        band = d2 < 900.0f ? 0 : d2 < 10000.0f ? 1 : 2;
    }
    const float *v = (const float *)(e + 0x44);                         /* CPhysical::m_vecMoveSpeed */
    int moving = v[0] * v[0] + v[1] * v[1] + v[2] * v[2] > 1e-6f;
    int flag = vehicle ? *(void *const *)(e + 0x460) != NULL            /* CVehicle::m_pDriver */
                       : (*(const uint32_t *)(e + 0x46C) >> 8) & 1;     /* CPed's bInVehicle */
    return vehicle * 12 + band * 4 + moving * 2 + flag;
}

/* -> an object's bucket: type (0 game, 1 mission, 2 temporary, 3 other) * 4
 * + attached * 2 + moving; -1 for what is not an object */
int cf_world_object(const void *entity)
{
    const uint8_t *e = entity;
    if ((e[0x36] & 7) != 4) return -1;
    uint8_t t = e[0x13C];                                               /* CObject::m_nObjectType */
    int cls = t == 1 ? 0 : t == 2 || t == 6 ? 1 : t == 3 ? 2 : 3;
    const float *v = (const float *)(e + 0x44);
    int moving = v[0] * v[0] + v[1] * v[1] + v[2] * v[2] > 1e-6f;
    return cls * 4 + (*(void *const *)(e + 0xFC) != NULL) * 2 + moving;    /* CPhysical::m_pAttachedTo */
}

static void world_virtual(void *entity, int ph, uint32_t slot, int pass)
{
    int bucket = pass >= 0 ? cf_world_bucket(entity) : -1;              /* as it is before the call */
    int object = pass >= 0 ? cf_world_object(entity) : -1;
    uint32_t model = *(const uint16_t *)((const uint8_t *)entity + 0x22);
    int64_t t0 = stream_now();
    ((EntityFn)(*(void ***)entity)[slot / 4])(entity);
    int64_t d = stream_now() - t0;
    draw_phase[ph].calls++;
    draw_phase[ph].ticks += d;
    if (bucket >= 0) world_detail[pass][bucket].calls++, world_detail[pass][bucket].ticks += d;
    if (object >= 0) {
        WorldSum *w = &world_object[pass][object / 4][(object >> 1) & 1][object & 1];
        w->calls++, w->ticks += d;
        if (pass == WD_COLLISION && model < WO_MODELS) world_object_model[model].calls++, world_object_model[model].ticks += d;
    }
    if (pass == WD_COLLISION && bucket >= 12) {
        const uint8_t *e = entity;
        WorldSum *w = &world_status[e[0x36] >> 3][e[0xB8] >= 10];
        w->calls++, w->ticks += d;
    }
}

void __cdecl cf_w_anim(void *entity)
{
    int64_t t0 = phase_enter(PH_W_ANIM);
    ((EntityFn)0x535F00)(entity);                       /* CEntity::UpdateAnim */
    phase_leave(PH_W_ANIM, t0);
}

void __cdecl cf_w_control(void *entity) { world_virtual(entity, PH_W_CONTROL + world_type(entity), 0x28, WD_CONTROL); }
void __cdecl cf_w_collision(void *entity) { world_virtual(entity, PH_W_COLLISION + world_type(entity), 0x2C, WD_COLLISION); }
void __cdecl cf_w_shift(void *entity) { world_virtual(entity, PH_W_SHIFT, 0x30, -1); }

/* The buckets that ran since the last call, a line each as a frame's calls
 * and ms, and they start again; the text is kept until the next call */
__declspec(dllexport) const char *__cdecl CrashFix_DrawWorldDetail(void)
{
    static const char *const pass_name[WD_PASSES] = {"control", "collision"};
    static const char *const band_name[3] = {"under 30 m", "30-100 m", "over 100 m"};
    uint32_t frame = *(const uint32_t *)0xB7CB4C, frames = frame - world_detail_frame;
    int n = 0;
    world_detail_frame = frame;
    world_text[0] = 0;
    if (!frames) frames = 1;
    for (int p = 0; p < WD_PASSES; p++)
        for (int b = 0; b < WD_BUCKETS; b++) {
            WorldSum *w = &world_detail[p][b];
            if (w->calls && n < (int)sizeof world_text - 160) {
                int vehicle = b >= 12, flag = b & 1;
                uint64_t us1000 = stream_us(w->ticks) * 1000u / frames;     /* a frame's ms, in millionths */
                uint32_t calls100 = (uint32_t)((uint64_t)w->calls * 100u / frames);
                n += snprintf(world_text + n, sizeof world_text - n, "%s %s %s %s %s: %u.%02u a frame, %u.%06u ms\r\n",
                              pass_name[p], vehicle ? "vehicles" : "peds", band_name[(b % 12) / 4],
                              b & 2 ? "moving" : "still",
                              vehicle ? (flag ? "driven" : "empty") : (flag ? "in a vehicle" : "on foot"),
                              calls100 / 100, calls100 % 100, (unsigned)(us1000 / 1000000u), (unsigned)(us1000 % 1000000u));
            }
            w->calls = 0, w->ticks = 0;
        }
    static const char *const status_name[14] = {"player", "playback", "simple", "physics", "abandoned", "wrecked",
                                                "train moving", "train still", "remote", "forced stop", "towed",
                                                "simple towed", "ghost", "13"};
    for (int st = 0; st < 32; st++)
        for (int f = 0; f < 2; f++) {
            WorldSum *w = &world_status[st][f];
            if (w->calls && n < (int)sizeof world_text - 160) {
                uint64_t us1000 = stream_us(w->ticks) * 1000u / frames;
                uint32_t calls100 = (uint32_t)((uint64_t)w->calls * 100u / frames);
                n += snprintf(world_text + n, sizeof world_text - n, "collision vehicles status %s%s: %u.%02u a frame, %u.%06u ms\r\n",
                              st < 14 ? status_name[st] : "?", f ? " fake physics" : "", calls100 / 100, calls100 % 100,
                              (unsigned)(us1000 / 1000000u), (unsigned)(us1000 % 1000000u));
            }
            w->calls = 0, w->ticks = 0;
        }
    static const char *const class_name[WO_CLASSES] = {"game", "mission", "temporary", "other"};
    for (int p = 0; p < WD_PASSES; p++)
        for (int c = 0; c < WO_CLASSES; c++)
            for (int a = 0; a < 2; a++)
                for (int m = 0; m < 2; m++) {
                    WorldSum *w = &world_object[p][c][a][m];
                    if (w->calls && n < (int)sizeof world_text - 160) {
                        uint64_t us1000 = stream_us(w->ticks) * 1000u / frames;
                        uint32_t calls100 = (uint32_t)((uint64_t)w->calls * 100u / frames);
                        n += snprintf(world_text + n, sizeof world_text - n, "%s objects %s%s %s: %u.%02u a frame, %u.%06u ms\r\n",
                                      pass_name[p], class_name[c], a ? " attached" : "", m ? "moving" : "still",
                                      calls100 / 100, calls100 % 100, (unsigned)(us1000 / 1000000u), (unsigned)(us1000 % 1000000u));
                    }
                    w->calls = 0, w->ticks = 0;
                }
    uint32_t top[WO_TOP];                       /* the models whose collision took longest, longest first */
    int tops = 0;
    for (uint32_t id = 0; id < WO_MODELS; id++) {
        if (!world_object_model[id].calls) continue;
        int at = tops;
        while (at > 0 && world_object_model[top[at - 1]].ticks < world_object_model[id].ticks) at--;
        if (at >= WO_TOP) continue;
        if (tops < WO_TOP) tops++;
        for (int i = tops - 1; i > at; i--) top[i] = top[i - 1];
        top[at] = id;
    }
    for (int i = 0; i < tops && n < (int)sizeof world_text - 160; i++) {
        WorldSum *w = &world_object_model[top[i]];
        uint64_t us1000 = stream_us(w->ticks) * 1000u / frames;
        uint32_t calls100 = (uint32_t)((uint64_t)w->calls * 100u / frames);
        n += snprintf(world_text + n, sizeof world_text - n, "collision objects model %u: %u.%02u a frame, %u.%06u ms\r\n",
                      top[i], calls100 / 100, calls100 % 100, (unsigned)(us1000 / 1000000u), (unsigned)(us1000 % 1000000u));
    }
    memset(world_object_model, 0, sizeof world_object_model);
    return world_text;
}

/* RwCameraShowRaster(camera, window, flags) -> the camera */
uint32_t __cdecl cf_ph_present_orig(void *camera, void *window, uint32_t flags);
uint32_t __cdecl cf_ph_present(void *camera, void *window, uint32_t flags)
{
    int64_t t0 = phase_enter(PH_PRESENT);
    uint32_t r = cf_ph_present_orig(camera, window, flags);
    phase_leave(PH_PRESENT, t0);
    return r;
}

/* Every call Idle and CGame::Process make, timed at its site whoever's
 * function it calls now:
 * the site's `call` goes to a stub made here that times the call of the
 * target it had (another mod's hook included, SA-MP's on CGame::Process and
 * Render2dStuff). AppEventHandler's call of Idle itself is Idle's last;
 * CGame::Process' clock reads are left out. */
static const struct { uint32_t site; const char *group, *name; } idle_site[] = {
    {0x53E968, "idle", "CTimer::Update"}, {0x53E96D, "idle", "CSprite2d::InitPerFrame"}, {0x53E972, "idle", "CFont::InitPerFrame"},
    {0x53E981, "idle", "CGame::Process"}, {0x53E98B, "idle", "AudioEngine.Service"}, {0x53E997, "idle", "SetLightsWithTimeOfDayColour"},
    {0x53E9F1, "idle", "RsMouseSetPos"}, {0x53E9F9, "idle", "CRenderer::ConstructRenderList"}, {0x53E9FE, "idle", "CRenderer::PreRender"},
    {0x53EA03, "idle", "CWorld::ProcessPedsAfterPreRender"}, {0x53EA0D, "idle", "CRealTimeShadowManager::Update"},
    {0x53EA12, "idle", "CMirrors::BeforeMainRender"}, {0x53EA7E, "idle", "DoRWStuffStartOfFrame_Horizon"}, {0x53EA8E, "idle", "DefinedState"},
    {0x53EAA1, "idle", "RwCameraSetFarClipPlane"}, {0x53EABA, "idle", "CMirrors::RenderMirrorBuffer"}, {0x53EABF, "idle", "RenderScene"},
    {0x53EAC4, "idle", "CVisibilityPlugins::RenderWeaponPedsForPC"}, {0x53EACE, "idle", "CVisibilityPlugins::ResetWeaponPedsForPC"},
    {0x53EAD3, "idle", "RenderEffects"}, {0x53EB0D, "idle", "CCamera::RenderMotionBlur"}, {0x53EB12, "idle", "Render2dStuff"},
    {0x53EB8C, "idle", "CMenuManager::DrawFrontEnd"}, {0x53EB9D, "idle", "DoFade"}, {0x53EBA2, "idle", "CHud::DrawAfterFade"},
    {0x53EBA9, "idle", "CMessages::Display"}, {0x53EBB1, "idle", "CFont::DrawFonts"}, {0x53EBE4, "idle", "CDebug::DebugDisplayTextBuffer"},
    {0x53EBE9, "idle", "FlushObrsPrintfs"}, {0x53EBF5, "idle", "RwCameraEndUpdate"}, {0x53EC01, "idle", "RsCameraShowRaster"},
    {0x53ECBD, "idle", "Idle"},
    {0x53BEE6, "process", "CPad::UpdatePads"},
    {0x53BEF0, "process", "CLoadMonitor::BeginFrame"},
    {0x53BF0B, "process", "CStreaming::Update"},
    {0x53BF28, "process", "CCutsceneMgr::Update"},
    {0x53BF44, "process", "CMenuManager::Process"},
    {0x53BF49, "process", "CTheZones::Update"},
    {0x53BF4E, "process", "CCover::Update"},
    {0x53BF7D, "process", "CAudioZones::Update"},
    {0x53BFA9, "process", "CSprite2d::SetRecipNearClip"},
    {0x53BFAE, "process", "CSprite2d::InitPerFrame"},
    {0x53BFB3, "process", "CFont::InitPerFrame"},
    {0x53BFB8, "process", "CCheat::DoCheats"},
    {0x53BFBD, "process", "CClock::Update"},
    {0x53BFC2, "process", "CWeather::Update"},
    {0x53BFC7, "process", "CTheScripts::Process"},
    {0x53BFCC, "process", "CCollision::Update"},
    {0x53BFD8, "process", "CPathFind::UpdateStreaming"},
    {0x53BFDD, "process", "CTrain::UpdateTrains"},
    {0x53BFE2, "process", "CHeli::UpdateHelis"},
    {0x53BFE7, "process", "CDarkel::Update"},
    {0x53BFEC, "process", "CSkidmarks::Update"},
    {0x53BFF1, "process", "CGlass::Update"},
    {0x53BFF6, "process", "CWanted::UpdateEachFrame"},
    {0x53BFFB, "process", "CCreepingFire::Update"},
    {0x53C000, "process", "CSetPieces::Update"},
    {0x53C00A, "process", "CFireManager::Update"},
    {0x53C030, "process", "CPopulation::Update"},
    {0x53C054, "process", "CPopulation::Update (again)"},
    {0x53C05C, "process", "CWeapon::UpdateWeapons"},
    {0x53C06A, "process", "CTheCarGenerators::Process"},
    {0x53C077, "process", "CCranes::UpdateCranes"},
    {0x53C07C, "process", "CClouds::Update"},
    {0x53C081, "process", "CMovingThings::Update"},
    {0x53C086, "process", "CWaterCannons::Update"},
    {0x53C08B, "process", "CUserDisplay::Process"},
    {0x53C090, "process", "CReplay::Update"},
    {0x53C095, "process", "CWorld::Process"},
    {0x53C09F, "process", "CLoadMonitor::EndFrame"},
    {0x53C0AD, "process", "CPickups::Update"},
    {0x53C0B2, "process", "CCarCtrl::PruneVehiclesOfInterest"},
    {0x53C0B7, "process", "CGarages::Update"},
    {0x53C0BC, "process", "CEntryExitManager::Update"},
    {0x53C0C1, "process", "CStuntJumpManager::Update"},
    {0x53C0C6, "process", "CBirds::Update"},
    {0x53C0CB, "process", "CSpecialFX::Update"},
    {0x53C0D0, "process", "CRopes::Update"},
    {0x53C0D5, "process", "CPostEffects::Update"},
    {0x53C0DA, "process", "CTimeCycle::Update"},
    {0x53C0DF, "process", "CPopCycle::Update"},
    {0x53C0F1, "process", "CInterestingEvents::ScanForNearbyEntities"},
    {0x53C104, "process", "CCamera::Process"},
    {0x53C10B, "process", "CCamera::ProcessFade"},
    {0x53C110, "process", "CCullZones::Update"},
    {0x53C11D, "process", "CGameLogic::Update"},
    {0x53C122, "process", "CGangWars::Update"},
    {0x53C127, "process", "CConversations::Update"},
    {0x53C12C, "process", "CPedToPlayerConversations::Update"},
    {0x53C131, "process", "CBridge::Update"},
    {0x53C136, "process", "CCoronas::DoSunAndMoon"},
    {0x53C13B, "process", "CCoronas::Update"},
    {0x53C140, "process", "CShadows::UpdatePermanentShadows"},
    {0x53C159, "process", "CPlantMgr::Update"},
    {0x53C15E, "process", "CCustomBuildingRenderer::Update"},
    {0x53C18F, "process", "CStencilShadows::RenderBuffer"},
    {0x53C1AB, "process", "CStencilShadows::Process"},
    {0x53C1C1, "process", "CCarCtrl::GenerateRandomCars"},
    {0x53C1C6, "process", "CRoadBlocks::GenerateRoadBlocks"},
    {0x53C1CB, "process", "CCarCtrl::RemoveDistantCars"},
    {0x53C1D0, "process", "CCarCtrl::RemoveCarsIfThePoolGetsFull"},
    {0x53C1F0, "process", "Fx_c::Update"},
    {0x53C201, "process", "BreakManager_c::Update"},
    {0x53C20B, "process", "InteriorManager_c::Update"},
    {0x53C215, "process", "ProcObjectMan_c::Update"},
    {0x53C22F, "process", "WaterCreatureManager_c::Update"},
};
enum { IDLE_SITES = sizeof idle_site / sizeof idle_site[0], IDLE_STUB = 64 };
typedef struct { uint32_t calls, target, ret; int64_t t0, ticks; uint8_t saved[5]; int chained; } IdleSum;
static IdleSum idle_sum[IDLE_SITES];
static uint8_t *idle_code;                      /* the stubs, IDLE_STUB bytes each */

void __cdecl cf_idle_enter(uint32_t k) { idle_sum[k].t0 = stream_now(); }
void __cdecl cf_idle_leave(uint32_t k)
{
    IdleSum *s = &idle_sum[k];
    s->calls++;
    s->ticks += stream_now() - s->t0;
}

/* pushfd; pushad; push k; call enter; add esp, 4; popad; popfd;
 * pop [ret]; call target; push [ret];
 * pushfd; pushad; push k; call leave; add esp, 4; popad; popfd; ret */
static void idle_stub_write(uint8_t *c, uint32_t k)
{
    uint32_t at = (uint32_t)c;
    uint8_t *p = c;
#define PUT1(b) (*p++ = (uint8_t)(b))
#define PUT4(v) (*(uint32_t *)p = (uint32_t)(v), p += 4)
#define CALL(fn) (PUT1(0xE8), PUT4((uint32_t)(fn) - (at + (uint32_t)(p - c) + 4)))
    PUT1(0x9C); PUT1(0x60); PUT1(0x68); PUT4(k); CALL(cf_idle_enter); PUT1(0x83); PUT1(0xC4); PUT1(4); PUT1(0x61); PUT1(0x9D);
    PUT1(0x8F); PUT1(0x05); PUT4(&idle_sum[k].ret);
    PUT1(0xFF); PUT1(0x15); PUT4(&idle_sum[k].target);                 /* call [target]: it can be re-read */
    PUT1(0xFF); PUT1(0x35); PUT4(&idle_sum[k].ret);
    PUT1(0x9C); PUT1(0x60); PUT1(0x68); PUT4(k); CALL(cf_idle_leave); PUT1(0x83); PUT1(0xC4); PUT1(4); PUT1(0x61); PUT1(0x9D);
    PUT1(0xC3);
#undef PUT1
#undef PUT4
#undef CALL
}

static int idle_chain(void)
{
    if (!idle_code) {
        idle_code = VirtualAlloc(NULL, IDLE_SITES * IDLE_STUB, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!idle_code) return 0;
        for (uint32_t k = 0; k < IDLE_SITES; k++) idle_stub_write(idle_code + k * IDLE_STUB, k);
    }
    int n = 0;
    for (uint32_t k = 0; k < IDLE_SITES; k++) {
        IdleSum *s = &idle_sum[k];
        const uint8_t *site = (const uint8_t *)idle_site[k].site;
        if (s->chained || !readable(site, 5) || site[0] != 0xE8) continue;
        uint32_t stub = (uint32_t)(idle_code + k * IDLE_STUB);
        memcpy(s->saved, site, 5);
        s->target = idle_site[k].site + 5 + *(const uint32_t *)(site + 1);
        uint8_t code[5] = {0xE8};
        *(uint32_t *)(code + 1) = stub - (idle_site[k].site + 5);
        if (write_code(idle_site[k].site, code, 5)) s->chained = 1, n++;
    }
    return n;
}

/* Each site back to what it called, unless somebody wrote it since */
static void idle_unchain(void)
{
    for (uint32_t k = 0; k < IDLE_SITES; k++) {
        IdleSum *s = &idle_sum[k];
        if (!s->chained) continue;
        const uint8_t *site = (const uint8_t *)idle_site[k].site;
        uint32_t stub = (uint32_t)(idle_code + k * IDLE_STUB);
        if (site[0] == 0xE8 && idle_site[k].site + 5 + *(const uint32_t *)(site + 1) == stub)
            write_code(idle_site[k].site, s->saved, 5);
        s->chained = 0;
    }
}

__declspec(dllexport) int __cdecl CrashFix_DrawIdleSites(void) { return IDLE_SITES; }

/* "<group> <name>", the group the function making the call */
__declspec(dllexport) const char *__cdecl CrashFix_DrawIdleName(int k)
{
    static char name[96];
    if (k < 0 || k >= IDLE_SITES) return NULL;
    snprintf(name, sizeof name, "%s %s", idle_site[k].group, idle_site[k].name);
    return name;
}

/* Since the DLL loaded, for Idle's call k: 0 ms in it, 1 calls, 2 whether it
 * is chained now, 3 the target it calls (another module's: a hook) */
__declspec(dllexport) double __cdecl CrashFix_DrawIdleStat(int k, int which)
{
    if (k < 0 || k >= IDLE_SITES) return 0;
    const IdleSum *s = &idle_sum[k];
    switch (which) {
    case 0: return (double)stream_us(s->ticks) / 1000.0;
    case 1: return s->calls;
    case 2: return s->chained;
    case 3: return s->target;
    }
    return 0;
}

static void draw_note(uint32_t prims)
{
    DrawSum *s = &draw_sum[draw_cat < DC_CATS ? draw_cat : DC_OTHER];
    s->draws++;
    s->prims += prims;
}

typedef int32_t (__stdcall *DrawFn)(void *dev, uint32_t type, uint32_t start, uint32_t prims);
typedef int32_t (__stdcall *DrawIndexedFn)(void *dev, uint32_t type, int32_t base, uint32_t min, uint32_t vertices,
                                           uint32_t start, uint32_t prims);
typedef int32_t (__stdcall *DrawUpFn)(void *dev, uint32_t type, uint32_t prims, const void *data, uint32_t stride);
typedef int32_t (__stdcall *DrawIndexedUpFn2)(void *dev, uint32_t type, uint32_t min, uint32_t vertices, uint32_t prims,
                                              const void *indices, uint32_t format, const void *data, uint32_t stride);

static int32_t __stdcall draw_dp(void *dev, uint32_t type, uint32_t start, uint32_t prims)
{
    draw_note(prims);
    return ((DrawFn)draw_orig[0])(dev, type, start, prims);
}

static int32_t __stdcall draw_dip(void *dev, uint32_t type, int32_t base, uint32_t min, uint32_t vertices, uint32_t start,
                                  uint32_t prims)
{
    draw_note(prims);
    return ((DrawIndexedFn)draw_orig[1])(dev, type, base, min, vertices, start, prims);
}

static int32_t __stdcall draw_dpup(void *dev, uint32_t type, uint32_t prims, const void *data, uint32_t stride)
{
    draw_note(prims);
    return ((DrawUpFn)draw_orig[2])(dev, type, prims, data, stride);
}

static int32_t __stdcall draw_dipup(void *dev, uint32_t type, uint32_t min, uint32_t vertices, uint32_t prims,
                                    const void *indices, uint32_t format, const void *data, uint32_t stride)
{
    draw_note(prims);
    return ((DrawIndexedUpFn2)draw_orig[3])(dev, type, min, vertices, prims, indices, format, data, stride);
}

static void *const draw_ours[4] = {draw_dp, draw_dip, draw_dpup, draw_dipup};

static int draw_write(void **slot, void *const *from)
{
    DWORD old;
    if (!VirtualProtect(slot, 4 * sizeof *slot, PAGE_READWRITE, &old)) return 0;
    memcpy(slot, from, 4 * sizeof *slot);
    VirtualProtect(slot, 4 * sizeof *slot, old, &old);
    return 1;
}

/* 1: the device's four draw slots are ours; 0: no device yet, or its vtable
 * cannot be written */
__declspec(dllexport) int __cdecl CrashFix_DrawStart(void)
{
    if (draw_vtbl) return 1;
    if (!readable((void *)0xC97C28, 4) || !DRAW_DEVICE || !readable(DRAW_DEVICE, 4)) return 0;
    void **slot = *DRAW_DEVICE + DRAW_SLOT;
    if (!readable(slot, 4 * sizeof *slot)) return 0;
    memcpy(draw_orig, slot, sizeof draw_orig);
    for (int i = 0; i < 4; i++) if (!draw_orig[i] || draw_orig[i] == draw_ours[i]) return 0;
    if (!draw_write(slot, draw_ours)) return 0;
    draw_vtbl = slot - DRAW_SLOT;
    idle_chain();
    return 1;
}

/* The slots as they were, unless somebody wrote them after us (0: left) */
__declspec(dllexport) int __cdecl CrashFix_DrawStop(void)
{
    idle_unchain();
    if (!draw_vtbl) return 1;
    void **slot = draw_vtbl + DRAW_SLOT;
    if (memcmp(slot, draw_ours, sizeof draw_ours) != 0) return 0;
    if (!draw_write(slot, draw_orig)) return 0;
    draw_vtbl = NULL;
    return 1;
}

__declspec(dllexport) int __cdecl CrashFix_DrawCategories(void) { return DC_CATS; }

__declspec(dllexport) const char *__cdecl CrashFix_DrawCategory(int cat)
{
    return cat >= 0 && cat < DC_CATS ? draw_cat_name[cat] : NULL;
}

/* Since the DLL loaded, for a category: 0 draws, 1 primitives, 2 ms inside
 * its scopes (none for "other"), 3 scopes entered */
__declspec(dllexport) double __cdecl CrashFix_DrawStat(int cat, int which)
{
    if (cat < 0 || cat >= DC_CATS) return 0;
    const DrawSum *s = &draw_sum[cat];
    switch (which) {
    case 0: return s->draws;
    case 1: return (double)s->prims;
    case 2: return (double)stream_us(s->ticks) / 1000.0;
    case 3: return s->scopes;
    }
    return 0;
}

__declspec(dllexport) int __cdecl CrashFix_DrawPhases(void) { return PH_COUNT; }

__declspec(dllexport) const char *__cdecl CrashFix_DrawPhase(int ph)
{
    return ph >= 0 && ph < PH_COUNT ? draw_phase_name[ph] : NULL;
}

/* Since the DLL loaded, for a phase: 0 ms inside it, 1 times it ran */
__declspec(dllexport) double __cdecl CrashFix_DrawPhaseStat(int ph, int which)
{
    if (ph < 0 || ph >= PH_COUNT) return 0;
    return which == 0 ? (double)stream_us(draw_phase[ph].ticks) / 1000.0 : which == 1 ? draw_phase[ph].calls : 0;
}

__asm__(
".intel_syntax noprefix\n"
".macro PHASEHOOK name\n"
"HOOK ph\\name\n"
"    jmp _cf_ph_\\name\n"
".globl _cf_ph_\\name\\()_orig\n"
"_cf_ph_\\name\\()_orig:\n"
".endm\n"
"PHASEHOOK world\n"
"    push esi\n"
"    mov esi, dword ptr ds:[0xB9ACC8]\n"
"    GO 0x5684A7\n"
"PHASEHOOK prerender\n"
"    mov eax, dword ptr ds:[0xB76840]\n"
"    GO 0x553915\n"
"PHASEHOOK present\n"
"    mov eax, dword ptr [esp+0xC]\n"
"    mov ecx, dword ptr [esp+8]\n"
"    GO 0x7EE378\n"
/* the entity is in esi at the virtuals' sites, in ecx at UpdateAnim's (esi is the list's next node there) */
".macro WORLDSTUB name\n"
".globl _cf_w_\\name\\()_stub\n"
"_cf_w_\\name\\()_stub:\n"
"    push esi\n"
"    call _cf_w_\\name\n"
"    add esp, 4\n"
"    ret\n"
".endm\n"
".globl _cf_w_anim_stub\n"
"_cf_w_anim_stub:\n"
"    push ecx\n"
"    call _cf_w_anim\n"
"    add esp, 4\n"
"    ret\n"
"WORLDSTUB control\n"
"WORLDSTUB collision\n"
"WORLDSTUB shift\n"
"HOOK drawnonroad\n"
"    jmp _cf_draw_nonroad\n"
".globl _cf_draw_nonroad_orig\n"
"_cf_draw_nonroad_orig:\n"
"    push esi\n"
"    mov esi, dword ptr [esp+8]\n"
"    GO 0x553265\n"
"HOOK drawentity\n"
"    jmp _cf_draw_entity\n"
".globl _cf_draw_entity_orig\n"
"_cf_draw_entity_orig:\n"
"    push esi\n"
"    mov esi, dword ptr [esp+8]\n"
"    GO 0x732B45\n"
"HOOK drawroads\n"
"    jmp _cf_draw_roads\n"
".globl _cf_draw_roads_orig\n"
"_cf_draw_roads_orig:\n"
"    mov eax, dword ptr ds:[0xC97B24]\n"
"    GO 0x553A15\n"
".att_syntax\n"
);
