/* Runs the removed-buildings store that fixes.lua 0.6.0 writes in place of
 * SA-MP's (fixRemovedBuildings) as real x86 code: entries are kept once, the
 * count stops at the cap, nothing past the table is touched, and the stack and
 * the registers the game relies on come back the way SA-MP's own store leaves
 * them.
 *
 * The bytes come from fixes.lua itself, built for this harness's addresses
 * (count 0x20000000, store 0x20000100, table 0x20001000, cap 8):
 *
 *     luajit lib/fixes/test/fixes_test.lua --store-bytes rb_store.bin
 *     i686-w64-mingw32-gcc -O2 -Wall -o rb_store_test.exe lib/fixes/test/rb_store_test.c
 *     wine rb_store_test.exe rb_store.bin [path\to\samp.dll]
 *
 * Given SA-MP 0.3.7-R5's samp.dll it also runs SA-MP's own handler and lookup:
 * the DLL is mapped without DllMain or imports and kept off its preferred base
 * (relocated, as in the game), the handler's world-removal call is NOPed (it
 * walks the game's pools), the stock store is shown running over SA-MP's
 * globals, and then the patch goes in exactly as fixes.lua writes it.
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BASE   0x20000000u
#define COUNT  (*(volatile uint32_t *)BASE)
#define CODE   (BASE + 0x100)
#define TABLE  ((Entry *)(BASE + 0x1000))
#define CAP    8

typedef struct { int32_t model; float x, y, z, r; } Entry;

void *store_code = (void *)CODE;

/* SA-MP's handler as the jmp leaves it: cdecl (model, x, y, z, radius), esi
 * pushed and loaded with the model, then the five args of the world-removal
 * call pushed (and, in the game, that call made). The store pops esi and
 * returns to our caller. `push 0x18(%esp)` reads before it decrements, so four
 * of them push radius, z, y, x. */
void rb_store(int32_t model, float x, float y, float z, float r);
/* The same, run with the callee-saved registers set to sentinels: returns 1
 * when ebx, esi, edi and ebp all come back intact. */
int rb_store_checked(int32_t model, float x, float y, float z, float r);
__asm__(
    ".globl _rb_store\n"
    "_rb_store:\n"
    "    push %esi\n"
    "    mov  8(%esp), %esi\n"
    "    push 0x18(%esp)\n"
    "    push 0x18(%esp)\n"
    "    push 0x18(%esp)\n"
    "    push 0x18(%esp)\n"
    "    push %esi\n"
    "    jmp  *_store_code\n"
    ".globl _rb_store_checked\n"
    "_rb_store_checked:\n"
    "    push %ebx\n"
    "    push %esi\n"
    "    push %edi\n"
    "    push %ebp\n"
    "    mov  $0x11111111, %ebx\n"
    "    mov  $0x22222222, %esi\n"
    "    mov  $0x33333333, %edi\n"
    "    mov  $0x44444444, %ebp\n"
    "    push 0x24(%esp)\n"
    "    push 0x24(%esp)\n"
    "    push 0x24(%esp)\n"
    "    push 0x24(%esp)\n"
    "    push 0x24(%esp)\n"
    "    call _rb_store\n"
    "    add  $20, %esp\n"
    "    xor  %eax, %eax\n"
    "    cmp  $0x11111111, %ebx\n"
    "    jne  1f\n"
    "    cmp  $0x22222222, %esi\n"
    "    jne  1f\n"
    "    cmp  $0x33333333, %edi\n"
    "    jne  1f\n"
    "    cmp  $0x44444444, %ebp\n"
    "    jne  1f\n"
    "    mov  $1, %eax\n"
    "1:  pop  %ebp\n"
    "    pop  %edi\n"
    "    pop  %esi\n"
    "    pop  %ebx\n"
    "    ret\n"
);

static int passed, failed;
static void ok(int cond, const char *what)
{
    if (cond) passed++; else { failed++; printf("  FAIL: %s\n", what); }
}

