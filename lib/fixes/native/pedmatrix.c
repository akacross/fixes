/* MTA's checks on a ped's bone matrices (multitheftauto/mtasa-blue,
 * CMultiplayerSA.cpp's CheckAnimMatrix and CMultiplayerSA_Rendering.cpp's
 * RenderPedCB, GPLv3), for gta_sa.exe 1.0 US; included by crashfix.c and
 * switched by the pedmatrix option.
 *
 * A bone matrix gone bad (NaN, or a size no skeleton has) draws a ped's
 * triangles across the screen. pedanim zeroes a bad bone's matrix as the
 * animation builds it, so that bone and its children shrink to nothing;
 * pedskin leaves out a ped atomic whose skin matrices left the world. Where
 * they differ from MTA's:
 *   - a zeroed bone sits at its parent's origin (MTA's 1 above it).
 *   - every element of a skin matrix is checked (MTA's tests y where z's
 *     upper bound should be).
 */

/* ------------------------------------------------------------------------ */
/* The rows: key, option, what, site, bytes, stock                          */
/* ------------------------------------------------------------------------ */
#define PEDMATRIX(FJ, FP) \
    FJ(pedanim, "pedmatrix", "RpHAnimHierarchyUpdateMatrices: a bone matrix with NaN or past any skeleton zeroed", \
        0x7C5A5C, 5, "\x8D\x4C\x24\x54\x50") \
    FJ(pedskin, "pedmatrix", "CVisibilityPlugins::RenderPedCB: a ped whose bone matrices left the world not drawn", \
        0x7335B0, 6, "\x56\x57\x8B\x7C\x24\x0C")

PEDMATRIX(FDECLARE_J, FDECLARE_P)

/* The 12 elements of an RwMatrix (right, up, at, pos), each under `limit`
 * as a float's magnitude bits: false for NaN and infinities too */
static const uint8_t MATRIX_ROT[9] = {0, 1, 2, 4, 5, 6, 8, 9, 10}, MATRIX_POS[3] = {12, 13, 14};
#define BITS_2 0x40000000u          /* 2.0f: no rotation reaches it */
#define BITS_10 0x41200000u         /* 10.0f: MTA's bound on a bone's offset */
#define BITS_1E5 0x47C35000u        /* 100000.0f: MTA's bound on a skin matrix */

static int below(const uint32_t *m, const uint8_t *at, int n, uint32_t limit)
{
    for (int i = 0; i < n; i++) if ((m[at[i]] & 0x7FFFFFFFu) >= limit) return 0;
    return 1;
}

/* A bone's matrix from its keyframe, before RwMatrixMultiply puts it under
 * its parent: zeroed if bad */
void cf_ped_anim(float *m)
{
    const uint32_t *b = (const uint32_t *)m;
    if (!m || (below(b, MATRIX_ROT, 9, BITS_2) && below(b, MATRIX_POS, 3, BITS_10 + 1))) return;
    for (int i = 0; i < 9; i++) m[MATRIX_ROT[i]] = 0.0f;
    for (int i = 0; i < 3; i++) m[MATRIX_POS[i]] = 0.0f;
    InterlockedIncrement(&cf_n_pedanim);
}

/* RpHAnimHierarchy: its matrices at +8 */
typedef struct { int32_t flags, count; float (*matrices)[16]; } SkinHierarchy;

/* -> false if any skin matrix of the atomic is NaN or past 100000 */
int cf_ped_skin_ok(const uint8_t *atomic)
{
    uint32_t offset = *(const uint32_t *)0xC978A4;             /* the skin plugin's atomic data */
    if (!atomic || !offset) return 1;
    const SkinHierarchy *h = *(const SkinHierarchy * const *)(atomic + offset);
    if (!h || !h->matrices || h->count <= 0 || h->count > 256) return 1;
    for (int i = 0; i < h->count; i++) {
        const uint32_t *b = (const uint32_t *)h->matrices[i];
        if (!below(b, MATRIX_ROT, 9, BITS_1E5) || !below(b, MATRIX_POS, 3, BITS_1E5)) return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------------ */
/* The hook bodies                                                          */
/* ------------------------------------------------------------------------ */
__asm__(
".intel_syntax noprefix\n"
/* RpHAnimHierarchyUpdateMatrices: the bone's matrix at [esp+54h] */
"HOOK pedanim\n"
"    lea ecx, [esp+0x54]\n"
"    pushad\n"
"    push ecx\n"
"    call _cf_ped_anim\n"
"    add esp, 4\n"
"    popad\n"
"    push eax\n"
"    GO 0x7C5A61\n"

/* CVisibilityPlugins::RenderPedCB(RpAtomic *): one left out returns its
 * atomic, as a drawn one does */
"HOOK pedskin\n"
"    push dword ptr [esp+4]\n"
"    call _cf_ped_skin_ok\n"
"    add esp, 4\n"
"    test eax, eax\n"
"    jz 1f\n"
"    push esi\n"
"    push edi\n"
"    mov edi, dword ptr [esp+0xC]\n"
"    GO 0x7335B6\n"
"1:  AVERTED pedskin\n"
"    mov eax, dword ptr [esp+4]\n"
"    ret\n"
".att_syntax\n");
