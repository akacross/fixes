-- signature_scanner/patterns.lua - samp.dll signature database.
--
-- Target builds: R1, R3-1, R5 and 0.3.DL (the four SAMPFUNCS supports, with
-- verified offsets). R2/R4 are detected but unmeasured: scan only. 'R3' means
-- R3-1 (entry point 0xCC4D0; see version.lua M.LABELS).
--
-- Rules:
--  1. Only signatures matched against a real samp.dll; each entry records the
--     builds it was verified on.
--  2. samp.dll only. GTA functions live in gta_sa.exe at absolute addresses
--     (gta_offsets.lua); scanning samp.dll for them cannot work.
--  3. Descriptor fields:
--       pattern   IDA-style bytes, '?'/'??' = wildcard
--       sections  which PE sections to scan (default: executable ones)
--       offset    added to the match RVA before extraction
--       extract   nil       -> the (match + offset) RVA itself
--                 'rel32'   -> E8/E9 branch target at match+offset, as an RVA
--                 'imm32va' -> absolute VA operand at match+offset, converted
--                              back to an RVA using the module's ImageBase
--                 'imm32'   -> raw dword operand at match+offset
--       expect    optional per-version RVA cross-check; a mismatch is logged
--
-- New patterns come from tools/gen_pattern.py (derives a wildcarded pattern
-- from a known address and counts matches per build); don't hand-write them.
--
-- R1 and R2 carry the packer's .poop0/1/2 sections: .text is plain x86 on disk,
-- but the packer sections are DISCARDABLE and memory.getScanRanges() skips them.

local M = {}

--------------------------------------------------------------------------------
-- SA-MP: F1 help key handler
--------------------------------------------------------------------------------
--   mov  eax, ds:g_SAMP        A1 <g_SAMP>
--   test eax, eax              85 C0
--   jz   skip                  74 18
--   push 70h                   6A 70          ; VK_F1
--   call esi                   FF D6          ; GetAsyncKeyState
--   test ax, ax                66 85 C0
--   jz   skip                  74 0F
--   mov  ecx, ds:<help ptr>    8B 0D <ptr>
--   test ecx, ecx              85 C9
--   jz   skip                  74 05
--   call ShowHelpDialogFunc        E8 <rel32>     ; match + 28
--
-- Unique in .text on every supported build:
--   R1    samp.dll md5 1d22eaa2605717ddf215f68e861de378 -> RVA 0x713D6
--   R3-1  samp.dll md5 08cf4166d916e314ed3ee8cff2f13cca -> RVA 0x752C6
--   R5    samp.dll md5 5ba5f0be7af99dfd03fb39e88a970a2b -> RVA 0x759D5
--   DLR1  samp.dll md5 449e4f985215ffb5bffadf23551c0d50 -> RVA 0x75456
-- R5 and DLR1 confirmed in game; R3-1 matches fixes.lua's hand-measured offset.
local SIG_F1_HELP =
    'A1 ?? ?? ?? ?? 85 C0 74 18 6A 70 FF D6 66 85 C0 74 0F 8B 0D ?? ?? ?? ?? 85 C9 74 05 E8 ?? ?? ?? ??'