static int same(const Entry *e, int32_t model, float x, float y, float z, float r)
{
    return e->model == model && e->x == x && e->y == y && e->z == z && e->r == r;
}

/* where fixes.lua's store holds its numbers (fixes_test.lua checks the same) */
#define AT_COUNT1 0x05
#define AT_TABLE  0x0A
#define AT_COUNT2 0x42
#define AT_CAP    0x48
#define AT_COUNT3 0x6F

static uint32_t get32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void put32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }

/* SA-MP 0.3.7-R5 */
#define R5_STAMP      0x6372C39Eu
#define RVA_HANDLER   0x9D3D0     /* cdecl (model, x, y, z, radius) */
#define RVA_WORLDCALL 0x9D3EA     /* call: remove the matching buildings from the world */
#define RVA_STORE     0x9D3EF     /* mov ecx, [count] */
#define RVA_LOOKUP    0x9D050     /* cdecl (model, x, y, z) -> 1 when removed */
#define RVA_LOOKUP_OP 0x9D068     /* imm32 of mov esi, table */
#define RVA_TABLE     0x14AF68
#define RVA_COUNT     0x14FD88
#define RVA_VICTIM    0x14FFAC    /* the object pointer entry 1027's y lands on */
#define RB_CAP        5000

typedef void (__cdecl *handler_t)(int32_t, float, float, float, float);
typedef int  (__cdecl *lookup_t)(int32_t, float, float, float);

