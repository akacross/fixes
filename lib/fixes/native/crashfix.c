/* fixes.lua's crash fixes: MTA:SA's crash-fix hooks (multitheftauto/mtasa-blue,
 * GPLv3) for gta_sa.exe 1.0 US, as gtasa_crashfix (crashes.asi 2.x by
 * Whitetigerswt, GPLv3) copied them and as MTA has them now
 * (docs/MTA_CRASHFIX_UPDATES.md). fixes loads this with LoadLibraryA and never
 * frees it, and installs each hook by number (CrashFix_Install), so every one
 * can stand down for another mod's hook and be switched off on its own.
 *
 * The hook bodies are MTA's, in GNU assembler. Where they differ:
 *   - misc14 and misc16 unwind the function's SEH frame and saved registers.
 *   - misc21 looks the vehicle up in the game's vehicle pool.
 *   - GetFrameFromId calls the original through its own entry, not the hook.
 *   - the SA-MP pointer check also refuses a bad [eax+10h].
 *   - CModelInfo::ms_modelInfoPtrs is read through the reference at 0x403DA7,
 *     which SA-MP R5 rewrites when it moves the array.
 *   - misc47 and misc48 keep eax, which the code after misc48 stores.
 *   - colliding pops the distance it skips; translation leaves through the
 *     function's own epilogue and checks the index against the frame count.
 *   - FindTextureCB keeps the game's lookup, with the remap copy bounded.
 *   - the train's rail distance wraps by the length the game's loop uses.
 *   - RwMatrixMultiply checks all three matrices and returns its destination.
 *   - CPlaceName clears bInVehicle in its copy of the flags, not on the ped.
 * stencilveh, stencilobj and ikchain are San Andreas Online's
 * (Dryxio/san-andreas-online, an MTA fork, GPLv3); ikchain checks where the
 * game looks the effector up, after its own checks.
 * camnan is ours: a camera gone NaN is set up again.
 * A crash averted is one `lock inc` on the hook's counter; fixes reads them.
 * MTA's frame-rate fixes are in framerate.c and FramerateVigilante's in
 * vigilante.c, each switched by its own option; MTA's ped matrix checks are in
 * pedmatrix.c, the vehicle glass list in glasslist.c, the dithered sky in sky.c,
 * the first-chance crash records (option faultlog) in faults.c, and the model
 * loading timer (option streamtime) in stream.c.
 *
 *   ./build.sh            -> ../crashfix.dll
 */
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------------------ */
/* The hooks: key, what, site, bytes replaced, offset of a ms_modelInfoPtrs   */
/* displacement in them (-1: none), the bytes as gta_sa.exe 1.0 US has them   */
/* ------------------------------------------------------------------------ */
#define HOOKS(H) \
    H(misc1, "CPed shadow render: missing shadow data", 0x5D9A6E, 6, -1, "\x8B\x44\x24\x18\x85\xC0") \
    H(misc2, "vehicle colmodel, coldata or suspension lines missing", 0x6B18B0, 9, -1, \
        "\x8B\x40\x2C\x8A\x8E\x29\x04\x00\x00") \
    H(misc4, "CAESoundManager::Service divide by zero", 0x4F02D2, 5, -1, "\x99\xF7\xF9\x03\xD5") \
    H(misc5, "ped model info with no model", 0x5DF949, 7, 3, "\x8B\x3C\x8D\xC8\xB0\xA9\x00") \
    H(misc6, "animation hierarchy null (MTA #5465)", 0x4D1750, 5, -1, "\x8B\x41\x10\x85\xC0") \
    H(misc7, "missing colmodel (MTA #5466)", 0x417BF8, 5, -1, "\x8B\x71\x2C\x85\xF6") \
    H(misc8, "missing 2d effect plugin (MTA #5468)", 0x73485D, 5, -1, "\x8B\x0C\x0A\x85\xC9") \
    H(misc9, "invalid projectile (MTA #5468)", 0x738B64, 6, -1, "\x8B\x46\x40\xF6\xC4\x01") \
    H(misc10, "invalid vector pointer (MTA #5468)", 0x5334FE, 6, -1, "\x8B\x11\x89\x54\x24\x00") \
    H(misc11, "invalid animation (MTA #5576)", 0x4D2C62, 5, -1, "\x8B\x41\x10\x85\xC0") \
    H(misc12, "invalid animation (MTA #5530)", 0x4D41C5, 5, -1, "\x8A\x47\x0B\x84\xC0") \
    H(misc13, "invalid animation pointer", 0x4D464E, 6, -1, "\x8A\x40\x0A\xC0\xE8\x05") \
    H(misc14, "CAEFrontendAudioEntity::AddAudioEvent before the volume table", 0x4DD4B5, 6, -1, \
        "\x81\xEC\xD4\x00\x00\x00") \
    H(freeze15, "the loading thread's wait, bounded", 0x156CDAE, 6, -1, "\xFF\x15\x88\x80\x85\x00") \
    H(misc16, "CPed::PlayFootSteps with no first association", 0x5E5815, 6, -1, "\x0F\xBF\x48\x2C\x33\xFF") \
    H(misc17, "RwFrameSetStaticPluginsSize null member", 0x7F120E, 6, -1, "\x8B\x80\x90\x00\x00\x00") \
    H(misc18, "CVehicleModelInfo::GetWheelPosn with no frame", 0x4C7DAD, 7, -1, "\x8B\x55\x40\x8B\x44\x24\x10") \
    H(misc19, "RwFrameCloneHierarchy bad data (breaks SkyGFX)", 0x7F0BF7, 6, -1, "\x8B\x86\x98\x00\x00\x00") \
    H(misc20, "CPlaceable::RemoveMatrix on null", 0x54F3B0, 6, -1, "\x83\xEC\x10\x8B\x41\x14") \
    H(matrix, "CMatrix::UpdateRwMatrix into a bad RwMatrix", 0x59AD70, 6, -1, "\x8B\x11\x8B\x44\x24\x04") \
    H(allocmatrix1, "CPlaceable::AllocateMatrix: a reused matrix keeps its attachment", 0x54F5A6, 8, -1, \
        "\x89\x70\x48\x89\x46\x14\x5E\xC3") \
    H(allocmatrix2, "CPlaceable::AllocateMatrix: a new matrix keeps its attachment", 0x54F5C5, 6, -1, \
        "\x89\x70\x48\x89\x46\x14") \
    H(misc21, "CTaskSimpleCarFallOut::FinishAnimFallOutCB with a stale task", 0x648EE0, 7, -1, \
        "\x8B\x44\x24\x08\x8B\x48\x10") \
    H(misc22, "CAnimBlendAssociation::Init with a bad node array", 0x4CEF08, 6, -1, "\x8B\x14\x82\x8B\x5E\x10") \
    H(misc23, "ComputeAnimDoorOffsets door index past 15", 0x6E3D10, 7, -1, "\x8B\x54\x24\x08\x8D\x04\x52") \
    H(misc24, "_RwFrameForAllChildren with no frame", 0x7F0DC8, 6, -1, "\x8B\x85\x98\x00\x00\x00") \
    H(misc25, "FinishAnimCarOpenDoorFromOutsideCB with no vehicle", 0x646026, 5, -1, "\x8D\x44\x24\x10\x50") \
    H(misc26, "CShotInfo::Update past its array", 0x739FA0, 6, -1, "\x8B\x3C\xAB\x4D\x85\xFF") \
    H(misc27, "CTaskComplexDieInCar ped with no vehicle (counted)", 0x6377FB, 7, -1, \
        "\x80\xBF\x84\x04\x00\x00\x02") \
    H(misc28, "ProcessGarageDoorBehaviour with no door", 0x44A4FD, 6, -1, "\x8B\x86\x70\x01\x00\x00") \
    H(misc29, "IsSoundBankLoaded with a slot out of range", 0x4E022C, 5, -1, "\x0F\xBF\x44\x24\x08") \
    H(misc43, "CAEMP3BankLoader::GetSoundBuffer with a slot out of range", 0x4E028C, 5, -1, "\x53\x8B\x19\x55\x56") \
    H(misc45, "CAEMP3BankLoader::GetSoundBuffer with impossible slot data", 0x4E02E8, 5, -1, "\x66\x81\xFF\x90\x01") \
    H(misc44, "CAEMP3BankLoader::GetSoundBuffer past the slot buffer", 0x4E0347, 7, -1, "\x8B\x11\x66\x8B\x44\x17\x1C") \
    H(misc47, "CAEMP3BankLoader::Service: one sound larger than the buffer", 0x4DFE7D, 5, -1, "\x8B\x53\xE6\x8B\x3A") \
    H(misc48, "CAEMP3BankLoader::Service: a bank larger than the buffer", 0x4DFF78, 5, -1, "\x8B\x53\xE6\x8B\x3A") \
    H(misc49, "CAEMP3BankLoader::Service: a bad size or bank from the stream", 0x4E0094, 5, -1, "\x2B\x08\x89\x4B\xEE") \
    H(misc30, "CAnimBlendAssociation::SetFinishCallback on null", 0x4CEBE8, 7, -1, "\xC7\x41\x30\x01\x00\x00\x00") \
    H(misc32, "CAnimBlendAssociation::SetCurrentTime with no hierarchy", 0x4CEA80, 8, -1, \
        "\x8B\x44\x24\x04\xD9\x44\x24\x04") \
    H(misc33, "RwTexDictionaryFindNamedTexture in a bad dictionary", 0x7F39F0, 5, -1, "\x8B\x44\x24\x04\x53") \
    H(misc34, "the car env-map pipeline with no env-map data", 0x5D9CB2, 6, -1, "\x0F\x84\x55\x01\x00\x00") \
    H(misc35, "RwTexDictionaryForAllTextures through a bad entry", 0x7F374A, 5, -1, "\x8B\x30\x83\xC0\xF8") \
    H(misc36, "RwTexDictionaryFindNamedTexture through a bad entry", 0x7F3A09, 6, -1, "\x8D\x43\xF8\x8D\x48\x10") \
    H(misc37, "RwTexDictionaryAddTexture to a bad list", 0x7F39B3, 5, -1, "\x89\x56\x04\x89\x11") \
    H(misc38, "skin geometry with no vertex buffer (out of video memory)", 0x7C91C0, 6, -1, \
        "\x8B\x30\x8D\x4C\x8C\x18") \
    H(vbv3d, "vertex instancing into an unlocked buffer", 0x752AD0, 7, -1, "\x51\x53\x55\x8B\x6C\x24\x10") \
    H(vbmorph, "morph vertex instancing into an unlocked buffer", 0x753B60, 5, -1, "\x51\x8B\x44\x24\x08") \
    H(vbweights, "skin weight instancing into an unlocked buffer", 0x752320, 7, -1, "\x8B\x44\x24\x04\x83\xEC\x10") \
    H(dynvb, "a dynamic vertex buffer that was never created", 0x7F5A36, 6, -1, "\x8B\x46\x08\x50\x8B\x08") \
    H(misc39, "CVehicleModelInfo::FindTextureCB with a long remap name", 0x4C7510, 5, -1, "\xA1\x88\xE6\xB4\x00") \
    H(translation, "CAnimBlendNode::GetCurrentTranslation past the key frames", 0x4CFCB5, 6, -1, \
        "\x53\x8A\x5A\x04\xD0\xEB") \
    H(assocgroups, "CAnimManager::CreateAnimAssocGroups for a model not loaded", 0x4D3D52, 5, 3, "\x8B\x04\x85\xC8\xB0") \
    H(draggedout, "CTaskComplexCarSlowBeDraggedOut with the vehicle gone", 0x6485AC, 6, -1, "\x8B\x88\x60\x04\x00\x00") \
    H(draggedout2, "CTaskComplexCarSlowBeDraggedOutAndStandUp with the vehicle gone", 0x648AAF, 6, -1, \
        "\x8B\x80\x84\x03\x00\x00") \
    H(matmul, "RwMatrixMultiply with a null matrix", 0x7F18B0, 6, -1, "\x8B\x0D\xBC\x79\xC9\x00") \
    H(train, "CTrain::ProcessControl: the rail distance wraps without looping", 0x6F8F89, 6, -1, \
        "\xD9\x86\xA8\x05\x00\x00") \
    H(placename, "CPlaceName::Process: in a vehicle with no vehicle", 0x571F39, 6, -1, "\x8B\x88\x6C\x04\x00\x00") \
    H(colliding, "CWorld::FindObjectsKindaCollidingSectorList with no colmodel", 0x56508C, 10, 3, \
        "\x8B\x04\x95\xC8\xB0\xA9\x00\x8B\x48\x14") \
    H(allatomics, "RpClumpForAllAtomics on a null clump", 0x749B70, 6, -1, "\x8B\x44\x24\x04\x53\x55") \
    H(firstassoc, "RpAnimBlendClumpGetFirstAssociation on a null clump", 0x4D6A70, 6, -1, "\x8B\x0D\x78\xF8\xB5\x00") \
    H(blendanim, "CAnimManager::BlendAnimation on a null clump", 0x4D4610, 7, -1, "\x83\xEC\x14\x8B\x4C\x24\x18") \
    H(fxprim, "FxPrim_c::Enable on null", 0x4A9F50, 10, -1, "\x8A\x44\x24\x04\x88\x41\x0C\xC2\x04\x00") \
    H(fire, "CFire::ProcessFire attached to nothing (MTA #1757)", 0x53A6FC, 9, -1, \
        "\x8B\x4E\x10\x8B\x81\x90\x05\x00\x00") \
    H(furniture, "Interior_c::Init: the furniture counter starts at 0", 0x593BF0, 6, -1, "\x64\xA1\x00\x00\x00\x00") \
    H(colalloc, "CColModel::MakeMultipleAlloc with corrupt shadow data", 0x40F740, 5, -1, "\xE9\xCB\x52\x15\x01") \
    H(misc50, "AreTexturesUsedByRequestedModels: a request with no model info", 0x156650D, 7, 3, \
        "\x8B\x0C\x95\xC8\xB0\xA9\x00") \
    H(misc51, "AreTexturesUsedByRequestedModels: a channel with no model info", 0x156658A, 7, 3, \
        "\x8B\x14\x85\xC8\xB0\xA9\x00") \
    H(misc52, "AreTexturesUsedByRequestedModels: a channel with no model info", 0x15665C9, 7, 3, \
        "\x8B\x0C\x85\xC8\xB0\xA9\x00") \
    H(txdnext, "CStreaming::GetNextFileOnCd: a freed texture dictionary", 0x408F6E, 7, -1, \
        "\x0F\xBF\x40\x06\x83\xF8\xFF") \
    H(txdconvert, "CStreaming::ConvertBufferToObject: a freed texture dictionary", 0x40C8CE, 7, -1, \
        "\x0F\xBF\x40\x06\x83\xF8\xFF") \
    H(contact, "CEventScanner::ScanForEvents: a destroyed contact entity", 0x6080B2, 10, -1, \
        "\x8B\x86\x84\x05\x00\x00\x85\xC0\x74\x58") \
    H(getalpha, "CVisibilityPlugins::GetClumpAlpha on a null clump", 0x732B20, 15, -1, \
        "\x8B\x44\x24\x04\x8B\x0D\x94\x60\x8D\x00\x8B\x44\x01\x04\xC3") \
    H(setalpha, "CVisibilityPlugins::SetClumpAlpha on a null clump", 0x732B00, 19, -1, \
        "\x8B\x44\x24\x08\x8B\x4C\x24\x04\x8B\x15\x94\x60\x8D\x00\x89\x44\x0A\x04\xC3") \
    H(pathnodes, "CPathFind: path nodes that failed to allocate", 0x156F966, 7, -1, "\x8B\x84\xBE\xA4\x0F\x00\x00") \
    H(los, "CWorld::ProcessLineOfSight from or to nowhere", 0x56BA00, 12, -1, \
        "\x83\xEC\x60\x66\x81\x3D\x78\xCD\xB7\x00\xFF\xFF") \
    H(losclear, "CWorld::GetIsLineOfSightClear from or to nowhere", 0x56A490, 12, -1, \
        "\x83\xEC\x5C\x66\x81\x3D\x78\xCD\xB7\x00\xFF\xFF") \
    H(camnan, "CCam::Process: a camera gone NaN set up again behind its target", 0x526FC0, 6, -1, \
        "\xDB\x05\x34\xFE\xB6\x00") \
    H(copyanim, "CAnimBlendAssocGroup::CopyAnimation of an id with no hierarchy", 0x4CE130, 6, -1, \
        "\x64\xA1\x00\x00\x00\x00") \
    H(frame, "CClumpModelInfo::GetFrameFromId: a neighbour for a missing frame", 0x4C53C0, 7, -1, \
        "\x83\xEC\x08\x8B\x44\x24\x10") \
    H(fxquality, "GetFxQuality: blob shadows for peds and five vehicles", 0x49EA50, 5, -1, "\x8B\x41\x54\xC3\x90") \
    H(shadow, "CShadows::StoreShadowForVehicle: the asking vehicle", 0x70BD9B, 9, -1, \
        "\x90\x90\x90\x90\x90\x83\xEC\x44\x53") \
    H(impact, "Fx_c::AddBulletImpact: sand and dust once per 500 ms", 0x49F3E8, 5, -1, "\xA1\x3C\xF0\xB6\x00") \
    H(mouse, "no SetCursorPos while the game is in the background", 0x745423, 8, -1, \
        "\x8B\x44\x24\x08\x8B\x4C\x24\x04") \
    H(stencilveh, "CStencilShadows::RenderForVehicle: a shadow mesh over 2048 vertices", 0x70FC87, 6, -1, \
        "\x85\xC0\xD9\x5C\x24\x6C") \
    H(stencilobj, "CStencilShadows::RenderForObject: a shadow mesh over 2048 vertices", 0x710471, 6, -1, \
        "\x85\xC0\xD9\x44\x24\x38") \
    H(ikchain, "IKChain_c::Init: a bone the ped's skin does not have", 0x6183B2, 6, -1, "\x50\xE8\x88\xC6\x11\x00") \
    H(followtask, "CCam::Process_FollowPed_SA: a followed ped with no primary task", 0x523E6B, 7, -1, \
        "\x8B\x10\x8B\xC8\xFF\x52\x10") \
    H(samp,"SA-MP's association helper: bad pointers", 0, 5, -1, "\x74\x12\x8B\x50\x10")

