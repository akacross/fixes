/* San Andreas Online's growing vehicle alpha list (Dryxio/san-andreas-online,
 * CMultiplayerSA_Rendering.cpp, GPLv3, after Open Limit Adjuster's
 * InsertSorted retry), for gta_sa.exe 1.0 US; included by crashfix.c and
 * switched by the glasslist option.
 *
 * The game sorts a vehicle's see-through atomics (glass, lights, rotors) in a
 * list of 20 (0xC88070) it fills and draws per vehicle; past 20 it draws them
 * at once, unsorted, so glass hides what is behind it. The six calls that fill
 * it go through cf_insert_alpha, which gives a full list as many links again.
 *
 * Every visible car not touching water also goes into the alpha entity list
 * (0xC88120, 200 links) and is drawn later by RenderEntity with the alpha test
 * at 100 and no directional light. One that finds it full is drawn at once
 * with the alpha test at 140, which throws away its glass and the tinted covers
 * of its lights, and lit differently: as the count on screen crosses 200, cars
 * flip between the two. Its two inserts grow it the same way.
 *
 * Where it differs from SA:O's: the links come from pools of ours, never
 * freed (the lists keep them). A limit adjuster that grows the lists inside
 * InsertSorted leaves nothing for these to do.
 *
 * The list is drawn farthest first, by "the car's distance + a dot product",
 * whose axis is the car's forward one for front and rear parts and its right
 * one for side parts (0 for the rest, -0.0001 for alpha-flagged ones). Side
 * against front or rear parts sort by unrelated numbers, and the parts write
 * depth, so a nearer window drawn first hides a taillight or far window behind
 * it, swapping as the angle changes. With cf_glass_sort (option glasssort)
 * each part's key is its world bounding sphere's squared distance from the
 * camera instead.
 */

/* ------------------------------------------------------------------------ */
/* The rows: key, option, what, site, bytes, stock, a call to `to`           */
/* ------------------------------------------------------------------------ */
#define GLASSLIST(FP) \
    FP(alpha1, "glasslist", KIND_CALL, "CVisibilityPlugins::InsertAtomicIntoSortedList", 0x733D35, 5, \
        "\xE8\xD6\xFB\xFF\xFF", cf_insert_alpha) \
    FP(alpha2, "glasslist", KIND_CALL, "CVisibilityPlugins::RenderVehicleHiDetailAlphaCB", 0x734092, 5, \
        "\xE8\x79\xF8\xFF\xFF", cf_insert_alpha) \
    FP(alpha3, "glasslist", KIND_CALL, "CVisibilityPlugins::RenderHeliRotorAlphaCB", 0x734141, 5, \
        "\xE8\xCA\xF7\xFF\xFF", cf_insert_alpha) \
    FP(alpha4, "glasslist", KIND_CALL, "CVisibilityPlugins::RenderHeliTailRotorAlphaCB", 0x734211, 5, \
        "\xE8\xFA\xF6\xFF\xFF", cf_insert_alpha) \
    FP(alpha5, "glasslist", KIND_CALL, "CVisibilityPlugins::RenderTrainHiDetailAlphaCB", 0x734356, 5, \
        "\xE8\xB5\xF5\xFF\xFF", cf_insert_alpha) \
    FP(alpha6, "glasslist", KIND_CALL, "CVisibilityPlugins::RenderVehicleHiDetailAlphaCB_BigVehicle", 0x734482, 5, \
        "\xE8\x89\xF4\xFF\xFF", cf_insert_alpha) \
    FP(alpha7, "glasslist", KIND_CALL, "CVisibilityPlugins::InsertObjectIntoSortedList (the alpha entity list)", 0x733DF5, 5, \
        "\xE8\x16\xFB\xFF\xFF", cf_insert_alpha) \
    FP(alpha8, "glasslist", KIND_CALL, "CVisibilityPlugins::InsertEntityIntoSortedList (the alpha entity list)", 0x7345F2, 5, \
        "\xE8\x19\xF3\xFF\xFF", cf_insert_alpha)

