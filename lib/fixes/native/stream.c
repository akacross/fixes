/* Model loading on the game's thread, timed (fixes.lua's streamtime option, a
 * diagnostic: docs/ASYNC_STREAMING_PLAN.md step 1), for gta_sa.exe 1.0 US;
 * included by crashfix.c after faults.c.
 *
 * CdStreamThread (0x406560) reads the IMG sectors on its own thread; what is
 * left on the game's thread is CStreaming::ConvertBufferToObject (the parse
 * and the D3D textures), FinishLoadingLargeFile (a big model's second half)
 * and LoadAllRequestedModels (a drain that also waits for the disk). Each is
 * entered through a wrapper that times the game's own function with
 * QueryPerformanceCounter. All three are cdecl and run on one thread; the
 * sums are not locked. Only integers are used: the x87 state is the game's.
 */
#include <stdarg.h>

#define STREAM(FJ) \
    FJ(streamconvert, "streamtime", "CStreaming::ConvertBufferToObject, timed", 0x40C6B0, 7, \
        "\x83\xEC\x20\x8B\x4C\x24\x24") \
    FJ(streamfinish, "streamtime", "CStreaming::FinishLoadingLargeFile, timed", 0x408CB0, 8, \
        "\x83\xEC\x08\x56\x8B\x74\x24\x14") \
    FJ(streamall, "streamtime", "CStreaming::LoadAllRequestedModels, timed", 0x40EA10, 5, "\xE9\x49\xBA\xFF\xFF")

STREAM(FDECLARE_J)

/* the game's functions past the bytes the hooks replace */
uint32_t __cdecl cf_stream_convert_orig(void *buffer, int32_t id);
uint32_t __cdecl cf_stream_finish_orig(void *buffer, int32_t id);
void __cdecl cf_stream_all_orig(uint32_t priority);

enum { ST_DFF, ST_TXD, ST_COL, ST_IFP, ST_OTHER, ST_TYPES };
enum { SK_CONVERT, SK_FINISH, SK_KINDS };
static const char *const stream_type_name[ST_TYPES] = {"dff", "txd", "col", "ifp", "other"};
static const char *const stream_kind_name[SK_KINDS] = {"convert", "finish"};
#define STREAM_IDS 26316        /* models, TXDs, COLs, IPLs, paths, IFPs, recordings, scripts */
#define STREAM_TOP 12           /* models in the summary */

/* The ranges ConvertBufferToObject branches on; IPLs, paths, recordings and
 * scripts are "other" */
int cf_stream_type(int32_t id)
{
    if (id < 0) return ST_OTHER;
    if (id < 20000) return ST_DFF;
    if (id < 25000) return ST_TXD;
    if (id < 25255) return ST_COL;
    if (id >= 25575 && id < 25755) return ST_IFP;
    return ST_OTHER;
}

typedef struct { uint32_t n, worst; int64_t ticks; } StreamSum;     /* worst in ticks */
static StreamSum stream_model[STREAM_IDS], stream_by[SK_KINDS][ST_TYPES];
static int64_t stream_freq, stream_began;
static int64_t stream_ticks, stream_all_ticks, stream_all_inner;    /* since the DLL loaded */
static uint32_t stream_count, stream_alls, stream_depth;
static int64_t stream_worst;                                        /* since the last reset */
static int32_t stream_worst_id = -1;
static int stream_on;
static char stream_path[MAX_PATH], stream_buf[1 << 16], stream_text[4096];
static uint32_t stream_len;