#define DECLARE(k, name, va, len, mi, stock) void cf_hook_##k(void); volatile LONG cf_n_##k;
HOOKS(DECLARE)

int cf_volumetric_shadows;              /* 0: blob shadows for every vehicle */
uint32_t cf_samp_check_site;

static int bad_write(void *p, size_t n)
{
    MEMORY_BASIC_INFORMATION mbi;
    uint8_t *at = p, *end = at + n;
    while (at < end) {
        if (!VirtualQuery(at, &mbi, sizeof mbi) || mbi.State != MEM_COMMIT || (mbi.Protect & PAGE_GUARD)
            || !(mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
            return 1;
        at = (uint8_t *)mbi.BaseAddress + mbi.RegionSize;
    }
    return 0;
}

int cf_bad_read(const void *p, unsigned n) { return IsBadReadPtr(p, n) != 0; }
int cf_bad_write(void *p, unsigned n) { return bad_write(p, n); }

/* ------------------------------------------------------------------------ */
/* CTaskSimpleCarFallOut::FinishAnimFallOutCB (misc21)                      */
/* ------------------------------------------------------------------------ */
static int is_vehicle(const uint8_t *p)
{
    const uint8_t *pool = *(const uint8_t **)0xB74494;                /* CPools::ms_pVehiclePool */
    if (!pool) return 0;
    const uint8_t *objects = *(const uint8_t **)pool, *flags = *(const uint8_t **)(pool + 4);
    int size = *(const int *)(pool + 8);
    if (p < objects || (uint32_t)(p - objects) % 0xA18) return 0;    /* sizeof(CHeli) */
    uint32_t i = (uint32_t)(p - objects) / 0xA18;
    return i < (uint32_t)size && !(flags[i] & 0x80);
}

/* MTA's test: the callback's data is a CTaskSimpleCarFallOut, and a vehicle
 * the pool does not hold is dropped (the callback handles none). */
int cf_fall_out_valid(void *assoc, uint32_t *task)
{
    (void)assoc;
    if (IsBadReadPtr(task, 0x14) || task[0] != 0x86EFD0) return 0;
    if (task[4] && !is_vehicle((const uint8_t *)task[4])) {
        task[4] = 0;
        InterlockedIncrement(&cf_n_misc21);
    }
    return 1;
}

/* ------------------------------------------------------------------------ */
/* CAnimBlendAssocGroup::CopyAnimation                                       */
/* ------------------------------------------------------------------------ */
typedef struct { uint8_t pad[16]; void *hierarchy; } StaticAssoc;   /* 0x14 bytes */
typedef struct { void *block; StaticAssoc *assocs; int count; int idOffset; int groupId; } AssocGroup;

/* The weapon fire anim (224) from a group without it comes from the Python
 * group (MTA #1109); an id with no hierarchy becomes the nearest one with. */
void cf_copy_animation(AssocGroup **group, int *animId)
{
    AssocGroup *g = *group, *groups = *(AssocGroup **)0xB4EA34;     /* CAnimManager::ms_aAnimAssocGroups */
    if (*animId == 224 && groups && g >= groups && ((uint8_t *)g - (uint8_t *)groups) % sizeof *g == 0) {
        int id = (int)(g - groups);
        if (id < 256 && id != 28 && (id < 11 || id > 32)) {          /* grenade; python .. goggles */
            *group = g = &groups[11];
            InterlockedIncrement(&cf_n_copyanim);
        }
    }
    int use = *animId - g->idOffset;
    if (g->assocs && use >= 0 && use < g->count && g->assocs[use].hierarchy == NULL) {
        int best = use;
        for (int i = 0; i < g->count; i++) {
            if (!g->assocs[i].hierarchy) continue;
            int dNew = use - i < 0 ? i - use : use - i;
            int dCur = use - best < 0 ? best - use : use - best;
            if (dNew < dCur || best == use) best = i;
        }
        if (best != use) InterlockedIncrement(&cf_n_copyanim);
        use = best;
    }
    *animId = use + g->idOffset;
}

/* ------------------------------------------------------------------------ */
/* CClumpModelInfo::GetFrameFromId: a neighbour frame for a missing one      */
/* ------------------------------------------------------------------------ */
void *__cdecl cf_frame_original(void *clump, int id);      /* the stolen bytes, then the game */

void *cf_frame_fallback(void *found, uint32_t calledFrom, void *clump, int id)
{
    if (found) return found;
    /* callers that expect NULL */
    if (calledFrom == 0x6D308F || calledFrom == 0x6D30BF || calledFrom == 0x4C7DDE || calledFrom == 0x4C96BD)
        return NULL;
    if (calledFrom < 0x401000 || calledFrom > 0x801000) return NULL;   /* not the game's */
    for (int i = 2; i < 40; i++) {
        void *frame = cf_frame_original(clump, id + (i / 2) * ((i & 1) ? -1 : 1));
        if (frame) {
            InterlockedIncrement(&cf_n_frame);
            return frame;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------------ */
/* CVehicleModelInfo::FindTextureCB (misc39)                                 */
/* ------------------------------------------------------------------------ */
typedef uint8_t *(__cdecl *FindNamedTexture)(void *dict, const char *name);
typedef void *(__cdecl *GetCurrentDictionary)(void);

/* The game's lookup; its copy of a "remap" name into 32 bytes is bounded. */
uint8_t *__cdecl cf_find_texture(const char *name)
{
    FindNamedTexture find = (FindNamedTexture)0x7F39F0;
    GetCurrentDictionary current = (GetCurrentDictionary)0x7F3A90;
    if (!name) return NULL;
    uint8_t *tex = find(*(void **)0xB4E688, name);                  /* the vehicle TXD */
    if (tex) return tex;
    tex = find(current(), name);
    if (strncmp(name, "remap", 5)) return tex;
    if (tex) {
        tex[0x10] = '#';
        return tex;
    }
    char remap[32];
    size_t n = strnlen(name, sizeof remap);
    if (n == sizeof remap) {
        InterlockedIncrement(&cf_n_misc39);
        return NULL;
    }
    memcpy(remap, name, n + 1);
    remap[0] = '#';
    return find(current(), remap);
}

/* ------------------------------------------------------------------------ */
/* CColModel::MakeMultipleAlloc, CTrain, line of sight                       */
/* ------------------------------------------------------------------------ */
/* Shadow counts no model has, or without their arrays, are dropped before
 * CCollisionData::Copy reads them. */
void cf_check_col_data(uint8_t *colModel)
{
    uint8_t *data = colModel ? *(uint8_t **)(colModel + 0x2C) : NULL;
    if (!data) return;
    uint32_t tris = *(uint32_t *)(data + 0x20), verts = *(uint32_t *)(data + 0x24);
    if (tris > 5000 || verts > 10000 || (verts && !*(void **)(data + 0x28)) || (tris && !*(void **)(data + 0x2C))) {
        memset(data + 0x20, 0, 16);
        data[7] &= ~0x14;                                           /* has shadow info, has shadow */
        InterlockedIncrement(&cf_n_colalloc);
    }
}

/* The rail distance into [0, length) at once; the game adds or subtracts the
 * length until it fits, which never ends with no length. */
void cf_wrap_rail_distance(float *distance, const float *length)
{
    float d = *distance, len = *length;
    if (d >= 0.0f && d < len) return;
    if (!(len >= 1.0f) || !(d > -1e30f && d < 1e30f)) {
        *distance = 0.0f;
        InterlockedIncrement(&cf_n_train);
        return;
    }
    if (d > 0.0f) d = fmodf(d, len);
    else {
        d = fmodf(-d, len);
        d = d > 0.0f ? len - d : 0.0f;
    }
    *distance = d;
}

/* x and y as MTA's; z wider than MTA's -5000..100000, since only x and y
 * enter the sector walk and servers keep objects and players at z -10000 */
static int valid_position(const float *v)
{
    return v[0] >= -16000.0f && v[0] <= 16000.0f && v[1] >= -16000.0f && v[1] <= 16000.0f
        && v[2] >= -1000000.0f && v[2] <= 1000000.0f;                 /* false for NaN */
}

/* MTA's: an end out of the world moves next to the other one */
void cf_limit_line(float *start, float *end, volatile LONG *count)
{
    int s = valid_position(start), e = valid_position(end);
    if (s && e) return;
    if (!s && !e) {
        start[0] = start[1] = end[0] = end[1] = 0.0f;
        start[2] = 100.0f;
        end[2] = 100.01f;
    } else if (!s) {
        start[0] = end[0], start[1] = end[1], start[2] = end[2] + 0.01f;
    } else {
        end[0] = start[0], end[1] = start[1], end[2] = start[2] + 0.01f;
    }
    InterlockedIncrement(count);
}

/* ------------------------------------------------------------------------ */
/* CCam::Process                                                            */
/* ------------------------------------------------------------------------ */
static int readable(const void *p, size_t n);

/* A CCam's floats by offset: its angles and speeds, bump, vectors, distances */
static const uint16_t CAM_FLOATS[][2] = {{0x2C, 0x110}, {0x118, 0x120}, {0x124, 0x208}, {0x220, 0x22C}};

/* A camera with a NaN or an infinity feeds it back to itself every frame and
 * the world is not drawn. Its bad floats are zeroed and it is set up again
 * as a change of mode does (CCam::Init, its mode kept), behind its target. */
void cf_cam_check(uint8_t *cam)
{
    int bad = 0;
    for (int r = 0; r < 4; r++)
        for (int o = CAM_FLOATS[r][0]; o < CAM_FLOATS[r][1]; o += 4)
            if ((*(uint32_t *)(cam + o) & 0x7F800000u) == 0x7F800000u) *(float *)(cam + o) = 0.0f, bad = 1;
    if (!bad) return;
    uint16_t mode = *(uint16_t *)(cam + 0x0C);
    ((void (__attribute__((thiscall)) *)(void *))0x50E490)(cam);
    *(uint16_t *)(cam + 0x0C) = mode;
    const uint8_t *target = *(uint8_t **)(cam + 0x21C);
    if (!target) target = *(uint8_t **)0xB7CD98;                   /* the player's ped */
    const float *m = target && readable(target, 0x18) ? *(const float **)(target + 0x14) : NULL;
    if (m && readable(m, 0x40)) {
        float fx = m[4], fy = m[5], len = sqrtf(fx * fx + fy * fy);
        if (valid_position(m + 12) && len > 0.01f) {               /* false for NaN */
            float *front = (float *)(cam + 0x190), *source = (float *)(cam + 0x19C);
            fx /= len, fy /= len;
            source[0] = m[12] - 4.0f * fx, source[1] = m[13] - 4.0f * fy, source[2] = m[14] + 1.0f;
            front[0] = fx * 0.9701425f, front[1] = fy * 0.9701425f, front[2] = -0.2425356f;    /* (4, -1) normalised */
            memcpy(cam + 0x1A8, source, 12);                          /* the source before looking behind */
        }
    }
    InterlockedIncrement(&cf_n_camnan);
}

/* ------------------------------------------------------------------------ */
/* IKChain_c::Init                                                          */
/* ------------------------------------------------------------------------ */
typedef struct { int16_t bone, prev; float limits[9]; } BoneInfo;  /* BoneNodeManager_c::ms_boneInfos */
typedef int (__cdecl *BoneIndexFn)(void *hierarchy, int bone);

/* Each bone from the effector to the pivot is in the hierarchy and the bone
 * table; the game indexes the frames with -1 for one that is not. */
int cf_ik_bones_valid(void *hierarchy, int effector, int pivot)
{
    const BoneIndexFn index = (BoneIndexFn)0x7C51A0;               /* RpHAnimIDGetIndex */
    const BoneInfo *info = (const BoneInfo *)0x8D26D0;
    if (!hierarchy) return 0;
    for (int bone = effector, depth = 0; bone != pivot; depth++) {
        int i = 0;
        if (depth == 32 || index(hierarchy, bone) < 0) return 0;
        while (i < 32 && info[i].bone != bone) i++;
        if (i == 32 || info[i].prev < 0) return 0;
        bone = info[i].prev;
    }
    return index(hierarchy, pivot) >= 0;
}

/* ------------------------------------------------------------------------ */
/* FX quality, bullet impacts, the cursor, the loading wait                 */
/* ------------------------------------------------------------------------ */
uint32_t cf_fx_quality;
uint16_t cf_shadow_model;

void cf_alter_fx_quality(uint32_t calledFrom)
{
    if (calledFrom > 0x70F990 && calledFrom < 0x711EB0) {      /* CVolumetricShadowMgr */
        if (!cf_volumetric_shadows) cf_fx_quality = 0;
        switch (cf_shadow_model) {                             /* broken volumetric shadows */
        case 460: case 511: case 572: case 590: case 592: cf_fx_quality = 0;
        }
        cf_shadow_model = 0;
    } else if (calledFrom > 0x5E65A0 && calledFrom < 0x5E7680) {   /* CPed::PreRenderAfterTest */
        cf_fx_quality = 0;
    }
}

static DWORD sand_dust_until;
int cf_bullet_impact_type(int type)
{
    /* sand (2) and dust (4) at most once per 500 ms; the rest are sparks */
    if (type == 2 || type == 4) {
        DWORD now = GetTickCount();
        if ((LONG)(now - sand_dust_until) >= 0) sand_dust_until = now + 500;
        else type = 1;
    }
    return type;
}

int cf_game_foreground(void)
{
    HWND game = *(HWND *)0xC97C1C;     /* RsGlobal.ps->window */
    return game != NULL && game == GetForegroundWindow();
}

void cf_bounded_wait(HANDLE h)
{
    if (WaitForSingleObject(h, 4000) == WAIT_TIMEOUT) {
        InterlockedIncrement(&cf_n_freeze15);
        WaitForSingleObject(h, 1000);
    }
}

/* ------------------------------------------------------------------------ */
/* The hook bodies                                                          */
/* ------------------------------------------------------------------------ */
/* Each is entered by a jmp written over the site's first bytes and leaves by
 * GO, a jmp to the address MTA's hook returns to. */
__asm__(
".intel_syntax noprefix\n"
".macro AVERTED key\n"
"    pushfd\n"
"    lock inc dword ptr [_cf_n_\\key]\n"
"    popfd\n"
".endm\n"
".macro GO addr\n"
"    jmp dword ptr [.Lgo\\@]\n"
".Lgo\\@: .long \\addr\n"
".endm\n"
".macro HOOK key\n"
".globl _cf_hook_\\key\n"
"_cf_hook_\\key:\n"
".endm\n"
".data\n"
"frame_result: .long 0\n"
"impact_type:  .long 0\n"
".text\n"

/* CPed shadow render, [eax+esi] null */
"HOOK misc1\n"
"    mov eax, dword ptr [esp+0x18]\n"
"    test eax, eax\n"
"    je 1f\n"
"    mov eax, dword ptr ds:0x8D12CC\n"
"    mov ecx, dword ptr [eax+esi]\n"
"    test ecx, ecx\n"
"    jne 1f\n"
"    AVERTED misc1\n"
"    xor ecx, ecx\n"
"1:  GO 0x5D9A74\n"

/* vehicle colmodel, coldata or suspension lines missing */
"HOOK misc2\n"
"    test eax, eax\n"
"    je 1f\n"
"    mov eax, dword ptr [eax+0x2C]\n"
"    test eax, eax\n"
"    je 1f\n"
"    mov ebx, dword ptr [eax+0x10]\n"
"    test ebx, ebx\n"
"    je 1f\n"
"    mov cl, byte ptr [esi+0x429]\n"
"    GO 0x6B18B9\n"
"1:  AVERTED misc2\n"
"    GO 0x6B3775\n"

/* CAESoundManager::Service divide by zero */
"HOOK misc4\n"
"    test ecx, ecx\n"
"    je 1f\n"
"    cdq\n"
"    idiv ecx\n"
"    add edx, ebp\n"
"    GO 0x4F02D7\n"
"1:  AVERTED misc4\n"
"    GO 0x4F0B07\n"

/* ped model info has no model */
"HOOK misc5\n"
"    mov edi, dword ptr ds:0x403DA7\n"
"    mov edi, dword ptr [edi+ecx*4]\n"
"    test edi, edi\n"
"    je 1f\n"
"    cmp dword ptr [edi+0x5C], 0\n"
"    je 1f\n"
"    GO 0x5DF950\n"
"1:  AVERTED misc5\n"
"    pop edi\n"
"    GO 0x5DFCC4\n"

/* anim null (MTA #5465) */
"HOOK misc6\n"
"    test ecx, ecx\n"
"    je 1f\n"
"    mov eax, dword ptr [ecx+0x10]\n"
"    test eax, eax\n"
"    GO 0x4D1755\n"
"1:  AVERTED misc6\n"
"    GO 0x4D1A44\n"

/* missing colmodel (MTA #5466) */
"HOOK misc7\n"
"    test ecx, ecx\n"
"    je 1f\n"
"    mov esi, dword ptr [ecx+0x2C]\n"
"    test esi, esi\n"
"    GO 0x417BFD\n"
"1:  AVERTED misc7\n"
"    GO 0x417BFF\n"

/* missing 2d effect plugin (MTA #5468) */
"HOOK misc8\n"
"    test ecx, ecx\n"
"    je 1f\n"
"    mov ecx, dword ptr [edx+ecx]\n"
"    test ecx, ecx\n"
"    GO 0x734862\n"
"1:  AVERTED misc8\n"
"    GO 0x734871\n"

/* invalid projectile (MTA #5468) */
"HOOK misc9\n"
"    test esi, esi\n"
"    je 1f\n"
"    mov eax, dword ptr [esi+0x40]\n"
"    test ah, 1\n"
"    GO 0x738B6A\n"
"1:  AVERTED misc9\n"
"    GO 0x73983A\n"

/* invalid vector pointer (MTA #5468): the output vector zeroed */
"HOOK misc10\n"
"    cmp ecx, 0x80\n"
"    jb 1f\n"
"    mov edx, dword ptr [ecx]\n"
"    mov dword ptr [esp], edx\n"
"    GO 0x533504\n"
"1:  AVERTED misc10\n"
"    mov ecx, dword ptr [esp+0x1C]\n"
"    mov dword ptr [ecx], 0\n"
"    mov dword ptr [ecx+4], 0\n"
"    mov dword ptr [ecx+8], 0\n"
"    GO 0x533539\n"

/* invalid anim (MTA #5576) */
"HOOK misc11\n"
"    test ecx, ecx\n"
"    je 1f\n"
"    mov eax, dword ptr [ecx+0x10]\n"
"    test eax, eax\n"
"    GO 0x4D2C67\n"
"1:  AVERTED misc11\n"
"    GO 0x4D2E03\n"

/* invalid anim (MTA #5530) */
"HOOK misc12\n"
"    test edi, edi\n"
"    je 1f\n"
"    mov al, byte ptr [edi+0x0B]\n"
"    test al, al\n"
"    GO 0x4D41CA\n"
"1:  AVERTED misc12\n"
"    GO 0x4D4222\n"

/* invalid anim pointer */
"HOOK misc13\n"
"    cmp eax, 0x2480\n"
"    jb 1f\n"
"    mov al, byte ptr [eax+0x0A]\n"
"    shr al, 5\n"
"    GO 0x4D4654\n"
"1:  AVERTED misc13\n"
"    GO 0x4D4764\n"

/* CAEFrontendAudioEntity::AddAudioEvent before the volume table exists:
 * its SEH frame (3 dwords) is unwound before `ret 0Ch` */
"HOOK misc14\n"
"    mov eax, dword ptr ds:0xBD00F8\n"
"    test eax, eax\n"
"    je 1f\n"
"    sub esp, 0xD4\n"
"    GO 0x4DD4BB\n"
"1:  AVERTED misc14\n"
"    mov eax, dword ptr [esp]\n"
"    mov dword ptr fs:0, eax\n"
"    add esp, 12\n"
"    ret 12\n"

/* the loading wait, bounded (the site is call [WaitForSingleObject]) */
"HOOK freeze15\n"
"    pop eax\n"                      /* handle */
"    pop edx\n"                      /* timeout */
"    pushad\n"
"    push eax\n"
"    call _cf_bounded_wait\n"
"    add esp, 4\n"
"    popad\n"
"    GO 0x156CDB4\n"

/* CPed::PlayFootSteps with no first association: the function's own
 * epilogue (its arg, edi, esi, 48h of locals, the SEH frame) */
"HOOK misc16\n"
"    test eax, eax\n"
"    je 1f\n"
"    movsx ecx, word ptr [eax+0x2C]\n"
"    xor edi, edi\n"
"    GO 0x5E581B\n"
"1:  AVERTED misc16\n"
"    add esp, 4\n"
"    pop edi\n"
"    pop esi\n"
"    add esp, 0x48\n"
"    mov eax, dword ptr [esp]\n"
"    mov dword ptr fs:0, eax\n"
"    add esp, 12\n"
"    ret\n"

/* RwFrameSetStaticPluginsSize null member */
"HOOK misc17\n"
"    test eax, eax\n"
"    je 1f\n"
"    mov eax, dword ptr [eax+0x90]\n"
"    GO 0x7F1214\n"
"1:  AVERTED misc17\n"
"    GO 0x7F1236\n"

/* CVehicleModelInfo::GetWheelPosn with no frame: a zero position */
"HOOK misc18\n"
"    test ebp, ebp\n"
"    je 1f\n"
"    mov edx, dword ptr [ebp+0x40]\n"
"    mov eax, dword ptr [esp+0x10]\n"
"    GO 0x4C7DB4\n"
"1:  AVERTED misc18\n"
"    xor edx, edx\n"
"    mov eax, dword ptr [esp+0x10]\n"
"    mov dword ptr [eax], edx\n"
"    mov dword ptr [eax+4], edx\n"
"    pop esi\n"
"    mov dword ptr [eax+8], edx\n"
"    pop ebp\n"
"    ret 12\n"

/* RwFrameCloneHierarchy bad data (off by default: SkyGFX) */
"HOOK misc19\n"
"    test esi, esi\n"
"    je 1f\n"
"    mov eax, dword ptr [esi+0x98]\n"
"    GO 0x7F0BFD\n"
"1:  AVERTED misc19\n"
"    mov edx, dword ptr [ecx+0x98]\n"
"    test edx, edx\n"
"    GO 0x7F0C20\n"

/* CPlaceable::RemoveMatrix on null */
"HOOK misc20\n"
"    test ecx, ecx\n"
"    je 1f\n"
"    sub esp, 0x10\n"
"    mov eax, dword ptr [ecx+0x14]\n"
"    GO 0x54F3B6\n"
"1:  AVERTED misc20\n"
"    ret\n"

/* CMatrix::UpdateRwMatrix (thiscall, the RwMatrix*) into a null, low or
 * unaligned matrix */
"HOOK matrix\n"
"    mov eax, dword ptr [esp+4]\n"
"    test al, 3\n"
"    jnz 1f\n"
"    cmp eax, 0x10000\n"
"    jb 1f\n"
"    mov edx, dword ptr [ecx]\n"
"    GO 0x59AD76\n"
"1:  AVERTED matrix\n"
"    xor eax, eax\n"
"    ret 4\n"

/* CPlaceable::AllocateMatrix's two tails: the CMatrixLink's attach matrix
 * cleared as it is handed out */
"HOOK allocmatrix1\n"
/* (6 bytes, not MTA's 8: the early return at 0x54F568 jumps to the pop esi /
 * ret at 0x54F5CB, which MTA's padding NOPs) */
"HOOK allocmatrix2\n"
"    mov dword ptr [eax+0x48], esi\n"
"    mov dword ptr [esi+0x14], eax\n"
"    mov dword ptr [eax+0x40], 0\n"
"    mov byte ptr [eax+0x44], 0\n"
"    pop esi\n"
"    ret\n"

/* CTaskSimpleCarFallOut::FinishAnimFallOutCB (assoc, task) */
"HOOK misc21\n"
"    pushad\n"
"    push dword ptr [esp+32+8]\n"
"    push dword ptr [esp+32+8]\n"
"    call _cf_fall_out_valid\n"
"    add esp, 8\n"
"    test eax, eax\n"
"    popad\n"
"    je 1f\n"
"    mov eax, dword ptr [esp+8]\n"
"    mov ecx, dword ptr [eax+0x10]\n"
"    GO 0x648EE7\n"
"1:  AVERTED misc21\n"
"    ret\n"

/* CAnimBlendAssociation::Init with a bad node array: zeroed nodes */
"HOOK misc22\n"
"    mov edx, dword ptr [edi+0x0C]\n"
"    cmp edx, 0x480\n"
"    jb 3f\n"
"2:  mov edx, dword ptr [edi+0x0C]\n"
"    mov edx, dword ptr [edx+eax*4]\n"
"    mov ebx, dword ptr [esi+0x10]\n"
"    mov dword ptr [ebx+ecx+0x10], edx\n"
"    mov edx, dword ptr [esi+0x10]\n"
"    mov dword ptr [edx+ecx+0x14], esi\n"
"    movsx edx, word ptr [esi+0x0C]\n"
"    inc eax\n"
"    add ecx, 0x18\n"
"    cmp eax, edx\n"
"    jl 2b\n"
"    GO 0x4CEF25\n"
"3:  AVERTED misc22\n"
"4:  xor edx, edx\n"
"    mov ebx, dword ptr [esi+0x10]\n"
"    mov dword ptr [ebx+ecx+0x10], edx\n"
"    mov dword ptr [ebx+ecx+0x14], edx\n"
"    movsx edx, word ptr [esi+0x0C]\n"
"    inc eax\n"
"    add ecx, 0x18\n"
"    cmp eax, edx\n"
"    jl 4b\n"
"    GO 0x4CEF25\n"

/* ComputeAnimDoorOffsets door index past 15 */
"HOOK misc23\n"
"    mov edx, dword ptr [esp+8]\n"
"    cmp edx, 16\n"
"    jb 1f\n"
"    mov dword ptr [esp+8], 0\n"
"    AVERTED misc23\n"
"1:  mov edx, dword ptr [esp+8]\n"
"    lea eax, [edx+edx*2]\n"
"    GO 0x6E3D17\n"

/* _RwFrameForAllChildren with no frame */
"HOOK misc24\n"
"    cmp ebp, 0x480\n"
"    jb 1f\n"
"    mov eax, dword ptr [ebp+0x98]\n"
"    GO 0x7F0DCE\n"
"1:  AVERTED misc24\n"
"    xor ebp, ebp\n"
"    xor eax, eax\n"
"    GO 0x7F0DCE\n"

/* FinishAnimCarOpenDoorFromOutsideCB with no vehicle */
"HOOK misc25\n"
"    mov eax, dword ptr [esi+0x10]\n"
"    test eax, eax\n"
"    jz 1f\n"
"    lea eax, [esp+0x10]\n"
"    push eax\n"
"    GO 0x64602B\n"
"1:  AVERTED misc25\n"
"    pop esi\n"
"    pop ecx\n"
"    ret\n"

/* CShotInfo::Update past its array */
"HOOK misc26\n"
"    cmp ebx, 0x130\n"
"    jz 1f\n"
"    mov edi, dword ptr [ebx+ebp*4]\n"
"    dec ebp\n"
"    test edi, edi\n"
"    GO 0x739FA6\n"
"1:  AVERTED misc26\n"
"    xor edi, edi\n"
"    dec ebp\n"
"    test edi, edi\n"
"    GO 0x739FA6\n"

/* CTaskComplexDieInCar: a ped marked in a vehicle with none (counted only) */
"HOOK misc27\n"
"    cmp byte ptr [edi+0x484], 2\n"
"    je 1f\n"
"    mov ecx, dword ptr [edi+0x58C]\n"
"    test ecx, ecx\n"
"    jne 1f\n"
"    AVERTED misc27\n"
"1:  GO 0x637802\n"

/* ProcessGarageDoorBehaviour with no door object */
"HOOK misc28\n"
"    mov eax, dword ptr [esi+0x170]\n"
"    test eax, eax\n"
"    jne 1f\n"
"    AVERTED misc28\n"
"    GO 0x44A650\n"
"1:  GO 0x44A503\n"

/* CAEMP3BankLoader::IsSoundBankLoaded (this, bank, slot): the slot below 0
 * or past the slot count */
"HOOK misc29\n"
"    movsx eax, word ptr [esp+8]\n"
"    test eax, eax\n"
"    js 1f\n"
"    cmp ax, word ptr [ecx+0x0C]\n"
"    jae 1f\n"
"    GO 0x4E0231\n"
"1:  AVERTED misc29\n"
"    GO 0x4E0227\n"

/* CAEMP3BankLoader::GetSoundBuffer (this, sound, slot, &size, &rate): the
 * slot out of range returns no buffer */
"HOOK misc43\n"
"    movsx eax, word ptr [esp+8]\n"
"    test eax, eax\n"
"    js 1f\n"
"    cmp ax, word ptr [ecx+0x0C]\n"
"    jae 1f\n"
"    push ebx\n"
"    mov ebx, dword ptr [ecx]\n"
"    push ebp\n"
"    push esi\n"
"    GO 0x4E0291\n"
"1:  AVERTED misc43\n"
"    xor eax, eax\n"
"    ret 0x10\n"

/* the slot's sound count (dx) 1..400 or -1, the sound (di) below 400, its
 * bank below the lookup count */
"HOOK misc45\n"
"    cmp dx, 0xFFFF\n"
"    je 2f\n"
"    test dx, dx\n"
"    jle 1f\n"
"    cmp dx, 0x190\n"
"    jg 1f\n"
"2:  cmp di, 0x190\n"
"    jae 1f\n"
"    mov ax, word ptr [ebx+esi+0x10]\n"
"    cmp ax, word ptr [ecx+0x0E]\n"
"    jae 1f\n"
"    cmp di, 0x190\n"
"    GO 0x4E02ED\n"
"1:  AVERTED misc45\n"
"    GO 0x4E036C\n"

/* the sound's offset plus its size inside the slot's buffer */
"HOOK misc44\n"
"    mov edx, dword ptr [esp+0x1C]\n"
"    mov eax, dword ptr [edx]\n"
"    add eax, dword ptr [edi+ebx+0x14]\n"
"    jc 1f\n"
"    cmp eax, dword ptr [ebx+esi+4]\n"
"    ja 1f\n"
"    mov edx, dword ptr [ecx]\n"
"    mov ax, word ptr [edi+edx+0x1C]\n"
"    GO 0x4E034E\n"
"1:  AVERTED misc44\n"
"    pop edi\n"
"    pop esi\n"
"    pop ebp\n"
"    xor eax, eax\n"
"    pop ebx\n"
"    ret 0x10\n"

/* CAEMP3BankLoader::Service (this in ebp, the request at ebx-1Ah): the
 * stream's byte count clamped to the room left in the buffer and the slot;
 * none left drops the request */
".macro BANKCLAMP key, ok, drop\n"
"HOOK \\key\n"
"    mov edx, dword ptr [ebx-0x1A]\n"
"    mov edi, dword ptr [edx]\n"
"    push eax\n"
"    push ecx\n"
"    mov eax, dword ptr [ebp+0x18]\n"
"    sub eax, edi\n"
"    jbe 1f\n"
"    mov ecx, dword ptr [edx+4]\n"
"    cmp eax, ecx\n"
"    jb 2f\n"
"    mov eax, ecx\n"
"2:  cmp dword ptr [ebx-0x12], eax\n"
"    jbe 3f\n"
"    mov dword ptr [ebx-0x12], eax\n"
"    AVERTED \\key\n"
"3:  pop ecx\n"
"    pop eax\n"
"    GO \\ok\n"
"1:  pop ecx\n"
"    pop eax\n"
"    AVERTED \\key\n"
"    GO \\drop\n"
".endm\n"
"BANKCLAMP misc47, 0x4DFE82, 0x4DFFED\n"
"BANKCLAMP misc48, 0x4DFF7D, 0x4DFFED\n"

/* the single-sound read: a bank out of range drops it, a byte count below
 * zero or past the slot is clamped */
"HOOK misc49\n"
"    sub ecx, dword ptr [eax]\n"
"    movsx edx, word ptr [ebx-2]\n"
"    test edx, edx\n"
"    js 1f\n"
"    cmp dx, word ptr [ebp+0x0E]\n"
"    jae 1f\n"
"    mov edx, dword ptr [ebx-0x1A]\n"
"    mov edi, dword ptr [edx+4]\n"
"    test ecx, ecx\n"
"    js 2f\n"
"    cmp ecx, edi\n"
"    jbe 3f\n"
"2:  mov ecx, edi\n"
"    AVERTED misc49\n"
"3:  mov dword ptr [ebx-0x12], ecx\n"
"    GO 0x4E0099\n"
"1:  AVERTED misc49\n"
"    GO 0x4DFFED\n"

/* CAnimBlendAssociation::SetFinishCallback on null */
"HOOK misc30\n"
"    test ecx, ecx\n"
"    jz 1f\n"
"    mov dword ptr [ecx+0x30], 1\n"
"    GO 0x4CEBEF\n"
"1:  AVERTED misc30\n"
"    ret 8\n"

/* CAnimBlendAssociation::SetCurrentTime on null or with no hierarchy */
"HOOK misc32\n"
"    test ecx, ecx\n"
"    jz 1f\n"
"    cmp dword ptr [ecx+0x14], 0\n"
"    jz 1f\n"
"    mov eax, dword ptr [esp+4]\n"
"    fld dword ptr [esp+4]\n"
"    GO 0x4CEA88\n"
"1:  AVERTED misc32\n"
"    ret 4\n"

/* RwTexDictionaryFindNamedTexture (dict, name): no name or a bad dictionary
 * finds nothing; something not a dictionary is the current one */
"HOOK misc33\n"
"    mov ecx, dword ptr [esp+4]\n"
"    mov edx, dword ptr [esp+8]\n"
"    test edx, edx\n"
"    jz 1f\n"
"    cmp ecx, 0x10000\n"
"    jb 1f\n"
"    cmp byte ptr [ecx], 6\n"
"    jne 2f\n"
"    cmp dword ptr [ecx+8], 0\n"
"    je 1f\n"
"3:  mov eax, dword ptr [esp+4]\n"
"    push ebx\n"
"    GO 0x7F39F5\n"
"2:  mov eax, 0x7F3A90\n"           /* RwTexDictionaryGetCurrent */
"    call eax\n"
"    test eax, eax\n"
"    jz 1f\n"
"    mov dword ptr [esp+4], eax\n"
"    AVERTED misc33\n"
"    jmp 3b\n"
"1:  AVERTED misc33\n"
"    xor eax, eax\n"
"    ret\n"

/* the car env-map pipeline: the EnvWave flag set with no env-map data (ebp) */
"HOOK misc34\n"
"    jz 2f\n"
"    cmp ebp, 0x10000\n"
"    jb 1f\n"
"    GO 0x5D9CB8\n"
"1:  AVERTED misc34\n"
"2:  GO 0x5D9E0D\n"

/* RwTexDictionaryForAllTextures: a list entry not readable ends the walk */
"HOOK misc35\n"
"    cmp eax, 0x10000\n"
"    jb 1f\n"
"    pushad\n"
"    push 4\n"
"    push eax\n"
"    call _cf_bad_read\n"
"    add esp, 8\n"
"    test eax, eax\n"
"    popad\n"
"    jnz 1f\n"
"    mov esi, dword ptr [eax]\n"
"    add eax, -8\n"
"    GO 0x7F374F\n"
"1:  AVERTED misc35\n"
"    GO 0x7F3760\n"

/* RwTexDictionaryFindNamedTexture: an entry whose name is not readable finds
 * nothing */
"HOOK misc36\n"
"    cmp ebx, 0x10000\n"
"    jb 1f\n"
"    lea eax, [ebx-8]\n"
"    lea ecx, [eax+0x10]\n"
"    pushad\n"
"    push 1\n"
"    push ecx\n"
"    call _cf_bad_read\n"
"    add esp, 8\n"
"    test eax, eax\n"
"    popad\n"
"    jnz 1f\n"
"    GO 0x7F3A0F\n"
"1:  AVERTED misc36\n"
"    GO 0x7F3A5C\n"

/* RwTexDictionaryAddTexture: the list head (ecx) links in only when its first
 * entry (esi) is writable and points back to it */
"HOOK misc37\n"
"    cmp esi, 0x10000\n"
"    jb 1f\n"
"    pushad\n"
"    push 8\n"
"    push esi\n"
"    call _cf_bad_write\n"
"    add esp, 8\n"
"    test eax, eax\n"
"    popad\n"
"    jnz 1f\n"
"    cmp esi, ecx\n"
"    je 2f\n"
"    cmp dword ptr [esi+4], ecx\n"
"    je 2f\n"
"1:  AVERTED misc37\n"
"    GO 0x7F39B8\n"
"2:  mov dword ptr [esi+4], edx\n"
"    mov dword ptr [ecx], edx\n"
"    GO 0x7F39B8\n"

/* __rpD3D9SkinGeometryReinstance: a stream with no vertex buffer is skipped */
"HOOK misc38\n"
"    mov esi, dword ptr [eax]\n"
"    test esi, esi\n"
"    jz 1f\n"
"    lea ecx, [esp+ecx*4+0x18]\n"
"    GO 0x7C91C6\n"
"1:  AVERTED misc38\n"
"    GO 0x7C91DA\n"

/* __rpD3D9VertexDeclarationInst* (type, mem, ...) into a buffer whose Lock
 * failed: nothing written, the type's size returned */
".macro VBSIZE key\n"
"1:  AVERTED \\key\n"
"    mov ecx, dword ptr [esp+4]\n"
"    mov eax, dword ptr [ecx*4+0x874EF8]\n"   /* D3D9VertexTypeSize */
"    ret\n"
".endm\n"
"HOOK vbv3d\n"
"    cmp dword ptr [esp+8], 0x10000\n"
"    jb 1f\n"
"    push ecx\n"
"    push ebx\n"
"    push ebp\n"
"    mov ebp, dword ptr [esp+0x10]\n"
"    GO 0x752AD7\n"
"VBSIZE vbv3d\n"
"HOOK vbmorph\n"
"    cmp dword ptr [esp+8], 0x10000\n"
"    jb 1f\n"
"    push ecx\n"
"    mov eax, dword ptr [esp+8]\n"
"    GO 0x753B65\n"
"VBSIZE vbmorph\n"
"HOOK vbweights\n"
"    cmp dword ptr [esp+8], 0x10000\n"
"    jb 1f\n"
"    mov eax, dword ptr [esp+4]\n"
"    sub esp, 0x10\n"
"    GO 0x752327\n"
"VBSIZE vbweights\n"

/* _RwD3D9DynamicVertexBufferCreate: an entry whose buffer was never created
 * is not released */
"HOOK dynvb\n"
"    mov eax, dword ptr [esi+8]\n"
"    test eax, eax\n"
"    jz 1f\n"
"    push eax\n"
"    mov ecx, dword ptr [eax]\n"
"    GO 0x7F5A3C\n"
"1:  AVERTED dynvb\n"
"    GO 0x7F5A8F\n"

/* CVehicleModelInfo::FindTextureCB, whole */
"HOOK misc39\n"
"    jmp _cf_find_texture\n"

/* CAnimBlendNode::GetCurrentTranslation: an end key frame (eax) past the
 * sequence's (edx) frames leaves with no translation, through the
 * function's own exit */
"HOOK translation\n"
"    push ebx\n"
"    movzx ebx, word ptr [edx+6]\n"
"    cmp eax, ebx\n"
"    jae 1f\n"
"    mov bl, byte ptr [edx+4]\n"
"    shr bl, 1\n"
"    GO 0x4CFCBB\n"
"1:  pop ebx\n"
"    AVERTED translation\n"
"    GO 0x4CFD7B\n"

/* CAnimManager::CreateAnimAssocGroups: a model (eax) with no model info or
 * no RwObject gets no associations */
"HOOK assocgroups\n"
"    mov edx, dword ptr ds:0x403DA7\n"
"    cmp eax, 20000\n"
"    jae 2f\n"
"    mov eax, dword ptr [edx+eax*4]\n"
"    test eax, eax\n"
"    jz 1f\n"
"    cmp dword ptr [eax+0x1C], 0\n"
"    jz 1f\n"
"    GO 0x4D3D59\n"
"1:  AVERTED assocgroups\n"
"    xor ebx, ebx\n"
"    GO 0x4D3D71\n"
"2:  mov eax, dword ptr [edx+eax*4]\n"
"    GO 0x4D3D59\n"

/* CTaskComplexCarSlowBeDraggedOut::CreateFirstSubTask with no vehicle (eax):
 * no subtask */
"HOOK draggedout\n"
"    test eax, eax\n"
"    jz 1f\n"
"    mov ecx, dword ptr [eax+0x460]\n"
"    GO 0x6485B2\n"
"1:  AVERTED draggedout\n"
"    GO 0x6485E1\n"

/* ...AndStandUp: the same */
"HOOK draggedout2\n"
"    test eax, eax\n"
"    jz 1f\n"
"    mov eax, dword ptr [eax+0x384]\n"
"    GO 0x648AB5\n"
"1:  AVERTED draggedout2\n"
"    pop edi\n"
"    pop esi\n"
"    ret 4\n"

/* RwMatrixMultiply (dst, a, b): a matrix below 480h leaves dst as it is */
"HOOK matmul\n"
"    cmp dword ptr [esp+4], 0x480\n"
"    jb 1f\n"
"    cmp dword ptr [esp+8], 0x480\n"
"    jb 1f\n"
"    cmp dword ptr [esp+0x0C], 0x480\n"
"    jb 1f\n"
"    mov ecx, dword ptr ds:0xC979BC\n"
"    GO 0x7F18B6\n"
"1:  AVERTED matmul\n"
"    mov eax, dword ptr [esp+4]\n"
"    ret\n"

/* CTrain::ProcessControl: the wrap of the rail distance (esi+5A8h) by the
 * track length (esp+10h) */
"HOOK train\n"
"    pushad\n"
"    lea eax, [esp+32+0x10]\n"
"    push eax\n"
"    lea eax, [esi+0x5A8]\n"
"    push eax\n"
"    call _cf_wrap_rail_distance\n"
"    add esp, 8\n"
"    popad\n"
"    GO 0x6F8FE5\n"

/* CPlaceName::Process: the player ped (eax) in a vehicle with none is read
 * as on foot */
"HOOK placename\n"
"    mov ecx, dword ptr [eax+0x46C]\n"
"    test ch, 1\n"
"    jz 1f\n"
"    cmp dword ptr [eax+0x58C], 0\n"
"    jne 1f\n"
"    and ch, 0xFE\n"
"    AVERTED placename\n"
"1:  GO 0x571F3F\n"

/* CWorld::FindObjectsKindaCollidingSectorList: an entity (model in edx) with
 * no model info or colmodel is passed over, its distance popped */
"HOOK colliding\n"
"    mov eax, dword ptr ds:0x403DA7\n"
"    mov eax, dword ptr [eax+edx*4]\n"
"    test eax, eax\n"
"    jz 1f\n"
"    mov ecx, dword ptr [eax+0x14]\n"
"    test ecx, ecx\n"
"    jz 1f\n"
"    GO 0x565096\n"
"1:  fstp st(0)\n"
"    AVERTED colliding\n"
"    GO 0x5650C3\n"

/* RpClumpForAllAtomics, RpAnimBlendClumpGetFirstAssociation and
 * CAnimManager::BlendAnimation on a null clump return null */
"HOOK allatomics\n"
"    mov eax, dword ptr [esp+4]\n"
"    test eax, eax\n"
"    jnz 1f\n"
"    AVERTED allatomics\n"
"    ret\n"
"1:  push ebx\n"
"    push ebp\n"
"    GO 0x749B76\n"
"HOOK firstassoc\n"
"    mov eax, dword ptr [esp+4]\n"
"    test eax, eax\n"
"    jnz 1f\n"
"    AVERTED firstassoc\n"
"    ret\n"
"1:  mov ecx, dword ptr ds:0xB5F878\n"
"    GO 0x4D6A76\n"
"HOOK blendanim\n"
"    mov eax, dword ptr [esp+4]\n"
"    test eax, eax\n"
"    jnz 1f\n"
"    AVERTED blendanim\n"
"    ret\n"
"1:  sub esp, 0x14\n"
"    mov ecx, dword ptr [esp+0x18]\n"
"    GO 0x4D4617\n"

/* FxPrim_c::Enable, whole */
"HOOK fxprim\n"
"    test ecx, ecx\n"
"    jz 1f\n"
"    mov al, byte ptr [esp+4]\n"
"    mov byte ptr [ecx+0x0C], al\n"
"    ret 4\n"
"1:  AVERTED fxprim\n"
"    ret 4\n"

/* CFire::ProcessFire (fire in esi): put out, or attached to nothing, it goes
 * to the game's own extinguish and return */
"HOOK fire\n"
"    test byte ptr [esi], 1\n"
"    jz 2f\n"
"    mov ecx, dword ptr [esi+0x10]\n"
"    test ecx, ecx\n"
"    jz 1f\n"
"    mov eax, dword ptr [ecx+0x590]\n"
"    GO 0x53A705\n"
"1:  AVERTED fire\n"
"2:  mov ecx, esi\n"
"    GO 0x53A69C\n"

/* Interior_c::Init: InteriorManager_c's furniture counter starts at 0 */
"HOOK furniture\n"
"    mov dword ptr ds:0xBB3A18, 0\n"
"    mov eax, dword ptr fs:0\n"
"    GO 0x593BF6\n"

/* CColModel::MakeMultipleAlloc's thunk: the data checked first */
"HOOK colalloc\n"
"    pushad\n"
"    push ecx\n"
"    call _cf_check_col_data\n"
"    add esp, 4\n"
"    popad\n"
"    GO 0x1564A10\n"

/* CStreaming::AreTexturesUsedByRequestedModels: a model below 0 or with no
 * model info is passed over */
"HOOK misc50\n"
"    test edx, edx\n"
"    js 1f\n"
"    mov ecx, dword ptr ds:0x403DA7\n"
"    mov ecx, dword ptr [ecx+edx*4]\n"
"    test ecx, ecx\n"
"    jz 1f\n"
"    GO 0x1566514\n"
"1:  AVERTED misc50\n"
"    GO 0x1566552\n"
"HOOK misc51\n"
"    test eax, eax\n"
"    js 1f\n"
"    mov edx, dword ptr ds:0x403DA7\n"
"    mov edx, dword ptr [edx+eax*4]\n"
"    test edx, edx\n"
"    jz 1f\n"
"    GO 0x1566591\n"
"1:  AVERTED misc51\n"
"    GO 0x15665BB\n"
"HOOK misc52\n"
"    test eax, eax\n"
"    js 1f\n"
"    mov ecx, dword ptr ds:0x403DA7\n"
"    mov ecx, dword ptr [ecx+eax*4]\n"
"    test ecx, ecx\n"
"    jz 1f\n"
"    GO 0x15665D0\n"
"1:  AVERTED misc52\n"
"    GO 0x1566600\n"

/* CStreaming::GetNextFileOnCd: a TXD request whose slot was freed goes to
 * the next request; ConvertBufferToObject drops it */
"HOOK txdnext\n"
"    test eax, eax\n"
"    jz 1f\n"
"    movsx eax, word ptr [eax+6]\n"
"    cmp eax, -1\n"
"    GO 0x408F75\n"
"1:  AVERTED txdnext\n"
"    GO 0x409001\n"
"HOOK txdconvert\n"
"    test eax, eax\n"
"    jz 1f\n"
"    movsx eax, word ptr [eax+6]\n"
"    cmp eax, -1\n"
"    GO 0x40C8D5\n"
"1:  AVERTED txdconvert\n"
"    GO 0x40C8FA\n"

/* CEventScanner::ScanForEvents: a contact entity whose model can't be read */
"HOOK contact\n"
"    mov eax, dword ptr [esi+0x584]\n"
"    test eax, eax\n"
"    jz 2f\n"
"    pushad\n"
"    add eax, 0x22\n"
"    push 2\n"
"    push eax\n"
"    call _cf_bad_read\n"
"    add esp, 8\n"
"    test eax, eax\n"
"    popad\n"
"    jnz 1f\n"
"    GO 0x6080BC\n"
"1:  AVERTED contact\n"
"2:  GO 0x608114\n"

/* CVisibilityPlugins::GetClumpAlpha and SetClumpAlpha, whole: a null clump
 * is opaque and keeps nothing */
"HOOK getalpha\n"
"    mov eax, dword ptr [esp+4]\n"
"    test eax, eax\n"
"    jz 1f\n"
"    mov ecx, dword ptr ds:0x8D6094\n"
"    mov eax, dword ptr [ecx+eax+4]\n"
"    ret\n"
"1:  AVERTED getalpha\n"
"    mov eax, 0xFF\n"
"    ret\n"
"HOOK setalpha\n"
"    mov ecx, dword ptr [esp+4]\n"
"    test ecx, ecx\n"
"    jz 1f\n"
"    mov eax, dword ptr [esp+8]\n"
"    mov edx, dword ptr ds:0x8D6094\n"
"    mov dword ptr [edx+ecx+4], eax\n"
"    ret\n"
"1:  AVERTED setalpha\n"
"    ret\n"

/* CPathFind's node loader: an area whose nodes failed to allocate loops over
 * none */
"HOOK pathnodes\n"
"    mov eax, dword ptr [esi+edi*4+0xFA4]\n"
"    cmp dword ptr [esi+edi*4+0x804], 0\n"
"    jne 1f\n"
"    test eax, eax\n"
"    jz 1f\n"
"    xor eax, eax\n"
"    AVERTED pathnodes\n"
"1:  GO 0x156F96D\n"

/* CWorld::ProcessLineOfSight and GetIsLineOfSightClear (start, end, ...) */
"HOOK los\n"
"    pushad\n"
"    push offset _cf_n_los\n"
"    push dword ptr [esp+36+8]\n"
"    push dword ptr [esp+40+4]\n"
"    call _cf_limit_line\n"
"    add esp, 12\n"
"    popad\n"
"    sub esp, 0x60\n"
"    cmp word ptr ds:0xB7CD78, 0xFFFF\n"
"    GO 0x56BA0C\n"
"HOOK losclear\n"
"    pushad\n"
"    push offset _cf_n_losclear\n"
"    push dword ptr [esp+36+8]\n"
"    push dword ptr [esp+40+4]\n"
"    call _cf_limit_line\n"
"    add esp, 12\n"
"    popad\n"
"    sub esp, 0x5C\n"
"    cmp word ptr ds:0xB7CD78, 0xFFFF\n"
"    GO 0x56A49C\n"

/* CCam::Process, this in ecx: the camera checked first */
"HOOK camnan\n"
"    pushad\n"
"    push ecx\n"
"    call _cf_cam_check\n"
"    add esp, 4\n"
"    popad\n"
"    fild dword ptr ds:0xB6FE34\n"
"    GO 0x526FC6\n"

/* CCam::Process_FollowPed_SA: the followed ped's active primary task in eax,
 * null for a ped with none (spectating one): past the use-gun check */
"HOOK followtask\n"
"    test eax, eax\n"
"    jz 1f\n"
"    mov edx, dword ptr [eax]\n"
"    mov ecx, eax\n"
"    call dword ptr [edx+0x10]\n"
"    GO 0x523E72\n"
"1:  AVERTED followtask\n"
"    GO 0x523EA0\n"

/* CAnimBlendAssocGroup::CopyAnimation (this, id): the group and the id */
"HOOK copyanim\n"
"    pushad\n"
"    lea eax, [esp+32+4]\n"
"    push eax\n"
"    lea eax, [esp+4+24]\n"          /* the saved ecx */
"    push eax\n"
"    call _cf_copy_animation\n"
"    add esp, 8\n"
"    popad\n"
"    mov eax, dword ptr fs:0\n"
"    GO 0x4CE136\n"

/* CClumpModelInfo::GetFrameFromId (cdecl clump, id) */
"HOOK frame\n"
"    push dword ptr [esp+8]\n"
"    push dword ptr [esp+8]\n"
"    call _cf_frame_original\n"
"    add esp, 8\n"
"    pushad\n"
"    push dword ptr [esp+32+8]\n"   /* id */
"    push dword ptr [esp+32+8]\n"   /* clump */
"    push dword ptr [esp+32+8]\n"   /* our caller */
"    push eax\n"
"    call _cf_frame_fallback\n"
"    add esp, 16\n"
"    mov dword ptr [frame_result], eax\n"
"    popad\n"
"    mov eax, dword ptr [frame_result]\n"
"    ret\n"
".globl _cf_frame_original\n"
"_cf_frame_original:\n"
"    sub esp, 8\n"
"    mov eax, dword ptr [esp+0x10]\n"
"    GO 0x4C53C7\n"

/* GetFxQuality, whole: blob shadows for peds and the broken vehicles */
"HOOK fxquality\n"
"    pushad\n"
"    mov eax, dword ptr [ecx+0x54]\n"
"    mov dword ptr [_cf_fx_quality], eax\n"
"    push dword ptr [esp+32]\n"
"    call _cf_alter_fx_quality\n"
"    add esp, 4\n"
"    popad\n"
"    mov eax, dword ptr [_cf_fx_quality]\n"
"    ret\n"

/* CShadows::StoreShadowForVehicle: which vehicle asks. Back in time for the
 * call at 0x70BDA4, which ProperShaders replaces. */
"HOOK shadow\n"
"    mov eax, dword ptr [esp+4]\n"
"    mov ax, word ptr [eax+0x22]\n"
"    mov word ptr [_cf_shadow_model], ax\n"
"    sub esp, 0x44\n"
"    push ebx\n"
"    GO 0x70BDA4\n"

/* Fx_c::AddBulletImpact: the impact type (esi) rate-limited */
"HOOK impact\n"
"    pushad\n"
"    push esi\n"
"    call _cf_bullet_impact_type\n"
"    add esp, 4\n"
"    mov dword ptr [impact_type], eax\n"
"    popad\n"
"    mov esi, dword ptr [impact_type]\n"
"    mov eax, dword ptr ds:0xB6F03C\n"
"    GO 0x49F3ED\n"

/* no SetCursorPos while another window has focus */
"HOOK mouse\n"
"    mov eax, dword ptr [esp+8]\n"
"    mov ecx, dword ptr [esp+4]\n"
"    pushad\n"
"    call _cf_game_foreground\n"
"    test eax, eax\n"
"    popad\n"
"    jz 1f\n"
"    GO 0x74542B\n"
"1:  GO 0x745433\n"

/* CStencilShadows::RenderForVehicle: eax shadow vertices go into room for
 * 2048; more skips this shadow. The test last: its flags feed a jle. */
"HOOK stencilveh\n"
"    fstp dword ptr [esp+0x6C]\n"
"    cmp eax, 2048\n"
"    ja 1f\n"
"    test eax, eax\n"
"    GO 0x70FC8D\n"
"1:  AVERTED stencilveh\n"
"    GO 0x7102D8\n"                 /* the epilogue */

/* CStencilShadows::RenderForObject: the same */
"HOOK stencilobj\n"
"    cmp eax, 2048\n"
"    ja 1f\n"
"    test eax, eax\n"
"    fld dword ptr [esp+0x38]\n"
"    GO 0x710477\n"
"1:  AVERTED stencilobj\n"
"    GO 0x710AB2\n"

/* IKChain_c::Init, the clump in eax: the effector's chain checked first */
"HOOK ikchain\n"
"    push eax\n"
"    mov eax, 0x734A40\n"           /* GetAnimHierarchyFromSkinClump */
"    call eax\n"
"    pushad\n"
"    push dword ptr [esp+0x54]\n"   /* the pivot bone */
"    push dword ptr [esp+0x48]\n"   /* the effector bone */
"    push eax\n"
"    call _cf_ik_bones_valid\n"
"    add esp, 12\n"
"    test eax, eax\n"
"    popad\n"
"    jz 1f\n"
"    GO 0x6183B8\n"
"1:  AVERTED ikchain\n"
"    add esp, 4\n"
"    GO 0x6184A3\n"                 /* return false */

/* SA-MP's association helper: a bad pointer from
 * RpAnimBlendClumpGetAssociation or in its +10h returns 0 */
"HOOK samp\n"
"    je 1f\n"
"    cmp eax, 1000\n"
"    jb 2f\n"
"    mov edx, dword ptr [eax+0x10]\n"
"    cmp edx, 1000\n"
"    jb 2f\n"
"    mov eax, dword ptr [_cf_samp_check_site]\n"
"    add eax, 5\n"
"    jmp eax\n"
"2:  AVERTED samp\n"
"1:  mov eax, dword ptr [_cf_samp_check_site]\n"
"    add eax, 0x14\n"
"    jmp eax\n"
".att_syntax\n");

#include "framerate.c"
#include "vigilante.c"
#include "mixsets.c"
#include "hfps.c"
#include "pedmatrix.c"
#include "glasslist.c"
#include "sky.c"
#include "faults.c"
#include "stream.c"
#include "drawcount.c"
#include "parked.c"

/* ------------------------------------------------------------------------ */
/* The table                                                                */
/* ------------------------------------------------------------------------ */
#define MAX_LEN 32

enum { KIND_JMP, KIND_NOP, KIND_PTR, KIND_CALL };   /* a jmp to the hook; NOPs; a dword = to; call to */

typedef struct {
    const char *key, *name;
    uint32_t va;
    uint8_t len;
    int8_t mi;                  /* offset of a ms_modelInfoPtrs displacement, -1: none */
    const char *stock;          /* the bytes it replaces, as gta_sa.exe 1.0 US has them */
    void (*hook)(void);
    volatile LONG *averted;
    const char *option;         /* the fixes option that switches it */
    uint8_t kind;
    const void *to;
    int installed;
    uint8_t saved[MAX_LEN];
} Hook;

#define ROW(k, name, va, len, mi, stock) {#k, name, va, len, mi, stock, cf_hook_##k, &cf_n_##k, "crashfix"},
#define FROW(k, option, name, va, len, stock) {#k, name, va, len, -1, stock, cf_hook_##k, &cf_n_##k, option},
#define PROW(k, option, kind, name, va, len, stock, to) {#k, name, va, len, -1, stock, NULL, &cf_n_##k, option, kind, to},
static Hook hooks[] = { HOOKS(ROW) PEDMATRIX(FROW, PROW) FRAMERATE(FROW, PROW) VIGILANTE(FROW, PROW) MIXSETS(FROW, PROW) HFPS(FROW, PROW) GLASSLIST(PROW) SKY(FROW, PROW) STREAM(FROW) DRAWCOUNT(FROW, PROW) PARKED(FROW) };
#define HOOK_COUNT ((int)(sizeof hooks / sizeof hooks[0]))

/* Whether the row `key` is in place (the hooks that depend on another's) */
int cf_installed(const char *key)
{
    for (int i = 0; i < HOOK_COUNT; i++) if (!strcmp(hooks[i].key, key)) return hooks[i].installed;
    return 0;
}

static int samp_hook(void)
{
    for (int i = 0; i < HOOK_COUNT; i++) if (!strcmp(hooks[i].key, "samp")) return i;
    return -1;
}

static int readable(const void *p, size_t n)
{
    MEMORY_BASIC_INFORMATION mbi;
    const uint8_t *at = p, *end = at + n;
    while (at < end) {
        if (!VirtualQuery(at, &mbi, sizeof mbi) || mbi.State != MEM_COMMIT
            || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) || mbi.Protect == 0) return 0;
        at = (const uint8_t *)mbi.BaseAddress + mbi.RegionSize;
    }
    return 1;
}

static int write_code(uint32_t va, const uint8_t *bytes, size_t n)
{
    DWORD old;
    if (!VirtualProtect((void *)va, n, PAGE_EXECUTE_READWRITE, &old)) return 0;
    memcpy((void *)va, bytes, n);
    VirtualProtect((void *)va, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void *)va, n);
    return memcmp((void *)va, bytes, n) == 0;
}

static HMODULE self(void)
{
    HMODULE h = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)self, &h);
    return h;
}

/* The site as stock, or as stock with ms_modelInfoPtrs where it is now */
static int stock_at(const Hook *h, const uint8_t *site)
{
    if (memcmp(site, h->stock, h->len) == 0) return 1;
    if (h->mi < 0 || !readable((void *)0x403DA7, 4)) return 0;
    uint8_t want[MAX_LEN + 4];
    memcpy(want, h->stock, h->len);
    memcpy(want + h->mi, (const void *)0x403DA7, 4);
    return memcmp(site, want, h->len) == 0;
}

/* ------------------------------------------------------------------------ */
/* Exports                                                                  */
/* ------------------------------------------------------------------------ */
__declspec(dllexport) int __cdecl CrashFix_Count(void) { return HOOK_COUNT; }

__declspec(dllexport) const char *__cdecl CrashFix_Key(int i) { return i >= 0 && i < HOOK_COUNT ? hooks[i].key : NULL; }

__declspec(dllexport) const char *__cdecl CrashFix_Name(int i) { return i >= 0 && i < HOOK_COUNT ? hooks[i].name : NULL; }

__declspec(dllexport) uint32_t __cdecl CrashFix_Address(int i) { return i >= 0 && i < HOOK_COUNT ? hooks[i].va : 0; }

/* The SA-MP hook's site (the pattern 74 12 8B 50 10 8B 02 in samp.dll), set
 * before it is installed. */
__declspec(dllexport) int __cdecl CrashFix_SetSampSite(uint32_t va)
{
    int s = samp_hook();
    if (s < 0 || hooks[s].installed) return 0;
    hooks[s].va = va;
    cf_samp_check_site = va;
    return 1;
}

/* SA-MP's moving-object test (the pattern D9 44 24 24 D8 5C 24 20 DF E0 F6 C4 01
 * 0F 85, once in samp.dll): sampmove there, samprot 12Ch on, where its turn
 * is. -> 1 set, 0 the turn is not where it should be */
__declspec(dllexport) int __cdecl CrashFix_SetSampObjectSite(uint32_t va)
{
    int rot = -1, move = -1;
    for (int i = 0; i < HOOK_COUNT; i++) {
        if (!strcmp(hooks[i].key, "samprot")) rot = i;
        if (!strcmp(hooks[i].key, "sampmove")) move = i;
    }
    if (rot < 0 || move < 0 || hooks[rot].installed || hooks[move].installed) return 0;
    if (!readable((void *)va, 0x140) || memcmp((void *)(va + 0x12C), hooks[rot].stock, hooks[rot].len)) return 0;
    hooks[move].va = va, hooks[rot].va = va + 0x12C;
    cf_samp_rot_back = va + 0x12C + 10;
    cf_samp_arrived = va + 19;
    cf_samp_moving = va + 19 + *(const int32_t *)(va + 15);
    return 1;
}

__declspec(dllexport) const char *__cdecl CrashFix_Option(int i) { return i >= 0 && i < HOOK_COUNT ? hooks[i].option : NULL; }

__declspec(dllexport) void __cdecl CrashFix_SetShadows(int volumetric) { cf_volumetric_shadows = volumetric != 0; }

/* Where a jmp or call written over a site goes: E9/E8 rel32, or FF 25/FF 15
 * through a pointer (SA-MP's hooks keep theirs in the padding before the
 * function). 0: not one. */
static uint32_t hook_target(uint32_t va, const uint8_t *site)
{
    if (site[0] == 0xE9 || site[0] == 0xE8) return va + 5 + *(const uint32_t *)(site + 1);
    if (site[0] == 0xFF && (site[1] == 0x25 || site[1] == 0x15)) {
        const uint32_t *slot = *(const uint32_t **)(site + 2);
        return readable(slot, 4) ? *slot : 0;
    }
    return 0;
}

/* The bytes a hook of ours puts at its site */
static void ours(const Hook *h, uint8_t *code)
{
    memset(code, 0x90, MAX_LEN);
    switch (h->kind) {
    case KIND_JMP:
        code[0] = 0xE9;
        *(uint32_t *)(code + 1) = (uint32_t)h->hook - (h->va + 5);
        /* the jump sits in the padding; the function's entry hops back to it */
        if (!strcmp(h->key, "shadow")) code[5] = 0xEB, code[6] = 0xF9;
        break;
    case KIND_CALL: code[0] = 0xE8; *(uint32_t *)(code + 1) = (uint32_t)h->to - (h->va + 5); break;
    case KIND_PTR:  *(uint32_t *)code = (uint32_t)h->to; break;
    }
}

/* -> 1 installed now, 2 already ours, 3 another hook (found = its target),
 *    0 other bytes (found = their first four), -1 no such site */
__declspec(dllexport) int __cdecl CrashFix_Install(int i, uint32_t *found)
{
    if (found) *found = 0;
    if (i < 0 || i >= HOOK_COUNT || !hooks[i].va) return -1;
    Hook *h = &hooks[i];
    if (h->installed) return 2;
    if (!readable((void *)h->va, h->len)) return -1;
    const uint8_t *site = (const uint8_t *)h->va;
    if (!stock_at(h, site)) {
        uint32_t target = readable(site, 6) ? hook_target(h->va, site) : 0;
        if (target) {
            HMODULE mine = self(), owner = NULL;
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)target, &owner);
            if (found) *found = target;
            return owner && owner == mine ? 2 : 3;
        }
        if (found) memcpy(found, site, 4);
        return 0;
    }
    uint8_t code[MAX_LEN];
    ours(h, code);
    memcpy(h->saved, site, h->len);
    if (!write_code(h->va, code, h->len)) {
        write_code(h->va, h->saved, h->len);
        return 0;
    }
    h->installed = 1;
    return 1;
}

/* -> 1 back to the bytes it found, 0 not ours there */
__declspec(dllexport) int __cdecl CrashFix_Uninstall(int i)
{
    if (i < 0 || i >= HOOK_COUNT || !hooks[i].installed) return 0;
    Hook *h = &hooks[i];
    uint8_t code[MAX_LEN];
    ours(h, code);
    if (memcmp((const void *)h->va, code, h->len)) return 0;
    if (!write_code(h->va, h->saved, h->len)) return 0;
    h->installed = 0;
    if (!strcmp(h->key, "tick")) cf_frame_due = 1;       /* the gates pass again */
    if (!strcmp(h->key, "boatfoam")) cf_boat_foam_stock();
    return 1;
}

__declspec(dllexport) int __cdecl CrashFix_Installed(int i) { return i >= 0 && i < HOOK_COUNT && hooks[i].installed; }

__declspec(dllexport) uint32_t __cdecl CrashFix_Averted(int i) { return i >= 0 && i < HOOK_COUNT ? (uint32_t)*hooks[i].averted : 0; }
