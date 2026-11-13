/* First-chance crash records (fixes.lua's faultlog option), written down even
 * when nothing else runs; included by crashfix.c.
 *
 * SA-MP runs CGame::Process and the opcodes it is sent under an exception
 * filter that swallows a crash (fixes.lua swallows every one, not ten), so a
 * fault every frame freezes the world with no dump, and MoonLoader's scripts
 * stop with it. A vectored handler, first in line, copies each crash-type
 * exception - where, what it touched, the registers and the top of the stack -
 * into a ring and lets it go on: it only watches. A thread of ours appends the
 * new records to the log every 250 ms: addresses as module+offset, and the
 * return addresses on the stack (a dword into a module's code right after a
 * call). The same address again is counted, not copied. A fault inside
 * lua51.dll also copies where Lua was: the key, the table and the Lua frames.
 * Any text the registers or the top of the stack point at is copied too, so a
 * fault in the code reporting an error still names the error.
 */
#include <stdio.h>

#define FAULT_RING   32
#define FAULT_WORDS  256        /* dwords of stack copied: 1 KB */
#define FAULT_FRAMES 16         /* return addresses written per fault */
#define FAULT_LINES  64         /* faults written in full per session; then counts only */
#define FAULT_EVERY  5000       /* ms between "again" lines for one fault */
#define FAULT_LUA    6          /* Lua frames copied per fault */
#define FAULT_CHUNK  80         /* the end of a chunk name */
#define FAULT_TEXTS  12         /* strings copied per fault */
#define FAULT_TEXT   240        /* bytes of each */
#define FAULT_TEXT_MIN 6        /* anything shorter is not taken for text */
#define FAULT_TEXT_WORDS 128    /* dwords of stack looked through for them */

typedef struct { uint32_t firstline, line; char chunk[FAULT_CHUNK]; } FaultFrame;
typedef struct { uint32_t where; char text[FAULT_TEXT]; } FaultText;    /* where: 0-7 a register, then stack dwords */

typedef struct {
    volatile LONG state;        /* 0 free, 1 filling, 2 to write, 3 written */
    volatile LONG repeats;      /* the same address again since it was copied */
    LONG reported;              /* repeats already written */
    DWORD reportedAt;
    uint32_t number, code, eip, access, target, thread;
    uint32_t regs[8];           /* eax ecx edx ebx esp ebp esi edi */
    uint32_t words, stack[FAULT_WORDS];
    FILETIME when;
    /* a fault in lua51.dll */
    uint32_t luaFrames, ins, table, tableWords[10], hasTable;
    char key[64];
    FaultFrame lua[FAULT_LUA];
    uint32_t texts;
    FaultText text[FAULT_TEXTS];
} Fault;

static Fault fault_ring[FAULT_RING];
static volatile LONG fault_next, fault_busy, fault_seen, fault_dropped, fault_stop;
static LONG fault_written, fault_numbered, fault_dropped_told;
static void *fault_veh;
static HANDLE fault_thread;
static CRITICAL_SECTION fault_lock;
static char fault_path[MAX_PATH];
static uint32_t fault_lua_lo, fault_lua_hi;     /* lua51.dll's image */

static int fault_watched(DWORD code)
{
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION: case EXCEPTION_ILLEGAL_INSTRUCTION: case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_PRIV_INSTRUCTION: case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: case EXCEPTION_IN_PAGE_ERROR:
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Where Lua was (LuaJIT 2.1.0-beta3, 32-bit)                               */
/* ------------------------------------------------------------------------ */
/* The interpreter keeps BASE in edx, the next instruction in esi; a table
 * lookup has the table in ebp and the key in eax. Every read is checked: the
 * point of this is a Lua heap that has gone wrong. */

/* Bytes readable from va, at most max */
static uint32_t fault_span(uint32_t va, uint32_t max)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (va < 0x10000 || !VirtualQuery((const void *)(uintptr_t)va, &mbi, sizeof mbi)) return 0;
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return 0;
    uint32_t left = (uint32_t)(uintptr_t)mbi.BaseAddress + (uint32_t)mbi.RegionSize - va;
    return left < max ? left : max;
}