#define GDECLARE(k, option, kind, name, va, len, stock, to) volatile LONG cf_n_##k;
GLASSLIST(GDECLARE)

/* A car's glass drawn after its solid parts (option glassdepth).
 *
 * The car's alpha-list parts are whole atomics, opaque panels and glass
 * together, drawn farthest first by one number each. Sorting by part can't
 * order a body and a door that overlap. When the far door comes after the
 * windscreen, the windscreen's depth hides it and the ground shows through the
 * glass; without the glass's depth, a part drawn after the glass paints over
 * it and the window seems gone. So the car's RenderAlphaAtomics (called from
 * CRenderer::RenderOneNonRoad, 0x553318) runs twice. The game's own per-mesh
 * switch to blending (CCustomCarEnvMapPipeline::CustomPipeRenderCB, 0x5D9E23:
 * vertex alpha or material alpha under 255) picks what each pass draws:
 *   1. the opaque meshes as before; a blended mesh is masked (no colour, no
 *      depth).
 *   2. only the blended meshes, writing no depth, over everything of the
 *      car's behind them. This pass starts masked, so a part another
 *      pipeline draws appears once, in pass 1.
 * After both, colour writes and depth writes are what they were. */
void __cdecl cf_glass_alpha(int on);
void __cdecl cf_render_car_alpha(void);

#define GLASSDEPTH(FJ, FP) \
    FP(glassz, "glassdepth", KIND_CALL, "CCustomCarEnvMapPipeline::CustomPipeRenderCB: a mesh drawn in its pass of the alpha list", \
        0x5D9E23, 5, "\xE8\x78\x42\x22\x00", cf_glass_alpha) \
    FP(glasszpass, "glassdepth", KIND_CALL, "CRenderer::RenderOneNonRoad: a car's alpha list, solid parts then glass", 0x553318, 5, \
        "\xE8\x73\x0B\x1E\x00", cf_render_car_alpha)

GLASSDEPTH(FDECLARE_J, FDECLARE_P)

typedef int (__cdecl *RenderStateFn)(int state, void *value);
#define RW_ENGINE (*(uint8_t **)0xC97B24)           /* RwEngineInstance */
#define RW_STATE_SET (*(RenderStateFn *)(RW_ENGINE + 0x20))
#define RW_STATE_GET (*(RenderStateFn *)(RW_ENGINE + 0x24))
#define ZWRITE 8                                    /* rwRENDERSTATEZWRITEENABLE */
#define D3D_SET_STATE ((void (__cdecl *)(uint32_t, uint32_t))0x7FC2D0)     /* RwD3D9SetRenderState */
#define D3D_GET_STATE ((void (__cdecl *)(uint32_t, uint32_t *))0x7FC320)   /* RwD3D9GetRenderState */
#define COLORWRITE 168                              /* D3DRS_COLORWRITEENABLE */
static int glass_pass;                              /* 0 none, 1 solid parts, 2 glass */
static uint32_t glass_colour = 0xF;

static void glass_mask(int show, int zwrite)
{
    D3D_SET_STATE(COLORWRITE, show ? glass_colour : 0);
    RW_STATE_SET(ZWRITE, (void *)(uintptr_t)zwrite);
}

void __cdecl cf_glass_alpha(int on)
{
    ((void (__cdecl *)(int))0x7FE0A0)(on);           /* _rwD3D9RenderStateVertexAlphaEnable */
    if (glass_pass == 1) glass_mask(!on, !on);
    else if (glass_pass == 2) glass_mask(on, 0);
}