M.common = {
    -- Start of the 28-byte VK_F1 poll block (what fixes.lua NOPs first).
    F1KeyHandler = {
        pattern  = SIG_F1_HELP,
        sections = { '.text' },
        offset   = 0,
        expect   = { DLR1 = 0x75456, R1 = 0x713D6, R3 = 0x752C6, R5 = 0x759D5 },
        note     = '28 bytes of VK_F1 polling; NOP to disable the F1 help key',
        size     = 28,
    },

    -- The `call ShowHelpDialogFunc` site immediately after it (the 5 bytes fixes.lua
    -- NOPs second). This is the call *instruction*, not the callee.
    ShowHelpDialogCall = {
        pattern  = SIG_F1_HELP,
        sections = { '.text' },
        offset   = 28,
        expect   = { DLR1 = 0x75472, R1 = 0x713F2, R3 = 0x752E2, R5 = 0x759F1 },
        note     = '5-byte E8 call site; NOP to suppress the help dialog',
        size     = 5,
    },

    -- The function that call targets.
    ShowHelpDialogFunc = {
        pattern  = SIG_F1_HELP,
        sections = { '.text' },
        offset   = 28,
        extract  = 'rel32',
        expect   = { DLR1 = 0x6B570, R1 = 0x67450, R3 = 0x6B3C0, R5 = 0x6BB30 },
    },

    -- The global this block loads first: the SAMP_INFO / g_SAMP pointer slot
    -- (read a dword for the struct). R1 0x21A0F8 (the published
    -- SAMP_INFO_OFFSET), R3-1 0x26E8DC, R5 0x26EB94, DLR1 0x2ACA24, all in .data.
    SAMP_INFO = {
        pattern  = SIG_F1_HELP,
        sections = { '.text' },
        offset   = 1,
        extract  = 'imm32va',
        expect   = { DLR1 = 0x2ACA24, R1 = 0x21A0F8, R3 = 0x26E8DC, R5 = 0x26EB94 },
        note     = 'RVA of the g_SAMP pointer slot (dereference for the struct)',
    },
}

--------------------------------------------------------------------------------
-- SA-MP: CNetGame
--------------------------------------------------------------------------------
-- CNetGame is the object g_SAMP points at: SAMP_INFO (SAMPFUNCS) and CNetGame
-- (sampapi) name one pointer, and SFAPI calls the struct stSAMP. It owns the
-- host (this+0x30), port (this+0x235) and the RakClient interface (R3-1 layout).
--
-- The member layout differs per build: look offsets up in fields.lua
-- (generated from the SFAPI headers, checked against all four DLLs), e.g.
--   rakClientInterface   R1 +0x3C9   R3-1 +0x2C   R5 +0x000   DL +0x2C
--   pools                R1 +0x3CD   R3-1 +0x3DE  R5 +0x3DE   DL +0x3DE
-- R1's whole struct is shifted (serverIp +0x20, serverPort +0x225) and its tail
-- is ordered differently.
--
-- Teardown site, unique on all four builds (the only .text write to g_SAMP):
--   mov  ecx, ds:g_SAMP     8B 0D <g_SAMP>
--   cmp  ecx, edi           3B CF            ; edi = 0
--   jz   skip               74 16
--   mov  esi, ecx           8B F1
--   call CNetGame::~...     E8 <rel32>       ; thiscall, this = the instance
--   push esi                56
--   call operator delete    E8 <rel32>
--   add  esp, 4             83 C4 04
--   mov  ds:g_SAMP, edi     89 3D <g_SAMP>   ; g_SAMP = NULL
--
-- Shares nothing with SIG_F1_HELP, so CNetGame and SAMP_INFO cross-check each
-- other; host_test.lua asserts they agree.
local SIG_CNETGAME_TEARDOWN =
    '8B 0D ?? ?? ?? ?? 3B CF 74 16 8B F1 E8 ?? ?? ?? ?? 56 E8 ?? ?? ?? ?? 83 C4 04 89 3D ?? ?? ?? ??'

M.common.CNetGame = {
    pattern  = SIG_CNETGAME_TEARDOWN,
    sections = { '.text' },
    offset   = 2,
    extract  = 'imm32va',
    expect   = { DLR1 = 0x2ACA24, R1 = 0x21A0F8, R3 = 0x26E8DC, R5 = 0x26EB94 },
    note     = 'RVA of the CNetGame instance pointer -- the same slot as SAMP_INFO',
}

-- The thiscall made on the instance immediately before its memory is freed.
M.common.CNetGameDtor = {
    pattern  = SIG_CNETGAME_TEARDOWN,
    sections = { '.text' },
    offset   = 12,
    extract  = 'rel32',
    expect   = { DLR1 = 0x9570, R1 = 0x9380, R3 = 0x9510, R5 = 0x9880 },
    note     = 'CNetGame destructor (thiscall, runs just before operator delete)',
}