static void real_samp(const uint8_t *store, size_t n, const char *path)
{
    VirtualAlloc((void *)0x10000000, 0x10000, MEM_RESERVE, PAGE_NOACCESS);
    HMODULE h = LoadLibraryExA(path, NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!h) { printf("cannot load %s (%lu)\n", path, GetLastError()); failed++; return; }
    uint8_t *base = (uint8_t *)h;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != R5_STAMP) { printf("\n%s is not SA-MP 0.3.7-R5; skipped\n", path); return; }
    printf("\n== SA-MP R5 itself: samp.dll at %p\n", (void *)base);
    ok(base != (uint8_t *)0x10000000, "relocated off its preferred base, as in the game");
    DWORD old;
    VirtualProtect(base + 0x1000, nt->OptionalHeader.SizeOfImage - 0x1000, PAGE_EXECUTE_READWRITE, &old);

    volatile uint32_t *count = (volatile uint32_t *)(base + RVA_COUNT);
    Entry *stock = (Entry *)(base + RVA_TABLE);
    handler_t handler = (handler_t)(base + RVA_HANDLER);
    lookup_t lookup = (lookup_t)(base + RVA_LOOKUP);
    ok(base[RVA_STORE] == 0x8B && get32(base + RVA_STORE + 2) == (uint32_t)(base + RVA_COUNT)
       && get32(base + RVA_LOOKUP_OP) == (uint32_t)(base + RVA_TABLE), "both sites hold the stock code");
    memset(base + RVA_WORLDCALL, 0x90, 5);

    static uint8_t globals[0x1000];          /* SA-MP's memory after the count */
    memcpy(globals, base + RVA_COUNT + 4, sizeof globals);
    float y = -250.0078f;
    uint32_t ybits; memcpy(&ybits, &y, 4);

    printf("\n== stock: 1030 removals run over SA-MP's own memory\n");
    *count = 0;
    for (int i = 0; i < 1030; i++) handler(1000 + i, (float)i, y, 10.0f, 0.25f);
    ok(*count == 1030, "count 1030 in a table of 1000");
    ok(get32(base + RVA_VICTIM) == ybits,
       "entry 1027's y (0xC37A01FF) now sits on the pointer at samp+0x14FFAC - the crash");
    memcpy(base + RVA_COUNT + 4, globals, sizeof globals);
    memset(stock, 0, 1000 * sizeof(Entry));
    *count = 3;
    for (int i = 0; i < 3; i++) stock[i] = (Entry){500 + i, (float)i, y, 10.0f, 0.25f};

    printf("\n== the patch, as fixes.lua writes it\n");
    uint8_t *block = VirtualAlloc(NULL, 0x100 + RB_CAP * sizeof(Entry), MEM_RESERVE | MEM_COMMIT,
                                  PAGE_EXECUTE_READWRITE);
    Entry *table = (Entry *)(block + 0x100);
    put32(block, 0x62527846); put32(block + 4, RB_CAP);
    put32(block + 8, (uint32_t)table); put32(block + 12, (uint32_t)(base + RVA_COUNT));
    memcpy(block + 0x10, store, n);
    put32(block + 0x10 + AT_COUNT1, (uint32_t)(base + RVA_COUNT));
    put32(block + 0x10 + AT_TABLE, (uint32_t)table);
    put32(block + 0x10 + AT_COUNT2, (uint32_t)(base + RVA_COUNT));
    put32(block + 0x10 + AT_CAP, RB_CAP);
    put32(block + 0x10 + AT_COUNT3, (uint32_t)(base + RVA_COUNT));
    memcpy(table, stock, 3 * sizeof(Entry));
    put32(base + RVA_LOOKUP_OP, (uint32_t)table);
    base[RVA_STORE] = 0xE9;
    put32(base + RVA_STORE + 1, (uint32_t)(block + 0x10) - (uint32_t)(base + RVA_STORE + 5));

    ok(lookup(501, 1.0f, y, 10.0f) == 1, "SA-MP's lookup finds a carried-over entry in the new table");
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < 1500; i++) handler(1000 + i, (float)i, y, 10.0f, 0.25f);
    ok(*count == 1503, "1500 removals sent twice: 1503 stored, each once");
    ok(memcmp(base + RVA_COUNT + 4, globals, sizeof globals) == 0, "SA-MP's memory after its old table is untouched");
    ok(stock[3].model == 0, "SA-MP's old table is not written any more");
    ok(table[1502].model == 2499 && table[1502].y == y, "entry 1502 is the last removal");
    ok(lookup(1000, 0.0f, y, 10.0f) == 1, "SA-MP's lookup finds the first new entry");
    ok(lookup(2499, 1499.0f, y, 10.1f) == 1, "and the 1500th, inside its radius");
    ok(lookup(2499, 1499.0f, y, 11.0f) == 0, "but not outside the radius");
    ok(lookup(2498, 1499.0f, y, 10.0f) == 0, "nor another model on the same spot");
    handler(-1, 5000.0f, 5000.0f, 0.0f, 50.0f);
    ok(lookup(1234, 5010.0f, 5000.0f, 0.0f) == 1, "a model -1 removal hides any model in its radius");

    printf("\n== full at %d\n", RB_CAP);
    for (int i = 0; i < 4000; i++) handler(9000, (float)i, 1.0f, 1.0f, 1.0f);
    ok(*count == RB_CAP, "stops at the cap");
    int clean = 1;
    size_t end = 0x100 + RB_CAP * sizeof(Entry);
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(block, &mbi, sizeof mbi);
    for (size_t i = end; i < mbi.RegionSize; i++) if (block[i]) clean = 0;
    ok(clean, "nothing written past our table");
    ok(memcmp(base + RVA_COUNT + 4, globals, sizeof globals) == 0, "SA-MP's memory still untouched");
}

