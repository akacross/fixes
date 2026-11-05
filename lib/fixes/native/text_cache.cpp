// fixes.lua's text cache: TextCacheFix by Софтик Инквизитор (blast.hk), used
// and modified with his permission. Attached when fixes calls
// TextCache_Install instead of from DllMain. d3dx9_25's CFont replays strings
// it has drawn before: Microsoft's build 0x423B781A (vtable at RVA 0x1580), or
// any other (Wine's builtin) through the vtable of a font it creates, its
// sprite-less draws passed through. fixes loads it with LoadLibraryA and never
// frees it: the hooks live here, not in Lua.
//
//   ./build.sh            -> ../text_cache.dll
#include <windows.h>
#include <intrin.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

    using Color = DWORD;

    struct Vec3 {
        float x, y, z;
    };

    constexpr DWORD kD3dxTimestamp = 0x423B781A;
    constexpr uintptr_t kFontVtableRva = 0x1580;
    constexpr uintptr_t kFontSpriteOffset = 0xC;    // the font's own sprite, in Microsoft's build
    constexpr DWORD kSpriteAlphaBlend = 0x10;

    constexpr size_t kMaxEntries = 16384;
    constexpr int kMaxChars = 1024;
    constexpr unsigned kCheckEvery = 256;
    constexpr int kMaxMismatches = 3;
    constexpr LONG kFloatExact = 1 << 21;

    struct Sprite;

    struct SpriteVtbl {
        HRESULT(__stdcall *QueryInterface)(Sprite *, REFIID, void **);
        ULONG(__stdcall *AddRef)(Sprite *);
        ULONG(__stdcall *Release)(Sprite *);
        HRESULT(__stdcall *GetDevice)(Sprite *, void **);
        HRESULT(__stdcall *GetTransform)(Sprite *, void *);
        HRESULT(__stdcall *SetTransform)(Sprite *, const void *);
        HRESULT(__stdcall *SetWorldViewRH)(Sprite *, const void *, const void *);
        HRESULT(__stdcall *SetWorldViewLH)(Sprite *, const void *, const void *);
        HRESULT(__stdcall *Begin)(Sprite *, DWORD);
        HRESULT(__stdcall *Draw)(Sprite *, void *, const RECT *, const Vec3 *, const Vec3 *, Color);
        HRESULT(__stdcall *Flush)(Sprite *);
        HRESULT(__stdcall *End)(Sprite *);
        HRESULT(__stdcall *OnLostDevice)(Sprite *);
        HRESULT(__stdcall *OnResetDevice)(Sprite *);
    };

    struct Sprite {
        const SpriteVtbl *vtbl;
    };

    struct Glyph {
        void *texture;
        RECT src;
        Vec3 center;
        Vec3 pos;
        bool hasSrc;
        bool hasCenter;
    };

    struct Layout {
        std::vector<Glyph> glyphs;
        LONG width;
        LONG height;
        INT result;
    };

    struct Recorder {
        const SpriteVtbl *vtbl;
        Sprite *inner;
        bool forward;
        bool unsupported;
        Color color;
        std::vector<Glyph> *glyphs;
    };

    struct KeyHeader {
        void *font;
        DWORD format;
        LONG width;
        LONG height;
        DWORD flags;
    };

    struct KeyHash {
        size_t operator()(const std::string &key) const noexcept
        {
            auto p = reinterpret_cast<const uint8_t *>(key.data());
            size_t n = key.size();
            uint32_t h = 0x9E3779B9u ^ static_cast<uint32_t>(n);
            for (; n >= 4; n -= 4, p += 4) {
                uint32_t v;
                std::memcpy(&v, p, 4);
                v *= 0xCC9E2D51u;
                h = _rotl(h ^ (_rotl(v, 15) * 0x1B873593u), 13) * 5 + 0xE6546B64u;
            }
            for (; n; --n, ++p)
                h = (h ^ *p) * 0x01000193u;
            h ^= h >> 16;
            h *= 0x85EBCA6Bu;
            h ^= h >> 13;
            return h;
        }
    };

    enum FontMethod { kFontRelease = 2, kFontDrawTextA = 14, kFontDrawTextW = 15, kFontOnLost = 16, kFontOnReset = 17, kFontMethods = 18 };

    using DrawTextAFn = INT(__stdcall *)(void *, Sprite *, LPCSTR, INT, LPRECT, DWORD, Color);
    using DrawTextWFn = INT(__stdcall *)(void *, Sprite *, LPCWSTR, INT, LPRECT, DWORD, Color);
    using ReleaseFn = ULONG(__stdcall *)(void *);
    using DeviceFn = HRESULT(__stdcall *)(void *);

    DrawTextAFn g_drawTextA;
    DrawTextWFn g_drawTextW;
    ReleaseFn g_release;
    DeviceFn g_onLost;
    DeviceFn g_onReset;

    std::unordered_map<std::string, Layout, KeyHash> g_cache;
    std::unordered_map<void *, std::vector<std::string>> g_fontKeys;
    std::unordered_set<std::string, KeyHash> g_seen;
    std::unordered_set<std::string, KeyHash> g_uncacheable;
    std::string g_key;
    unsigned g_hits;
    unsigned g_recorded;
    int g_mismatches;
    bool g_disabled;
    uintptr_t g_ownSprite;      // kFontSpriteOffset, or 0: a build whose own sprite is unknown
    bool g_inside;              // Wine's DrawTextA calls DrawTextW through the vtable

    Recorder *AsRecorder(Sprite *s)
    {
        return reinterpret_cast<Recorder *>(s);
    }

    HRESULT __stdcall RecQueryInterface(Sprite *s, REFIID id, void **out)
    {
        Recorder *r = AsRecorder(s);
        return r->inner->vtbl->QueryInterface(r->inner, id, out);
    }

    ULONG __stdcall RecAddRef(Sprite *)
    {
        return 1;
    }

    ULONG __stdcall RecRelease(Sprite *)
    {
        return 1;
    }

    HRESULT __stdcall RecGetDevice(Sprite *s, void **out)
    {
        Recorder *r = AsRecorder(s);
        return r->inner->vtbl->GetDevice(r->inner, out);
    }

    HRESULT __stdcall RecGetTransform(Sprite *s, void *out)
    {
        Recorder *r = AsRecorder(s);
        return r->inner->vtbl->GetTransform(r->inner, out);
    }

    HRESULT __stdcall RecSetTransform(Sprite *s, const void *m)
    {
        Recorder *r = AsRecorder(s);
        r->unsupported = true;
        return r->forward ? r->inner->vtbl->SetTransform(r->inner, m) : S_OK;
    }

    HRESULT __stdcall RecSetWorldViewRH(Sprite *s, const void *a, const void *b)
    {
        Recorder *r = AsRecorder(s);
        r->unsupported = true;
        return r->forward ? r->inner->vtbl->SetWorldViewRH(r->inner, a, b) : S_OK;
    }

    HRESULT __stdcall RecSetWorldViewLH(Sprite *s, const void *a, const void *b)
    {
        Recorder *r = AsRecorder(s);
        r->unsupported = true;
        return r->forward ? r->inner->vtbl->SetWorldViewLH(r->inner, a, b) : S_OK;
    }

    HRESULT __stdcall RecBegin(Sprite *s, DWORD flags)
    {
        Recorder *r = AsRecorder(s);
        r->unsupported = true;
        return r->forward ? r->inner->vtbl->Begin(r->inner, flags) : S_OK;
    }

    HRESULT __stdcall RecDraw(Sprite *s, void *texture, const RECT *src, const Vec3 *center, const Vec3 *pos, Color color)
    {
        Recorder *r = AsRecorder(s);
        if (!pos || color != r->color) {
            r->unsupported = true;
        } else {
            Glyph g{};
            g.texture = texture;
            g.hasSrc = src != nullptr;
            if (src)
                g.src = *src;
            g.hasCenter = center != nullptr;
            if (center)
                g.center = *center;
            g.pos = *pos;
            r->glyphs->push_back(g);
        }
        return r->forward ? r->inner->vtbl->Draw(r->inner, texture, src, center, pos, color) : S_OK;
    }

    HRESULT __stdcall RecFlush(Sprite *s)
    {
        Recorder *r = AsRecorder(s);
        r->unsupported = true;
        return r->forward ? r->inner->vtbl->Flush(r->inner) : S_OK;
    }

    HRESULT __stdcall RecEnd(Sprite *s)
    {
        Recorder *r = AsRecorder(s);
        r->unsupported = true;
        return r->forward ? r->inner->vtbl->End(r->inner) : S_OK;
    }

    HRESULT __stdcall RecOnLostDevice(Sprite *s)
    {
        Recorder *r = AsRecorder(s);
        return r->inner->vtbl->OnLostDevice(r->inner);
    }

    HRESULT __stdcall RecOnResetDevice(Sprite *s)
    {
        Recorder *r = AsRecorder(s);
        return r->inner->vtbl->OnResetDevice(r->inner);
    }

    const SpriteVtbl kRecorderVtbl = {
        RecQueryInterface, RecAddRef, RecRelease, RecGetDevice,
        RecGetTransform, RecSetTransform, RecSetWorldViewRH, RecSetWorldViewLH,
        RecBegin, RecDraw, RecFlush, RecEnd,
        RecOnLostDevice, RecOnResetDevice,
    };

    void Purge()
    {
        g_cache.clear();
        g_fontKeys.clear();
        g_seen.clear();
        g_uncacheable.clear();
    }

    void PurgeFont(void *font)
    {
        auto it = g_fontKeys.find(font);
        if (it == g_fontKeys.end())
            return;
        for (const auto &key : it->second)
            g_cache.erase(key);
        g_fontKeys.erase(it);
    }

    void MarkUncacheable()
    {
        if (g_uncacheable.size() >= kMaxEntries)
            g_uncacheable.clear();
        g_uncacheable.insert(g_key);
    }

    void BlockOrigin(DWORD format, const RECT &rc, LONG width, LONG height, LONG &x, LONG &y)
    {
        if (format & DT_RTLREADING)
            format ^= DT_RIGHT;

        if (format & DT_CENTER)
            x = (rc.right - width + rc.left) / 2;
        else if (format & DT_RIGHT)
            x = rc.right - width;
        else
            x = rc.left;

        if (format & DT_VCENTER)
            y = (rc.bottom + rc.top - height) / 2;
        else if (format & DT_BOTTOM)
            y = rc.bottom - height;
        else
            y = rc.top;
    }

    bool SameGlyphs(const std::vector<Glyph> &cached, const std::vector<Glyph> &real, float dx, float dy)
    {
        if (cached.size() != real.size())
            return false;
        for (size_t i = 0; i < cached.size(); ++i) {
            const Glyph &a = cached[i];
            const Glyph &b = real[i];
            const Vec3 moved = { a.pos.x + dx, a.pos.y + dy, a.pos.z };
            if (a.texture != b.texture || a.hasSrc != b.hasSrc || a.hasCenter != b.hasCenter)
                return false;
            if (a.hasSrc && std::memcmp(&a.src, &b.src, sizeof(RECT)) != 0)
                return false;
            if (a.hasCenter && std::memcmp(&a.center, &b.center, sizeof(Vec3)) != 0)
                return false;
            if (std::memcmp(&moved, &b.pos, sizeof(Vec3)) != 0)
                return false;
        }
        return true;
    }

    bool IsFarAway(const RECT &rc)
    {
        return labs(rc.left) >= kFloatExact || labs(rc.top) >= kFloatExact ||
        labs(rc.right) >= kFloatExact || labs(rc.bottom) >= kFloatExact;
    }

    template <typename Char>
    int Length(const Char *str, INT count)
    {
        if (count >= 0)
            return count;
        int len = 0;
        while (len <= kMaxChars && str[len])
            ++len;
        return len;
    }

    template <typename Char>
    void BuildKey(void *font, Sprite *sprite, const Char *str, int len, const RECT &rc, DWORD format)
    {
        const bool clip = !(format & (DT_NOCLIP | DT_CALCRECT));
        const bool wraps = (format & (DT_WORDBREAK | DT_END_ELLIPSIS | DT_PATH_ELLIPSIS | DT_WORD_ELLIPSIS)) != 0;

        KeyHeader header{};
        header.font = font;
        header.format = format;
        header.width = (clip || wraps) ? rc.right - rc.left : 0;
        header.height = clip ? rc.bottom - rc.top : 0;
        header.flags = (sizeof(Char) == 2 ? 1u : 0u) | (sprite ? 2u : 0u);

        g_key.assign(reinterpret_cast<const char *>(&header), sizeof(header));
        g_key.append(reinterpret_cast<const char *>(str), len * sizeof(Char));
    }

    bool Replay(const Layout &layout, Sprite *target, bool ownSprite, float dx, float dy, Color color)
    {
        if (ownSprite && FAILED(target->vtbl->Begin(target, kSpriteAlphaBlend)))
            return false;
        for (const Glyph &g : layout.glyphs) {
            const Vec3 pos = { g.pos.x + dx, g.pos.y + dy, g.pos.z };
            if (FAILED(target->vtbl->Draw(target, g.texture, g.hasSrc ? &g.src : nullptr, g.hasCenter ? &g.center : nullptr, &pos, color)))
                break;
        }
        if (ownSprite)
            target->vtbl->End(target);
        return true;
    }

    template <typename Char, typename DrawFn>
    INT CachedDrawText(DrawFn draw, void *font, Sprite *sprite, const Char *str, INT count, LPRECT rc, DWORD format, Color color)
    {
        if (g_disabled || !str || !rc || (format & DT_MODIFYSTRING))
            return draw(font, sprite, str, count, rc, format, color);

        const bool measure = (format & DT_CALCRECT) != 0;
        Sprite *own = nullptr;
        if (!sprite && !measure) {
            if (!g_ownSprite)
                return draw(font, sprite, str, count, rc, format, color);
            own = *reinterpret_cast<Sprite **>(static_cast<uint8_t *>(font) + g_ownSprite);
            if (!own)
                return draw(font, sprite, str, count, rc, format, color);
        }
        if (!measure && !(format & DT_NOCLIP) && (format & (DT_CENTER | DT_RIGHT | DT_VCENTER | DT_BOTTOM)))
            return draw(font, sprite, str, count, rc, format, color);
        if (!measure && IsFarAway(*rc))
            return draw(font, sprite, str, count, rc, format, color);

        const int len = Length(str, count);
        if (len > kMaxChars)
            return draw(font, sprite, str, count, rc, format, color);

        BuildKey(font, sprite, str, len, *rc, format);
        Sprite *target = sprite ? sprite : own;
        const RECT in = *rc;

        auto it = g_cache.find(g_key);
        if (it != g_cache.end()) {
            const Layout &layout = it->second;
            LONG x, y;
            BlockOrigin(format, in, layout.width, layout.height, x, y);
            const RECT out = { x, y, x + layout.width, y + layout.height };
            const INT result = (y - in.top) + layout.result;
            const float dx = static_cast<float>(x);
            const float dy = static_cast<float>(y);

            if (++g_hits % kCheckEvery == 0) {
                std::vector<Glyph> glyphs;
                Recorder rec = { &kRecorderVtbl, target, false, false, color, &glyphs };
                RECT real = in;
                const INT realResult = draw(font, target ? reinterpret_cast<Sprite *>(&rec) : nullptr, str, count, &real, format, color);
                const bool same = !rec.unsupported && realResult == result && SameGlyphs(layout.glyphs, glyphs, dx, dy) &&
                std::memcmp(measure ? &out : &in, &real, sizeof(RECT)) == 0;
                if (!same) {
                    if (++g_mismatches >= kMaxMismatches) {
                        g_disabled = true;
                        Purge();
                    } else {
                        g_cache.erase(it);
                    }
                    return draw(font, sprite, str, count, rc, format, color);
                }
            }

            if (target && !Replay(layout, target, own != nullptr, dx, dy, color))
                return draw(font, sprite, str, count, rc, format, color);
            if (measure)
                *rc = out;
            return result;
        }

        if (g_uncacheable.count(g_key))
            return draw(font, sprite, str, count, rc, format, color);
        if (g_seen.insert(g_key).second) {
            if (g_seen.size() > kMaxEntries) {
                g_seen.clear();
                g_seen.insert(g_key);
            }
            return draw(font, sprite, str, count, rc, format, color);
        }

        Layout layout{};
        RECT size = in;
        const INT sizeResult = draw(font, nullptr, str, count, &size, format | DT_CALCRECT, color);
        if (sizeResult == 0) {
            MarkUncacheable();
            if (!measure)
                return draw(font, sprite, str, count, rc, format, color);
            *rc = size;
            return sizeResult;
        }

        layout.width = size.right - size.left;
        layout.height = size.bottom - size.top;
        LONG x, y;
        BlockOrigin(format, in, layout.width, layout.height, x, y);
        bool cacheable = x == size.left && y == size.top && sizeResult == (y - in.top) + layout.height;

        INT result;
        if (measure) {
            *rc = size;
            result = sizeResult;
        } else {
            Recorder rec = { &kRecorderVtbl, target, true, false, color, &layout.glyphs };
            if (own && FAILED(own->vtbl->Begin(own, kSpriteAlphaBlend)))
                return draw(font, sprite, str, count, rc, format, color);
            result = draw(font, reinterpret_cast<Sprite *>(&rec), str, count, rc, format, color);
            if (own)
                own->vtbl->End(own);
            if (rec.unsupported || std::memcmp(&in, rc, sizeof(RECT)) != 0)
                cacheable = false;
        }

        if (!cacheable) {
            MarkUncacheable();
            return result;
        }

        ++g_recorded;
        layout.result = result - (y - in.top);
        for (Glyph &g : layout.glyphs) {
            g.pos.x -= static_cast<float>(x);
            g.pos.y -= static_cast<float>(y);
        }
        if (g_cache.size() >= kMaxEntries)
            Purge();
        g_fontKeys[font].push_back(g_key);
        g_cache.emplace(g_key, std::move(layout));
        return result;
    }

    template <typename Char, typename DrawFn>
    INT OuterDrawText(DrawFn draw, void *font, Sprite *sprite, const Char *str, INT count, LPRECT rc, DWORD format, Color color)
    {
        if (g_inside)
            return draw(font, sprite, str, count, rc, format, color);
        g_inside = true;
        const INT result = CachedDrawText(draw, font, sprite, str, count, rc, format, color);
        g_inside = false;
        return result;
    }

    INT __stdcall HookDrawTextA(void *font, Sprite *sprite, LPCSTR str, INT count, LPRECT rc, DWORD format, Color color)
    {
        return OuterDrawText(g_drawTextA, font, sprite, str, count, rc, format, color);
    }

    INT __stdcall HookDrawTextW(void *font, Sprite *sprite, LPCWSTR str, INT count, LPRECT rc, DWORD format, Color color)
    {
        return OuterDrawText(g_drawTextW, font, sprite, str, count, rc, format, color);
    }

    ULONG __stdcall HookRelease(void *font)
    {
        const ULONG refs = g_release(font);
        if (refs == 0)
            PurgeFont(font);
        return refs;
    }

    HRESULT __stdcall HookOnLostDevice(void *font)
    {
        Purge();
        return g_onLost(font);
    }

    HRESULT __stdcall HookOnResetDevice(void *font)
    {
        Purge();
        return g_onReset(font);
    }

    bool InModule(const uint8_t *base, DWORD size, const void *p)
    {
        auto addr = static_cast<const uint8_t *>(p);
        return addr >= base && addr < base + size;
    }

    void **g_vtbl;

    using CreateFontAFn = HRESULT(__stdcall *)(void *, INT, UINT, UINT, UINT, BOOL, DWORD, DWORD, DWORD, DWORD, LPCSTR, void **);

    // The vtable of a font the module makes on `device`, if it lies in the module
    void **FontVtable(uint8_t *base, DWORD size, void *device)
    {
        auto create = reinterpret_cast<CreateFontAFn>(GetProcAddress(reinterpret_cast<HMODULE>(base), "D3DXCreateFontA"));
        void *font = nullptr;
        if (!create || FAILED(create(device, 12, 0, FW_NORMAL, 1, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH, "Arial", &font)) || !font)
            return nullptr;
        auto vtbl = *static_cast<void ***>(font);
        reinterpret_cast<ReleaseFn>(vtbl[kFontRelease])(font);
        return InModule(base, size, vtbl) ? vtbl : nullptr;
    }

    void WriteSlots(void **vtbl, void *const *values)
    {
        static const int kSlots[] = { kFontRelease, kFontDrawTextA, kFontDrawTextW, kFontOnLost, kFontOnReset };
        DWORD protect;
        VirtualProtect(vtbl, kFontMethods * sizeof(void *), PAGE_READWRITE, &protect);
        for (int i = 0; i < 5; ++i)
            vtbl[kSlots[i]] = values[i];
        VirtualProtect(vtbl, kFontMethods * sizeof(void *), protect, &protect);
    }

}