static int fault_mem(uint32_t va, void *out, uint32_t size)
{
    if (fault_span(va, size) < size) return 0;
    memcpy(out, (const void *)(uintptr_t)va, size);
    return 1;
}

/* A GCstr's text (its end, when longer than out); empty when it is not one */
static void fault_str(uint32_t str, char *out, uint32_t size)
{
    uint8_t gct = 0;
    uint32_t len = 0;
    out[0] = 0;
    if (!fault_mem(str + 5, &gct, 1) || gct != 4 || !fault_mem(str + 12, &len, 4)) return;
    uint32_t n = len < size - 1 ? len : size - 1;
    if (!fault_mem(str + 16 + (len - n), out, n)) return;
    for (uint32_t i = 0; i < n; i++) if ((uint8_t)out[i] < 32 || (uint8_t)out[i] > 126) out[i] = '?';
    out[n] = 0;
}

/* One Lua function: fn a GCfuncL, pc the instruction after the current one (0: unknown) */
static void fault_lua_frame(Fault *f, uint32_t fn, uint32_t pc)
{
    uint32_t bc = 0, chunk = 0, first = 0, numline = 0, lineinfo = 0, sizebc = 0;
    uint8_t gct = 0;
    if (!fault_mem(fn + 0x10, &bc, 4)) return;
    uint32_t pt = bc - 0x40;
    if (!fault_mem(pt + 5, &gct, 1) || gct != 7) return;
    if (!fault_mem(pt + 8, &sizebc, 4) || !fault_mem(pt + 0x28, &chunk, 4) || !fault_mem(pt + 0x2C, &first, 4)
        || !fault_mem(pt + 0x30, &numline, 4) || !fault_mem(pt + 0x34, &lineinfo, 4)) return;
    FaultFrame *fr = &f->lua[f->luaFrames++];
    fr->firstline = first;
    fr->line = 0;
    fault_str(chunk, fr->chunk, sizeof fr->chunk);
    if (pc <= bc || !lineinfo) return;
    uint32_t pos = (pc - bc) / 4 - 1;
    fr->line = first;
    if (pos == 0 || pos > sizebc) return;
    if (numline < 256) {
        uint8_t d = 0;
        if (fault_mem(lineinfo + pos - 1, &d, 1)) fr->line = first + d;
    } else if (numline < 65536) {
        uint16_t d = 0;
        if (fault_mem(lineinfo + (pos - 1) * 2, &d, 2)) fr->line = first + d;
    } else {
        uint32_t d = 0;
        if (fault_mem(lineinfo + (pos - 1) * 4, &d, 4)) fr->line = first + d;
    }
}

static void fault_lua(Fault *f, const CONTEXT *c)
{
    f->luaFrames = f->ins = f->table = f->hasTable = 0;
    f->key[0] = 0;
    if (c->Eip < fault_lua_lo || c->Eip >= fault_lua_hi) return;
    fault_mem(c->Esi - 4, &f->ins, 4);
    fault_str(c->Eax, f->key, sizeof f->key);
    f->table = c->Ebp;
    f->hasTable = (uint32_t)fault_mem(c->Ebp, f->tableWords, sizeof f->tableWords);
    uint32_t frame = c->Edx - 8, pc = c->Esi;
    for (int depth = 0; depth < 48 && f->luaFrames < FAULT_LUA; depth++) {
        uint32_t fn = 0, link = 0;
        uint8_t head[2] = {0, 0};                   /* gct, ffid */
        if (!fault_mem(frame, &fn, 4) || !fault_mem(frame + 4, &link, 4) || !fault_mem(fn + 5, head, 2)) return;
        if (head[0] != 8) return;                   /* not a function: the bottom, or not a frame */
        if (head[1] == 0) fault_lua_frame(f, fn, pc);
        if ((link & 3) == 0) {                      /* returns to Lua: link is its next instruction */
            uint32_t ins = 0;
            if (!fault_mem(link - 4, &ins, 4)) return;
            pc = link;
            frame -= (((ins >> 8) & 0xFF) + 1) * 8;
        } else {                                    /* C, continuation, vararg or pcall: link holds the size */
            if (!(link & ~7u)) return;
            pc = 0;
            frame -= link & ~7u;
        }
    }
}

