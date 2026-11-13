/* lib/fixes/native/faults.c against the game's own lua51.dll, under Wine, in a
 * throwaway prefix: a table whose node pointer is garbage makes the interpreter
 * fault at lua51.dll+0x1CA3, and the log names the key, the table and the Lua
 * frames. First a fault of its own, with text in a register and on the stack.
 * See faults_lua_test.sh.
 *
 *     faults_lua_test.exe GAME_DIR LOG
 */
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "../native/faults.c"

typedef void lua_State;
static const char *log_path;
static int failed;

static const char SCRIPT[] =
    "jit.off() collectgarbage('stop')\n"
    "local ffi = require('ffi')\n"
    "local t = {a = 1}\n"
    "local p = ffi.cast('uint32_t*', tonumber(string.format('%p', t)))\n"
    "p[7] = 0\n"
    "p[5] = 0x27262607\n"
    "local function reader()\n"
    "    return t.sampGetGamestate\n"
    "end\n"
    "local function outer()\n"
    "    local v = reader()\n"
    "    return v\n"
    "end\n"
    "outer()\n";

static void ok(int cond, const char *what)
{
    printf("  %s: %s\n", cond ? "ok" : "FAIL", what);
    if (!cond) failed++;
}

static const char MESSAGE[] = "S:\\test\\probe.lua:12: attempt to index a nil value (field 'x')";
static const char *ON_STACK = "a second string, on the stack";
static const char *SHORT_TEXT = "abc";
static const char *BINARY = "\x01\x02" "abcdefgh";
static int stage;

/* mov eax,[eax] on a bad address: two bytes for after_fault to step over */
static void fault_with_text(void)
{
    __asm__ volatile ("pushl %[stack]\n\t"
                      "movl $0xFF010013, %%eax\n\t"
                      "movl (%%eax), %%eax\n\t"
                      "addl $4, %%esp\n\t"
                      : : [stack] "m" (ON_STACK), "b" (MESSAGE), "S" (SHORT_TEXT), "D" (BINARY)
                      : "eax", "memory", "cc");
}

static LONG CALLBACK after_fault(EXCEPTION_POINTERS *e)
{
    if (e->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) return EXCEPTION_CONTINUE_SEARCH;
    static char text[32768];
    CrashFix_FaultsFlush();
    FILE *f = fopen(log_path, "rb");
    size_t n = f ? fread(text, 1, sizeof text - 1, f) : 0;
    if (f) fclose(f);
    text[n] = 0;
    if (stage == 0) {
        stage = 1;
        ok(strstr(text, "text ebx: \"S:\\test\\probe.lua:12: attempt to index a nil value (field 'x')\"") != NULL,
           "the text a register points at");
        ok(strstr(text, "text esp+0x000: \"a second string, on the stack\"") != NULL, "the text the stack points at");
        ok(strstr(text, "text esi:") == NULL, "too short to be text");
        ok(strstr(text, "text edi:") == NULL, "not text");
        if (failed) printf("%s", text);
        e->ContextRecord->Eip += 2;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    ok(strstr(text, "access violation reading 0x27262613 at lua51.dll+0x1CA3") != NULL, "the fault, at the table lookup");
    ok(strstr(text, "lua: key \"sampGetGamestate\"") != NULL, "the key it was reading");
    ok(strstr(text, " 27262607 ") != NULL, "the table's garbage node pointer");
    const char *a = strstr(text, "lua 1: @S:\\test\\fault_probe.lua:8 (function at line 7)");
    const char *b = a ? strstr(a, "lua 2: @S:\\test\\fault_probe.lua:11 (function at line 10)") : NULL;
    const char *c = b ? strstr(b, "lua 3: @S:\\test\\fault_probe.lua:14 (main chunk)") : NULL;
    ok(a != NULL, "the function that faulted, and its line");
    ok(b != NULL, "its caller");
    ok(c != NULL, "the main chunk");
    if (failed) printf("%s", text);
    printf("faults lua: %s\n", failed ? "FAILED" : "passed");
    fflush(stdout);
    ExitProcess(failed ? 1 : 0);
    return EXCEPTION_CONTINUE_SEARCH;
}

int main(int argc, char **argv)
{
    if (argc < 3) return 2;
    log_path = argv[2];
    char path[MAX_PATH];
    SetDllDirectoryA(argv[1]);
    snprintf(path, sizeof path, "%s\\lua51.dll", argv[1]);
    HMODULE dll = LoadLibraryA(path);
    if (!dll) { printf("lua51 load error %lu\n", GetLastError()); return 3; }
    lua_State *(__cdecl *newstate)(void) = (void *)GetProcAddress(dll, "luaL_newstate");
    void (__cdecl *openlibs)(lua_State *) = (void *)GetProcAddress(dll, "luaL_openlibs");
    int (__cdecl *loadbuffer)(lua_State *, const char *, size_t, const char *) = (void *)GetProcAddress(dll, "luaL_loadbuffer");
    int (__cdecl *pcall)(lua_State *, int, int, int) = (void *)GetProcAddress(dll, "lua_pcall");
    const char *(__cdecl *tostring)(lua_State *, int, void *) = (void *)GetProcAddress(dll, "lua_tolstring");
    DeleteFileA(log_path);
    if (CrashFix_FaultsStart(log_path) != 1) return 4;
    AddVectoredExceptionHandler(0, after_fault);
    fault_with_text();
    lua_State *L = newstate();
    openlibs(L);
    int error = loadbuffer(L, SCRIPT, sizeof SCRIPT - 1, "@S:\\test\\fault_probe.lua");
    if (!error) error = pcall(L, 0, 0, 0);
    printf("no fault: %s\n", error ? tostring(L, -1, NULL) : "the script finished");
    return 5;
}
