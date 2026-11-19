/* poollimits.asi: the game's pool and store sizes raised before it builds
 * them, for a stock gta_sa.exe 1.0 US. fixes.lua copies it beside the exe
 * (option poollimits), and the ASI loader loads it from there. The values are
 * the ones a limits-patched exe carried. A pool that fills returns null, and a
 * large server map then crashes CFileLoader::LoadObjectInstance at 0x538103
 * (the dummy pool, 4000 under SA-MP R1, is the first to go).
 *
 * Each site is the size operand of a `push` in front of a CPool constructor.
 * DllMain writes each one that still holds its stock size. SA-MP R1 then
 * writes its own sizes into CPools::Initialise (Dummies 4000, Buildings 20000,
 * Objects 3000, ...), after DllMain and before the game builds the pools. So
 * the call of CPools::Initialise in CGame::Init1 (0x5BF85B) goes through
 * pl_init_pools first, which raises each CPools site to at least its value and
 * then jumps to 0x550F10 (whoever hooks its entry gets it as before). A size
 * is only ever raised: a module that set a bigger one keeps it. A line per
 * site goes to poollimits.log beside the exe.
 */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

typedef struct {
    uint32_t va;
    uint8_t len;            /* 1: push imm8, 4: push imm32 */
    uint32_t stock, value;
    const char *what;
} Site;

#define CPOOLS_FIRST 4      /* sites[4..] sit in CPools::Initialise */
#define CPOOLS_LAST 17

static const Site sites[] = {
    {0x405F26, 4, 0x100, 0xA00, "CStreaming: a pool"},
    {0x405F4D, 1, 0x28, 0x63, "CStreaming: a pool's allocation"},
    {0x411458, 4, 0xFF, 0x9F6, "CColStore: collision files"},
    {0x41147F, 1, 0x28, 0x63, "CColStore: the pool's allocation"},
    {0x550F82, 4, 3200, 32000, "CPools: PtrNodeDouble"},
    {0x550FBA, 4, 500, 9999, "CPools: EntryInfoNode"},
    {0x550FF2, 4, 140, 999, "CPools: Peds"},
    {0x55105F, 4, 13000, 90000, "CPools: Buildings"},
    {0x551097, 4, 350, 9999, "CPools: Objects"},
    {0x5510CF, 4, 2500, 25000, "CPools: Dummies"},
    {0x551107, 4, 10150, 101500, "CPools: ColModel"},
    {0x55113F, 4, 500, 9999, "CPools: Tasks"},
    {0x551177, 4, 200, 9999, "CPools: Events"},
    {0x5511AF, 1, 64, 99, "CPools: PointRoute"},
    {0x5511E4, 1, 32, 99, "CPools: PatrolRoute"},
    {0x551219, 1, 64, 99, "CPools: NodeRoute"},
    {0x55124E, 1, 16, 99, "CPools: TaskAllocator"},
    {0x5512BC, 1, 64, 99, "CPools: PedAttractors"},
    {0x552C3F, 4, 400, 999, "a pool (0x552C3E)"},
    {0x5B8FE4, 1, 50, 99, "a pool (0x5B8FE3)"},
    {0x5DA08E, 4, 0x1000, 9999, "a pool (0x5DA08D)"},
    {0x5DA0CA, 4, 0x400, 9999, "a pool (0x5DA0C9)"},
    {0x5DA106, 4, 0x1000, 9999, "a pool (0x5DA105)"},
};

#define INIT_CALL 0x5BF85B                          /* CGame::Init1: call CPools::Initialise */
static const uint8_t INIT_CALL_STOCK[5] = {0xE8, 0xB0, 0x16, 0xF9, 0xFF};
uint32_t pl_init_target = 0x550F10;

static char log_path[MAX_PATH];

static uint32_t read_site(const Site *s)
{
    return s->len == 1 ? *(const uint8_t *)s->va : *(const uint32_t *)s->va;
}