int main(int argc, char **argv)
{
    if (argc < 2) { printf("usage: rb_store_test <rb_store.bin>\n"); return 2; }
    uint8_t *mem = VirtualAlloc((void *)BASE, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (mem != (uint8_t *)BASE) { printf("cannot map 0x%08X\n", BASE); return 2; }

    FILE *f = fopen(argv[1], "rb");
    if (!f) { printf("cannot open %s\n", argv[1]); return 2; }
    size_t n = fread((void *)CODE, 1, 0x1000 - 0x100, f);
    fclose(f);
    printf("store: %u bytes\n", (unsigned)n);
    ok(n == 117, "the store is the 117 bytes the assembler made");

    /* everything after the table's CAP entries must stay as it is */
    memset((uint8_t *)&TABLE[CAP], 0xA5, 0x400);

    printf("\n== the first entry is stored\n");
    COUNT = 0;
    rb_store(4000, 1.5f, -250.0078f, 10.25f, 0.25f);
    ok(COUNT == 1, "count 1");
    ok(same(&TABLE[0], 4000, 1.5f, -250.0078f, 10.25f, 0.25f), "entry 0 holds model, x, y, z, radius");

    printf("\n== the same removal again is not stored twice\n");
    rb_store(4000, 1.5f, -250.0078f, 10.25f, 0.25f);
    ok(COUNT == 1, "count still 1");

    printf("\n== any one field different is a new entry\n");
    rb_store(4000, 1.5f, -250.0078f, 10.25f, 0.5f);   /* radius */
    rb_store(4001, 1.5f, -250.0078f, 10.25f, 0.25f);  /* model */
    rb_store(-1, 1.5f, -250.0078f, 10.25f, 0.25f);    /* model -1: every model in the radius */
    rb_store(4000, 1.5f, -250.0078f, 10.5f, 0.25f);   /* z */
    ok(COUNT == 5, "count 5");
    ok(same(&TABLE[4], 4000, 1.5f, -250.0078f, 10.5f, 0.25f), "entry 4 is the last one stored");
    ok(same(&TABLE[2], 4001, 1.5f, -250.0078f, 10.25f, 0.25f), "entry 2 is the second model");
    rb_store(-1, 1.5f, -250.0078f, 10.25f, 0.25f);
    ok(COUNT == 5, "a repeat of a middle entry is found too");

    printf("\n== full at the cap: the rest is dropped, nothing past the table written\n");
    rb_store(1, 1, 1, 1, 1);
    rb_store(2, 2, 2, 2, 2);
    rb_store(3, 3, 3, 3, 3);
    ok(COUNT == CAP, "count reached the cap");
    rb_store(9, 9, 9, 9, 9);
    rb_store(10, 10, 10, 10, 10);
    ok(COUNT == CAP, "and stays there");
    int untouched = 1;
    for (int i = 0; i < 0x400; i++) if (((uint8_t *)&TABLE[CAP])[i] != 0xA5) untouched = 0;
    ok(untouched, "the bytes after the table are untouched");
    rb_store(3, 3, 3, 3, 3);
    ok(COUNT == CAP, "a repeat of the last entry when full changes nothing");

    printf("\n== registers and stack\n");
    COUNT = 0;
    ok(rb_store_checked(77, 7, 7, 7, 7) == 1, "ebx, esi, edi and ebp survive a store");
    ok(rb_store_checked(77, 7, 7, 7, 7) == 1, "and a repeat");
    ok(COUNT == 1 && TABLE[0].model == 77, "stored through the checked path");
    /* a wrong stack adjustment would have returned somewhere else by now; a
     * thousand more calls make a slow drift show as well */
    for (int i = 0; i < 1000; i++) rb_store(77, 7, 7, 7, 7);
    ok(COUNT == 1, "1000 repeats later, still one entry and still here");

    if (argc > 2) {
        static uint8_t store[0x100];
        memcpy(store, (void *)CODE, n);
        ok(get32(store + AT_COUNT1) == BASE && get32(store + AT_TABLE) == BASE + 0x1000
           && get32(store + AT_COUNT2) == BASE && get32(store + AT_CAP) == CAP
           && get32(store + AT_COUNT3) == BASE, "the store's numbers sit where this harness substitutes them");
        real_samp(store, n, argv[2]);
    }

    printf("\nrb_store: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