/* The text at va, if it reads as text: mostly ASCII, no control characters */
static int fault_text_at(uint32_t va, char *out)
{
    uint32_t n = fault_span(va, FAULT_TEXT - 1), len = 0, ascii = 0;
    if (n < FAULT_TEXT_MIN) return 0;
    memcpy(out, (const void *)(uintptr_t)va, n);
    while (len < n && out[len]) len++;
    if (len < FAULT_TEXT_MIN) return 0;
    for (uint32_t i = 0; i < len; i++) {
        uint8_t c = (uint8_t)out[i];
        if (c == '\r' || c == '\n' || c == '\t') { out[i] = ' '; continue; }
        if (c < 32 || c == 127) return 0;
        if (c > 126) out[i] = '?'; else ascii++;
    }
    if (ascii * 4 < len * 3) return 0;
    out[len] = 0;
    return 1;
}

/* The registers first, then the stack; the same text once */
static void fault_texts(Fault *f)
{
    f->texts = 0;
    for (uint32_t i = 0; i < 8 + FAULT_TEXT_WORDS && f->texts < FAULT_TEXTS; i++) {
        if (i >= 8 && i - 8 >= f->words) break;
        FaultText *t = &f->text[f->texts];
        if (!fault_text_at(i < 8 ? f->regs[i] : f->stack[i - 8], t->text)) continue;
        uint32_t j = 0;
        while (j < f->texts && strcmp(f->text[j].text, t->text)) j++;
        if (j < f->texts) continue;
        t->where = i;
        f->texts++;
    }
}

static LONG CALLBACK fault_handler(EXCEPTION_POINTERS *e)
{
    EXCEPTION_RECORD *r = e->ExceptionRecord;
    if (!fault_watched(r->ExceptionCode)) return EXCEPTION_CONTINUE_SEARCH;
    InterlockedIncrement(&fault_seen);
    /* one at a time: another thread's fault meanwhile is counted, not copied */
    if (InterlockedCompareExchange(&fault_busy, 1, 0) != 0) return EXCEPTION_CONTINUE_SEARCH;
    uint32_t eip = (uint32_t)(uintptr_t)r->ExceptionAddress;
    for (int i = 0; i < FAULT_RING; i++) {
        Fault *f = &fault_ring[i];
        if (f->state >= 2 && f->eip == eip && f->code == r->ExceptionCode) {
            InterlockedIncrement(&f->repeats);
            InterlockedExchange(&fault_busy, 0);
            return EXCEPTION_CONTINUE_SEARCH;
        }
    }
    Fault *f = &fault_ring[(uint32_t)(InterlockedIncrement(&fault_next) - 1) % FAULT_RING];
    if (f->state == 1 || f->state == 2) {           /* the writer is behind */
        InterlockedIncrement(&fault_dropped);
        InterlockedExchange(&fault_busy, 0);
        return EXCEPTION_CONTINUE_SEARCH;
    }
    f->state = 1;
    f->code = r->ExceptionCode;
    f->eip = eip;
    f->access = r->NumberParameters >= 2 ? (uint32_t)r->ExceptionInformation[0] : 0;
    f->target = r->NumberParameters >= 2 ? (uint32_t)r->ExceptionInformation[1] : 0;
    f->thread = GetCurrentThreadId();
    CONTEXT *c = e->ContextRecord;
    f->regs[0] = c->Eax; f->regs[1] = c->Ecx; f->regs[2] = c->Edx; f->regs[3] = c->Ebx;
    f->regs[4] = c->Esp; f->regs[5] = c->Ebp; f->regs[6] = c->Esi; f->regs[7] = c->Edi;
    /* esp up to the thread's stack base is committed */
    NT_TIB *tib = (NT_TIB *)NtCurrentTeb();
    uint32_t sp = c->Esp, base = (uint32_t)(uintptr_t)tib->StackBase, limit = (uint32_t)(uintptr_t)tib->StackLimit;
    uint32_t n = 0;
    if (sp >= limit && sp < base) {
        n = (base - sp) / 4;
        if (n > FAULT_WORDS) n = FAULT_WORDS;
        memcpy(f->stack, (const void *)(uintptr_t)sp, n * 4);
    }
    f->words = n;
    fault_lua(f, c);
    fault_texts(f);
    f->repeats = 0;
    f->reported = 0;
    GetSystemTimeAsFileTime(&f->when);
    MemoryBarrier();
    f->state = 2;
    InterlockedExchange(&fault_busy, 0);
    return EXCEPTION_CONTINUE_SEARCH;
}

