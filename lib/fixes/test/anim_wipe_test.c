/* Runs the animation wipe that fixes.lua 0.7.0 writes in place of
 * RpAnimBlendClumpRemoveAllAssociations (fixAnimationWipe) as real x86 code,
 * on hand-built animations laid out as the game's CAnimBlendAssociation:
 * every animation is deleted once, in list order, as before; a delete
 * callback on the list runs first, with the animation still intact; any other
 * callback, and every finish callback, is left alone; and the registers and
 * stack come back as the stock function leaves them.
 *
 * The bytes come from fixes.lua itself, built for this harness's addresses
 * (the game's plugin offset global 0xB5F878, the block at 0x20000000):
 *
 *     luajit lib/fixes/test/fixes_test.lua --wipe-bytes anim_wipe.bin
 *     i686-w64-mingw32-gcc -O2 -Wall -o anim_wipe_test.exe lib/fixes/test/anim_wipe_test.c
 *     wine anim_wipe_test.exe anim_wipe.bin [path\to\gta_sa.exe]
 *
 * Given gta_sa.exe 1.0 US it also runs the game's own code: the stock
 * function, which leaves CTaskSimpleInAir's pointer on a deleted animation
 * (the 2026-10-12 crash), and CTaskSimpleInAir::DeleteAnimCB, which the patch
 * then calls. Both are copied out of the exe; they only reference 0xB5F878.
 */
#include <windows.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define OFFSET_VA 0xB5F878u          /* the game's CAnimBlendClumpData plugin offset */
#define PLUGIN    0x3C               /* what it holds in the game */
#define BLOCK     0x20000000u
#define CODE      (BLOCK + 0x10)
#define TOLD      (*(volatile uint32_t *)(BLOCK + 4))
#define ENTRIES   (*(volatile uint32_t *)(BLOCK + 8))
#define LIST      ((uint32_t *)(BLOCK + 0x80))

/* where fixes.lua's code holds its numbers (fixes_test.lua checks the same) */
#define AT_OFFSET  0x06
#define AT_LIST    0x25
#define AT_ENTRIES 0x2B
#define AT_TOLD    0x48

typedef struct Link { struct Link *next, *prev; } Link;
typedef struct Assoc {
    void   **vtable;     /* +00 */
    Link     link;       /* +04 */
    uint8_t  pad[0x24];
    uint32_t cbType;     /* +30: 0 none, 1 finish, 2 delete */
    void    *cbFunc;     /* +34 */
    void    *cbData;     /* +38 */
} Assoc;
_Static_assert(offsetof(Assoc, link) == 4 && offsetof(Assoc, cbType) == 0x30
               && offsetof(Assoc, cbData) == 0x38, "CAnimBlendAssociation layout");

typedef struct { uint8_t pad[0x1C]; Assoc *anim; } InAirTask;   /* CTaskSimpleInAir::m_pAnim at +1C */
_Static_assert(offsetof(InAirTask, anim) == 0x1C, "CTaskSimpleInAir layout");

typedef void (__cdecl *wipe_t)(void *clump);
typedef void (__cdecl *cb_t)(Assoc *, void *);

static int passed, failed;
static void ok(int cond, const char *what)
{
    if (cond) passed++; else { failed++; printf("  FAIL: %s\n", what); }
}

/* what happened, in order: 'D' deleted, 'T' told, 'F' the foreign callback ran */
static char events[64];
static int  nevents;
static Assoc *order[16];
static int  ndeleted;
static void event(char c) { if (nevents < 63) events[nevents++] = c; events[nevents] = 0; }

/* the scalar deleting destructor: thiscall, one stack arg, ret 4 */
static __attribute__((thiscall)) void *destroy(Assoc *self, int flags)
{
    (void)flags;
    event('D');
    if (ndeleted < 16) order[ndeleted++] = self;
    if (self->link.prev) self->link.prev->next = self->link.next;
    if (self->link.next) self->link.next->prev = self->link.prev;
    memset(&self->link, 0xDD, sizeof *self - offsetof(Assoc, link));   /* freed */
    return self;
}
static void *vtable[1] = {(void *)destroy};

/* a delete callback on the list: the animation must still be whole */
static int told_intact;
static void __cdecl listed_cb(Assoc *a, void *data)
{
    event('T');
    told_intact = a->cbFunc == (void *)listed_cb && a->cbData == data && a->link.next != (Link *)0xDDDDDDDD;
}
/* one that is not: must never run */
static void __cdecl foreign_cb(Assoc *a, void *data) { (void)a; (void)data; event('F'); }
/* CTaskSimpleInAir::DeleteAnimCB, unless the real one is copied in */
static void __cdecl inair_c(Assoc *a, void *data)
{
    InAirTask *t = data;
    if (t && a && t->anim == a) t->anim = NULL;
}
static cb_t inair_cb = inair_c;

