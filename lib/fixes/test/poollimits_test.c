/* Loaded by a copy of gta_sa.exe whose entry point loads this DLL
 * (poollimits_test.sh): loads poollimits.asi, checks its DllMain writes, then
 * writes SA-MP R1's sizes over them and checks that the raise at
 * CPools::Initialise puts ours back and keeps a bigger one. */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

static int failed;
static void check(int cond, const char *what)
{
    printf("%s %s\n", cond ? "ok  " : "FAIL", what);
    failed += !cond;
}

static uint32_t u32(uint32_t va) { return *(uint32_t *)va; }
static void put32(uint32_t va, uint32_t v)
{
    DWORD old;
    VirtualProtect((void *)va, 4, PAGE_EXECUTE_READWRITE, &old);
    *(uint32_t *)va = v;
    VirtualProtect((void *)va, 4, old, &old);
}

__declspec(dllexport) int run(void)
{
    check(u32(0x55105F) == 13000 && u32(0x5510CF) == 2500, "stock sizes before the load");
    HMODULE asi = LoadLibraryA("poollimits.asi");
    if (!asi) { printf("FAIL load %lu\n", GetLastError()); return 1; }

    check(u32(0x55105F) == 90000 && u32(0x550F82) == 32000 && u32(0x551107) == 101500 && u32(0x405F26) == 0xA00
          && *(uint8_t *)0x5511AF == 99 && u32(0x5DA0CA) == 9999 && *(uint8_t *)0x5B8FE4 == 99,
          "DllMain raised the sites");

    void *stub = (void *)GetProcAddress(asi, "pl_init_pools");
    void (*raise)(void) = (void (*)(void))GetProcAddress(asi, "pl_raise_pools");
    uint32_t target = 0x5BF860 + *(int32_t *)0x5BF85C;
    check(*(uint8_t *)0x5BF85B == 0xE8 && stub && target == (uint32_t)stub, "CGame::Init1's call goes through pl_init_pools");
    check(*(uint32_t *)GetProcAddress(asi, "pl_init_target") == 0x550F10, "which goes on to CPools::Initialise");

    /* SA-MP R1's writes, and a module that sets more than ours */
    put32(0x5510CF, 4000);
    put32(0x55105F, 20000);
    put32(0x551097, 3000);
    put32(0x550FF2, (u32(0x550FF2) & ~0xFFu) | 0xF0); /* its byte write: 999 -> 1008 */
    put32(0x550F82, 210000);
    raise();
    check(u32(0x5510CF) == 25000 && u32(0x55105F) == 90000 && u32(0x551097) == 9999, "SA-MP's smaller sizes raised back");
    check(u32(0x550FF2) == 1008 && u32(0x550F82) == 210000, "bigger sizes kept");

    check(*(uint8_t *)0x9654B8 == 0, "CStreaming::Init2 has not run");
    check(u32(0x5B8E55) == 90000 && u32(0x5B8EB0) == 90000, "DllMain raised the object-instance list to 7500 links");
    put32(0x5B8E55, 15000);                         /* SilentPatch's 1250 links, both sites */
    put32(0x5B8EB0, 15000);
    raise();
    check(u32(0x5B8E55) == 90000 && u32(0x5B8EB0) == 90000, "a smaller list raised back, both sites");
    put32(0x5B8E55, 120000);
    put32(0x5B8EB0, 120000);
    raise();
    check(u32(0x5B8E55) == 120000 && u32(0x5B8EB0) == 120000, "a bigger list kept");
    put32(0x5B8E55, 15000);
    put32(0x5B8EB0, 12000);
    raise();
    check(u32(0x5B8E55) == 15000 && u32(0x5B8EB0) == 12000, "two different sizes left alone");
    put32(0x5B8E55, 12001);
    put32(0x5B8EB0, 12001);
    raise();
    check(u32(0x5B8E55) == 12001, "a size that is not a multiple of 12 left alone");

    printf("poollimits: %s\n", failed ? "FAILED" : "all checks passed");
    fflush(stdout);
    return failed;
}