void __cdecl cf_render_car_alpha(void)
{
    uintptr_t zwrite = 1;
    RW_STATE_GET(ZWRITE, &zwrite);
    D3D_GET_STATE(COLORWRITE, &glass_colour);
    if (!glass_colour) glass_colour = 0xF;
    glass_pass = 1;
    glass_mask(1, zwrite);
    ((void (__cdecl *)(void))0x733E90)();            /* CVisibilityPlugins::RenderAlphaAtomics */
    glass_pass = 2;
    glass_mask(0, 0);
    ((void (__cdecl *)(void))0x733E90)();
    glass_pass = 0;
    D3D_SET_STATE(COLORWRITE, glass_colour);
    RW_STATE_SET(ZWRITE, (void *)zwrite);
}

/* ------------------------------------------------------------------------ */
/* CLinkList<AlphaObjectInfo>                                               */
/* ------------------------------------------------------------------------ */
typedef struct AlphaLink { void *object, *render; float distance; struct AlphaLink *prev, *next; } AlphaLink;
typedef struct { AlphaLink usedHead, usedTail, freeHead, freeTail; AlphaLink *links; } AlphaList;
typedef AlphaLink *(__attribute__((thiscall)) *InsertSortedFn)(AlphaList *list, const void *info);
typedef struct { void *object, *render; float distance; } AlphaInfo;
typedef const float *(__cdecl *WorldSphereFn)(void *atomic);

#define CAMERA_POS (*(const float **)0xC88050)      /* CVisibilityPlugins::ms_pCameraPosn */
int cf_glass_sort;

#define ALPHA_LIST ((AlphaList *)0xC88070)          /* CVisibilityPlugins::m_alphaList */
#define ENTITY_LIST ((AlphaList *)0xC88120)         /* CVisibilityPlugins::m_alphaEntityList */
#define GLASS_POOL 980                              /* 1000 with the game's 20 */
#define ENTITY_POOL 1800                            /* 2000 with the game's 200 */
static AlphaLink glass_pool[GLASS_POOL], entity_pool[ENTITY_POOL];
int cf_glass_used, cf_entity_used;

/* In place of the call to CLinkList::InsertSorted (0x733910, thiscall); a
 * growth counts on the row of the site it came from. */
AlphaLink *__attribute__((fastcall)) cf_insert_alpha(AlphaList *list, void *edx, const void *info)
{
    (void)edx;
    const InsertSortedFn insert = (InsertSortedFn)0x733910;
    AlphaInfo sorted;
    if (cf_glass_sort && list == ALPHA_LIST && CAMERA_POS && ((const AlphaInfo *)info)->object) {
        const float *sphere = ((WorldSphereFn)0x749330)(((const AlphaInfo *)info)->object);  /* RpAtomicGetWorldBoundingSphere */
        if (sphere) {
            const float *cam = CAMERA_POS;
            float dx = sphere[0] - cam[0], dy = sphere[1] - cam[1], dz = sphere[2] - cam[2];
            sorted = *(const AlphaInfo *)info;
            sorted.distance = dx * dx + dy * dy + dz * dz;
            info = &sorted;
        }
    }
    AlphaLink *link = insert(list, info);
    AlphaLink *pool;
    int *used, size, least;
    if (list == ALPHA_LIST) pool = glass_pool, used = &cf_glass_used, size = GLASS_POOL, least = 20;
    else if (list == ENTITY_LIST) pool = entity_pool, used = &cf_entity_used, size = ENTITY_POOL, least = 200;
    else return link;
    if (link || *used == size) return link;
    int n = 0;
    for (AlphaLink *l = list->usedHead.next; l != &list->usedTail && n < size; l = l->next) n++;
    if (n < least) n = least;
    if (n > size - *used) n = size - *used;
    for (int i = 0; i < n; i++) {
        AlphaLink *l = &pool[(*used)++];
        l->prev = &list->freeHead;
        l->next = list->freeHead.next;
        l->next->prev = l;
        list->freeHead.next = l;
    }
    uint32_t from = (uint32_t)__builtin_return_address(0);
#define GCOUNT(k, option, kind, name, va, len, stock, to) if (from == va + 5) InterlockedIncrement(&cf_n_##k);
    GLASSLIST(GCOUNT)
#undef GCOUNT
    return insert(list, info);
}