/* a clump: the plugin's data pointer at +PLUGIN, the data's first member the list head */
static uint8_t clump[0x100];
static Link    head;
static Assoc   anims[8];

static void build(int n)
{
    memset(anims, 0, sizeof anims);
    head.next = head.prev = NULL;
    Link *prev = &head;
    for (int i = 0; i < n; i++) {
        anims[i].vtable = vtable;
        anims[i].link.prev = prev;
        prev->next = &anims[i].link;
        prev = &anims[i].link;
    }
    Link **data = (Link **)(clump + PLUGIN);
    *data = &head;
    nevents = 0; events[0] = 0; ndeleted = 0; told_intact = 0;
}

/* call through `fn` with ebx, esi, edi and ebp set to sentinels: returns 1
 * when they and esp all come back intact */
wipe_t wipe_fn;                        /* global: the asm below calls through it */
int wipe_checked(void *clump);
__asm__(
    ".globl _wipe_checked\n"
    "_wipe_checked:\n"
    "    push %ebx\n"
    "    push %esi\n"
    "    push %edi\n"
    "    push %ebp\n"
    "    mov  20(%esp), %eax\n"
    "    mov  $0x11111111, %ebx\n"
    "    mov  $0x22222222, %esi\n"
    "    mov  $0x33333333, %edi\n"
    "    mov  %esp, %ebp\n"
    "    push %eax\n"
    "    call *_wipe_fn\n"
    "    add  $4, %esp\n"
    "    xor  %eax, %eax\n"
    "    cmp  %esp, %ebp\n"
    "    jne  1f\n"
    "    cmp  $0x11111111, %ebx\n"
    "    jne  1f\n"
    "    cmp  $0x22222222, %esi\n"
    "    jne  1f\n"
    "    cmp  $0x33333333, %edi\n"
    "    jne  1f\n"
    "    mov  $1, %eax\n"
    "1:  pop  %ebp\n"
    "    pop  %edi\n"
    "    pop  %esi\n"
    "    pop  %ebx\n"
    "    ret\n"
);

static uint32_t get32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }

/* the scene from the crash: the falling task's animation, one with no
 * callback, one with a mod's delete callback, a listed callback registered
 * as a finish callback, and a listed delete callback */
static InAirTask task;
static int other_data;
static void scene(void)
{
    build(5);
    task.anim = &anims[0];
    anims[0].cbType = 2; anims[0].cbFunc = (void *)inair_cb;   anims[0].cbData = &task;
    anims[2].cbType = 2; anims[2].cbFunc = (void *)foreign_cb; anims[2].cbData = &other_data;
    anims[3].cbType = 1; anims[3].cbFunc = (void *)listed_cb;  anims[3].cbData = &other_data;
    anims[4].cbType = 2; anims[4].cbFunc = (void *)listed_cb;  anims[4].cbData = &other_data;
    LIST[0] = (uint32_t)(uintptr_t)inair_cb;
    LIST[1] = (uint32_t)(uintptr_t)listed_cb;
    ENTRIES = 2;
}

static int all_deleted_in_order(int n)
{
    if (ndeleted != n || head.next != NULL) return 0;
    for (int i = 0; i < n; i++) if (order[i] != &anims[i]) return 0;
    return 1;
}

/* n bytes of gta_sa.exe's .text at va, in a page of their own */
static uint8_t *exe_bytes(FILE *f, uint32_t va, size_t n)
{
    uint8_t *code = VirtualAlloc(NULL, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!code || fseek(f, (long)(va - 0x401000 + 0x400), SEEK_SET) != 0 || fread(code, 1, n, f) != n) return NULL;
    return code;
}

static const uint8_t STOCK[43] = {
    0x8B, 0x44, 0x24, 0x04, 0x8B, 0x0D, 0x78, 0xF8, 0xB5, 0x00, 0x8B, 0x04, 0x01, 0x8B, 0x00, 0x85, 0xC0,
    0x74, 0x17, 0x56, 0x8B, 0x30, 0x8D, 0x48, 0xFC, 0x85, 0xC9, 0x74, 0x06, 0x8B, 0x11, 0x6A, 0x01, 0xFF,
    0x12, 0x85, 0xF6, 0x8B, 0xC6, 0x75, 0xEB, 0x5E, 0xC3};