-- Construction site. R1 constructs CNetGame from the packed .poop1 region, so it
-- has no such symbol (byVersion, not common).
local CNETGAME_CTOR = {
    pattern  = '8B CE E8 ?? ?? ?? ?? EB 02 33 C0 A3 ?? ?? ?? ?? C7 05 ?? ?? ?? ?? 01 00 00 00',
    sections = { '.text' },
    offset   = 2,
    extract  = 'rel32',
    expect   = { DLR1 = 0xB610, R3 = 0xB5F0, R5 = 0xB930 },
    note     = 'CNetGame constructor; sets szIP/port and creates the RakClient',
}

--------------------------------------------------------------------------------
-- SA-MP: frame limiter / timestep override
--------------------------------------------------------------------------------
-- SA-MP's per-frame timing update, behind the "SA-MP FPS lock":
--
--   push ecx / push esi / push edi   51 56 57
--   mov  edi, ecx                    8B F9            ; thiscall, no stack args
--   call GetTickCount                E8 <rel32>
--   mov  esi, eax                    8B F0
--   mov  eax, ds:<lastSampleTick>    A1 <imm32>
--   test eax, eax                    85 C0
--   jnz  measure                     75 2C
--   mov  eax, 1.0f                   B8 00 00 80 3F
--   mov  ds:0xB7CB5C, eax            A3 5C CB B7 00   ; CTimer::ms_fTimeStep
--   mov  ds:0xB7CB58, eax            A3 58 CB B7 00   ; CTimer::ms_fTimeStepNonClipped
--
-- Every 250 ms it writes the frame time over CTimer::ms_fTimeStep /
-- ms_fTimeStepNonClipped. 0xC3 (ret) at the entry disables all of it - FPS
-- bookkeeping and timestep override, not just the throttle. A bare `ret` is
-- right: thiscall, no stack arguments.
--
-- The CTimer addresses are gta_sa.exe globals (gta-reversed game_sa/Timer.h),
-- identical in every build, so one signature covers all four.
--
-- Unique in .text: R1 0x9D170   R3-1 0xA14E0   R5 0xA1C10   DL 0xA1A30, each
-- after CC padding. R1 is the known `samp.dll + 0x9D170` FPS-unlock patch.
M.common.FrameLimiter = {
    pattern  = '51 56 57 8B F9 E8 ?? ?? ?? ?? 8B F0 A1 ?? ?? ?? ?? 85 C0 75 2C ' ..
               'B8 00 00 80 3F A3 5C CB B7 00 A3 58 CB B7 00',
    sections = { '.text' },
    offset   = 0,
    expect   = { DLR1 = 0xA1A30, R1 = 0x9D170, R3 = 0xA14E0, R5 = 0xA1C10 },
    note     = 'per-frame timing update; hook +0x0C to bypass the throttle and the timestep override',
    size     = 1,
}

--------------------------------------------------------------------------------
-- SA-MP: frame-sleep stub inside GTA's own frame limiter
--------------------------------------------------------------------------------
-- gta_sa.exe's frame limiter busy-waits at 0x53E920 until 14 ms pass, then
-- calls CTimer::Update:
--
--   0x53E930  call 0x561A80            ; elapsed-time helper  <- SA-MP overwrites
--   0x53E935  mov  esi, eax
--             ...
--   0x53E94A  cmp  eax, 0xE            ; 14 ms
--   0x53E94D  jb   0x53E930            ; spin
--   0x53E968  call 0x561B10            ; CTimer::Update
--
-- SA-MP points the call at 0x53E930 at this stub, turning the busy-wait into a
-- sleeping wait:
--
--   push 1                             6A 01            ; <- match, +1 is the imm8
--   call ds:[Sleep]                    FF 15 <slot>
--   mov  edx, 0x561A80                 BA 80 1A 56 00
--   jmp  edx                           FF E2
--
-- Resolves the push's imm8; writing 0 makes it Sleep(0) (as crashes.asi does).
-- The import slot relocates (four wildcards); 0x561A80 is a gta_sa.exe address.
--
-- Unique in .text: R1 0x9D8F0, R3-1 0xA1CB0, R5 0xA23E0, DL 0xA2200 (imm8 at +1).
M.common.FrameSleepStub = {
    pattern  = '6A 01 FF 15 ?? ?? ?? ?? BA 80 1A 56 00 FF E2',
    sections = { '.text' },
    offset   = 1,
    expect   = { DLR1 = 0xA2201, R1 = 0x9D8F1, R3 = 0xA1CB1, R5 = 0xA23E1 },
    note     = 'imm8 of `push 1` before Sleep in GTA\'s limiter spin; write 0 for Sleep(0)',
    size     = 1,
}