/* ------------------------------------------------------------------------ */
/* The writer (never in the handler)                                        */
/* ------------------------------------------------------------------------ */
/* "gta_sa.exe+0x349E75 (0x749E75)", or the bare address outside any module */
static void fault_name(uint32_t va, char *out, size_t size)
{
    HMODULE m = NULL;
    char path[MAX_PATH];
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)(uintptr_t)va, &m) && m && GetModuleFileNameA(m, path, sizeof path)) {
        const char *b = strrchr(path, '\\');
        snprintf(out, size, "%s+0x%X (0x%X)", b ? b + 1 : path, va - (uint32_t)(uintptr_t)m, va);
    } else {
        snprintf(out, size, "0x%08X", va);
    }
}

/* [va - len, va) is committed, executable memory */
static int fault_code_before(uint32_t va, uint32_t len)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (va < 0x10000 + len || !VirtualQuery((const void *)(uintptr_t)(va - len), &mbi, sizeof mbi)) return 0;
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return 0;
    if (!(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) return 0;
    return (uint32_t)(uintptr_t)mbi.BaseAddress + mbi.RegionSize >= va;
}

/* A return address: a call instruction ends right before it */
static int fault_after_call(uint32_t va)
{
    if (!fault_code_before(va, 7)) return 0;
    const uint8_t *p = (const uint8_t *)(uintptr_t)va;
    if (p[-5] == 0xE8) return 1;                                            /* call rel32 */
    if (p[-2] == 0xFF && (p[-1] & 0xF8) == 0xD0) return 1;                  /* call reg */
    if (p[-2] == 0xFF && (p[-1] & 0xF8) == 0x10 && (p[-1] & 7) != 4 && (p[-1] & 7) != 5) return 1;   /* call [reg] */
    if (p[-3] == 0xFF && (p[-2] & 0xF8) == 0x50 && (p[-2] & 7) != 4) return 1;   /* call [reg+d8] */
    if (p[-4] == 0xFF && p[-3] == 0x54) return 1;                           /* call [sib+d8] */
    if (p[-6] == 0xFF && (p[-5] == 0x15 || ((p[-5] & 0xF8) == 0x90 && (p[-5] & 7) != 4))) return 1;  /* call [d32], [reg+d32] */
    if (p[-7] == 0xFF && p[-6] == 0x94) return 1;                           /* call [sib+d32] */
    return 0;
}

static const char *fault_what(uint32_t code)
{
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION: return "access violation";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal instruction";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "integer divide by zero";
    case EXCEPTION_PRIV_INSTRUCTION: return "privileged instruction";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "array bounds exceeded";
    case EXCEPTION_IN_PAGE_ERROR: return "in-page error";
    }
    return "exception";
}

static void fault_stamp(const FILETIME *ft, char *out, size_t size)
{
    FILETIME local;
    SYSTEMTIME t;
    FileTimeToLocalFileTime(ft, &local);
    FileTimeToSystemTime(&local, &t);
    snprintf(out, size, "%04u-%02u-%02u %02u:%02u:%02u.%03u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
             t.wSecond, t.wMilliseconds);
}