static int write_bytes(uint32_t va, const void *data, size_t n)
{
    DWORD old;
    if (!VirtualProtect((void *)va, n, PAGE_EXECUTE_READWRITE, &old)) return 0;
    memcpy((void *)va, data, n);
    VirtualProtect((void *)va, n, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void *)va, n);
    return memcmp((void *)va, data, n) == 0;
}

static int write_site(const Site *s)
{
    uint8_t b = (uint8_t)s->value;
    return s->len == 1 ? write_bytes(s->va, &b, 1) : write_bytes(s->va, &s->value, 4);
}

/* Before CPools::Initialise: every CPools site to at least its value */
void __cdecl pl_raise_pools(void)
{
    FILE *log = fopen(log_path, "a");
    int raised = 0, kept = 0;
    for (int i = CPOOLS_FIRST; i <= CPOOLS_LAST; i++) {
        const Site *s = &sites[i];
        uint32_t found = read_site(s);
        if (found >= s->value) {
            kept++;
            if (found != s->value && log) fprintf(log, "  0x%06X %s: %u, more than ours; kept\n", s->va, s->what, found);
        } else if (write_site(s)) {
            raised++;
            if (found != s->stock && log) fprintf(log, "  0x%06X %s: %u (another module's) -> %u\n", s->va, s->what, found, s->value);
        } else if (log) {
            fprintf(log, "  0x%06X %s: the write did not stick\n", s->va, s->what);
        }
    }
    if (log) {
        fprintf(log, "at CPools::Initialise: %d raised again, %d already at or above ours\n", raised, kept);
        fclose(log);
    }
}

/* In place of the call: the return address is CGame::Init1's, the jump keeps it */
__asm__(
".globl _pl_init_pools\n"
"_pl_init_pools:\n"
"    pushal\n"
"    pushfl\n"
"    call _pl_raise_pools\n"
"    popfl\n"
"    popal\n"
"    jmp *_pl_init_target\n");
void pl_init_pools(void);

static void apply(void)
{
    DWORD n = GetModuleFileNameA(NULL, log_path, MAX_PATH);
    while (n && log_path[n - 1] != '\\' && log_path[n - 1] != '/') n--;
    snprintf(log_path + n, MAX_PATH - n, "poollimits.log");
    FILE *log = fopen(log_path, "w");

    /* the 1.0 US exe: CPools::Initialise's first push is where it should be */
    int us10 = !IsBadReadPtr((void *)0x550F81, 5) && *(const uint8_t *)0x550F81 == 0x68;
    int done = 0, kept = 0;
    for (size_t i = 0; us10 && i < sizeof sites / sizeof *sites; i++) {
        const Site *s = &sites[i];
        uint32_t found = read_site(s);
        if (found == s->value) {
            kept++;
            if (log) fprintf(log, "0x%06X %s: already %u\n", s->va, s->what, found);
        } else if (found != s->stock) {
            kept++;
            if (log) fprintf(log, "0x%06X %s: %u, not the stock %u; left alone\n", s->va, s->what, found, s->stock);
        } else if (write_site(s)) {
            done++;
            if (log) fprintf(log, "0x%06X %s: %u -> %u\n", s->va, s->what, s->stock, s->value);
        } else if (log) {
            fprintf(log, "0x%06X %s: the write did not stick\n", s->va, s->what);
        }
    }

    int hooked = 0;
    if (us10 && memcmp((void *)INIT_CALL, INIT_CALL_STOCK, 5) == 0) {
        uint8_t call[5] = {0xE8};
        *(int32_t *)(call + 1) = (int32_t)((uint32_t)pl_init_pools - (INIT_CALL + 5));
        hooked = write_bytes(INIT_CALL, call, 5);
    }
    if (log) {
        if (!us10) fprintf(log, "not gta_sa.exe 1.0 US; nothing written\n");
        else fprintf(log, "%d raised, %d left as found; %s\n", done, kept,
                     hooked ? "the CPools sites are raised again when CPools::Initialise is called"
                            : "CGame::Init1's call of CPools::Initialise is not stock: not raised again there");
        fclose(log);
    }
}

BOOL WINAPI DllMain(HINSTANCE dll, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(dll);
        apply();
    }
    return TRUE;
}