--------------------------------------------------------------------------------
-- SA-MP: radar toggle
--------------------------------------------------------------------------------
-- SA-MP's __stdcall "show/hide the radar" helper:
--
--   mov  ecx, [esp+4]           8B 4C 24 04
--   xor  eax, eax               33 C0
--   cmp  ecx, eax               3B C8
--   jz   hide                   74 0F            ; <- match + 8
--   mov  byte ds:0xBA6769, 1    C6 05 69 67 BA 00 01
--   mov  ds:0xBAA3FB, al        A2 FB A3 BA 00   ; CHud::bScriptDontDisplayRadar = 0
--   ret  4                      C2 04 00
--  hide:
--   mov  ds:0xBA6769, al
--   mov  byte ds:0xBAA3FB, 1                     ; bScriptDontDisplayRadar = 1
--
-- NOPing the jz at +8 (90 90) keeps the radar shown; 74 0F restores it.
-- 0xBAA3FB is CHud::bScriptDontDisplayRadar (gta-reversed game_sa/Hud.h); both
-- globals are gta_sa.exe addresses, so no wildcards.
--
-- Unique in .text: R1 0x9D318, R3-1 0xA1688, R5 0xA1DB8, DL 0xA1BD8
-- (R1 = sampGetBase() + 643864).
M.common.RadarToggleBranch = {
    pattern  = '8B 4C 24 04 33 C0 3B C8 74 0F C6 05 69 67 BA 00 01 A2 FB A3 BA 00 C2 04 00',
    sections = { '.text' },
    offset   = 8,
    expect   = { DLR1 = 0xA1BD8, R1 = 0x9D318, R3 = 0xA1688, R5 = 0xA1DB8 },
    note     = 'jz inside the radar toggle; 0x9090 forces the radar on, 0x0F74 restores',
    size     = 2,
}

--------------------------------------------------------------------------------
-- SA-MP: debug entity label format string
--------------------------------------------------------------------------------
-- The format string of SA-MP's debug labels over entities:
--   "[id: %d, type: %d subtype: %d Health: %.1f preloaded: %u]\n
--    Distance: %.2fm\nPassengerSeats: %u\ncPos: %.3f,%.3f,%.3f\n
--    sPos: %.3f,%.3f,%.3f\0"
-- 135 (0x87) bytes with the terminator on every build; zero it and the labels
-- render empty. It is data in .rdata, read-only at runtime: unprotect around the
-- write (fixes.lua:blankDebugEntityLabels()).
--
-- Unique in .rdata: R1 0xD83A8, R3-1 0xEA780, R5 0xEA7D8, DL 0x11C800.
M.common.DebugLabelFormat = {
    pattern  = '5B 69 64 3A 20 25 64 2C 20 74 79 70 65 3A 20 25 64 20 73 75 62 74 79 70 65 3A 20 ' ..
               '25 64 20 48 65 61 6C 74 68 3A 20 25 2E 31 66 20 70 72 65 6C 6F 61 64 65 64 3A 20 25 75 5D',
    sections = { '.rdata' },
    offset   = 0,
    expect   = { DLR1 = 0x11C800, R1 = 0xD83A8, R3 = 0xEA780, R5 = 0xEA7D8 },
    note     = 'debug entity label format string, 135 bytes including the NUL',
    size     = 135,
}

