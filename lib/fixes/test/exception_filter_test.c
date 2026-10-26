/* Runs the exception filter tail that fixes.lua 0.12.0 writes into SA-MP R5's
 * own exception filter (fixExceptionFilter): every exception is swallowed, the
 * first ten still print, the entry's no-chat path still counts, the
 * add_car_component path is left as it was, and the stack and the registers
 * the caller relies on come back as SA-MP's stdcall filter leaves them.
 *
 * The tail comes from fixes.lua itself, built for samp.dll at its preferred
 * base (the counter is moved here; the jmp to the print is relative):
 *
 *     luajit lib/fixes/test/fixes_test.lua --filter-bytes filter.bin
 *     i686-w64-mingw32-gcc -O2 -Wall -o exception_filter_test.exe lib/fixes/test/exception_filter_test.c
 *     wine exception_filter_test.exe filter.bin path\to\samp.dll
 *
 * samp.dll (0.3.7-R5) is mapped without DllMain or imports and kept off its
 * preferred base (relocated, as in the game). Its print is redirected here.
 */
#include <windows.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* SA-MP 0.3.7-R5 */
#define R5_STAMP   0x6372C39Eu
#define RVA_FILTER 0x609B0     /* int __stdcall (code, EXCEPTION_POINTERS *, name) */
#define RVA_SITE   0x60A4D     /* the warning call */
#define RVA_COUNT  0x125A58    /* the warnings counter */
#define RVA_PRINT  0x680F0     /* cdecl (chat, fmt, ...) */
#define RVA_CHAT   0x26EB80    /* the chat; 0 jumps straight to the counter */
#define RVA_OPCODE 0x26B55C    /* the opcode ScriptCommand is running */
#define PREFERRED  0x10000000u
#define TAIL       37
#define AT_COUNT1  10          /* where the tail holds the counter (fixes_test.lua checks the same) */
#define AT_COUNT2  24

static int passed, failed;
static void ok(int cond, const char *what)
{
    if (cond) passed++; else { failed++; printf("  FAIL: %s\n", what); }
}

static uint32_t get32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void put32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }

static int printed;
static char last[256];
static void __cdecl fake_print(void *chat, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(last, sizeof last, fmt, ap);
    va_end(ap);
    printed++;
}

/* The filter called as SA-MP's __except does, with ebx/esi/edi set to
 * sentinels and esp recorded: regs_ok is 1 when all four come back. */
void *filter_fn;
uint32_t esp_before, esp_after, regs_ok;
int call_filter(uint32_t code, void *ptrs, const char *name);
__asm__(
    ".globl _call_filter\n"
    "_call_filter:\n"
    "    push %ebp\n"
    "    mov  %esp, %ebp\n"
    "    push %ebx\n"
    "    push %esi\n"
    "    push %edi\n"
    "    mov  $0x11111111, %ebx\n"
    "    mov  $0x22222222, %esi\n"
    "    mov  $0x33333333, %edi\n"
    "    mov  %esp, _esp_before\n"
    "    push 16(%ebp)\n"
    "    push 12(%ebp)\n"
    "    push 8(%ebp)\n"
    "    call *_filter_fn\n"
    "    mov  %esp, _esp_after\n"
    "    movl $0, _regs_ok\n"
    "    cmp  $0x11111111, %ebx\n"
    "    jne  1f\n"
    "    cmp  $0x22222222, %esi\n"
    "    jne  1f\n"
    "    cmp  $0x33333333, %edi\n"
    "    jne  1f\n"
    "    mov  _esp_after, %ecx\n"
    "    cmp  _esp_before, %ecx\n"
    "    jne  1f\n"
    "    movl $1, _regs_ok\n"
    "1:  pop  %edi\n"
    "    pop  %esi\n"
    "    pop  %ebx\n"
    "    pop  %ebp\n"
    "    ret\n");