static int64_t stream_now(void)
{
    LARGE_INTEGER t;
    if (!stream_freq) {
        QueryPerformanceFrequency(&t);
        stream_freq = t.QuadPart > 0 ? t.QuadPart : 1;
    }
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

static uint64_t stream_us(int64_t ticks) { return ticks <= 0 ? 0 : (uint64_t)ticks * 1000000u / (uint64_t)stream_freq; }
/* a tick count as the two arguments of "%u.%03u" ms */
#define STREAM_MS(t) (unsigned)(stream_us(t) / 1000), (unsigned)(stream_us(t) % 1000)

static void stream_flush(void)
{
    if (!stream_len || !stream_path[0]) { stream_len = 0; return; }
    HANDLE h = CreateFileA(stream_path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD done;
        WriteFile(h, stream_buf, stream_len, &done, NULL);
        CloseHandle(h);
    }
    stream_len = 0;
}

static void stream_put(const char *fmt, ...)
{
    if (stream_len + 256 > sizeof stream_buf) stream_flush();
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(stream_buf + stream_len, 256, fmt, args);
    va_end(args);
    if (n > 0) stream_len += n < 256 ? n : 255;
}

static void stream_add(StreamSum *s, int64_t d)
{
    uint32_t w = d > 0xFFFFFFFFll ? 0xFFFFFFFFu : (uint32_t)d;
    s->n++;
    s->ticks += d;
    if (w > s->worst) s->worst = w;
}

/* The file's size in the IMG, KB: CStreamingInfo's sector count, through the
 * function's own reference to it */
static uint32_t stream_kb(int32_t id)
{
    if (id < 0 || id >= STREAM_IDS) return 0;
    return *(const uint32_t *)(*(const uint32_t *)0x40C6CE + (uint32_t)id * 20) * 2;
}

static void stream_record(int kind, int32_t id, int64_t t0, int64_t t1)
{
    int64_t d = t1 - t0;
    int type = cf_stream_type(id);
    stream_add(&stream_by[kind][type], d);
    if (id >= 0 && id < STREAM_IDS) stream_add(&stream_model[id], d);
    stream_ticks += d;
    stream_count++;
    if (d > stream_worst) stream_worst = d, stream_worst_id = id;
    if (stream_on)
        stream_put("+%u.%03u f%u %s %s %d %u.%03u ms %u KB%s\r\n", STREAM_MS(t0 - stream_began),
                   *(const uint32_t *)0xB7CB4C, stream_kind_name[kind], stream_type_name[type], (int)id, STREAM_MS(d),
                   stream_kb(id), stream_depth ? " in loadall" : "");
}

uint32_t __cdecl cf_stream_convert(void *buffer, int32_t id)
{
    int64_t t0 = stream_now();
    uint32_t r = cf_stream_convert_orig(buffer, id);
    stream_record(SK_CONVERT, id, t0, stream_now());
    return r;
}

uint32_t __cdecl cf_stream_finish(void *buffer, int32_t id)
{
    int64_t t0 = stream_now();
    uint32_t r = cf_stream_finish_orig(buffer, id);
    stream_record(SK_FINISH, id, t0, stream_now());
    return r;
}

/* The whole drain, the wait for the disk included, with who asked for it */
void __cdecl cf_stream_all(uint32_t priority)
{
    uint32_t from = (uint32_t)(uintptr_t)__builtin_return_address(0);
    int64_t inner = stream_ticks, t0 = stream_now();
    uint32_t count = stream_count;
    stream_depth++;
    cf_stream_all_orig(priority);
    stream_depth--;
    int64_t d = stream_now() - t0;
    if (stream_depth) return;                   /* a drain inside a drain is the outer one's */
    inner = stream_ticks - inner;
    stream_all_ticks += d;
    stream_all_inner += inner;
    stream_alls++;
    if (stream_on) {
        char who[MAX_PATH + 32];
        fault_name(from, who, sizeof who);
        stream_put("+%u.%03u f%u loadall %u.%03u ms, %u file(s) %u.%03u ms, from %s\r\n", STREAM_MS(t0 - stream_began),
                   *(const uint32_t *)0xB7CB4C, STREAM_MS(d), stream_count - count, STREAM_MS(inner), who);
    }
}

/* By kind and type, the drains, and the models that took longest in all */
static int stream_summary(char *out, int size)
{
    int n = 0;
    out[0] = 0;
    for (int k = 0; k < SK_KINDS; k++)
        for (int t = 0; t < ST_TYPES; t++) {
            const StreamSum *s = &stream_by[k][t];
            if (!s->n || n > size - 160) continue;
            n += snprintf(out + n, size - n, "%s %s: %u in %u.%03u ms, mean %u.%03u, worst %u.%03u\r\n",
                          stream_kind_name[k], stream_type_name[t], s->n, STREAM_MS(s->ticks),
                          STREAM_MS(s->ticks / s->n), STREAM_MS((int64_t)s->worst));
        }
    if (n <= size - 160)
        n += snprintf(out + n, size - n, "loadall: %u in %u.%03u ms, %u.%03u ms of it converting\r\n", stream_alls,
                      STREAM_MS(stream_all_ticks), STREAM_MS(stream_all_inner));
    int32_t top[STREAM_TOP];
    for (int i = 0; i < STREAM_TOP; i++) {
        top[i] = -1;
        for (int32_t id = 0; id < STREAM_IDS; id++) {
            if (!stream_model[id].n || (top[i] >= 0 && stream_model[id].ticks <= stream_model[top[i]].ticks)) continue;
            int taken = 0;
            for (int j = 0; j < i; j++) taken |= top[j] == id;
            if (!taken) top[i] = id;
        }
        if (top[i] < 0 || n > size - 160) break;
        const StreamSum *s = &stream_model[top[i]];
        n += snprintf(out + n, size - n, "%s %d: %u load(s), %u.%03u ms, worst %u.%03u, %u KB\r\n",
                      stream_type_name[cf_stream_type(top[i])], (int)top[i], s->n, STREAM_MS(s->ticks),
                      STREAM_MS((int64_t)s->worst), stream_kb(top[i]));
    }
    return n;
}

/* Each conversion and drain is a line in `path` from now on. */
__declspec(dllexport) int __cdecl CrashFix_StreamStart(const char *path)
{
    if (stream_on) return 1;
    SYSTEMTIME now;
    GetLocalTime(&now);
    snprintf(stream_path, sizeof stream_path, "%s", path ? path : "stream.log");
    stream_began = stream_now();
    stream_len = 0;
    stream_put("== %04u-%02u-%02u %02u:%02u:%02u fixes streamtime: +ms since this line, f = frame; convert/finish <type> <id>"
               " <ms> <KB in the IMG>\r\n", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    stream_flush();
    stream_on = 1;
    return 1;
}

__declspec(dllexport) void __cdecl CrashFix_StreamFlush(void) { stream_flush(); }

/* The summary, also appended to the log; the text is kept until the next call */
__declspec(dllexport) const char *__cdecl CrashFix_StreamSummary(void)
{
    int n = stream_summary(stream_text, sizeof stream_text);
    if (stream_on && stream_path[0]) {
        stream_flush();
        stream_put("-- summary at +%u.%03u\r\n", STREAM_MS(stream_now() - stream_began));
        stream_flush();
        HANDLE h = CreateFileA(stream_path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            DWORD done;
            WriteFile(h, stream_text, n, &done, NULL);
            CloseHandle(h);
        }
    }
    return stream_text;
}

__declspec(dllexport) int __cdecl CrashFix_StreamStop(void)
{
    if (!stream_on) return 1;
    CrashFix_StreamSummary();
    stream_on = 0;
    return 1;
}

/* 0 ms converting and finishing, 1 how many, 2 ms in drains, 3 drains, all
 * since the DLL loaded; 4 the longest one's ms and 5 its id since the last
 * CrashFix_StreamResetWorst (-1: none) */
__declspec(dllexport) double __cdecl CrashFix_StreamStat(int which)
{
    switch (which) {
    case 0: return (double)stream_us(stream_ticks) / 1000.0;
    case 1: return stream_count;
    case 2: return (double)stream_us(stream_all_ticks) / 1000.0;
    case 3: return stream_alls;
    case 4: return (double)stream_us(stream_worst) / 1000.0;
    case 5: return stream_worst_id;
    }
    return 0;
}

__declspec(dllexport) void __cdecl CrashFix_StreamResetWorst(void) { stream_worst = 0, stream_worst_id = -1; }

__asm__(
".intel_syntax noprefix\n"
"HOOK streamconvert\n"
"    jmp _cf_stream_convert\n"
".globl _cf_stream_convert_orig\n"
"_cf_stream_convert_orig:\n"
"    sub esp, 0x20\n"
"    mov ecx, dword ptr [esp+0x24]\n"
"    GO 0x40C6B7\n"
"HOOK streamfinish\n"
"    jmp _cf_stream_finish\n"
".globl _cf_stream_finish_orig\n"
"_cf_stream_finish_orig:\n"
"    sub esp, 8\n"
"    push esi\n"
"    mov esi, dword ptr [esp+0x14]\n"
"    GO 0x408CB8\n"
/* the entry is the exe's own jump to the first instruction, kept elsewhere */
"HOOK streamall\n"
"    jmp _cf_stream_all\n"
".globl _cf_stream_all_orig\n"
"_cf_stream_all_orig:\n"
"    GO 0x40A45E\n"
".att_syntax\n"
);