extern "C" {

    struct TextCacheStats {
        uint32_t installed;
        uint32_t hits;
        uint32_t recorded;
        uint32_t entries;
        uint32_t mismatches;
        uint32_t disabled;
    };

    // 1: hooked (or already). 0: not build 0x423B781A, and no font could be made
    // on `device` (may be null) to find another build's vtable. 2: a slot points
    // outside the module (another hook): *foreign gets its address.
    __declspec(dllexport) int __cdecl TextCache_Install(void *d3dxBase, void *device, uintptr_t *foreign)
    {
        if (g_vtbl)
            return 1;
        auto base = static_cast<uint8_t *>(d3dxBase);
        auto dos = reinterpret_cast<IMAGE_DOS_HEADER *>(base);
        auto nt = reinterpret_cast<IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
        const DWORD size = nt->OptionalHeader.SizeOfImage;
        void **vtbl = nullptr;
        uintptr_t ownSprite = 0;
        if (nt->FileHeader.TimeDateStamp == kD3dxTimestamp) {
            vtbl = reinterpret_cast<void **>(base + kFontVtableRva);
            ownSprite = kFontSpriteOffset;
        } else if (device) {
            vtbl = FontVtable(base, size, device);
        }
        if (!vtbl)
            return 0;
        for (int i = 0; i < kFontMethods; ++i) {
            if (!InModule(base, size, vtbl[i])) {
                if (foreign)
                    *foreign = reinterpret_cast<uintptr_t>(vtbl[i]);
                return 2;
            }
        }

        g_release = reinterpret_cast<ReleaseFn>(vtbl[kFontRelease]);
        g_drawTextA = reinterpret_cast<DrawTextAFn>(vtbl[kFontDrawTextA]);
        g_drawTextW = reinterpret_cast<DrawTextWFn>(vtbl[kFontDrawTextW]);
        g_onLost = reinterpret_cast<DeviceFn>(vtbl[kFontOnLost]);
        g_onReset = reinterpret_cast<DeviceFn>(vtbl[kFontOnReset]);
        g_ownSprite = ownSprite;
        MemoryBarrier();

        void *const hooks[] = {
            reinterpret_cast<void *>(&HookRelease), reinterpret_cast<void *>(&HookDrawTextA),
            reinterpret_cast<void *>(&HookDrawTextW), reinterpret_cast<void *>(&HookOnLostDevice),
            reinterpret_cast<void *>(&HookOnResetDevice),
        };
        WriteSlots(vtbl, hooks);
        g_vtbl = vtbl;
        return 1;
    }

    // Puts the stock slots back where they still hold ours. 1: every slot was ours.
    __declspec(dllexport) int __cdecl TextCache_Uninstall(void)
    {
        if (!g_vtbl)
            return 1;
        void **vtbl = g_vtbl;
        const bool ours = vtbl[kFontRelease] == reinterpret_cast<void *>(&HookRelease) &&
            vtbl[kFontDrawTextA] == reinterpret_cast<void *>(&HookDrawTextA) &&
            vtbl[kFontDrawTextW] == reinterpret_cast<void *>(&HookDrawTextW) &&
            vtbl[kFontOnLost] == reinterpret_cast<void *>(&HookOnLostDevice) &&
            vtbl[kFontOnReset] == reinterpret_cast<void *>(&HookOnResetDevice);
        if (!ours)
            return 0;   // someone hooked on top of us: leave the chain whole
        void *const stock[] = {
            reinterpret_cast<void *>(g_release), reinterpret_cast<void *>(g_drawTextA),
            reinterpret_cast<void *>(g_drawTextW), reinterpret_cast<void *>(g_onLost),
            reinterpret_cast<void *>(g_onReset),
        };
        WriteSlots(vtbl, stock);
        g_vtbl = nullptr;
        Purge();
        return 1;
    }

    __declspec(dllexport) void __cdecl TextCache_Stats(TextCacheStats *out)
    {
        out->installed = g_vtbl != nullptr;
        out->hits = g_hits;
        out->recorded = g_recorded;
        out->entries = static_cast<uint32_t>(g_cache.size());
        out->mismatches = static_cast<uint32_t>(g_mismatches);
        out->disabled = g_disabled;
    }

}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        g_key.reserve(4096);
        g_cache.max_load_factor(0.5f);
        g_seen.max_load_factor(0.5f);
    }
    return TRUE;
}
