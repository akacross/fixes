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
        if (r != 1 || ((uint8_t *)CrashFix_Address(i))[0] != 0xE9) { all_in = 0; printf("  %s: %d\n", CrashFix_Key(i), r); }
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

    printf("crashfix: %d passed, %d failed\n", passed, failed);
    fflush(stdout);
    return failed != 0;
}