--------------------------------------------------------------------------------
-- SA-MP: connection retry delay
--------------------------------------------------------------------------------
-- The 3000 ms throttle between connection attempts, in the CNetGame method that
-- logs "Connecting to %s:%d...":
--
--   call GetTickCount
--   sub  eax, [esi+0x3D1]    2B 86 D1 03 00 00   ; this->lastConnectAttempt
--   cmp  eax, 0xBB8          3D B8 0B 00 00      ; 3000 ms
--   jbe  skip                76 ??
--
-- Resolves the cmp's imm32 (match + 7); a dword 0 there makes reconnects
-- immediate. R1 keeps this in packed .poop1 with mutated surroundings, so it has
-- its own signature; both put the imm32 at +7.
--
-- Unique: R1 0x2D3C45 (.poop1, = sampGetBase() + 2964549), R3-1 0x85E3,
-- R5 0x8953, DL 0x8643.
local CONNECT_DELAY_EXPECT = { DLR1 = 0x8643, R1 = 0x2D3C45, R3 = 0x85E3, R5 = 0x8953 }

M.common.ConnectDelayCmp = {
    pattern  = '2B 86 D1 03 00 00 3D B8 0B 00 00',
    sections = { '.text' },
    offset   = 7,
    expect   = CONNECT_DELAY_EXPECT,
    note     = 'imm32 of `cmp eax, 3000`; write dword 0 to drop the reconnect wait',
    size     = 4,
}

-- R1's copy lives in the mutated .poop1 code, so it needs its own anchor and
-- must not be restricted to .text.
local CONNECT_DELAY_R1 = {
    pattern  = 'F5 68 31 5A 05 27 3D B8 0B 00 00 9C 68 5F 23 53 2D 9C',
    offset   = 7,
    expect   = CONNECT_DELAY_EXPECT,
    note     = 'imm32 of `cmp eax, 3000` (R1, inside .poop1)',
    size     = 4,
}

--------------------------------------------------------------------------------
-- SA-MP: smoking animation (SPECIAL_ACTION_SMOKE_CIGGY)
--------------------------------------------------------------------------------
-- One CPlayerPed method drives the four drink/smoke special actions off a 1..4
-- selector. Drinks (1-3) apply "BAR:dnk_stndM_loop"; the cigarette (4) applies
-- "GANGS:smkcig_prtl" with two constants of its own:
--
--   cmp  eax, 4                    83 F8 04
--   jne  out                       0F 85 ?? ?? ?? ??
--   push 0ABEh                     68 BE 0A 00 00      ; time  = 2750 ms  <- +1
--   push 0 (x4)                    6A 00 6A 00 6A 00 6A 00
--   push 442F0000h                 68 00 00 2F 44      ; blend = 700.0f   <- +14
--   push offset "GANGS"            68 <ptr>
--   push offset "smkcig_prtl"      68 <ptr>
--   mov  ecx, esi                  8B CE
--   call ApplyAnimation            E8 <rel32>
--
-- The drink branch pushes -1 and 40800000h (4.0f); writing those over the
-- cigarette's operands is fixes.lua:fixSmokingAnimation(). Both descriptors
-- resolve an operand (match + 1, match + 14).
--
-- Only the string pushes relocate. Unique in .text on all four DLLs (md5s as
-- for SIG_F1_HELP): R1 0xA85D4, R3-1 0xAD4A4, R5 0xADD34, DLR1 0xAD934.
local SIG_SMOKE_ANIM = '68 BE 0A 00 00 6A 00 6A 00 6A 00 6A 00 68 00 00 2F 44 68 ?? ?? ?? ?? 68'

M.common.SmokeAnimTime = {
    pattern  = SIG_SMOKE_ANIM,
    sections = { '.text' },
    offset   = 1,
    expect   = { DLR1 = 0xAD935, R1 = 0xA85D5, R3 = 0xAD4A5, R5 = 0xADD35 },
    note     = 'imm32 of `push 2750` (animation time, ms); write -1 to play until replaced',
    size     = 4,
}

M.common.SmokeAnimBlend = {
    pattern  = SIG_SMOKE_ANIM,
    sections = { '.text' },
    offset   = 14,
    expect   = { DLR1 = 0xAD942, R1 = 0xA85E2, R3 = 0xAD4B2, R5 = 0xADD42 },
    note     = 'imm32 of `push 700.0f` (blend delta); write 4.0f to match the drinking cases',
    size     = 4,
}

