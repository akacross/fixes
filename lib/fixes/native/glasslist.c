/* San Andreas Online's growing vehicle alpha list (Dryxio/san-andreas-online,
 * CMultiplayerSA_Rendering.cpp, GPLv3, after Open Limit Adjuster's
 * InsertSorted retry), for gta_sa.exe 1.0 US; included by crashfix.c and
 * switched by the glasslist option.
 *
 * The game sorts a vehicle's see-through atomics (glass, lights, rotors) in a
 * list of 20 (0xC88070) it fills and draws per vehicle; past 20 it draws them
 * at once, unsorted, so glass hides what is behind it. The six calls that fill
 * it go through cf_insert_alpha, which gives a full list as many links again.
 * Where it differs from SA:O's:
 *   - the links come from a pool of ours, never freed: the list keeps them.
 *   - the world-entity list is left to the limit adjuster (AlphaEntityList).
 */

/* ------------------------------------------------------------------------ */
/* The rows: key, option, what, site, bytes, stock, a call to `to`           */
/* ------------------------------------------------------------------------ */
#define GLASSLIST(FP) \
    FP(glass1, "glasslist", KIND_CALL, "CVisibilityPlugins::InsertAtomicIntoSortedList", 0x733D35, 5, \
        "\xE8\xD6\xFB\xFF\xFF", cf_insert_alpha) \
    FP(glass2, "glasslist", KIND_CALL, "CVisibilityPlugins::RenderVehicleHiDetailAlphaCB", 0x734092, 5, \
        "\xE8\x79\xF8\xFF\xFF", cf_insert_alpha) \
    FP(glass3, "glasslist", KIND_CALL, "CVisibilityPlugins::RenderHeliRotorAlphaCB", 0x734141, 5, \
        "\xE8\xCA\xF7\xFF\xFF", cf_insert_alpha) \
    FP(glass4, "glasslist", KIND_CALL, "CVisibilityPlugins::RenderHeliTailRotorAlphaCB", 0x734211, 5, \
        "\xE8\xFA\xF6\xFF\xFF", cf_insert_alpha) \
    FP(glass5, "glasslist", KIND_CALL, "CVisibilityPlugins::RenderTrainHiDetailAlphaCB", 0x734356, 5, \
        "\xE8\xB5\xF5\xFF\xFF", cf_insert_alpha) \
    FP(glass6, "glasslist", KIND_CALL, "CVisibilityPlugins::RenderVehicleHiDetailAlphaCB_BigVehicle", 0x734482, 5, \
        "\xE8\x89\xF4\xFF\xFF", cf_insert_alpha)

#define GDECLARE(k, option, kind, name, va, len, stock, to) volatile LONG cf_n_##k;
GLASSLIST(GDECLARE)

/* ------------------------------------------------------------------------ */
/* CLinkList<AlphaObjectInfo>                                               */
/* ------------------------------------------------------------------------ */
typedef struct AlphaLink { void *object, *render; float distance; struct AlphaLink *prev, *next; } AlphaLink;
typedef struct { AlphaLink usedHead, usedTail, freeHead, freeTail; AlphaLink *links; } AlphaList;
typedef AlphaLink *(__attribute__((thiscall)) *InsertSortedFn)(AlphaList *list, const void *info);

#define ALPHA_LIST ((AlphaList *)0xC88070)          /* CVisibilityPlugins::m_alphaList */
#define GLASS_POOL 980                              /* 1000 with the game's 20 */
static AlphaLink glass_pool[GLASS_POOL];
int cf_glass_used;

/* In place of the call to CLinkList::InsertSorted (0x733910, thiscall); a
 * growth counts on the row of the site it came from. */
AlphaLink *__attribute__((fastcall)) cf_insert_alpha(AlphaList *list, void *edx, const void *info)
{
    (void)edx;
    const InsertSortedFn insert = (InsertSortedFn)0x733910;
    AlphaLink *link = insert(list, info);
    if (link || list != ALPHA_LIST || cf_glass_used == GLASS_POOL) return link;
    int n = 0;
    for (AlphaLink *l = list->usedHead.next; l != &list->usedTail && n < GLASS_POOL; l = l->next) n++;
    if (n < 20) n = 20;
    if (n > GLASS_POOL - cf_glass_used) n = GLASS_POOL - cf_glass_used;
    for (int i = 0; i < n; i++) {
        AlphaLink *l = &glass_pool[cf_glass_used++];
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