static void fault_append(const char *text, size_t len)
{
    HANDLE h = CreateFileA(fault_path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD done;
    WriteFile(h, text, (DWORD)len, &done, NULL);
    CloseHandle(h);
}

static void fault_write(Fault *f, char *buf, size_t size)
{
    char when[48], at[MAX_PATH + 32], name[MAX_PATH + 32];
    size_t n = 0;
    fault_stamp(&f->when, when, sizeof when);
    fault_name(f->eip, at, sizeof at);
    f->number = (uint32_t)++fault_numbered;
    if (f->code == EXCEPTION_ACCESS_VIOLATION || f->code == EXCEPTION_IN_PAGE_ERROR)
        n += snprintf(buf + n, size - n, "%s fault %u: %s %s 0x%08X at %s, thread %u\r\n", when, f->number,
                      fault_what(f->code), f->access == 1 ? "writing" : f->access == 8 ? "executing" : "reading",
                      f->target, at, f->thread);
    else
        n += snprintf(buf + n, size - n, "%s fault %u: %s (0x%08X) at %s, thread %u\r\n", when, f->number,
                      fault_what(f->code), f->code, at, f->thread);
    n += snprintf(buf + n, size - n, "  eax=%08X ecx=%08X edx=%08X ebx=%08X esp=%08X ebp=%08X esi=%08X edi=%08X\r\n",
                  f->regs[0], f->regs[1], f->regs[2], f->regs[3], f->regs[4], f->regs[5], f->regs[6], f->regs[7]);
    static const char *const REGS[8] = {"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"};
    for (uint32_t i = 0; i < f->texts; i++) {
        const FaultText *t = &f->text[i];
        if (t->where < 8) n += snprintf(buf + n, size - n, "  text %s: \"%s\"\r\n", REGS[t->where], t->text);
        else n += snprintf(buf + n, size - n, "  text esp+0x%03X: \"%s\"\r\n", (t->where - 8) * 4, t->text);
    }
    if (f->luaFrames || f->key[0] || f->hasTable) {
        n += snprintf(buf + n, size - n, "  lua: key \"%s\", instruction %08X\r\n", f->key, f->ins);
        if (f->hasTable) {
            n += snprintf(buf + n, size - n, "  lua: table at %08X:", f->table);
            for (int i = 0; i < 10; i++) n += snprintf(buf + n, size - n, " %08X", f->tableWords[i]);
            n += snprintf(buf + n, size - n, "\r\n");
        }
        for (uint32_t i = 0; i < f->luaFrames; i++) {
            const FaultFrame *fr = &f->lua[i];
            if (fr->firstline) n += snprintf(buf + n, size - n, "  lua %u: %s:%u (function at line %u)\r\n", i + 1,
                                             fr->chunk, fr->line, fr->firstline);
            else n += snprintf(buf + n, size - n, "  lua %u: %s:%u (main chunk)\r\n", i + 1, fr->chunk, fr->line);
        }
    }
    int frames = 0;
    for (uint32_t i = 0; i < f->words && frames < FAULT_FRAMES && n < size - 128; i++) {
        if (!fault_after_call(f->stack[i])) continue;
        fault_name(f->stack[i], name, sizeof name);
        n += snprintf(buf + n, size - n, "  esp+0x%03X  %s\r\n", i * 4, name);
        frames++;
    }
    if (!frames) n += snprintf(buf + n, size - n, "  (no return address in %u bytes of stack)\r\n", f->words * 4);
    fault_append(buf, n);
}

/* force: "again" lines now, not FAULT_EVERY after the last */
static void fault_flush(int force)
{
    static char buf[16384];
    EnterCriticalSection(&fault_lock);
    DWORD now = GetTickCount();
    for (int i = 0; i < FAULT_RING; i++) {
        Fault *f = &fault_ring[i];
        if (f->state == 2) {
            if (fault_written < FAULT_LINES) {
                fault_write(f, buf, sizeof buf);
                fault_written++;
            } else {
                InterlockedIncrement(&fault_dropped);
            }
            f->reportedAt = now;
            MemoryBarrier();
            f->state = 3;
        }
        if (f->state == 3 && f->number && f->repeats != f->reported
            && (force || now - f->reportedAt >= FAULT_EVERY)) {
            LONG repeats = f->repeats;
            uint32_t eip = f->eip, number = f->number;
            char at[MAX_PATH + 32];
            fault_name(eip, at, sizeof at);
            int n = snprintf(buf, sizeof buf, "  fault %u again %ld time(s) at %s, %ld in all\r\n", number,
                             repeats - f->reported, at, repeats + 1);
            if (f->state == 3 && f->eip == eip) fault_append(buf, (size_t)n);
            f->reported = repeats;
            f->reportedAt = now;
        }
    }
    LONG dropped = fault_dropped;
    if (dropped != fault_dropped_told) {
        int n = snprintf(buf, sizeof buf, "  %ld more fault(s) counted but not copied (writer behind, or %d written)\r\n",
                         dropped - fault_dropped_told, FAULT_LINES);
        fault_append(buf, (size_t)n);
        fault_dropped_told = dropped;
    }
    LeaveCriticalSection(&fault_lock);
}

static DWORD WINAPI fault_writer(void *unused)
{
    (void)unused;
    while (!fault_stop) {
        Sleep(250);
        fault_flush(0);
    }
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Exports                                                                  */
/* ------------------------------------------------------------------------ */
/* Start watching, appending to `path` (once: a second call keeps the first). -> 1 */
__declspec(dllexport) int __cdecl CrashFix_FaultsStart(const char *path)
{
    static volatile LONG once;
    if (InterlockedCompareExchange(&once, 1, 0) == 0) InitializeCriticalSection(&fault_lock);
    if (fault_veh) return 1;
    HMODULE lua = GetModuleHandleA("lua51.dll");
    if (lua) {
        const IMAGE_NT_HEADERS *nt = (const IMAGE_NT_HEADERS *)((const char *)lua + ((const IMAGE_DOS_HEADER *)lua)->e_lfanew);
        fault_lua_lo = (uint32_t)(uintptr_t)lua;
        fault_lua_hi = fault_lua_lo + nt->OptionalHeader.SizeOfImage;
    }
    EnterCriticalSection(&fault_lock);
    snprintf(fault_path, sizeof fault_path, "%s", path ? path : "faults.log");
    char when[48], buf[256];
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    fault_stamp(&ft, when, sizeof when);
    int n = snprintf(buf, sizeof buf, "== %s fixes faultlog: crash-type exceptions as they happen (watched, not "
                     "handled; SA-MP or a mod may swallow them)\r\n", when);
    fault_append(buf, (size_t)n);
    LeaveCriticalSection(&fault_lock);
    fault_veh = AddVectoredExceptionHandler(1, fault_handler);
    if (!fault_veh) return 0;
    fault_stop = 0;
    if (!fault_thread) fault_thread = CreateThread(NULL, 0, fault_writer, NULL, 0, NULL);
    return 1;
}

/* Stop watching; what was copied is written first. -> 1 */
__declspec(dllexport) int __cdecl CrashFix_FaultsStop(void)
{
    if (!fault_veh) return 1;
    RemoveVectoredExceptionHandler(fault_veh);
    fault_veh = NULL;
    fault_flush(1);
    fault_stop = 1;
    if (fault_thread) {
        WaitForSingleObject(fault_thread, 1000);
        CloseHandle(fault_thread);
        fault_thread = NULL;
    }
    return 1;
}

/* Write what is copied now (and every "again" line). */
__declspec(dllexport) void __cdecl CrashFix_FaultsFlush(void) { if (fault_path[0]) fault_flush(1); }

/* Crash-type exceptions seen since the start, repeats included. */
__declspec(dllexport) uint32_t __cdecl CrashFix_FaultsSeen(void) { return (uint32_t)fault_seen; }