static void real_game(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { printf("cannot open %s\n", path); failed++; return; }
    uint8_t *stock = exe_bytes(f, 0x4D6C00, sizeof STOCK);
    uint8_t *cb = exe_bytes(f, 0x678E60, 0x1D);
    fclose(f);
    if (!stock || !cb) { printf("cannot read %s\n", path); failed++; return; }

    printf("\n== gta_sa.exe itself: the stock RpAnimBlendClumpRemoveAllAssociations\n");
    ok(memcmp(stock, STOCK, sizeof STOCK) == 0, "it is the function fixes.lua expects, byte for byte");
    inair_cb = (cb_t)cb;                         /* the game's CTaskSimpleInAir::DeleteAnimCB */
    scene();
    wipe_fn = (wipe_t)stock;
    ok(wipe_checked(clump) == 1, "the stock function runs here");
    ok(all_deleted_in_order(5), "it deletes all five");
    ok(task.anim == &anims[0], "and leaves the falling task pointing at the deleted one: the crash");
    ok(strchr(events, 'T') == NULL, "no callback runs");

    printf("\n== the patch, with the game's CTaskSimpleInAir::DeleteAnimCB\n");
    scene();
    uint32_t before = TOLD;
    wipe_fn = (wipe_t)CODE;
    ok(wipe_checked(clump) == 1, "runs, registers and stack intact");
    ok(all_deleted_in_order(5), "all five deleted, in order");
    ok(task.anim == NULL, "the game's own callback cleared the task's pointer");
    ok(TOLD == before + 2, "two callbacks counted");
    inair_cb = inair_c;
}

int main(int argc, char **argv)
{
    if (argc < 2) { printf("usage: anim_wipe_test <anim_wipe.bin> [gta_sa.exe]\n"); return 2; }
    uint8_t *page = VirtualAlloc((void *)(OFFSET_VA & ~0xFFFu), 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    uint8_t *mem = VirtualAlloc((void *)BLOCK, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!page || !mem) { printf("cannot map 0x%X or 0x%X\n", OFFSET_VA & ~0xFFFu, BLOCK); return 2; }
    *(uint32_t *)OFFSET_VA = PLUGIN;

    FILE *f = fopen(argv[1], "rb");
    if (!f) { printf("cannot open %s\n", argv[1]); return 2; }
    size_t n = fread((void *)CODE, 1, 0x70, f);
    fclose(f);
    ok(n == 94, "the code is 94 bytes");
    const uint8_t *code = (const uint8_t *)CODE;
    ok(get32(code + AT_OFFSET) == OFFSET_VA && get32(code + AT_LIST) == BLOCK + 0x80
       && get32(code + AT_ENTRIES) == BLOCK + 8 && get32(code + AT_TOLD) == BLOCK + 4,
       "its numbers sit where this harness expects them");
    wipe_fn = (wipe_t)CODE;

    printf("\n== the scene from the crash\n");
    scene();
    ok(wipe_checked(clump) == 1, "runs, registers and stack intact");
    ok(all_deleted_in_order(5), "every animation deleted once, in list order");
    ok(task.anim == NULL, "the falling task no longer points at a deleted animation");
    ok(strcmp(events, "DDDDTD") == 0, "the listed delete callback ran right before its animation went");
    if (strcmp(events, "DDDDTD") != 0) printf("    events: %s\n", events);
    ok(told_intact, "and saw it whole");
    ok(strchr(events, 'F') == NULL, "a mod's delete callback is not run");
    ok(TOLD == 2, "two callbacks counted: the falling task's and the listed one");

    printf("\n== nothing listed: exactly the stock behaviour\n");
    scene();
    ENTRIES = 0;
    task.anim = &anims[0];
    ok(wipe_checked(clump) == 1 && all_deleted_in_order(5), "all deleted");
    ok(task.anim == &anims[0] && strchr(events, 'T') == NULL && TOLD == 2, "no callback runs");

    printf("\n== an empty clump, one animation\n");
    build(0);
    ok(wipe_checked(clump) == 1 && ndeleted == 0 && nevents == 0, "empty: returns, touches nothing");
    build(1);
    anims[0].cbType = 2; anims[0].cbFunc = (void *)listed_cb; anims[0].cbData = &other_data;
    LIST[0] = (uint32_t)(uintptr_t)listed_cb; ENTRIES = 1;
    ok(wipe_checked(clump) == 1 && all_deleted_in_order(1) && strcmp(events, "TD") == 0, "one: told, then deleted");

    printf("\n== the callback matched anywhere in the list\n");
    build(1);
    anims[0].cbType = 2; anims[0].cbFunc = (void *)listed_cb; anims[0].cbData = &other_data;
    for (int i = 0; i < 8; i++) LIST[i] = 0x11000000u + i;
    LIST[8] = (uint32_t)(uintptr_t)listed_cb; ENTRIES = 9;
    ok(wipe_checked(clump) == 1 && strcmp(events, "TD") == 0, "the last of 9 entries");
    build(1);
    anims[0].cbType = 2; anims[0].cbFunc = (void *)listed_cb; anims[0].cbData = &other_data;
    ENTRIES = 8;
    ok(wipe_checked(clump) == 1 && strcmp(events, "D") == 0, "past the entry count: not matched");

    /* a wrong stack adjustment would have returned somewhere else by now; a
     * thousand more calls make a slow drift show as well */
    for (int i = 0; i < 1000; i++) { scene(); wipe_checked(clump); }
    ok(task.anim == NULL && all_deleted_in_order(5), "1000 scenes later, still right and still here");

    if (argc > 2) real_game(argv[2]);

    printf("\nanim_wipe: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