-- RemoveBuildingForPlayer's table: 1000 entries of {model, x, y, z, radius}
-- (20 bytes), the count right after it, and no bounds check on the store (the
-- overflow fixes.lua:fixRemovedBuildings() closes). The only two sites that
-- touch the table, on all four builds:
--
-- The store, in the RPC handler:
--   mov  ecx, [count]             8B 0D <count>        <- RemoveBuildingStore
--   mov  edx, [esp+20h]           8B 54 24 20          ; the pattern starts here,
--   lea  eax, [ecx+ecx*4]         8D 04 89             ; 6 bytes in, so it still
--   shl  eax, 2                   C1 E0 02             ; matches once fixes has
--   mov  [eax+table], esi         89 B0 <table>        ; written its 5-byte jmp
--   ... x, y, z, radius to table+4..+10h, then
--   inc  ecx / mov [count], ecx / pop esi / ret
--
-- The lookup SA-MP's CFileLoader::LoadObjectInstance hook calls:
--   mov  eax, [count]             A1 <count>           ; the pattern starts after it:
--   push ebx/ebp/esi/edi, xor edi,edi, test eax,eax,   ; open.mp's client hooks the
--   jz, two arg loads                                  ; lookup with a 5-byte jmp
--   mov  esi, table               BE <table>           <- RemoveBuildingLookup (+19)
--
-- Identical code on every build. Unique in .text:
--   R1 0x98D8F / 0x98A08, R3-1 0x9CCDF / 0x9C958, R5 0x9D3EF / 0x9D068,
--   DLR1 0x9D22F / 0x9CAF8; the lookup also R2 0x98AA8, R3 0x9C958, R4 0x9D098
M.common.RemoveBuildingStore = {
    pattern  = '8B 54 24 20 8D 04 89 C1 E0 02 89 B0 ?? ?? ?? ?? 89 90 ?? ?? ?? ?? 8B 54 24 24 89 90 ?? ?? ?? ?? ' ..
               '8B 54 24 28 83 C4 14 89 90 ?? ?? ?? ?? 8B 54 24 18 41 89 90 ?? ?? ?? ?? 89 0D ?? ?? ?? ?? 5E C3',
    sections = { '.text' },
    offset   = -6,
    expect   = { DLR1 = 0x9D22F, R1 = 0x98D8F, R3 = 0x9CCDF, R5 = 0x9D3EF },
    note     = '`mov ecx,[count]` of the removed-building store (fixes jumps from here to a bounded store)',
}

M.common.RemoveBuildingLookup = {
    pattern  = '53 55 56 57 33 FF 85 C0 74 4D 8B 5C 24 20 8B 6C 24 1C BE ?? ?? ?? ??',
    sections = { '.text' },
    offset   = 19,
    expect   = { DLR1 = 0x9CAF8, R1 = 0x98A08, R2 = 0x98AA8, R3 = 0x9C958, R4 = 0x9D098, R5 = 0x9D068 },
    note     = 'imm32 of `mov esi, table` in the removed-building lookup (fixes points it at its own table)',
    size     = 4,
}

-- Per-version overrides (same descriptor format; they win over M.common), keyed
-- by the repo's version tags: DLR1, R1, R2, R3, R4, R5.
M.byVersion = {
    DLR1 = { CNetGameCtor = CNETGAME_CTOR },
    R1   = { ConnectDelayCmp = CONNECT_DELAY_R1 },
    R2   = {},
    R3   = { CNetGameCtor = CNETGAME_CTOR },
    R4   = {},
    R5   = { CNetGameCtor = CNETGAME_CTOR },
}

function M.get(version, name)
    local perVersion = version and M.byVersion[version]
    if perVersion and perVersion[name] then return perVersion[name] end
    return M.common[name]
end

function M.names(version)
    local seen, out = {}, {}
    for name in pairs(M.common) do
        if not seen[name] then seen[name] = true out[#out + 1] = name end
    end
    local perVersion = version and M.byVersion[version]
    if perVersion then
        for name in pairs(perVersion) do
            if not seen[name] then seen[name] = true out[#out + 1] = name end
        end
    end
    table.sort(out)
    return out
end

return M