int main(int argc, char **argv)
{
    if (argc < 3) { printf("usage: %s filter.bin samp.dll\n", argv[0]); return 2; }
    uint8_t tail[TAIL];
    FILE *f = fopen(argv[1], "rb");
    if (!f) { printf("cannot open %s\n", argv[1]); return 2; }
    size_t n = fread(tail, 1, sizeof tail, f);
    fclose(f);
    ok(n == TAIL, "the tail is 37 bytes");
    ok(get32(tail + AT_COUNT1) == PREFERRED + RVA_COUNT && get32(tail + AT_COUNT2) == PREFERRED + RVA_COUNT,
       "the counter sits where this harness moves it");

    VirtualAlloc((void *)PREFERRED, 0x10000, MEM_RESERVE, PAGE_NOACCESS);
    HMODULE h = LoadLibraryExA(argv[2], NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (!h) { printf("cannot load %s (%lu)\n", argv[2], GetLastError()); return 2; }
    uint8_t *base = (uint8_t *)h;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    if (nt->FileHeader.TimeDateStamp != R5_STAMP) { printf("%s is not SA-MP 0.3.7-R5\n", argv[2]); return 2; }
    printf("== SA-MP R5's exception filter: samp.dll at %p\n", (void *)base);
    ok(base != (uint8_t *)PREFERRED, "relocated off its preferred base, as in the game");
    DWORD old;
    VirtualProtect(base + 0x1000, nt->OptionalHeader.SizeOfImage - 0x1000, PAGE_EXECUTE_READWRITE, &old);

    volatile uint32_t *count = (volatile uint32_t *)(base + RVA_COUNT);
    volatile uint32_t *chat = (volatile uint32_t *)(base + RVA_CHAT);
    volatile uint32_t *opcode = (volatile uint32_t *)(base + RVA_OPCODE);
    filter_fn = base + RVA_FILTER;
    uint8_t *site = base + RVA_SITE;
    ok(site[0] == 0xE8 && (uint32_t)(site + 5 + get32(site + 1)) == (uint32_t)(base + RVA_PRINT)
       && get32(site + 9) == (uint32_t)count, "the stock tail calls the print and reads the counter");
    base[RVA_PRINT] = 0xE9;
    put32(base + RVA_PRINT + 1, (uint32_t)fake_print - (uint32_t)(base + RVA_PRINT + 5));

    static uint32_t chatObject[0x100];
    CONTEXT ctx = {0};
    ctx.Eip = 0x00401234;
    EXCEPTION_RECORD rec = {0};
    rec.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
    EXCEPTION_POINTERS ptrs = {&rec, &ctx};
    *chat = (uint32_t)chatObject;
    *opcode = 0x0A;

    printf("\n== stock: ten exceptions are swallowed, the eleventh is not\n");
    *count = 0; printed = 0;
    int ones = 0, regs = 1, result = 1;
    for (int i = 0; i < 11; i++) {
        result = call_filter(EXCEPTION_ACCESS_VIOLATION, &ptrs, "opcode");
        if (i < 10 && result == 1) ones++;
        regs &= regs_ok;
    }
    ok(ones == 10 && result == 0, "1 ten times, then 0: the game's own handler gets the eleventh (the crash)");
    ok(printed == 11 && strstr(last, "Warning(opcode 0xA): Exception 0xC0000005 at 0x401234") != NULL,
       "and every one is printed");
    ok(regs, "stack and registers intact");

    printf("\n== the tail, as fixes.lua writes it\n");
    memcpy(site, tail, TAIL);
    put32(site + AT_COUNT1, (uint32_t)count);
    put32(site + AT_COUNT2, (uint32_t)count);
    *count = 0; printed = 0; ones = 0; regs = 1;
    for (int i = 0; i < 30; i++) {
        if (call_filter(EXCEPTION_ACCESS_VIOLATION, &ptrs, "opcode") == 1) ones++;
        regs &= regs_ok;
    }
    ok(ones == 30, "30 exceptions, all swallowed");
    ok(printed == 10, "the first ten printed, as stock");
    ok(*count == 30, "the counter counts all 30");
    ok(regs, "stack and registers intact on both paths");

    printed = 0;
    ok(call_filter(EXCEPTION_ACCESS_VIOLATION, &ptrs, "CNetGame") == 1 && printed == 0 && *count == 31 && regs_ok,
       "the Warning(%s) branch shares the tail");
    *count = 0;
    call_filter(EXCEPTION_ACCESS_VIOLATION, &ptrs, "CNetGame");
    ok(printed == 1 && strstr(last, "Warning(CNetGame): Exception 0xC0000005 at 0x401234") != NULL,
       "and prints while under ten: " );

    *chat = 0; printed = 0; *count = 50;
    ok(call_filter(EXCEPTION_ACCESS_VIOLATION, &ptrs, "opcode") == 1 && printed == 0 && *count == 51 && regs_ok,
       "no chat yet: the entry's jump lands on the counter, swallowed, nothing printed");
    *chat = (uint32_t)chatObject;

    *opcode = 0x6E7; printed = 0; *count = 50;
    ok(call_filter(EXCEPTION_ACCESS_VIOLATION, &ptrs, "opcode") == 1 && printed == 1 && *count == 50 && regs_ok,
       "add_car_component keeps its own path: printed, swallowed, not counted");

    printf("\nexception filter: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
