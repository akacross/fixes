script_name("fixes")
script_version("0.18.0")

-- /perf (akacross.perf): first, so it sees this script's waits and handlers. Inert until /perf.
do local ok, perf = pcall(require, 'akacross.perf'); if ok then perf.setup{env = getfenv(1)} end end
-- a named /perf row for per-frame work; a plain call without the runtime
local timed do
    local ok, perf = pcall(require, 'akacross.perf')
    timed = (ok and type(perf) == 'table' and perf.timed) or function(_, fn, ...) return fn(...) end
end

-- akacross.log (logs\akacross.log, levelled by wording) when the shared runtime
-- is installed, else moonloader.log.
local logLine, logScope do
    local ok, logM = pcall(require, 'akacross.log')
    local okSetup, lg = false, nil
    if ok and type(logM) == 'table' and logM.setup then okSetup, lg = pcall(logM.setup, {name = 'fixes'}) end
    logLine = okSetup and type(lg) == 'table' and lg.line or nil
    logScope = okSetup and type(lg) == 'table' and type(lg.scope) == 'function' and lg.scope() or nil
end
-- log(text[, level]): `level` ('info', 'warn') where the wording would mislead
-- ("0 refused" is not a warning).
local function log(text, level)
    if level and logScope and logScope[level] then
        logScope[level]('%s', (text:gsub('^%[fixes%]%s*', '')))
    elseif logLine then logLine(text) else print(text) end
end

local memory = require("memory")
local ffi = require("ffi")
ffi.cdef [[
    typedef void (__cdecl *AddAnimBlockRef_t)(int index);
]]
-- its own cdef: another library in this state may have declared it already
pcall(ffi.cdef, [[ void* VirtualAlloc(void* address, size_t size, uint32_t type, uint32_t protect); ]])
pcall(ffi.cdef, [[ int GetModuleHandleExA(uint32_t flags, const void* address, void** module); ]])
pcall(ffi.cdef, [[ uint32_t GetModuleFileNameA(void* module, char* name, uint32_t size); ]])

-- Optional: resolves samp.dll sites by signature, so an unlisted build is still
-- patched; without it, the fallback table below.
local sigscanOk, sigscan = pcall(require, "signature_scanner")

function getSampVersion()
    local version = "unknown"
    local versions = {[0xFDB60] = "DLR1",  [0x31DF13] = "R1", [0x3195DD] = "R2", [0xCC4D0] = "R3",  [0xCBCB0] = "R4", [0xCBC90] = "R5"}
    local sampHandle = getModuleHandle("samp.dll")
    if sampHandle and sampHandle ~= 0 then
        -- e_lfanew and AddressOfEntryPoint are DWORDs: read as uint32_t.
        local e_lfanew = ffi.cast("uint32_t*", sampHandle + 60)[0]
        local ntHeader = sampHandle + e_lfanew
        local entryPoint = ffi.cast("uint32_t*", ntHeader + 40)[0]
        if versions[tonumber(entryPoint)] then version = versions[tonumber(entryPoint)] end
    end
    return version
end

-- samp.dll RVAs for when signature_scanner is unavailable, verified against the
-- DLLs (R1 1d22eaa2, R3-1 08cf4166, R5 5ba5f0be, DL 449e4f98; the F1 pair on R5
-- and DL also in game). 'R3' is R3-1. The frame cap's are FC_RVA.
local sampFallback = {
    F1KeyHandler       = {['DLR1'] = 0x75456, ['R1'] = 0x713D6, ['R3'] = 0x752C6, ['R5'] = 0x759D5},
    ShowHelpDialogCall = {['DLR1'] = 0x75472, ['R1'] = 0x713F2, ['R3'] = 0x752E2, ['R5'] = 0x759F1},
    -- The jz inside SA-MP's radar toggle.
    RadarToggleBranch  = {['DLR1'] = 0xA1BD8, ['R1'] = 0x9D318, ['R3'] = 0xA1688, ['R5'] = 0xA1DB8},
    -- The 135-byte debug entity label format string, in .rdata.
    DebugLabelFormat   = {['DLR1'] = 0x11C800, ['R1'] = 0xD83A8, ['R3'] = 0xEA780, ['R5'] = 0xEA7D8},
    -- The two push operands of the SPECIAL_ACTION_SMOKE_CIGGY animation call
    -- (fixSmokingAnimation()), scanned from the reference DLLs; R1/R3 agree with
    -- the offsets that circulate for this fix.
    SmokeAnimTime      = {['DLR1'] = 0xAD935, ['R1'] = 0xA85D5, ['R3'] = 0xAD4A5, ['R5'] = 0xADD35},
    SmokeAnimBlend     = {['DLR1'] = 0xAD942, ['R1'] = 0xA85E2, ['R3'] = 0xAD4B2, ['R5'] = 0xADD42},
    -- The removed-building table's two sites (fixRemovedBuildings()): the store's
    -- `mov ecx,[count]` and the imm32 of the lookup's `mov esi, table`.
    RemoveBuildingStore  = {['DLR1'] = 0x9D22F, ['R1'] = 0x98D8F, ['R3'] = 0x9CCDF, ['R5'] = 0x9D3EF},
    RemoveBuildingLookup = {['DLR1'] = 0x9CAF8, ['R1'] = 0x98A08, ['R3'] = 0x9C958, ['R5'] = 0x9D068}
}

local F1_BLOCK_SIZE = 28    -- the VK_F1 polling block
local F1_CALL_SIZE  = 5     -- the E8 call to the help dialog that follows it

local function readU8(va)
    return ffi.cast("uint8_t*", va)[0]
end

local function readU32(va)
    return ffi.cast("uint32_t*", va)[0]
end

-- Resolve one samp.dll symbol: signature first, table second.
-- -> va, how  |  nil, nil, reason
local sigscanReady
local function resolveSamp(name)
    local base = getModuleHandle("samp.dll")
    if not base or base == 0 then return nil, nil, "samp.dll is not loaded" end

    if sigscanReady() then
        local ok, va, source = pcall(sigscan.resolve, name)
        if ok and va then
            return va, string.format("signature (%s)", tostring(source))
        end
    end

    local version = getSampVersion()
    local rva = sampFallback[name] and sampFallback[name][version]
    if not rva then
        return nil, nil, string.format("no signature match and no offset table entry for %s on SAMP %s",
                                       name, tostring(version))
    end
    return base + rva, "table (" .. version .. ")"
end

local sigscanStarted = false
function sigscanReady()
    if not sigscanOk then return false end
    if sigscanStarted then return true end
    sigscanStarted = pcall(function()
        -- no logfile: its lines go to logs\akacross.log (area sigscan)
        sigscan.init{cache_file = "config/signature_cache.lua"}
    end)
    if not sigscanStarted then
        log("[fixes] signature_scanner failed to initialise; using the offset table")
    end
    return sigscanStarted
end

-- Both sites get 33 NOPs, so check they are the F1 block first:
--   +0   A1 <g_SAMP>      mov eax, ds:g_SAMP
--   +9   6A 70            push 70h            ; VK_F1
--   +28  E8 <rel32>       call ShowHelpDialog
local function looksLikeF1Block(keyVa, callVa)
    if callVa ~= keyVa + F1_BLOCK_SIZE then
        return false, string.format("sites are not adjacent (0x%X vs 0x%X)", callVa, keyVa + F1_BLOCK_SIZE)
    end
    local ok, err = pcall(function()
        if readU8(keyVa) ~= 0xA1 then error("no 'mov eax, ds:imm32' at +0", 0) end
        if readU8(keyVa + 9) ~= 0x6A or readU8(keyVa + 10) ~= 0x70 then error("no 'push 70h' (VK_F1) at +9", 0) end
        if readU8(callVa) ~= 0xE8 then error("no 'call rel32' at +28", 0) end
    end)
    if not ok then return false, tostring(err) end
    return true
end

local function alreadyPatched(keyVa)
    local ok, isNop = pcall(function() return readU8(keyVa) == 0x90 end)
    return ok and isNop
end

-- Returns keyVa, callVa, how  --  or nil, nil, reason
local function locateF1Sites()
    local keyVa, how, why = resolveSamp("F1KeyHandler")
    if not keyVa then return nil, nil, why end
    local callVa, how2, why2 = resolveSamp("ShowHelpDialogCall")
    if not callVa then return nil, nil, why2 end
    return keyVa, callVa, (how == how2) and how or (how .. "/" .. how2)
end

function disableF1KeyAndDialog()
    local keyVa, callVa, how = locateF1Sites()
    if not keyVa then
        log("[fixes] F1 key and help dialog disabling is not supported: " .. tostring(how))
        return false
    end

    if alreadyPatched(keyVa) then
        log(string.format("[fixes] F1 handler at 0x%X is already NOPed; nothing to do", keyVa))
        return true
    end

    local sane, why = looksLikeF1Block(keyVa, callVa)
    if not sane then
        log(string.format("[fixes] refusing to patch 0x%X via %s: %s", keyVa, how, tostring(why)))
        return false
    end

    memory.fill(keyVa,  0x90, F1_BLOCK_SIZE, true)
    memory.fill(callVa, 0x90, F1_CALL_SIZE,  true)

    local verified = alreadyPatched(keyVa) and readU8(callVa) == 0x90
    log(string.format("[fixes] F1 help disabled on SAMP %s via %s: handler 0x%X (rva 0x%X), call 0x%X -- %s",
        getSampVersion(), how, keyVa, keyVa - getModuleHandle("samp.dll"), callVa,
        verified and "33 bytes NOPed" or "WRITE DID NOT STICK"), verified and "info" or "warn")
    return verified
end

--------------------------------------------------------------------------------
-- Radar toggle
--------------------------------------------------------------------------------
-- SA-MP's radar show/hide helper branches on its argument:
--   cmp ecx, eax / jz hide / ... bScriptDontDisplayRadar = 0 ... / ret 4
-- NOPing the jz makes "show" unconditional. Two bytes either way:
--
-- forceOn == true  -> 90 90  (radar always visible)
-- forceOn == false -> 74 0F  (stock behaviour)
local RADAR_ON  = {0x90, 0x90}
local RADAR_JZ  = {0x74, 0x0F}

function forceRadarVisible(forceOn)
    if forceOn == nil then forceOn = true end
    local va, how, why = resolveSamp("RadarToggleBranch")
    if not va then
        log("[fixes] radar toggle patch is not supported: " .. tostring(why))
        return false
    end

    local want = forceOn and RADAR_ON or RADAR_JZ
    local other = forceOn and RADAR_JZ or RADAR_ON

    local ok, b0 = pcall(readU8, va)
    local _, b1 = pcall(readU8, va + 1)
    if not ok then
        log(string.format("[fixes] cannot read the radar branch at 0x%X; not patching", va))
        return false
    end

    if b0 == want[1] and b1 == want[2] then
        return true                                   -- already in the wanted state
    end

    -- Only ever between the two known encodings.
    if not (b0 == other[1] and b1 == other[2]) then
        log(string.format("[fixes] refusing to patch the radar branch at 0x%X via %s: found %02X %02X, expected %02X %02X",
            va, how, b0, b1, other[1], other[2]))
        return false
    end

    memory.setuint8(va,     want[1], true)
    memory.setuint8(va + 1, want[2], true)

    local verified = readU8(va) == want[1] and readU8(va + 1) == want[2]
    log(string.format("[fixes] radar %s on SAMP %s via %s: 0x%X (rva 0x%X) -- %s",
        forceOn and "forced visible" or "restored to stock", getSampVersion(), how,
        va, va - getModuleHandle("samp.dll"),
        verified and "2 bytes written" or "WRITE DID NOT STICK"))
    return verified
end

--------------------------------------------------------------------------------
-- Debug entity labels
--------------------------------------------------------------------------------
-- Blank SA-MP's debug label format string (135 bytes with the terminator on all
-- four builds) so those overlays render empty. It is in .rdata: unprotect around
-- the write.
local DEBUG_LABEL_LEN = 135

function blankDebugEntityLabels()
    local va, how, why = resolveSamp("DebugLabelFormat")
    if not va then
        log("[fixes] debug label blanking is not supported: " .. tostring(why))
        return false
    end

    local ok, first = pcall(readU8, va)
    if not ok then
        log(string.format("[fixes] cannot read the debug label string at 0x%X; not patching", va))
        return false
    end

    if first == 0 then
        log(string.format("[fixes] debug labels at 0x%X are already blank; nothing to do", va))
        return true
    end

    -- It must start with "[id: ", or this is not it.
    local expected = "[id: "
    for i = 1, #expected do
        local okByte, got = pcall(readU8, va + i - 1)
        if not okByte or got ~= expected:byte(i) then
            log(string.format("[fixes] refusing to blank 0x%X via %s: expected %q at +%d, got %s",
                va, how, expected:sub(i, i), i - 1,
                okByte and string.format("%02X", got) or "<unreadable>"))
            return false
        end
    end

    local oldProtection = memory.unprotect(va, DEBUG_LABEL_LEN)
    ffi.fill(ffi.cast("void*", va), DEBUG_LABEL_LEN, 0)
    memory.protect(va, DEBUG_LABEL_LEN, oldProtection)

    local verified = readU8(va) == 0
    log(string.format("[fixes] debug entity labels blanked on SAMP %s via %s: 0x%X (rva 0x%X) -- %s",
        getSampVersion(), how, va, va - getModuleHandle("samp.dll"),
        verified and (DEBUG_LABEL_LEN .. " bytes zeroed") or "WRITE DID NOT STICK"))
    return verified
end

--------------------------------------------------------------------------------
-- Smoking animation
--------------------------------------------------------------------------------
-- SPECIAL_ACTION_SMOKE_CIGGY applies "GANGS:smkcig_prtl" through the method the
-- drinking actions use, but with a 700.0f blend delta and a 2750 ms play time
-- (drinking: 4.0f, -1), so another player's animation snaps in and drops out.
-- The fix rewrites the two `push imm32` operands:
--
--   push 0ABEh           (2750)  ->  push -1            ; hold until replaced
--   push 442F0000h      (700.0f) ->  push 40800000h     ; 4.0f, as everywhere else
--
-- Writing the stock values back undoes it. Build detection is the signature
-- scanner, then getSampVersion() (the circulated patch's `samp.dll + 0x90`
-- read is in the DOS stub: R3-1 and R5 read the same there).
local SMOKE_TIME_STOCK  = 0x00000ABE   -- push 2750 (ms)
local SMOKE_TIME_WANT   = 0xFFFFFFFF   -- push -1
local SMOKE_BLEND_STOCK = 0x442F0000   -- push 700.0f
local SMOKE_BLEND_WANT  = 0x40800000   -- push 4.0f
local SMOKE_OPERAND_GAP = 13           -- time operand -> blend operand

-- Each operand must be a `push` imm32 holding SA-MP's value or ours; anything
-- else means a wrong address or another rewrite.
local function checkSmokeOperand(va, label, found, stock, want)
    if readU8(va - 1) ~= 0x68 then
        return string.format("no `push imm32` opcode before the %s operand at 0x%X", label, va)
    end
    if found ~= stock and found ~= want then
        return string.format("%s operand at 0x%X is %08X, expected %08X (stock) or %08X (patched)",
                             label, va, found, stock, want)
    end
    return nil
end

function fixSmokingAnimation()
    local timeVa, how, why = resolveSamp("SmokeAnimTime")
    if not timeVa then
        log("[fixes] the smoking animation fix is not supported: " .. tostring(why))
        return false
    end
    local blendVa, how2, why2 = resolveSamp("SmokeAnimBlend")
    if not blendVa then
        log("[fixes] the smoking animation fix is not supported: " .. tostring(why2))
        return false
    end
    how = (how == how2) and how or (how .. "/" .. how2)

    -- The two operands belong to one call, 13 bytes apart, or neither is written.
    if blendVa ~= timeVa + SMOKE_OPERAND_GAP then
        log(string.format("[fixes] refusing the smoking animation fix via %s: operands are not %d bytes apart (0x%X vs 0x%X)",
            how, SMOKE_OPERAND_GAP, blendVa, timeVa + SMOKE_OPERAND_GAP))
        return false
    end

    local okRead, time, blend = pcall(function() return readU32(timeVa), readU32(blendVa) end)
    if not okRead then
        log(string.format("[fixes] cannot read the smoking animation operands at 0x%X; not patching", timeVa))
        return false
    end

    if time == SMOKE_TIME_WANT and blend == SMOKE_BLEND_WANT then
        log(string.format("[fixes] the smoking animation at 0x%X is already patched; nothing to do", timeVa))
        return true
    end

    local okCheck, why3 = pcall(function()
        return checkSmokeOperand(timeVa, "time", time, SMOKE_TIME_STOCK, SMOKE_TIME_WANT)
            or checkSmokeOperand(blendVa, "blend", blend, SMOKE_BLEND_STOCK, SMOKE_BLEND_WANT)
    end)
    if not okCheck or why3 then
        log(string.format("[fixes] refusing to patch 0x%X via %s: %s", timeVa, how,
            okCheck and tostring(why3) or "the site is unreadable"))
        return false
    end

    memory.setint32(timeVa,  -1,  true)
    memory.setfloat(blendVa, 4.0, true)

    local verified = readU32(timeVa) == SMOKE_TIME_WANT and readU32(blendVa) == SMOKE_BLEND_WANT
    log(string.format("[fixes] smoking animation fixed on SAMP %s via %s: time 0x%X (rva 0x%X), blend 0x%X -- %s",
        getSampVersion(), how, timeVa, timeVa - getModuleHandle("samp.dll"), blendVa,
        verified and "2 operands rewritten" or "WRITE DID NOT STICK"))
    return verified
end

--------------------------------------------------------------------------------
-- gta_sa.exe patches
--------------------------------------------------------------------------------
-- Never written blind: each entry carries the bytes it expects and the bytes it
-- wants:
--
--   already  -- the site is already in the wanted state, so nothing is written.
--               Either this script ran before, or an ASI that loads earlier
--               (SilentPatch and friends) made the same change.
--   ok       -- the site matched `from`, was rewritten, and read back correct.
--   refused  -- neither, so something else patched it a different way or the
--               address is wrong for this exe. Left untouched and logged.
--
-- `from` is gta_sa.exe 1.0 US (14,383,616 bytes); any other build refuses.
-- docs/MEMORY_FIXES.md explains each entry with its disassembly.

local function hexBytes(s)
    local t = {}
    for b in s:gmatch("%x%x") do t[#t + 1] = tonumber(b, 16) end
    return t
end

local function nops(n)
    local t = {}
    for i = 1, n do t[i] = 0x90 end
    return t
end

local function zeros(n)
    local t = {}
    for i = 1, n do t[i] = 0x00 end
    return t
end

local function readBytes(va, n)
    local ok, bytes = pcall(function()
        local p, t = ffi.cast("uint8_t*", va), {}
        for i = 1, n do t[i] = p[i - 1] end
        return t
    end)
    if not ok then return nil end
    return bytes
end

local function bytesEqual(a, b)
    if not a or not b or #a ~= #b then return false end
    for i = 1, #a do
        if a[i] ~= b[i] then return false end
    end
    return true
end

local function bytesToHex(t)
    if not t then return "<unreadable>" end
    local out = {}
    for i = 1, #t do out[i] = string.format("%02X", t[i]) end
    return table.concat(out, " ")
end

-- The file name of the module holding `va`, or nil.
local function moduleAt(va)
    local ok, name = pcall(function()
        local handle = ffi.new("void*[1]")
        -- GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | _UNCHANGED_REFCOUNT
        if ffi.C.GetModuleHandleExA(6, ffi.cast("const void*", va), handle) == 0 then return nil end
        local buf = ffi.new("char[260]")
        local n = ffi.C.GetModuleFileNameA(handle[0], buf, 260)
        return n > 0 and ffi.string(buf, n):match("[^\\/]+$") or nil
    end)
    return ok and name or nil
end

-- ms_aVisibleEntityPtrs, and the store operand that names it.
local VISIBLE_ENTITY_PTRS    = 0xB75898
local VISIBLE_ENTITY_OPERAND = 0x553529

local LIMIT_ADJUSTERS = {
    "$fastman92limitAdjuster.asi", "fastman92limitAdjuster_GTASA.asi", "III.VC.SA.LimitAdjuster.asi",
}

local function limitAdjusterName()
    for _, name in ipairs(LIMIT_ADJUSTERS) do
        local ok, handle = pcall(getModuleHandle, name)
        if ok and handle and handle ~= 0 then return name end
    end
    return "a limit adjuster"
end

-- Who owns a site that starts with a call or jmp (another mod's hook), or nil:
-- rel32 (E8/E9) or through a pointer (FF 15/FF 25, as SA-MP's satchel hook).
local function hookOwner(va, found)
    local target
    if found[1] == 0xE8 or found[1] == 0xE9 then
        target = (va + 5 + readU32(va + 1)) % 0x100000000
    elseif found[1] == 0xFF and (found[2] == 0x15 or found[2] == 0x25) then
        local ok, at = pcall(function() return readU32(readU32(va + 2)) end)
        if not ok then return string.format("an indirect jump at 0x%X", va) end
        target = at
    else
        return nil
    end
    return moduleAt(target) or string.format("code at 0x%X", target)
end

local gamePatches = {
    {   -- both rear-light checks call GetLightStatus(LIGHT_REAR_LEFT=3) and
        -- LIGHT_REAR_RIGHT=2 is never read; push 3 -> push 2 so a shot-out right
        -- tail light actually goes dark
        name = "CVehicle::DoVehicleLights rear-right tail light",
        va = 0x6E1D4F, from = hexBytes("03"), to = hexBytes("02")
    },
    {   -- fld [RwEngineInstance+0x18] (RwDevice::zBufferNear) -> +0x1C
        -- (zBufferFar); puts the wet-road reflection at the back of the depth
        -- range instead of the front. SilentPatch fixes it its own way and wins
        -- by load order, so this applies only without it
        name = "CCoronas::RenderReflections depth",
        va = 0x6FB9A0, from = hexBytes("18"), to = hexBytes("1C"),
        conflicts = {{bytes = hexBytes("00"), by = "SilentPatch"}}
    },
    {   -- je -> jmp. Stock hands m_apShadows[0] to any ped with
        -- m_nPedType == PED_TYPE_PLAYER1; under SA-MP several qualify and stomp
        -- each other, so send everyone through the free-slot search
        name = "CRealTimeShadowManager::GetRealTimeShadow slot",
        va = 0x7069F5, from = hexBytes("74"), to = hexBytes("EB")
    },
    {   -- NOP `test [entity+0x1E],1 / jne skip`, i.e. CEntity flag bit 16
        -- m_bDontCastShadowsOn, so shadows land on those entities too
        name = "CShadows::CastRealTimeShadowSectorList shadow flag",
        va = 0x70A83B, from = hexBytes("0F 85 FE 00 00 00"), to = nops(6)
    },
    {
        name = "CShadows::CastPlayerShadowSectorList shadow flag",
        va = 0x70A4CB, from = hexBytes("0F 85 43 01 00 00"), to = nops(6)
    },
    {   -- the closing RwRenderStateSet(rwRENDERSTATECULLMODE, CULLBACK=2)
        -- becomes CULLNONE=1, so culling stays off after the beam is drawn.
        -- Behaviour change, not a bug fix -- revert this first if two-sided
        -- geometry starts showing through once headlights are on screen
        name = "CVehicle::DoHeadLightBeam cull mode restore",
        va = 0x6E1425, from = hexBytes("02"), to = hexBytes("01")
    },
    {   -- .rdata table of {magMinFilter, mipFilter} D3DTEXF pairs at 0x884950,
        -- indexed by RwTextureFilterMode. Entry 6 is rwFILTERLINEARMIPLINEAR;
        -- its mip filter goes D3DTEXF_LINEAR -> D3DTEXF_NONE, which kills
        -- mipmapping for trilinear textures: sharper at distance, more aliasing.
        -- This is the one site in a read-only section; see applyGamePatch
        name = "D3D9 trilinear mip filter",
        va = 0x884984, from = hexBytes("02 00 00 00"), to = zeros(4)
    },
    {   -- exactly the max(1.0f, CTimer::ms_fTimeStep) sequence used as an
        -- angle-delta divisor; falls through into the `fld ms_fTimeStep` at
        -- 0x524FF2, so the chase camera stops lagging above 50 fps
        name = "CCam::Process_FollowCar_SA timestep clamp",
        va = 0x524FD7,
        from = hexBytes("D9 05 24 86 85 00 D8 1D 5C CB B7 00 DF E0 F6 C4 41 75 08 D9 05 24 86 85 00 EB 06"),
        to = nops(27)
    },
    {   -- zeroes the 0xBB8 in `lea eax,[ecx+eax+0xBB8]` so m_nExpireTime is
        -- now + lifetime instead of now + lifetime + 3000 ms. Longest-lived
        -- explosion type in the game
        name = "CExplosion::AddExplosion boat/aircraft lifetime",
        va = 0x736F88, from = hexBytes("B8 0B 00 00"), to = zeros(4)
    },
    {   -- NOP the last `jp reject` of the height/width whitelist, which only
        -- allows 0.8, 0.75, 0.6 and 0.625 and therefore drops every 16:9 mode.
        -- The 640x480 minimum and the VRAM check are left in place
        name = "GetVideoModeList aspect ratio filter",
        va = 0x745BC9, from = hexBytes("7A 6D"), to = nops(2)
    },
    {
        name = "FillAvailableVMs aspect ratio filter",
        va = 0x7459E1, from = hexBytes("7A 71"), to = nops(2)
    },
    {   -- NOP the `m_nVehicleType != VEHICLE_TYPE_AUTOMOBILE` guard around
        -- CustomCarPlate_BeforeRenderingStart / _AfterRenderingStop so bikes,
        -- trailers, planes and boats get custom plates too. The two sites move
        -- together (or the plate texture leaks). OPTIONAL, off by default: SA-MP
        -- R5 draws every plate from one shared raster and hooks next to
        -- 0x6D0E43, for cars only, and a crash after a server restart
        -- (samp.dll+0xA4487) read that raster freed. /fixes plates on
        name = "CVehicle::SetupRender plate guard",
        va = 0x6D6517, from = hexBytes("75 08"), to = nops(2),
        option = "plates"
    },
    {
        name = "CVehicle::ResetAfterRender plate guard",
        va = 0x6D0E43, from = hexBytes("75 13"), to = nops(2),
        option = "plates"
    },
    {   -- je -> jmp on the CPed::IsAlive(m_Car->m_pDriver) result, so the
        -- displaced driver always gets the plain 3-arg CTaskComplexLeaveCar
        -- instead of the forced-get-out 5-arg one. SilentPatch inverts the
        -- branch instead (0x63F576 = 0x75) and wins by load order
        name = "CTaskComplexEnterCar driver drag-out",
        va = 0x63F576, from = hexBytes("74"), to = hexBytes("EB"),
        conflicts = {{bytes = hexBytes("75"), by = "SilentPatch"}}
    },
    {   -- Inherited from SAMPGraphicRestore.asi, which is no longer installed.
        -- It NOPs `mov [ForegroundApp], ebx` on WM_ACTIVATE's activating branch
        -- (ebx = 1). ForegroundApp (0x8D621C, initially 1) has four writers and
        -- only this one is touched --
        --   0x747FB0 WM_ACTIVATE deactivating -> 0
        --   0x747FFE WM_ACTIVATE activating   -> 1   (this patch)
        --   0x748054 WM_KILLFOCUS             -> 0
        --   0x748068 WM_SETFOCUS              -> 1
        -- Purpose unclear (perhaps the menu re-activation path); drop it if
        -- alt-tab behaviour gets worse
        name = "WndProc WM_ACTIVATE ForegroundApp writeback (ex-SAMPGraphicRestore)",
        va = 0x747FFE, from = hexBytes("89 1D 1C 62 8D 00"), to = nops(6)
    },
    {   -- AddHospitalRestartPoint has no bounds check and MAX_RESTART_POINTS is
        -- 10, so call 11 walks past HospitalRestartPoints (0xA43318, 120 bytes)
        -- into PoliceRestartPoints (0xA43390); SA-MP drives this through
        -- SetSpawnInfo. NOPing the `mov [0xA4326C],cx` write-back pins
        -- NumberOfHospitalRestarts so every call rewrites slot 0. Side effect:
        -- hospital restart points stop registering, which does not matter when
        -- the server owns respawns
        name = "CRestart::AddHospitalRestartPoint overflow",
        va = 0x460773, from = hexBytes("66 89 0D 6C 32 A4 00"), to = nops(7)
    },
    {   -- CRenderer::AddEntityToRenderList stores into ms_aVisibleEntityPtrs
        -- (1000 slots) unchecked; past 1000 it overwrites the counts after it,
        -- and the LOD count then walks the renderer off its list. The count is
        -- checked in the 11 bytes of padding after the function. The store
        -- stays at 0x553526: ProperShaders reads the array's address from its
        -- operand (0x553529). A limit adjuster that raises this limit either
        -- moves the array (that operand changes) or hooks the store (a call at
        -- 0x553521); the cap would undo it, so it stands down
        name = "CRenderer::AddEntityToRenderList visible list bound",
        va = 0x553521,
        standDown = function(found)
            -- the Open Limit Adjuster's "unlimited" hooks the count read
            local owner = hookOwner(0x553521, found)
            if owner then return owner .. " (it hooks the store)" end
            local array = readU32(VISIBLE_ENTITY_OPERAND)
            if array == VISIBLE_ENTITY_PTRS then return nil end
            return string.format("%s (ms_aVisibleEntityPtrs moved to 0x%X)", limitAdjusterName(), array)
        end,
        from = hexBytes("A1 44 68 B7 00 89 34 85 98 58 B7 00 40 A3 44 68 B7 00 5E C3" ..
            " 90 90 90 90 90 90 90 90 90 90 90"),
        to = hexBytes("EB 12 73 0E 90 89 34 85 98 58 B7 00 40 A3 44 68 B7 00 5E C3" ..
            " A1 44 68 B7 00 66 3D E8 03 EB E3")
    },
    {   -- COccluder::NearCamera returns false: no exterior occluder ever joins
        -- the nearby list, so none culls. The game's occluders otherwise hide
        -- objects a server maps behind them. Interior occluders do not use it.
        -- #FixGameMapBuilding_byAIR.asi does the same through a hook.
        name = "COccluder::NearCamera exterior occluders off",
        va = 0x71F960, from = hexBytes("83 EC 10"), to = hexBytes("32 C0 C3"),   -- xor al,al / ret
        standDown = function(found) return hookOwner(0x71F960, found) end,
        option = "occluders"
    },
    {   -- AddBigMessage indexes BIGMessages[7] with the style unchecked, and
        -- SA-MP passes a server's GameText style straight in. The style check
        -- goes where a strlen loop whose result is never used sat:
        -- xor edx,edx / add esp,10h / cmp word [esp+330h],6 / ja epilogue.
        -- From MiTurboFix, which hooks the function instead
        name = "CMessages::AddBigMessage style bound",
        va = 0x69F2D8,
        from = hexBytes("8A 4C 24 14 33 D2 83 C4 10 3A CA 8D 44 24 04 74 0F 8D A4 24 00 00 00 00 8A 48 01 40 3A CA 75 F8"),
        to = hexBytes("31 D2 83 C4 10 66 83 BC 24 30 03 00 00 06 77 71" ..
            " 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90"),
        standDown = function(found) return hookOwner(0x69F2D8, found) end
    },
    {   -- RwTexDictionaryGtaStreamRead reads the dictionary's struct chunk,
        -- whatever size the file gives, into 4 bytes on the stack. It now reads
        -- 4 (push 4 for push size) and the existing `size == read` check fails
        -- any other size. MiTurboFix checks the same field in CTxdStore::LoadTxd
        name = "RwTexDictionaryGtaStreamRead struct size",
        va = 0x730FE1, from = hexBytes("8B 54 24 08 52"), to = hexBytes("6A 04 90 90 90"),
        standDown = function(found) return hookOwner(0x730FE1, found) end
    },
    {   -- the same read in the first half of a streamed dictionary
        name = "RwTexDictionaryGtaStreamRead1 struct size",
        va = 0x73109B, from = hexBytes("8B 54 24 08 52"), to = hexBytes("6A 04 90 90 90"),
        standDown = function(found) return hookOwner(0x73109B, found) end
    }
}

-- The game timer as a float. SA-MP's RPC 60 sets CTimer::m_snTimeInMilliseconds
-- to the server's value (30 bits on R5), and with the FPU at float precision
-- `fild [m_snTimeInMilliseconds]` then moves in steps of up to 128 ms: camera
-- moves and shakes stutter and the weapon spread repeats. These loads read
-- m_snTimeInMillisecondsNonClipped (0xB7CB80) instead: counted from game start,
-- paused and slowed with the game, and set by nothing but CTimer. It stays
-- below 2^31, so the uint32 sign test (the jge over `fadd 2^32`) becomes a jmp.
-- Only the 21 loads whose float is a sin/cos phase, or whose every writer and
-- reader is in this list (the camera's move, track, shake and FOV stamps,
-- CShotInfo's destroy time); the other 26 meet stamps kept on the game's own
-- clock (docs/MEMORY_FIXES.md 28). From a Lua script that pointed 46 of them
-- at the audio clock. Option "floatclock".
local FLOAT_CLOCK_SITES = {     -- the fild through its jge
    {0x50D166, "CCamera::VectorMoveLinear start/end", "DB 05 84 CB B7 00 85 C0 56 7D 06"},
    {0x50D1D6, "CCamera::VectorTrackLinear start/end", "DB 05 84 CB B7 00 85 C0 56 7D 06"},
    {0x50D254, "CCamera::AddShakeSimple start/end", "DB 05 84 CB B7 00 A1 84 CB B7 00 85 C0 7D 06"},
    {0x516415, "CCamera shake start/end (0x516400)", "DB 05 84 CB B7 00 A1 84 CB B7 00 85 C0 7D 06"},
    {0x50D286, "CCamera::LerpFOV start/end", "DB 05 84 CB B7 00 85 C0 7D 06"},
    {0x474876, "CCamera vector track finished (script)", "DB 05 84 CB B7 00 85 C0 7D 06"},
    {0x4748A6, "CCamera vector move finished (script)", "DB 05 84 CB B7 00 85 C0 7D 06"},
    {0x516446, "CCamera::ProcessVectorTrackLinear", "DB 05 84 CB B7 00 85 C0 7D 06"},
    {0x5164A6, "CCamera::ProcessVectorMoveLinear", "DB 05 84 CB B7 00 85 C0 7D 06"},
    {0x516506, "CCamera::ProcessFOVLerp", "DB 05 84 CB B7 00 85 C0 7D 06"},
    {0x51A6F6, "CCamera shake (0x51A6F6)", "DB 05 84 CB B7 00 85 C0 7D 06"},
    {0x52BA31, "CCamera::Process shake", "DB 05 84 CB B7 00 85 C9 7D 06"},
    {0x739DB3, "CShotInfo::AddShot destroy time", "DB 05 84 CB B7 00 7D 06"},
    {0x739EB6, "CShotInfo::Update destroy time",
        "DB 05 84 CB B7 00 8B 15 84 CB B7 00 8B 78 18 83 C4 08 85 D2 89 44 24 18 7D 06"},
    {0x67F7DC, "CTaskSimpleJetPack::RenderJetPack flame (1)", "DB 05 84 CB B7 00 85 D2 7D 06"},
    {0x67F885, "CTaskSimpleJetPack::RenderJetPack flame (2)", "DB 05 84 CB B7 00 85 D2 7D 06"},
    {0x6802F6, "CTaskSimpleJetPack::ProcessPed wobble", "DB 05 84 CB B7 00 85 C0 7D 06"},
    {0x703A46, "CPostEffects::UnderWaterRipple (1)", "DB 05 84 CB B7 00 85 C9 7D 06"},
    {0x703B92, "CPostEffects::UnderWaterRipple (2)", "DB 05 84 CB B7 00 7D 06"},
    {0x7400E7, "CWeapon::FireInstantHit spread (1)", "DB 05 84 CB B7 00 8B 0D 84 CB B7 00 85 C9 7D 06"},
    {0x74057F, "CWeapon::FireInstantHit spread (2)", "DB 05 84 CB B7 00 8B 0D 84 CB B7 00 85 C9 7D 06"},
}

for _, s in ipairs(FLOAT_CLOCK_SITES) do
    local from = hexBytes(s[3])
    local to, script = {unpack(from)}, {unpack(from)}
    to[3], to[4], to[5], to[6] = 0x80, 0xCB, 0xB7, 0x00            -- fild [m_snTimeInMillisecondsNonClipped]
    to[#to - 1] = 0xEB                                             -- jge -> jmp: never 2^32 to add
    script[3], script[4], script[5], script[6] = 0xE0, 0x10, 0xB6, 0x00   -- that script's audio clock
    gamePatches[#gamePatches + 1] = {name = s[2] .. " clock", va = s[1], from = from, to = to,
        option = "floatclock", group = "game timer as a float",
        conflicts = {{bytes = script, by = "a timer script (the audio clock, 0xB610E0)"}}}
end

-- MTA's multiplayer patches, as gtasa_crashfix (crashes.asi 2.x) applies them
-- with SA-MP: single-player systems a server does not want (population,
-- wanted level, roadblocks, gang wars, conversations, replays, idle camera,
-- stats messages) and multiplayer fixes (satchels, drive-by fall-off, the
-- first bullets after a reload, paintjob colours, trains, garages, the mouse
-- after alt-tab). MTA:SA's, GPLv3. Late: SA-MP is up and has written its own;
-- where SA-MP writes a site itself (its satchel hook, a bigger matrix array)
-- it is SA-MP's. Not here: the main.scm and loading-screen skips (they run
-- before any script), the frame-sleep bytes (autofps's), 0x717180 (it writes
-- what is there) and 0x6E1D4F (entry 1). docs/MEMORY_FIXES.md 31. Option
-- "mtapatches".
local MTA_PATCHES = {     -- va, name, stock, theirs
    {0x8A5A84, "value_8A5A84_127", "16 00 00 00", "7F 00 00 00"},
    {0x550F82, "value_550F82_8000", "00 7D 00 00", "40 1F 00 00"},
    {0x742685, "jmp_742685", "0F 84", "90 E9"},
    {0x7399B0, "disableRemoveAllProjectiles", "53", "C3"},
    {0x4629E0, "disableGenerateRoadblocks", "6A", "C3"},
    {0x4E8410, "ret_4E8410", "51", "C3"},
    {0x614720, "disableAddToPopulation", "6A FF 68", "32 C0 C3"},
    {0x61EFFE, "jmp_61EFFE", "74", "EB"},
    {0x441770, "ret_441770", "83", "C3"},
    {0x532010, "disableReportCrime", "83", "C3"},
    {0x4DCF87, "nop_4DCF87", "50 E8 33 D9 FC FF", "90 90 90 90 90 90"},
    {0x6A436C, "nop_6A436C", "75 53", "90 90"},
    {0x738F3A, "satchelCrashFix", "8B 4B 04 8A 51 36 80 E2 07 80 FA 03 0F 85 C6 08 00 00 E8 9F 69 EA FF 84 C0 0F 84 B9 08 00 00 8B 7B 04 6A 28 8B CF E8 9B 62 EA FF 6B C0 1C 83 BC 38 A0 05 00 00 28 75 1B 6A 28 8B CF E8 85 62 EA FF 6B C0 1C 8B 8C 38 AC 05 00 00 85 C9 0F 85 85 08 00 00", "90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90"},
    {0x6B5B17, "drivebyFallOffFix", "0F 85 05 04 00 00", "90 90 90 90 90 90"},
    {0x5B8342, "patch_5B8342", "8B 7C 24 0C 8B C7", "33 C0 B0 FF 8B F8"},
    {0x4486F7, "nop_4486F7", "C6 41 4D 02", "90 90 90 90"},
    {0x559FD5, "nop_559FD5", "D9 14 8D 80 93 B7 00", "90 90 90 90 90 90 90"},
    {0x559FEB, "nop_559FEB", "D9 1C 8D 80 93 B7 00", "90 90 90 90 90 90 90"},
    {0x55B980, "disableStatsMessages", "51", "C3"},
    {0x559760, "ret_559760", "6A", "C3"},
    {0x53BC78, "disableMenuAfterAltTab", "01", "00"},
    {0x6F2089, "patch_6F2089", "E8 02 93 EA FF", "58 90 90 90 90"},
    {0x550FBA, "patch_550FBA", "0F 27", "00 10"},
    {0x5E68A0, "jmp_5E68A0", "74", "EB"},
    {0x561FA4, "disableWantedHelis", "74 03", "90 90"},
    {0x53BFF6, "disableWantedUpdateEachFrame", "E8 65 63 02 00", "90 90 90 90 90"},
    {0x60EBCC, "disableWantedUpdate", "E8 BF 40 F5 FF", "90 90 90 90 90"},
    {0x632140, "alwaysCanSunbathe", "A0 53 01", "B0 01 C3"},
    {0x644C18, "carDrivePassengerExitFix", "0F 84", "90 E9"},
    {0x5E8E84, "noSetClumpAlphaPlayer", "E8 77 9C 14 00", "90 90 90 90 90"},
    {0x6D29CB, "noSetClumpAlphaVehicle", "E8 30 01 06 00", "90 90 90 90 90"},
    {0x6872C0, "disableIdleAnimations", "83 EC 20", "C2 04 00"},
    {0x6F701D, "trainRailSpeedFix1", "D9 96 A4 05 00 00", "90 90 90 90 90 90"},
    {0x6F7069, "trainRailSpeedFix2", "75", "EB"},
    {0x73FDF9, "instantHitBulletFix", "75", "EB"},
    {0x6D65C5, "paintjobColorFix", "A8 04 75 22 C6 86 34 04 00 00 01", "90 90 90 90 90 90 90 90 90 90 90"},
    {0x547441, "vehicleDetachmentFix", "3C 02 0F 85 F7", "E9 FA 02 00 00"},
    {0x522C80, "disableIdleCam", "51", "C3"},
    {0x53A982, "burningBmxFix", "75", "EB"},
    {0x4F9CCE, "patch_4F9CCE", "EE", "CE"},
    {0x576CCC, "dinputMouseReinitFix1", "74", "EB"},
    {0x576EBA, "dinputMouseReinitFix2", "74", "EB"},
    {0x576F8A, "dinputMouseReinitFix3", "74", "EB"},
    {0x7469A0, "dinputNonExclusiveMouse", "8A 44 24 04", "B0 00 90 90"},
    {0x5EFFE0, "ret18_5EFFE0", "8B 44 24 04", "C2 18 00 90"},
    {0x40E7DF, "noAmbientTrafficStreaming", "75", "EB"},
    {0x611FC0, "disableManagePed", "51", "C3"},
    {0x616698, "stopPopulationUpdate", "E8 63", "5E C3"},
    {0x460500, "disableReplayUpdate", "A0", "C3"},
    {0x605A30, "disableScanForNearbyEntities", "55", "C3"},
    {0x446610, "disableGangWars", "83", "C3"},
    {0x43C590, "disableConversations", "E9", "C3"},
    {0x43B0F0, "disablePedToPlayerConversations", "64", "C3"},
    {0x4322B0, "disableRemoveCarsIfPoolFull", "A1", "C3"},
    {0x40B650, "disableStreamVehiclesAlways", "6A", "C3"},
    {0x54F3A1, "placeableMatrixArraySize", "84 03 00 00", "08 07 00 00"},
    {0x71240E, "birdHeightFix1", "00 00 20 41", "00 00 A0 41"},
    {0x712413, "birdHeightFix2", "00 00 00 40", "00 00 40 41"},
    {0x712447, "birdHeightFix3", "00 00 50 41", "00 00 B8 41"},
    {0x71244C, "birdHeightFix4", "00 00 80 40", "00 00 60 41"},
    {0x508450, "noInteriorMusic1", "89 2D BC DC B6 00", "90 90 90 90 90 90"},
    {0x508817, "noInteriorMusic2", "89 2D BC DC B6 00", "90 90 90 90 90 90"},
    {0x716380, "noCloudsRender", "A1", "C3"},
    {0x615FE3, "jmp_615FE3", "7A", "EB"},
    {0x40892A, "modelLoadingPriority", "6A 08", "53 90"},
    {0x4341C0, "skipGenerateRandomCars", "A0", "C3"},
    {0x449C50, "garagesKeepVehicles1", "83", "C3"},
    {0x449D10, "garagesKeepVehicles2", "83", "C3"},
    {0x542483, "jmp_542483", "74", "EB"},
    {0x5E1E72, "patch_5E1E72a", "0F 84 B8", "E9 B9 00"},
    {0x5E1E77, "patch_5E1E72b", "00", "90"},
    {0x6E1DBC, "vehicleLightsEngineOff", "A8 10 0F 84 2B 0B 00 00", "90 90 90 90 90 90 90 90"},
    {0x82C5CC, "s0beitAntiCrash", "83 EC", "C9 C3"},
}

local SAMP_WRITES = {          -- what samp.dll R5 writes at a site of these
    [0x550FBA] = hexBytes("88 13"),          -- 5000
    [0x54F3A1] = hexBytes("70 17 00 00"),    -- 6000 matrices, more than MTA's 1800
}
local MTA_OWNERS = {           -- another mod's bytes at a site of these
    [0x7469A0] = {bytes = hexBytes("30 C0 90 90"), by = "SilentPatch (xor al,al: the same non-exclusive mouse)"},
}
-- Data the game sets during init, before any script: its stock value is that,
-- not the exe file's. Any other value was set on purpose and stands down.
--   0x8A5A84  CStreaming::desiredNumVehiclesLoaded: the file holds 12, init
--             sets 22 (0x5B8E6E), stream.ini's `vehicles` key another
local MTA_RUNTIME_STOCK = {
    [0x8A5A84] = {file = hexBytes("0C 00 00 00"), by = "stream.ini's vehicles or another mod (it set this value)"},
}

for _, m in ipairs(MTA_PATCHES) do
    local va, from = m[1], hexBytes(m[3])
    local runtime = MTA_RUNTIME_STOCK[va]
    gamePatches[#gamePatches + 1] = {name = "MTA " .. m[2], va = va, from = from, to = hexBytes(m[4]),
        phase = "late", option = "mtapatches", group = "MTA's multiplayer patches",
        fileFrom = runtime and runtime.file,     -- what gta_sa.exe holds before init
        standDown = function(found)
            if bytesEqual(found, from) then return nil end           -- stock: several start with the call they NOP
            local owner = hookOwner(va, found)
            if owner then return owner end
            if SAMP_WRITES[va] and bytesEqual(found, SAMP_WRITES[va]) then return "samp.dll (it writes this site itself)" end
            local mod = MTA_OWNERS[va]
            if mod and bytesEqual(found, mod.bytes) then return mod.by end
            if runtime and not bytesEqual(found, hexBytes(m[4])) then return runtime.by end
        end}
end

-- CMessages::GetStringLength (0x69DB50), a GXT string's strlen, reads the
-- string before checking it: a null one (a missing GXT key, a server's
-- bad text) crashes at 0x69DB54. It is rewritten in place as a strlen that
-- returns 0 for null (gxt_crash_fix's fix; its hook left the length unset).
-- gxt_crash_fix.asi's hook at 0x69DB54 stands this down.
gamePatches[#gamePatches + 1] = {name = "CMessages::GetStringLength null string", va = 0x69DB50,
    from = hexBytes("8B 4C 24 04 8A 11 33 C0 84 D2 74 0D 8D 64 24 00 8A 51 01 40 41 84 D2 75 F7 C3"),
    to = hexBytes("8B 4C 24 04 33 C0 85 C9 74 09 80 3C 01 00 74 03 40 EB F7 C3 90 90 90 90 90 90"),
    standDown = function(found)
        if found[5] == 0xE9 then return hookOwner(0x69DB54, {unpack(found, 5, 9)}) end
    end}

-- GTA's own 14 ms frame delay (0x53E920, a busy-wait before CTimer::Update):
-- `cmp eax, 0Eh` -> `cmp eax, 0`, so its `jb` never spins (MTA's "remove 14ms
-- wait", crashes.asi's frameSleepOff2; autofps writes the same byte). The
-- Frame Limiter setting is untouched. Option "framecaps", with SA-MP's.
gamePatches[#gamePatches + 1] = {name = "GTA frame delay", va = 0x53E94A, from = hexBytes("83 F8 0E 72 E1"),
    to = hexBytes("83 F8 00 72 E1"), option = "framecaps",
    standDown = function(found) return hookOwner(0x53E94A, found) end}

-- gtasa_crashfix's visual and display switches (its crashes.cfg keys); each
-- is an option. Its black-roads fix is entry 7, its resolution unlock 11 and 12.
for _, v in ipairs({
    {0x701780, "heat haze off", "83", "C3", "noheathaze"},
    {0x53E1B4, "cloud render call off", "E8 C7 81 1D 00", "90 90 90 90 90", "noclouds"},
    {0x7000E0, "flashes off", "8B", "C3", "noflashes"},
    {0x507750, "all sound off", "56", "C3", "nosound"},
    {0x5D9A8F, "vehicle lighting off", "01", "00", "novehiclelighting"},
    {0x5D9ABE, "specular vehicle lighting off", "01", "00", "nospecular"},
    {0x53E1EC, "target blip off", "74", "EB", "notargetblip"},
    {0x555854, "interior reflections off", "E8 97 15 1D 00", "90 90 90 90 90", "nointeriorreflections"},
    {0x53E227, "post effects off", "E9", "C3", "nopostfx"},
    {0x74872D, "Alt+Enter windowed mode", "E8 AE E1 FF FF 85 C0 75 0F", "90 90 90 90 90 90 90 90 90", "windowed"},
    {0x53E9B3, "keep running in the background (unpaused)", "0F 85 60 01 00 00", "75 44 90 90 90 90", "background"},
    {0x561AF6, "keep running in the pause menu", "01", "00", "background"},
}) do
    local va, from = v[1], hexBytes(v[3])
    gamePatches[#gamePatches + 1] = {name = v[2], va = va, from = from, to = hexBytes(v[4]), option = v[5],
        standDown = function(found)            -- a call there redirected (SilentPatch's windowed mode)
            if not bytesEqual(found, from) then return hookOwner(va, found) end
        end}
end

-- WndProc's WM_SETFOCUS (0x748063): `call 0x53BC60` opens the pause menu on
-- the next frame when the window gets focus back and no menu is up. SA-MP NOPs
-- the call at load while its flag at samp.dll+0x26EB28 (R5) is 1, else the one at
-- 0x7480B3. This puts it back (sampSetFocusFix.lua). A site SA-MP left alone
-- is already stock; off writes SA-MP's NOPs back only where this wrote the call.
gamePatches[#gamePatches + 1] = {name = "pause menu on focus (WM_SETFOCUS)", va = 0x748063,
    from = hexBytes("90 90 90 90 90"), to = hexBytes("E8 F8 3B DF FF"),
    phase = "late", option = "focusmenu", revertOnlyOurs = true,
    fromSamp = true}                         -- `from` is SA-MP's; the exe holds `to`

-- MARK_CAR_AS_NO_LONGER_NEEDED (opcode 01C3, case 0x47D75A): its call to
-- CTheScripts::CleanUpThisVehicle (0x486670) makes the car a random vehicle,
-- which the population code then deletes: a server's car vanishes when you get
-- out, its upgrade parts left behind. NOPing the call keeps the car; the
-- opcode still reads its parameter and returns (the push and `add esp,4`
-- around the call balance).
gamePatches[#gamePatches + 1] = {name = "01C3 keeps the car (CleanUpThisVehicle call)", va = 0x47D775,
    from = hexBytes("E8 F6 8E 00 00"), to = hexBytes("90 90 90 90 90"), option = "keepcars",
    standDown = function(found)                -- a call there redirected
        if found[1] == 0xE8 and not bytesEqual(found, hexBytes("E8 F6 8E 00 00")) then
            return hookOwner(0x47D775, found)
        end
    end}

--------------------------------------------------------------------------------
-- Optional patches
--------------------------------------------------------------------------------
-- A descriptor with `option = "<key>"` applies only while that option is on
-- (config/fixes.json, {"plates": false}). /fixes lists them; /fixes <key> on|off
-- switches one at once (off writes the stock bytes back). Several descriptors
-- may share a key.
local OPTIONS = {
    plates = {default = false, label = "custom plates on bikes, boats, planes and trailers"},
    occluders = {default = true, label = "no exterior occlusion, so server-mapped objects stop vanishing"},
    framecaps = {default = true, label = "SA-MP's and GTA's frame caps off while the in-game Frame Limiter is off (as autofps)",
        apply = function(on)
            if on then return fixFrameCap() and 1 or 0, 0 end
            return stopFrameCap() and 1 or 0, 0
        end},
    mtapatches = {default = true, label = "MTA's multiplayer patches (73 sites, as crashes.asi applies them with SA-MP)"},
    wheelfps = {default = true, label = "the coasting slow-down off the throttle is the same at any fps (cars stop dead at high fps)",
        apply = function(on)
            if on then return fixFrameRate("wheelfps"), 0 end
            return stopFrameRate("wheelfps"), 0
        end},
    swimfps = {default = true, label = "swimming is as fast at any fps (slow at high fps)",
        apply = function(on)
            if on then return fixFrameRate("swimfps"), 0 end
            return stopFrameRate("swimfps"), 0
        end},
    floatfps = {default = true, label = "buoyancy scaled as Swim FPS Fix does: less lift above 30 fps, boats included",
        apply = function(on)
            if on then return fixFrameRate("floatfps"), 0 end
            return stopFrameRate("floatfps"), 0
        end},
    positionguard = {default = true, label = "NaN, inf and off-map positions put back before a ped, car or object is placed",
        apply = function(on)
            if on then return fixPositionGuard() and 1 or 0, 0 end
            return stopPositionGuard() and 1 or 0, 0
        end},
    fogrestore = {default = true, label = "the game's fog sent again twice a frame, after SA-MP leaves it stale (as fogrestore.asi)",
        apply = function(on)
            if on then return fixFogRestore() and 1 or 0, 0 end
            return stopFogRestore() and 1 or 0, 0
        end},
    crashfix = {default = true, label = "MTA's crash-fix hooks (lib/fixes/crashfix.dll)",
        apply = function() local c = fixCrashes("game") + fixCrashes("samp"); return c, 0 end},
    frameclone = {default = false, label = "crash fix 19 (RwFrameCloneHierarchy); breaks SkyGFX",
        apply = function() return fixCrashes("game"), 0 end},
    oldcrashfixes = {default = false, label = "crash fixes 12 and 17 (UncompressAnimation, RwFrameForAllObjects), which MTA no longer installs",
        apply = function() return fixCrashes("game"), 0 end},
    volshadows = {default = false, label = "volumetric vehicle shadows (off: cheap blob shadows, as crashes.asi)",
        apply = function(on) return setCrashFixShadows(on) and 1 or 0, 0 end},
    noheathaze = {default = true, label = "no heat haze"},
    noclouds = {default = true, label = "no clouds (a few frames)"},
    noflashes = {default = false, label = "no flash effects"},
    nosound = {default = false, label = "no game sound at all"},
    novehiclelighting = {default = false, label = "no vehicle lighting"},
    nospecular = {default = false, label = "no specular vehicle lighting"},
    notargetblip = {default = false, label = "no target blip over aimed peds"},
    nointeriorreflections = {default = false, label = "no interior floor reflections"},
    nopostfx = {default = false, label = "no post effects"},
    windowed = {default = true, label = "Alt+Enter switches to windowed mode"},
    background = {default = true, label = "the game keeps running in the background and the pause menu"},
    keepcars = {default = true, label = "a car marked no longer needed (opcode 01C3) is kept, not deleted with its parts left behind"},
    focusmenu = {default = false, label = "the pause menu opens when the window gets focus back (SA-MP turns this off)"},
    floatclock = {default = true,
        label = "camera moves, shakes and zooms and the weapon spread keep a smooth clock the server cannot set"},
    -- not a byte patch: read by onReceiveRpc
    dialoglines = {default = true, label = "cut list dialog lines (tablist cells) past 128 characters",
        apply = function() return 1, 0 end},
    rpcpositions = {default = true, label = "drop server teleports (RPCs 12, 13, 159) to NaN or off the world",
        apply = function() return 1, 0 end},
    -- not a byte patch: switched through fixTextCache / stopTextCache
    textcache = {default = true, label = "replay SA-MP's text layouts instead of laying them out every frame",
        apply = function(on)
            if on then return fixTextCache() and 1 or 0, 0 end
            return stopTextCache() and 1 or 0, 0
        end},
}
local options = {}
local OPTIONS_PATH = getWorkingDirectory() .. "\\config\\fixes.json"

local function loadOptions()
    for key, o in pairs(OPTIONS) do options[key] = o.default end
    local f = io.open(OPTIONS_PATH, "r")
    if not f then return end
    local text = f:read("*a"); f:close()
    local ok, data = pcall(decodeJson, text)
    if ok and type(data) == "table" then
        for key in pairs(OPTIONS) do
            if type(data[key]) == "boolean" then options[key] = data[key] end
        end
    end
end

local function saveOptions()
    local f = io.open(OPTIONS_PATH, "w")
    if not f then return false end
    f:write(encodeJson(options)); f:close()
    return true
end

local function optionOn(p)
    return p.option == nil or options[p.option] == true
end

-- Returns "already" | "conflict" | "ok" | "refused", reason
local function applyGamePatch(p)
    if #p.from ~= #p.to then
        return "refused", string.format("descriptor is %d bytes in, %d bytes out", #p.from, #p.to)
    end

    local found = readBytes(p.va, #p.to)
    if not found then return "refused", "unreadable" end
    -- Another mod handles what this patch is for: leave its bytes alone.
    if p.standDown then
        local by = p.standDown(found)
        if by then return "conflict", by end
    end
    if bytesEqual(found, p.to) then return "already" end

    -- A site a known third-party patch owns: stand down (it applies again once
    -- that mod is gone).
    for _, c in ipairs(p.conflicts or {}) do
        if bytesEqual(found, c.bytes) then return "conflict", c.by end
    end

    if not bytesEqual(found, p.from) then
        return "refused", string.format("found %s, expected %s", bytesToHex(found), bytesToHex(p.from))
    end

    -- Always unprotect: .text (0xE0000020) and .data (0xC0000040) are writable,
    -- but 0x884984 is in .rdata (0x40000040), and a store into a read-only page
    -- is an access violation pcall cannot catch.
    for i = 1, #p.to do
        memory.setuint8(p.va + i - 1, p.to[i], true)
    end

    local after = readBytes(p.va, #p.to)
    if not bytesEqual(after, p.to) then
        return "refused", string.format("write did not stick (still %s)", bytesToHex(after))
    end
    return "ok"
end

-- CStreaming::ms_bIsInitialised, set at the end of CStreaming::Init2: after it,
-- constants baked into allocation calls (SAMPGraphicRestore's
-- ms_rwObjectInstances.Init(12000) at 0x5B8E55) have been read already. It is
-- always set when MoonLoader starts scripts (Init-time patches need an ASI);
-- this speaks up only if it ever is not.
local STREAMING_INITIALISED = 0x9654B8

function reportGameInitState()
    local ok, initialised = pcall(function()
        return ffi.cast("int32_t*", STREAMING_INITIALISED)[0]
    end)
    if not ok then
        log("[fixes] cannot read CStreaming::ms_bIsInitialised")
        return nil
    end

    initialised = tonumber(initialised) ~= 0
    if not initialised then
        log("[fixes] CStreaming::Init2 has not run yet -- init-time constants " ..
              "(e.g. ms_rwObjectInstances.Init at 0x5B8E55) are patchable from here after all")
    end
    return initialised
end

--- Put a site back to stock if it holds ours: "reverted", "stock" (nothing of
--- ours) or "refused" (someone else's bytes).
local function revertGamePatch(p)
    if p.revertOnlyOurs and p.state ~= "ok" then return "stock" end
    local found = readBytes(p.va, #p.from)
    if not found then return "refused" end
    if bytesEqual(found, p.from) then return "stock" end
    if not bytesEqual(found, p.to) then return "refused" end
    for i = 1, #p.from do
        memory.setuint8(p.va + i - 1, p.from[i], true)
    end
    return bytesEqual(readBytes(p.va, #p.from), p.from) and "reverted" or "refused"
end

--- Switch an option and apply or revert its patches at once. -> sites changed,
--- and any that refused.
function setOption(key, on)
    options[key] = on and true or false
    local changed, refused = 0, 0
    for _, p in ipairs(gamePatches) do
        if p.option == key then
            local state, why
            if on then state, why = applyGamePatch(p) else state = revertGamePatch(p) end
            if state == "ok" or state == "reverted" then changed = changed + 1
            elseif state == "refused" then refused = refused + 1 end
            if on or state ~= "refused" then p.state, p.why = state, why end
        end
    end
    if OPTIONS[key] and OPTIONS[key].apply then
        local c, r = OPTIONS[key].apply(options[key])
        changed, refused = changed + c, refused + r
    end
    saveOptions()
    return changed, refused
end

-- phase: "early" (gta_sa.exe code, nothing needs to be up; the default) or
-- "late" (needs game init).
-- One line per site, as the other fixes announce themselves.
local function logGamePatch(p, state, why)
    if state == "ok" then
        log(string.format("[fixes] 0x%X (%s): applied, %d byte(s) written", p.va, p.name, #p.to), "info")
    elseif state == "already" then
        -- named, so a site another mod owns is visible
        log(string.format("[fixes] 0x%X (%s): already in the wanted state", p.va, p.name), "info")
    elseif state == "conflict" then
        log(string.format("[fixes] 0x%X (%s) is owned by %s; standing down", p.va, p.name, tostring(why)), "info")
    else
        log(string.format("[fixes] refusing 0x%X (%s): %s", p.va, p.name, tostring(why)), "warn")
    end
end

function applyGamePatches(phase)
    local counts = {ok = 0, already = 0, conflict = 0, refused = 0}
    local considered = 0

    for _, p in ipairs(gamePatches) do
        if (p.phase or "early") == phase and optionOn(p) then
            considered = considered + 1

            local state, why = applyGamePatch(p)
            counts[state] = counts[state] + 1
            p.state, p.why = state, why
            logGamePatch(p, state, why)
        end
    end

    -- the counts are logged once, by reportStartup, when every fix has run
    if considered > 0 and counts.refused == considered then
        log(string.format("[fixes] every %s gta_sa.exe patch refused; this is not the 1.0 US exe the offsets came from",
            phase), "warn")
    end
    return counts
end

--------------------------------------------------------------------------------
-- Removed buildings (samp.dll)
--------------------------------------------------------------------------------
-- RemoveBuildingForPlayer: SA-MP keeps every removal in a table of exactly 1000
-- {model, x, y, z, radius} entries (20 bytes) and never bounds-checks the store.
-- Past 1000 it overwrites its count and globals: entry 1028's Y lands on an
-- object pointer, and the game dies at samp.dll+0x12A80 reading a float (R5).
-- Unity RP sends ~1240. The count is never reset, so a reconnect stores the
-- removals again.
--
-- The fix moves the table into our own block (RB_CAP entries):
--   * the store: its `mov ecx,[count]`, after the call that removes the matching
--     buildings from the world, becomes a jmp to code that keeps each entry
--     once and stops at RB_CAP;
--   * the lookup (SA-MP's CFileLoader::LoadObjectInstance hook): the imm32 of
--     its `mov esi, table` points at the new table.
-- The count stays where SA-MP keeps it; stored entries are carried over. Both
-- sites are checked against the stock code (identical on DL, R1, R3-1, R5)
-- first. The block is never freed; a reload finds the patch and leaves it.
-- open.mp's client replaces the handler and the lookup (a jmp at the start of
-- either): this stands down and names the owner.
local RB_STOCK_CAP = 1000          -- entries in SA-MP's table
local RB_ENTRY     = 20            -- model, x, y, z, radius
local RB_CAP       = 5000          -- entries in ours
local RB_MAGIC     = 0x62527846    -- "FxRb", the block's first dword
local RB_CODE      = 0x10          -- the block: header {magic, cap, table, count},
local RB_TABLE     = 0x100         -- then the store, then the table
local RB_SIZE      = RB_TABLE + RB_CAP * RB_ENTRY

local RB_HANDLER     = -0x1F       -- the handler's first byte, from the store site
local RB_LOOKUP_FUNC = -0x18       -- the lookup's first byte, from its operand

local removedBuildings             -- {count = va, cap = n} once in place, {owner = name} if another mod's

local function u32bytes(v)
    v = v % 0x100000000
    return {v % 0x100, math.floor(v / 0x100) % 0x100,
            math.floor(v / 0x10000) % 0x100, math.floor(v / 0x1000000) % 0x100}
end

local function bytesU32(b)
    return b[1] + b[2] * 0x100 + b[3] * 0x10000 + b[4] * 0x1000000
end

-- The stock store from its `mov ecx,[count]` on: fixed bytes, and the dwords
-- that hold the count's address ("count") or the table's plus a field offset
-- ("table", n). Only those addresses differ between builds.
local RB_STORE_STOCK = {
    {0x00, "8B 0D"}, {0x02, "count"},                       -- mov ecx, [count]
    {0x06, "8B 54 24 20 8D 04 89 C1 E0 02 89 B0"},          -- edx = x / eax = count * 20
    {0x12, "table", 0x00},                                  -- mov [eax+table], esi    ; model
    {0x16, "89 90"}, {0x18, "table", 0x04},                 -- x
    {0x1C, "8B 54 24 24 89 90"}, {0x22, "table", 0x08},     -- y
    {0x26, "8B 54 24 28 83 C4 14 89 90"}, {0x2F, "table", 0x0C},   -- z
    {0x33, "8B 54 24 18 41 89 90"}, {0x3A, "table", 0x10},  -- radius / inc ecx
    {0x3E, "89 0D"}, {0x40, "count"},                       -- mov [count], ecx
    {0x44, "5E C3"},                                        -- pop esi / ret
}

-- The lookup, around the operand of its `mov esi, table`.
local RB_LOOKUP_STOCK = {
    {-0x18, "A1"}, {-0x17, "count"},                        -- mov eax, [count]
    {-0x13, "53 55 56 57 33 FF 85 C0 74 4D 8B 5C 24 20 8B 6C 24 1C BE"},
    {0x00, "table", 0x00},                                  -- mov esi, table
    {0x37, "A1"}, {0x38, "count"},                          -- the loop's mov eax, [count]
}

-- Checks `layout` at `va`. Returns {count = va, table = va} as the code names
-- them, or nil and what differs. `known` pins either address in advance.
local function matchLayout(va, layout, known)
    local found = {count = known and known.count, table = known and known.table}
    for _, part in ipairs(layout) do
        local at, what, field = va + part[1], part[2], part[3]
        if what == "count" or what == "table" then
            local b = readBytes(at, 4)
            if not b then return nil, string.format("0x%X is unreadable", at) end
            local v = bytesU32(b) - (field or 0)
            if found[what] == nil then
                found[what] = v
            elseif found[what] ~= v then
                return nil, string.format("the %s address at 0x%X is 0x%X, expected 0x%X",
                    what, at, v + (field or 0), found[what] + (field or 0))
            end
        else
            local want = hexBytes(what)
            local got = readBytes(at, #want)
            if not bytesEqual(got, want) then
                return nil, string.format("found %s at 0x%X, expected %s", bytesToHex(got), at, what)
            end
        end
    end
    return found
end

-- The store that replaces SA-MP's, entered with the stack as the stock store had
-- it: the world-removal call's 5 args, then the saved esi, the return address
-- and the handler's args (model .. radius); esi = model. Bytes from
-- i686-w64-mingw32-as; lib/fixes/test/rb_store_test.c runs them.
local function removedBuildingsStore(countVa, tableVa, cap)
    local out = {}
    local function emit(hex, dword)
        for _, b in ipairs(hexBytes(hex)) do out[#out + 1] = b end
        if dword then for _, b in ipairs(u32bytes(dword)) do out[#out + 1] = b end end
    end
    emit("83 C4 14")              -- add  esp, 14h       ; the world-removal call's args
    emit("8B 0D", countVa)        -- mov  ecx, [count]   ; [esp+8] model .. [esp+18h] radius
    emit("BA", tableVa)           -- mov  edx, table
    emit("85 C9")                 -- test ecx, ecx
    emit("74 2E")                 -- jz   store
                                  -- scan:
    emit("39 32")                 -- cmp  [edx], esi     ; model
    emit("75 24")                 -- jne  next
    emit("8B 44 24 0C 39 42 04")  -- mov  eax, [esp+0Ch] / cmp [edx+4], eax     ; x
    emit("75 1B")                 -- jne  next
    emit("8B 44 24 10 39 42 08")  -- y
    emit("75 12")                 -- jne  next
    emit("8B 44 24 14 39 42 0C")  -- z
    emit("75 09")                 -- jne  next
    emit("8B 44 24 18 39 42 10")  -- radius
    emit("74 39")                 -- je   done           ; stored already
                                  -- next:
    emit("83 C2 14")              -- add  edx, 14h
    emit("49")                    -- dec  ecx
    emit("75 D2")                 -- jnz  scan
                                  -- store:              ; edx = table + count * 20
    emit("8B 0D", countVa)        -- mov  ecx, [count]
    emit("81 F9", cap)            -- cmp  ecx, cap
    emit("73 25")                 -- jae  done           ; full: the world removal still ran
    emit("89 32")                 -- mov  [edx], esi
    emit("8B 44 24 0C 89 42 04")  -- mov  eax, [esp+0Ch] / mov [edx+4], eax     ; x
    emit("8B 44 24 10 89 42 08")  -- y
    emit("8B 44 24 14 89 42 0C")  -- z
    emit("8B 44 24 18 89 42 10")  -- radius
    emit("41")                    -- inc  ecx
    emit("89 0D", countVa)        -- mov  [count], ecx
                                  -- done:
    emit("5E C3")                 -- pop  esi / ret
    return out
end

-- Who a function's first instruction jumps to, when it is a jmp rel32.
local function hookedBy(va)
    local b = readBytes(va, 5)
    if not b or b[1] ~= 0xE9 then return nil end
    local target = (va + 5 + bytesU32({b[2], b[3], b[4], b[5]})) % 0x100000000
    return moduleAt(target) or string.format("code at 0x%X", target)
end

local function poke(va, bytes)    -- into our own block
    local p = ffi.cast("uint8_t*", va)
    for i = 1, #bytes do p[i - 1] = bytes[i] end
end

local function writeCode(va, bytes)
    for i = 1, #bytes do memory.setuint8(va + i - 1, bytes[i], true) end
end

function fixRemovedBuildings()
    local storeVa, how, why = resolveSamp("RemoveBuildingStore")
    if not storeVa then
        log("[fixes] the removed-buildings fix is not supported: " .. tostring(why))
        return false
    end
    local lookupVa, how2, why2 = resolveSamp("RemoveBuildingLookup")
    if not lookupVa then
        log("[fixes] the removed-buildings fix is not supported: " .. tostring(why2))
        return false
    end
    how = (how == how2) and how or (how .. "/" .. how2)
    local function refuse(fmt, ...)
        log(string.format("[fixes] refusing the removed-buildings fix via %s: " .. fmt, how, ...))
        return false
    end

    -- SA-MP's code replaced by another mod (open.mp's client): not ours.
    local owner = hookedBy(storeVa + RB_HANDLER) or hookedBy(lookupVa + RB_LOOKUP_FUNC)
    if owner then
        removedBuildings = {owner = owner}
        log(string.format("[fixes] removed buildings: SA-MP's handler and lookup are replaced by %s, " ..
            "which keeps its own table; standing down", owner))
        return false
    end

    -- the lookup names the count, and whichever table it reads now
    local lookup, lwhy = matchLayout(lookupVa, RB_LOOKUP_STOCK)
    if not lookup then return refuse("lookup at 0x%X: %s", lookupVa, lwhy) end

    local head = readBytes(storeVa, 5)
    if head and head[1] == 0xE9 then
        -- Already a jmp: ours from an earlier run, or another mod's. Ours points
        -- the lookup at the same block, which is checked before it is read.
        local rel = bytesU32({head[2], head[3], head[4], head[5]})
        local block = (storeVa + 5 + rel - RB_CODE) % 0x100000000
        if lookup.table ~= block + RB_TABLE then
            return refuse("the store at 0x%X already jumps to 0x%X, which is not this script's",
                storeVa, block + RB_CODE)
        end
        local h = readBytes(block, 16)
        if not h or bytesU32({h[1], h[2], h[3], h[4]}) ~= RB_MAGIC
            or bytesU32({h[13], h[14], h[15], h[16]}) ~= lookup.count then
            return refuse("the block at 0x%X is not this script's", block)
        end
        removedBuildings = {count = lookup.count, cap = bytesU32({h[5], h[6], h[7], h[8]})}
        log(string.format("[fixes] removed buildings: already moved to 0x%X (%d stored, room for %d)",
            lookup.table, readU32(lookup.count), removedBuildings.cap))
        return true
    end

    local store, swhy = matchLayout(storeVa, RB_STORE_STOCK, lookup)
    if not store then return refuse("store at 0x%X: %s", storeVa, swhy) end
    if store.count ~= store.table + RB_STOCK_CAP * RB_ENTRY then
        return refuse("the count (0x%X) does not follow a %d-entry table (0x%X)",
            store.count, RB_STOCK_CAP, store.table)
    end

    local p = ffi.C.VirtualAlloc(nil, RB_SIZE, 0x3000, 0x40)   -- MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE
    if p == nil then return refuse("VirtualAlloc of %d bytes failed", RB_SIZE) end
    local block = tonumber(ffi.cast("uintptr_t", p))
    local tableVa = block + RB_TABLE

    local header = {}
    for _, v in ipairs({RB_MAGIC, RB_CAP, tableVa, store.count}) do
        for _, b in ipairs(u32bytes(v)) do header[#header + 1] = b end
    end
    poke(block, header)
    poke(block + RB_CODE, removedBuildingsStore(store.count, tableVa, RB_CAP))

    -- Carry over what SA-MP stored; past 1000 its table already overran, so the
    -- first 1000 are kept and it is said.
    local count = readU32(store.count)
    local carried = math.min(count, RB_STOCK_CAP)
    local src, dst = ffi.cast("uint8_t*", store.table), ffi.cast("uint8_t*", tableVa)
    for i = 0, carried * RB_ENTRY - 1 do dst[i] = src[i] end
    if count > RB_STOCK_CAP then
        ffi.cast("uint32_t*", store.count)[0] = RB_STOCK_CAP
        log(string.format("[fixes] SA-MP had already stored %d removed buildings and run over its own " ..
            "memory; kept the first %d. Restart the game if anything misbehaves.", count, RB_STOCK_CAP))
    end

    -- The lookup first (the new table already holds the old one's entries), then
    -- the jmp into the new store.
    writeCode(lookupVa, u32bytes(tableVa))
    writeCode(storeVa, {0xE9, unpack(u32bytes(block + RB_CODE - (storeVa + 5)))})

    local verified = readU32(lookupVa) == tableVa and readU8(storeVa) == 0xE9
    if verified then removedBuildings = {count = store.count, cap = RB_CAP} end
    log(string.format("[fixes] removed buildings on SAMP %s via %s: table moved from 0x%X to 0x%X, room for %d " ..
        "(was %d), each entry kept once; %d carried over -- %s",
        getSampVersion(), how, store.table, tableVa, RB_CAP, RB_STOCK_CAP, carried,
        verified and "store and lookup rewritten" or "WRITE DID NOT STICK"))
    return verified
end

--------------------------------------------------------------------------------
-- SA-MP's exception filter (samp.dll)
--------------------------------------------------------------------------------
-- SA-MP runs game opcodes, and some calls of its own, under an exception filter
-- that prints "Warning(opcode 0x%X): Exception 0x%X at 0x%X" and swallows the
-- exception: ten times, and the eleventh crashes the game. The anticrashers by
-- Export (0.3.7 R1, 0.3.DL; CLEO and ASI) and DarkP1xel (R4) NOP the warning
-- and the cap. Here every exception is swallowed and the first ten still
-- print: the filter's 37-byte tail is rewritten in place, and its warning call
-- goes through a check in the dead `xor eax,eax / ret`. The entry's jump to
-- the counter (no chat yet, at +8) still lands on an instruction, and the
-- counter counts every exception. The warning call is the `push eax / call`
-- after the push of "Warning(%s)...", the same on every build (R1 0x5CF2C,
-- R3-1 0x602CC, R4 0x609FD, R5 0x60A4D, DL 0x604CC).
-- Bytes from i686-w64-mingw32-as; lib/fixes/test/exception_filter_test.c runs
-- them inside SA-MP R5's own filter.
local EF_FORMAT = "Warning(%s): Exception 0x%X at 0x%X\0"
local EF_SIZE   = 37
local EF_SHOWN  = 10           -- warnings printed, as stock

local exceptionFilter          -- {count = va, seen = n} once in place, {owner = text} if another mod's

-- The stock tail from the warning call. -> counter va, print va, or nil.
local function efStock(site, t)
    local function at(i, hex)
        local want = hexBytes(hex)
        for k = 1, #want do if t[i + k - 1] ~= want[k] then return false end end
        return true
    end
    if not (t[1] == 0xE8 and at(6, "83 C4 14 A1") and at(14, "83 F8 0A 7D 0E 40 A3")
            and at(25, "B8 01 00 00 00 C2 0C 00 33 C0 C2 0C 00")) then return nil end
    local count = bytesU32({t[10], t[11], t[12], t[13]})
    if bytesU32({t[21], t[22], t[23], t[24]}) ~= count then return nil end
    return count, (site + 5 + bytesU32({t[2], t[3], t[4], t[5]})) % 0x100000000
end

local function exceptionFilterCode(site, count, print)
    local out = {}
    local function emit(hex, dword)
        for _, b in ipairs(hexBytes(hex)) do out[#out + 1] = b end
        if dword then for _, b in ipairs(u32bytes(dword)) do out[#out + 1] = b end end
    end
    emit("E8 11 00 00 00")        -- call shown           ; the warning, through the check
    emit("83 C4 14")              -- add  esp, 14h        ; its five args, as stock
                                  -- +8 (the entry's jump when there is no chat):
    emit("FF 05", count)          -- inc  dword [count]
    emit("B8 01 00 00 00")        -- mov  eax, 1          ; EXCEPTION_EXECUTE_HANDLER, always
    emit("C2 0C 00")              -- ret  0Ch
                                  -- shown:
    emit("83 3D", count)          -- cmp  dword [count], 10
    emit("0A")
    emit("73 05")                 -- jae  done
    emit("E9", print - (site + 0x24))   -- jmp print     ; returns to the add esp
                                  -- done:
    emit("C3")                    -- ret
    return out
end

pcall(ffi.cdef, [[ size_t VirtualQuery(const void*, void*, size_t); ]])

-- True when [va, va + size) is committed and readable: R1/R2's packed sections
-- can be unmapped by now, and a fault in ffi.string is not a Lua error.
local function readable(va, size)
    local mbi = ffi.new("uint32_t[7]")          -- MEMORY_BASIC_INFORMATION
    local at, stop = va, va + size
    while at < stop do
        if ffi.C.VirtualQuery(ffi.cast("const void*", at), mbi, 28) == 0 then return false end
        local protect = tonumber(mbi[5])
        if tonumber(mbi[4]) ~= 0x1000 or protect % 0x200 >= 0x100 or protect == 0x01 or protect == 0 then
            return false                        -- not MEM_COMMIT, PAGE_GUARD, PAGE_NOACCESS
        end
        at = tonumber(mbi[0]) + tonumber(mbi[3])   -- BaseAddress + RegionSize
    end
    return true
end

-- samp.dll's mapped sections: {va, size, code}.
local function moduleSections(base)
    local nt = base + readU32(base + 0x3C)
    local count = readU8(nt + 6) + readU8(nt + 7) * 0x100
    local first = nt + 24 + readU8(nt + 20) + readU8(nt + 21) * 0x100
    local out = {}
    for i = 0, count - 1 do
        local s = first + i * 40
        local size, rva, flags = readU32(s + 8), readU32(s + 12), readU32(s + 36)
        if size == 0 then size = readU32(s + 16) end
        if size > 0 and readable(base + rva, size) then
            out[#out + 1] = {va = base + rva, size = size, code = flags % 0x40 >= 0x20}   -- IMAGE_SCN_CNT_CODE
        end
    end
    return out
end

-- The first `needle` in the mapped sections of that kind, as a va.
local function findInSections(sections, code, needle)
    for _, s in ipairs(sections) do
        if s.code == code then
            local at = ffi.string(ffi.cast("const char*", s.va), s.size):find(needle, 1, true)
            if at then return s.va + at - 1 end
        end
    end
end

-- The warning call: the push of the "Warning(%s)" format, then `push eax`. Its
-- bytes are not read here, so another mod's patch there is still found.
local function locateExceptionFilter(base)
    local ok, sections = pcall(moduleSections, base)
    if not ok then return nil, "samp.dll's sections are unreadable" end
    local s = findInSections(sections, false, EF_FORMAT)
    if not s then return nil, "its warning string is not in samp.dll" end
    local push = findInSections(sections, true, string.char(0x68, unpack(u32bytes(s))))
    if not push then return nil, "nothing pushes its warning string" end
    if readU8(push + 5) ~= 0x50 then return nil, "no `push eax` after the string's push" end
    return push + 6
end

function fixExceptionFilter()
    local base = getModuleHandle("samp.dll")
    if not base or base == 0 then
        log("[fixes] the exception filter fix is not supported: samp.dll is not loaded")
        return false
    end
    local site, why = locateExceptionFilter(base)
    if not site then
        log("[fixes] the exception filter fix is not supported: " .. why)
        return false
    end
    local t = readBytes(site, EF_SIZE)
    if not t then
        log(string.format("[fixes] refusing the exception filter fix: 0x%X is unreadable", site))
        return false
    end
    local rva = site - base

    if bytesEqual({unpack(t, 1, 5)}, hexBytes("E8 11 00 00 00")) and t[9] == 0xFF and t[10] == 0x05 then
        local count = bytesU32({t[11], t[12], t[13], t[14]})
        exceptionFilter = {count = count, seen = readU32(count)}
        log(string.format("[fixes] SA-MP's exception filter: already patched (0x%X, %d exceptions swallowed so far)",
            site, exceptionFilter.seen), "info")
        return true
    end

    local count, print = efStock(site, t)
    if not count then
        -- the anticrashers' own patches
        local nop5 = nops(5)
        local owner
        if bytesEqual({unpack(t, 1, 5)}, nop5) and bytesEqual({unpack(t, 14, 18)}, nop5) then
            owner = "an anticrasher (Export's: no warnings, every exception swallowed)"
        elseif bytesEqual({unpack(t, 9, 34)}, nops(26)) then
            owner = "DarkP1xel's anticrasher (R4), whose filter returns whatever the warning left in eax"
        elseif bytesEqual({unpack(t, 1, 4)}, nops(4)) and t[5] ~= 0x90 then
            owner = "the 0.3.DL anticrasher ASI, which NOPs 4 of the call's 5 bytes: the first exception " ..
                "crashes the game - remove it"
        end
        if owner then
            exceptionFilter = {owner = owner}
            log(string.format("[fixes] SA-MP's exception filter (rva 0x%X) is patched by %s; standing down", rva, owner),
                owner:find("remove it", 1, true) and "warn" or "info")
            return false
        end
        log(string.format("[fixes] refusing the exception filter fix: 0x%X (rva 0x%X) holds %s", site, rva, bytesToHex(t)))
        return false
    end

    writeCode(site, exceptionFilterCode(site, count, print))
    local verified = bytesEqual(readBytes(site, EF_SIZE), exceptionFilterCode(site, count, print))
    if verified then exceptionFilter = {count = count, seen = readU32(count)} end
    log(string.format("[fixes] SA-MP's exception filter at 0x%X (rva 0x%X): every exception is swallowed, the first %d " ..
        "still print -- %s", site, rva, EF_SHOWN, verified and "tail rewritten" or "WRITE DID NOT STICK"),
        verified and "info" or "warn")
    return verified
end

--------------------------------------------------------------------------------
-- SA-MP's frame cap (samp.dll)
--------------------------------------------------------------------------------
-- autofps's port of crashes.asi (disableFPSLock + FPSStrafeHook), carried here
-- too. SA-MP's frame limiter sleeps to ~100 fps and, every 250 ms, writes
-- 50/fps over CTimer::ms_fTimeStep. A 7-byte jmp at +0Ch goes to a stub that
-- reads GTA's Frame Limiter setting (0xBA6794): off -> +15h, the init arm (no
-- measuring, no timestep write, no sleep); on -> +11h, stock. Its compare's
-- flags drive the `jne` at +13h, so the stub keeps them. The ms_fTimeStep
-- store (+1Ah) and the default sleep (+182h) are NOPed, and SA-MP's Sleep(1)
-- in GTA's limiter spin becomes Sleep(0). With the in-game Frame Limiter on,
-- nothing changes. A hook already at +0Ch (autofps, crashes.asi) is left to
-- its owner: the other writes are the same bytes either way. Option
-- "framecaps"; GTA's own 14 ms delay is the gamePatches entry beside it.
local FC_RVA = {                   -- verified against R1, R3-1, R5 and DL (autofps's table)
    limiter = {DLR1 = 0xA1A30, R1 = 0x9D170, R3 = 0xA14E0, R5 = 0xA1C10},
    sleep   = {DLR1 = 0xA2201, R1 = 0x9D8F1, R3 = 0xA1CB1, R5 = 0xA23E1},   -- the push's imm8
}
local FC_PREF     = 0xBA6794       -- CMenuManager::m_bFrameLimiter
local FC_HOOK     = 0x0C           -- mov eax,[lastRecalc] / test eax,eax: 7 bytes
local FC_ON       = 0x11
local FC_OFF      = 0x15
local FC_TIMESTEP = 0x1A           -- mov ds:0xB7CB5C, eax
local FC_SLEEP    = 0x182          -- push edx / call [Sleep]
local FC_SHAPE = {
    {0x00, "51 56 57 8B F9"}, {FC_HOOK, "A1"}, {FC_ON, "85 C0 75 2C"},
    {FC_OFF, "B8 00 00 80 3F"}, {FC_TIMESTEP, "A3 5C CB B7 00"},
}

local frameCap                     -- {stub = va, saved = bytes} once ours, {owner = text} if another's

local function frameCapSites()
    local base = getModuleHandle("samp.dll")
    if not base or base == 0 then return nil, nil, "samp.dll is not loaded" end
    local version = getSampVersion()
    local rva, sleep = FC_RVA.limiter[version], FC_RVA.sleep[version]
    if not rva then return nil, nil, "no offsets for SAMP " .. version end
    return base + rva, base + sleep
end

local function frameCapStubCode(stub, va)
    local out = {}
    local function emit(hex, dword)
        for _, b in ipairs(hexBytes(hex)) do out[#out + 1] = b end
        if dword then for _, b in ipairs(u32bytes(dword)) do out[#out + 1] = b end end
    end
    emit("80 3D", FC_PREF); emit("00")     -- cmp byte [frame limiter setting], 0
    emit("75 05")                          -- jne on
    emit("E9", va + FC_OFF - (stub + 14))  -- jmp the init arm: uncapped
    emit("E9", va + FC_ON - (stub + 19))   -- on: jmp the stock test
    return out
end

-- The two writes that are the same whoever hooks: the default sleep, SA-MP's
-- Sleep(1). -> a word for the log
local function frameCapSleeps(va, sleepVa)
    local said = {}
    local s = readBytes(va + FC_SLEEP, 7)
    if bytesEqual(s, nops(7)) or (s and s[1] == 0x00 and s[2] == 0xFF and s[3] == 0x90) then
        said[#said + 1] = "sleep already off"
    elseif s and s[1] == 0x52 and s[2] == 0xFF and s[3] == 0x15 then
        writeCode(va + FC_SLEEP, nops(7)); said[#said + 1] = "sleep NOPed"
    else
        said[#said + 1] = "sleep not stock (" .. bytesToHex(s) .. "), left"
    end
    local imm = readU8(sleepVa)
    if imm == 0 then said[#said + 1] = "Sleep(0) already"
    elseif imm == 1 and readU8(sleepVa - 1) == 0x6A then writeCode(sleepVa, {0}); said[#said + 1] = "Sleep(1) -> Sleep(0)"
    else said[#said + 1] = ("the frame sleep is %02X, left"):format(imm) end
    return table.concat(said, ", ")
end

function fixFrameCap()
    local va, sleepVa, why = frameCapSites()
    if not va then
        log("[fixes] the SAMP frame cap removal is not supported: " .. why)
        return false
    end
    local head = readBytes(va + FC_HOOK, 5)
    if head and head[1] == 0xE9 then
        local target = (va + FC_HOOK + 5 + bytesU32({head[2], head[3], head[4], head[5]})) % 0x100000000
        if frameCap and frameCap.stub == target then return true end
        local okStub = readBytes(target, 2)
        if okStub and okStub[1] == 0x80 and okStub[2] == 0x3D and not moduleAt(target) then
            -- a dispatch stub of this shape: ours from an earlier run, or autofps's
            frameCap = {owner = "a frame-cap stub at " .. string.format("0x%X", target) .. " (autofps, or an earlier run)"}
        else
            frameCap = {owner = moduleAt(target) or string.format("code at 0x%X", target)}
        end
        log(string.format("[fixes] SAMP frame cap: +0x%X is hooked by %s; standing down (%s)",
            FC_HOOK, frameCap.owner, frameCapSleeps(va, sleepVa)), "info")
        return false
    end
    for _, part in ipairs(FC_SHAPE) do
        local want = hexBytes(part[2])
        if not bytesEqual(readBytes(va + part[1], #want), want) then
            log(string.format("[fixes] refusing the SAMP frame cap removal: +0x%X is %s, expected %s",
                part[1], bytesToHex(readBytes(va + part[1], #want)), part[2]))
            return false
        end
    end
    local p = ffi.C.VirtualAlloc(nil, 0x20, 0x3000, 0x40)   -- MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE
    if p == nil then
        log("[fixes] refusing the SAMP frame cap removal: VirtualAlloc failed")
        return false
    end
    local stub = tonumber(ffi.cast("uintptr_t", p))
    poke(stub, frameCapStubCode(stub, va))
    local saved = {hook = readBytes(va + FC_HOOK, 7), timestep = readBytes(va + FC_TIMESTEP, 5)}
    writeCode(va + FC_HOOK, {0xE9, unpack(u32bytes(stub - (va + FC_HOOK + 5)))})
    writeCode(va + FC_HOOK + 5, nops(2))
    writeCode(va + FC_TIMESTEP, nops(5))
    local sleeps = frameCapSleeps(va, sleepVa)
    local verified = readU8(va + FC_HOOK) == 0xE9 and bytesEqual(readBytes(va + FC_TIMESTEP, 5), nops(5))
    if verified then frameCap = {stub = stub, saved = saved} end
    log(string.format("[fixes] SAMP frame cap on SAMP %s: 0x%X (rva 0x%X) dispatches on the Frame Limiter setting " ..
        "(stub 0x%X), timestep store NOPed, %s -- %s", getSampVersion(), va, va - getModuleHandle("samp.dll"), stub,
        sleeps, verified and "uncapped while the in-game Frame Limiter is off" or "WRITE DID NOT STICK"),
        verified and "info" or "warn")
    return verified
end

-- Off: our hook and timestep NOPs go back (the sleeps stay off until a restart).
function stopFrameCap()
    if not frameCap or not frameCap.stub then return false end
    local va = frameCapSites()
    if not va then return false end
    writeCode(va + FC_HOOK, frameCap.saved.hook)
    writeCode(va + FC_TIMESTEP, frameCap.saved.timestep)
    frameCap = nil
    return true
end

-- A line when SA-MP has swallowed more exceptions; from the main loop.
local function exceptionFilterTick()
    if not exceptionFilter or not exceptionFilter.count then return end
    local n = readU32(exceptionFilter.count)
    if n == exceptionFilter.seen then return end
    exceptionFilter.seen = n
    log(string.format("[fixes] SA-MP has swallowed %d exception(s) so far (stock crashes on the 11th); the first %d " ..
        "printed as warnings in chat", n, EF_SHOWN), "warn")
end

--------------------------------------------------------------------------------
-- Fog (gta_sa.exe)
--------------------------------------------------------------------------------
-- fogrestore.asi's fix, carried here. RenderWare keeps each D3D render state it
-- sent (0xC991D0 + state*8: the value, then a queued flag) and sends only
-- changes; SA-MP sets the device's fog behind it, so the game's fog never goes
-- back. Twice a frame in CGame::RenderScene (after the world, after the
-- fading entities) a routine re-sends FOGENABLE, FOGCOLOR, FOGTABLEMODE and
-- FOGVERTEXMODE through the game's own RwD3D9SetRenderState with the value it
-- holds, poked first so the setter's compare misses; the game's next draw
-- flushes them. Both hooks or neither. Another hook there (fogrestore.asi)
-- keeps the sites, and ENB or SkyGFX, which own the fog, stand it down.
-- Option "fogrestore".
local FOG_SET    = 0x7FC2D0        -- void RwD3D9SetRenderState(state, value)
local FOG_SHADOW = 0xC991D0
local FOG_STATES = {0x1C, 0x22, 0x23, 0x8C}
local FOG_SITES = {
    {va = 0x53DFE2, from = hexBytes("6A 00 B9 40 42 BB 00")},   -- push 0 / mov ecx, 0xBB4240
    {va = 0x53E01E, from = hexBytes("A0 28 C7 C7 00")},         -- mov al, ds:0xC7C728
}
local FOG_OWNERS = {"enbseries.ini", "enbseries\\enbseries.ini", "enbsuite.ini", "skygfx.ini"}

local fogRestore                   -- {entries = {va, va}} once ours, {owner = text} if another's
local fogBlock                     -- our code, kept for the next time the option goes on

-- The routine, which keeps every register and the flags (absolute addresses
-- only), then an entry per site: call it, the site's bytes, jmp back.
local function fogRestoreCode(block)
    local code = {}
    local function emit(hex, dword)
        for _, b in ipairs(hexBytes(hex)) do code[#code + 1] = b end
        if dword then for _, b in ipairs(u32bytes(dword)) do code[#code + 1] = b end end
    end
    emit("9C 60")                                  -- pushfd / pushad
    for _, state in ipairs(FOG_STATES) do
        local shadow = FOG_SHADOW + state * 8
        emit("A1", shadow)                         -- mov eax, [shadow]
        emit("50 83 F0 01")                        -- push eax (the value) / xor eax, 1
        emit("A3", shadow)                         -- mov [shadow], eax: the compare misses
        emit("68", state)                          -- push state
        emit("B8", FOG_SET)                        -- mov eax, RwD3D9SetRenderState
        emit("FF D0 83 C4 08")                     -- call eax / add esp, 8
    end
    emit("61 9D C3")                               -- popad / popfd / ret
    local entries = {}
    for i, site in ipairs(FOG_SITES) do
        entries[i] = block + #code
        emit("E8", block - (entries[i] + 5))
        for _, b in ipairs(site.from) do code[#code + 1] = b end
        emit("E9", site.va + #site.from - (block + #code + 5))
    end
    return code, entries
end

-- The entry a site jumps to when it is ours (this run's or an earlier one's),
-- or nil.
local function fogEntryAt(site)
    local b = readBytes(site.va, 5)
    if not b or b[1] ~= 0xE9 then return nil end
    local at = (site.va + 5 + bytesU32({b[2], b[3], b[4], b[5]})) % 0x100000000
    if moduleAt(at) then return nil end
    local head = readBytes(at, 5 + #site.from)
    if not head or head[1] ~= 0xE8 then return nil end
    for i, v in ipairs(site.from) do if head[5 + i] ~= v then return nil end end
    return at
end

local function gameFileExists(name)
    local ok, yes = pcall(function() return doesFileExist(getGameDirectory() .. "\\" .. name) end)
    return ok and yes == true
end

function fixFogRestore()
    if fogRestore and fogRestore.entries then return true end
    for _, name in ipairs(FOG_OWNERS) do
        if gameFileExists(name) then
            fogRestore = {owner = name}
            log(string.format("[fixes] fog restore: %s is here, a shader mod that owns the fog; standing down", name),
                "info")
            return false
        end
    end
    local earlier = {fogEntryAt(FOG_SITES[1]), fogEntryAt(FOG_SITES[2])}
    if earlier[1] and earlier[2] then
        fogRestore = {entries = earlier}
        log(string.format("[fixes] 0x%X, 0x%X fog restore: already hooked (an earlier run's)", FOG_SITES[1].va,
            FOG_SITES[2].va), "info")
        return true
    end
    for _, site in ipairs(FOG_SITES) do                -- both stock before either is written
        local found = readBytes(site.va, #site.from)
        if not bytesEqual(found, site.from) then
            local owner = found and hookOwner(site.va, found)
            if owner then
                fogRestore = {owner = owner}
                log(string.format("[fixes] 0x%X fog restore: CGame::RenderScene is hooked by %s; standing down",
                    site.va, owner), "info")
            else
                log(string.format("[fixes] refusing the fog restore: 0x%X is %s, expected %s", site.va,
                    bytesToHex(found), bytesToHex(site.from)), "warn")
            end
            return false
        end
    end
    if not fogBlock then
        local p = ffi.C.VirtualAlloc(nil, 0x100, 0x3000, 0x40)   -- MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE
        if p == nil then
            log("[fixes] refusing the fog restore: VirtualAlloc failed")
            return false
        end
        fogBlock = tonumber(ffi.cast("uintptr_t", p))
    end
    local code, entries = fogRestoreCode(fogBlock)
    poke(fogBlock, code)
    for i, site in ipairs(FOG_SITES) do
        local jmp = {0xE9, unpack(u32bytes(entries[i] - (site.va + 5)))}
        for _ = 6, #site.from do jmp[#jmp + 1] = 0x90 end
        writeCode(site.va, jmp)
    end
    if fogEntryAt(FOG_SITES[1]) ~= entries[1] or fogEntryAt(FOG_SITES[2]) ~= entries[2] then
        for _, site in ipairs(FOG_SITES) do writeCode(site.va, site.from) end
        log("[fixes] refusing the fog restore: a hook did not stick; both sites put back", "warn")
        return false
    end
    fogRestore = {entries = entries}
    log(string.format("[fixes] 0x%X, 0x%X fog restore: FOGENABLE, FOGCOLOR, FOGTABLEMODE and FOGVERTEXMODE " ..
        "re-sent twice a frame (code at 0x%X)", FOG_SITES[1].va, FOG_SITES[2].va, fogBlock), "info")
    return true
end

-- Off: each site that jumps to an entry of ours gets its bytes back.
function stopFogRestore()
    local changed = false
    for _, site in ipairs(FOG_SITES) do
        if fogEntryAt(site) then
            writeCode(site.va, site.from)
            changed = true
        end
    end
    if fogRestore and fogRestore.entries then fogRestore = nil end
    return changed
end

--------------------------------------------------------------------------------
-- Entity positions (gta_sa.exe)
--------------------------------------------------------------------------------
-- A position that is NaN, infinite or far off the map (a server's teleport, a
-- player's sync, a script) sends the streamer and the world's sector maths out
-- of range: a loading screen that never ends, the screen washed white. Every
-- moved ped, car and object passes CPhysical::Add (0x544A30, what CWorld::Add
-- calls after a teleport) or CPhysical::RemoveAndAdd (0x542560, each frame it
-- moves), so both start with a check: x and y within 20000, z from -10000 to
-- 100000, by the float's bits (one unsigned compare also catches NaN and inf).
-- A bad position on the player's ped or its vehicle takes the last good one
-- the check saw for it, so a bad teleport does not happen; on anything else
-- it takes the RenderWare frame's (the last one drawn), else 0, 0, 0. The
-- speeds are zeroed; NaN or inf speeds alone are zeroed too. Each is counted. SA-MP's version, other mods and the RPC path do not matter.
-- Option "positionguard".
local PG = {
    XY_MAX = 0x469C4000,           -- 20000.0
    Z_MAX  = 0x47C35000,           -- 100000.0
    Z_MIN  = 0x461C4000,           -- 10000.0, for -10000
    SITES = {
        {va = 0x544A30, from = hexBytes("64 A1 00 00 00 00")},  -- CPhysical::Add: mov eax, fs:[0]
        {va = 0x542560, from = hexBytes("83 EC 2C 55 8B E9")},  -- RemoveAndAdd: sub esp,2Ch / push ebp / mov ebp,ecx
    },
    BLOCK_SIZE = 0x200,            -- counters and two position slots, then the code at ROUTINE
    ROUTINE = 0x30,
    PLAYER = 0xB7CD98,             -- CWorld::Players[0].m_pPed
    block = nil,                   -- our code, kept for the next time the option goes on
    seen = {0, 0},
}
local posGuard                     -- {entries = {va, va}} once ours, {owner = text} if another's

-- The routine (ecx = the entity; every register and the flags kept), then an
-- entry per site: call it, the site's bytes, jmp back. The block: positions put
-- back, speeds zeroed, then two slots {entity, x, y, z} holding the last good
-- position seen for the player's ped ([player]) and its vehicle (+58Ch).
function PG.code(block, player)
    player = player or PG.PLAYER
    local code, labels, fixups = {}, {}, {}
    local function emit(hex, dword)
        for _, b in ipairs(hexBytes(hex)) do code[#code + 1] = b end
        if dword then for _, b in ipairs(u32bytes(dword % 0x100000000)) do code[#code + 1] = b end end
    end
    local function label(name) labels[name] = #code end
    local function jump(hex, name, wide)       -- a jcc/jmp/call opcode, then its displacement
        assert(not wide or hex == "E8" or hex == "E9" or hex:match("^0F 8"), "PG.code: a short opcode with a wide jump")
        emit(hex)
        fixups[#fixups + 1] = {at = #code, name = name, wide = wide}
        for _ = 1, wide and 4 or 1 do code[#code + 1] = 0 end
    end
    local POSITIONS, SPEEDS = block, block + 4
    local PED, PED_POS, CAR, CAR_POS = block + 8, block + 0x0C, block + 0x18, block + 0x1C
    for _ = 1, PG.ROUTINE do code[#code + 1] = 0 end
    emit("9C 60 8B F1")                    -- pushfd / pushad / mov esi, ecx
    emit("8B 7E 14 85 FF")                 -- mov edi, [esi+14h] (m_matrix) / test edi, edi
    jump("74", "placement")
    emit("83 C7 30")                       -- add edi, 30h: the matrix's position
    jump("EB", "check")
    label("placement")
    emit("8D 7E 04")                       -- lea edi, [esi+4]: m_placement's
    label("check")
    emit("8B D7")                          -- mov edx, edi
    jump("E8", "valid", true)
    emit("85 C0")
    jump("0F 85", "bad", true)
    -- good: the player's ped or vehicle keeps it as the last good one
    emit("A1", player)                     -- mov eax, [player]
    emit("85 C0")
    jump("0F 84", "speeds", true)
    emit("3B F0")                          -- cmp esi, eax
    jump("75", "car")
    emit("89 35", PED)                     -- mov [ped], esi
    emit("BA", PED_POS)                    -- mov edx, ped's x, y, z
    jump("EB", "keep")
    label("car")
    emit("3B B0 8C 05 00 00")              -- cmp esi, [eax+58Ch] (m_pVehicle)
    jump("0F 85", "speeds", true)
    emit("89 35", CAR)                     -- mov [car], esi
    emit("BA", CAR_POS)                    -- mov edx, car's x, y, z
    label("keep")
    emit("8B 07 89 02 8B 47 04 89 42 04 8B 47 08 89 42 08")   -- x, y, z into the slot
    jump("E9", "speeds", true)
    label("bad")
    emit("FF 05", POSITIONS)               -- inc dword [positions]
    emit("BA", PED_POS)
    emit("3B 35", PED)                     -- cmp esi, [ped]: put its last good one back
    jump("74", "copy")
    emit("BA", CAR_POS)
    emit("3B 35", CAR)
    jump("74", "copy")
    emit("8B 46 18 85 C0")                 -- mov eax, [esi+18h] (m_pRwObject) / test eax, eax
    jump("74", "zero")
    emit("8B 40 04 85 C0")                 -- mov eax, [eax+4] (its frame) / test eax, eax
    jump("74", "zero")
    emit("8D 50 40")                       -- lea edx, [eax+40h]: the frame's position
    jump("E8", "valid", true)
    emit("85 C0")
    jump("75", "zero")
    label("copy")
    emit("8B 02 89 07 8B 42 04 89 47 04 8B 42 08 89 47 08")   -- x, y, z from edx
    jump("EB", "stop")
    label("zero")
    emit("31 C0 89 07 89 47 04 89 47 08")  -- 0, 0, 0
    label("stop")
    emit("31 C0 89 46 44 89 46 48 89 46 4C 89 46 50 89 46 54 89 46 58")   -- move and turn speed 0
    jump("EB", "done")
    label("speeds")
    emit("B9 06 00 00 00 8D 56 44")        -- mov ecx, 6 / lea edx, [esi+44h]
    label("next")
    emit("8B 02 25 00 00 80 7F 3D 00 00 80 7F")   -- the exponent all ones: NaN or inf
    jump("74", "badspeed")
    emit("83 C2 04 49")                    -- add edx, 4 / dec ecx
    jump("75", "next")
    jump("EB", "done")
    label("badspeed")
    emit("FF 05", SPEEDS)                  -- inc dword [speeds]
    jump("EB", "stop")
    label("done")
    emit("61 9D C3")                       -- popad / popfd / ret
    -- edx -> x, y, z: eax 0 when all three are in range, else 1 (only eax changes)
    label("valid")
    emit("8B 02 25 FF FF FF 7F 3D", PG.XY_MAX)
    jump("77", "invalid")
    emit("8B 42 04 25 FF FF FF 7F 3D", PG.XY_MAX)
    jump("77", "invalid")
    emit("8B 42 08 85 C0")
    jump("78", "below")
    emit("3D", PG.Z_MAX)
    jump("77", "invalid")
    emit("31 C0 C3")
    label("below")
    emit("25 FF FF FF 7F 3D", PG.Z_MIN)
    jump("77", "invalid")
    emit("31 C0 C3")
    label("invalid")
    emit("B8 01 00 00 00 C3")
    for _, f in ipairs(fixups) do
        local rel = labels[f.name] - (f.at + (f.wide and 4 or 1))
        if f.wide then
            for i, b in ipairs(u32bytes(rel % 0x100000000)) do code[f.at + i] = b end
        else
            assert(rel >= -128 and rel <= 127, "PG.code: short jump out of range")
            code[f.at + 1] = rel % 0x100
        end
    end
    local routine = block + PG.ROUTINE
    local entries = {}
    for i, site in ipairs(PG.SITES) do
        entries[i] = block + #code
        emit("E8", routine - (entries[i] + 5))
        for _, b in ipairs(site.from) do code[#code + 1] = b end
        emit("E9", site.va + #site.from - (block + #code + 5))
    end
    return code, entries
end

-- (fogEntryAt reads any site hooked this way: jmp to an entry that calls the
-- routine, then holds the site's own bytes.)
function fixPositionGuard()
    if posGuard and posGuard.entries then return true end
    local earlier = {fogEntryAt(PG.SITES[1]), fogEntryAt(PG.SITES[2])}
    if earlier[1] and earlier[2] then
        posGuard = {entries = earlier, block = earlier[1] - earlier[1] % 0x10000}   -- VirtualAlloc's 64K base
        log(string.format("[fixes] 0x%X, 0x%X position guard: already hooked (an earlier run's)", PG.SITES[1].va,
            PG.SITES[2].va), "info")
        return true
    end
    for _, site in ipairs(PG.SITES) do                 -- both stock before either is written
        local found = readBytes(site.va, #site.from)
        if not bytesEqual(found, site.from) then
            local owner = found and hookOwner(site.va, found)
            if owner then
                posGuard = {owner = owner}
                log(string.format("[fixes] 0x%X position guard: hooked by %s; standing down", site.va, owner), "info")
            else
                log(string.format("[fixes] refusing the position guard: 0x%X is %s, expected %s", site.va,
                    bytesToHex(found), bytesToHex(site.from)), "warn")
            end
            return false
        end
    end
    if not PG.block then
        local p = ffi.C.VirtualAlloc(nil, PG.BLOCK_SIZE, 0x3000, 0x40)   -- MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE
        if p == nil then
            log("[fixes] refusing the position guard: VirtualAlloc failed")
            return false
        end
        PG.block = tonumber(ffi.cast("uintptr_t", p))
    end
    local code, entries = PG.code(PG.block)
    poke(PG.block, code)
    for i, site in ipairs(PG.SITES) do
        local jmp = {0xE9, unpack(u32bytes((entries[i] - (site.va + 5)) % 0x100000000))}
        for _ = 6, #site.from do jmp[#jmp + 1] = 0x90 end
        writeCode(site.va, jmp)
    end
    if fogEntryAt(PG.SITES[1]) ~= entries[1] or fogEntryAt(PG.SITES[2]) ~= entries[2] then
        for _, site in ipairs(PG.SITES) do writeCode(site.va, site.from) end
        log("[fixes] refusing the position guard: a hook did not stick; both sites put back", "warn")
        return false
    end
    posGuard = {entries = entries, block = PG.block}
    log(string.format("[fixes] 0x%X, 0x%X position guard: NaN, inf and off-map positions put back before a ped, " ..
        "car or object is placed (code at 0x%X)", PG.SITES[1].va, PG.SITES[2].va, PG.block), "info")
    return true
end

-- Off: each site that jumps to an entry of ours gets its bytes back.
function stopPositionGuard()
    local changed = false
    for _, site in ipairs(PG.SITES) do
        if fogEntryAt(site) then
            writeCode(site.va, site.from)
            changed = true
        end
    end
    if posGuard and posGuard.entries then posGuard = nil end
    return changed
end

-- A line when the guard has put more back; from the main loop.
function PG.tick()
    if not (posGuard and posGuard.block) then return end
    local positions, speeds = readU32(posGuard.block), readU32(posGuard.block + 4)
    if positions == PG.seen[1] and speeds == PG.seen[2] then return end
    PG.seen = {positions, speeds}
    log(string.format("[fixes] position guard: %d bad position(s) put back, %d bad speed(s) zeroed so far",
        positions, speeds), "warn")
end

--------------------------------------------------------------------------------
-- Frame-rate fixes (gta_sa.exe)
--------------------------------------------------------------------------------
-- Physics the game applies once per step without the timestep, tuned at 30
-- fps (a 1.6667 timestep). Each site becomes a call (or a jmp) to a stub in
-- one shared block that scales by timestep / 1.6667, so 30 fps is stock.
--   wheelfps  CVehicle::ProcessWheel and ProcessBikeWheel read
--             gHandlingDataMgr.fWheelFriction (0xC2B9CC) as the coasting
--             brake: a fixed slow-down per step, so off the throttle a car
--             stops dead at high fps (Junior_Djjr's Car Speed FPS Fix).
--   swimfps   CTaskSimpleSwim::ProcessSwimmingResistance blends toward a
--             target built from the animation's per-frame movement: slow
--             swimming at high fps. The target is scaled by 1.6667 /
--             timestep; its other (vertical) branch keeps stock by scaling
--             its -0.1 the other way (Swim FPS Fix.asi).
--   floatfps  cBuoyancy::CalcBuoyancyForce's timestep becomes t * (t / 1.5
--             + 1), t = timestep / 1.6667: less lift above 30 fps, which
--             keeps a swimmer at the surface. Everything that floats, boats
--             too (Swim FPS Fix.asi).
-- A stub starts with a game-address instruction, which is how a reload knows
-- its own hooks. Options "wheelfps", "swimfps", "floatfps".
local FPS = {
    TIMESTEP = 0xB7CB5C,           -- CTimer::ms_fTimeStep
    SITES = {
        {va = 0x6D6E69, option = "wheelfps", stub = "wheel", from = hexBytes("D9 05 CC B9 C2 00")},
        {va = 0x6D6EA8, option = "wheelfps", stub = "wheel", from = hexBytes("D9 05 CC B9 C2 00")},
        {va = 0x6D767F, option = "wheelfps", stub = "wheel", from = hexBytes("D9 05 CC B9 C2 00")},
        {va = 0x6D76AB, option = "wheelfps", stub = "wheel", from = hexBytes("D9 05 CC B9 C2 00")},
        {va = 0x6D76CD, option = "wheelfps", stub = "wheel", from = hexBytes("D9 05 CC B9 C2 00")},
        -- fstp [esp+1Ch] / fld [esp+14h] / fmul st,st1 / fstp [esp+20h] / fmul [esp+18h]: jmp, not call
        {va = 0x68A4FC, option = "swimfps", stub = "swim", jmp = true,
         from = hexBytes("D9 5C 24 1C D9 44 24 14 D8 C9 D9 5C 24 20 D8 4C 24 18")},
        {va = 0x68A42B, option = "swimfps", stub = "dive", from = hexBytes("D8 0D F4 8E 85 00")},   -- fmul [-0.1]
        {va = 0x6C27AE, option = "floatfps", stub = "float", from = hexBytes("D8 0D 5C CB B7 00")},  -- fmul [timestep]
    },
    block = nil,
    stubs = nil,                   -- name -> va, once the block is written
    owner = {},                    -- option -> another mod's name, when it stood down
}

-- The block: four constants, then each stub. -> bytes, {name = va}. `game`
-- moves the game globals the stubs read (the tests' harness).
function FPS.code(block, game)
    game = game or {timestep = FPS.TIMESTEP, friction = 0xC2B9CC, minus = 0x858EF4, back = 0x68A50E}
    local code, at = {}, {}
    local function emit(hex, dword)
        for _, b in ipairs(hexBytes(hex)) do code[#code + 1] = b end
        if dword then for _, b in ipairs(u32bytes(dword % 0x100000000)) do code[#code + 1] = b end end
    end
    local K30, INV30, K15, ONE = block, block + 4, block + 8, block + 12
    emit("55 55 D5 3F 9A 99 19 3F 00 00 C0 3F 00 00 80 3F")    -- 1.6666666, 0.6, 1.5, 1.0
    at.wheel = block + #code
    emit("D9 05", game.friction)           -- fld [fWheelFriction]
    emit("D8 0D", game.timestep)           -- fmul [timestep]
    emit("D8 0D", INV30)                   -- fmul [0.6]
    emit("C3")
    at.swim = block + #code
    emit("D9 05", game.timestep)           -- fld [timestep]
    emit("D8 3D", K30)                     -- fdivr [1.6667]: k = 1.6667 / timestep; x*blend, blend, pow under it
    emit("DC C9 D9 C9")                    -- fmul st(1), st / fxch: x*blend*k on top, k under it
    emit("D9 5C 24 1C")                    -- fstp [esp+1Ch]
    emit("D9 44 24 14 D8 CA D8 C9")        -- fld [esp+14h] / fmul st, st(2) (blend) / fmul st, st(1) (k)
    emit("D9 5C 24 20")                    -- fstp [esp+20h]
    emit("DE C9 D8 4C 24 18")              -- fmulp st(1), st: blend*k / fmul [esp+18h]
    emit("E9", game.back - (block + #code + 5))
    at.dive = block + #code
    emit("D8 0D", game.minus)              -- fmul [-0.1]
    emit("D8 0D", game.timestep)           -- fmul [timestep]
    emit("D8 0D", INV30)                   -- fmul [0.6]: the swim stub's k cancels it
    emit("C3")
    at.float = block + #code
    emit("D9 05", game.timestep)           -- fld [timestep]
    emit("D8 0D", INV30)                   -- fmul [0.6]: t
    emit("D9 C0 D8 35", K15)               -- fld st(0) / fdiv [1.5]
    emit("D8 05", ONE)                     -- fadd [1.0]
    emit("DE C9 DE C9 C3")                 -- fmulp: t * (t/1.5 + 1) / fmulp: times the buoyancy / ret
    at.cleoEnd = block + #code
    emit("93 0A")                          -- 0A93 end_custom_script, for a CLEO script this ends
    return code, at
end

-- The stub head a site of ours calls or jumps to, or nil.
local function fpsHookTarget(site)
    local b = readBytes(site.va, 5)
    if not b or b[1] ~= (site.jmp and 0xE9 or 0xE8) then return nil end
    local at = (site.va + 5 + bytesU32({b[2], b[3], b[4], b[5]})) % 0x100000000
    if moduleAt(at) then return nil end
    local head = readBytes(at, 6)
    local want = ({wheel = "D9 05 CC B9 C2 00", swim = "D9 05 5C CB B7 00", dive = "D8 0D F4 8E 85 00",
                   float = "D9 05 5C CB B7 00"})[site.stub]
    return head and bytesEqual(head, hexBytes(want)) and at or nil
end

-- -> sites in place (0 when it stood down or refused)
function fixFrameRate(option)
    local mine = {}
    for _, site in ipairs(FPS.SITES) do if site.option == option then mine[#mine + 1] = site end end
    local todo = {}
    for _, site in ipairs(mine) do
        local adopted = fpsHookTarget(site)
        if adopted and not FPS.block then                -- an earlier run's: VirtualAlloc's 64K base
            FPS.block = adopted - adopted % 0x10000
            local _
            _, FPS.stubs = FPS.code(FPS.block)
        end
        if not adopted then
            local found = readBytes(site.va, #site.from)
            if not bytesEqual(found, site.from) then
                local owner = found and hookOwner(site.va, found)
                FPS.owner[option] = owner
                if owner then
                    log(string.format("[fixes] 0x%X %s: hooked by %s; standing down", site.va, option, owner), "info")
                else
                    log(string.format("[fixes] refusing %s: 0x%X is %s, expected %s", option, site.va,
                        bytesToHex(found), bytesToHex(site.from)), "warn")
                end
                return 0
            end
            todo[#todo + 1] = site
        end
    end
    if #todo == 0 then return #mine end
    if not FPS.block then
        local p = ffi.C.VirtualAlloc(nil, 0x100, 0x3000, 0x40)   -- MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE
        if p == nil then
            log("[fixes] refusing " .. option .. ": VirtualAlloc failed")
            return 0
        end
        FPS.block = tonumber(ffi.cast("uintptr_t", p))
        local code
        code, FPS.stubs = FPS.code(FPS.block)
        poke(FPS.block, code)
    end
    for _, site in ipairs(todo) do
        local to = FPS.stubs[site.stub]
        local bytes = {site.jmp and 0xE9 or 0xE8, unpack(u32bytes((to - (site.va + 5)) % 0x100000000))}
        for _ = 6, #site.from do bytes[#bytes + 1] = 0x90 end
        writeCode(site.va, bytes)
    end
    log(string.format("[fixes] %s: %d site(s) scale by the timestep (code at 0x%X)", option, #mine, FPS.block), "info")
    return #mine
end

-- Off: each site of ours gets its bytes back. -> sites changed
function stopFrameRate(option)
    local changed = 0
    for _, site in ipairs(FPS.SITES) do
        if site.option == option and fpsHookTarget(site) then
            writeCode(site.va, site.from)
            changed = changed + 1
        end
    end
    return changed
end

--------------------------------------------------------------------------------
-- Animation wipes (gta_sa.exe)
--------------------------------------------------------------------------------
-- RpAnimBlendClumpRemoveAllAssociations (0x4D6C00) deletes every animation on a
-- clump (a model change - SA-MP's skin change - a ped going away,
-- CPlayerPed::SetInitialState). A task playing an animation keeps a pointer and
-- registers a delete callback to clear it, but SA's CAnimBlendAssociation
-- destructor never runs that callback (only UpdateBlend does, for a faded-out
-- animation), so the task writes through freed memory later. Seen as a car's
-- material list overwritten by CTaskSimpleInAir::MakeAbortable after a
-- reconnect, crashing at gta_sa.exe+0x41214A.
--
-- The function becomes a jmp to the same loop plus: before an animation is
-- deleted, its delete callback runs - only gta_sa.exe's own (AW_CALLBACKS),
-- each of which only clears a pointer or sets a flag; a mod's callback and all
-- finish callbacks stay stock. Every task resets its callback in its
-- destructor, so a registered one names a live task. The whole stock function
-- must match first (it also pins the 1.0 US exe).
local AW_VA     = 0x4D6C00     -- RpAnimBlendClumpRemoveAllAssociations
local AW_OFFSET = 0xB5F878     -- the CAnimBlendClumpData plugin offset it reads
local AW_MAGIC  = 0x77417846   -- "FxAw", the block's first dword
local AW_CODE   = 0x10         -- the block: header {magic, told, entries, list},
local AW_LIST   = 0x80         -- then the code, then the callbacks
local AW_SIZE   = 0x100

local AW_STOCK = hexBytes(
    "8B 44 24 04 8B 0D 78 F8 B5 00 8B 04 01 8B 00 85 C0 74 17 56 8B 30 8D 48 FC " ..
    "85 C9 74 06 8B 11 6A 01 FF 12 85 F6 8B C6 75 EB 5E C3")

-- gta_sa.exe 1.0 US's delete callbacks: every SetDeleteCallback (0x4CEBC0) site
-- but the 47 passing CDefaultAnimCallback::DefaultAnimCB (a bare ret) and one
-- passing a register. Each is 3-27 instructions without a call.
local AW_CALLBACKS = {
    0x678E60,   -- CTaskSimpleInAir::DeleteAnimCB (clears m_pAnim if it is this one)
    0x692550,   -- CTaskSimpleDuck: clears the duck or move animation, by anim id
    0x67A380,   -- CTaskSimpleClimb::DeleteAnimCB
    0x61A8A0,   -- CTaskSimpleAnim::FinishRunAnimCB (finished, m_pAnim = 0)
    0x61DAE0,   -- CTaskSimpleFight (clears whichever of its two it is)
    0x621BE0,   -- registered at 0x627C1E (clears +2Ch if it is this one)
    0x68B7D0,   -- CTaskSimpleArrestPed::FinishAnimArrestPedCB
    0x690C60,   -- CTaskSimpleShakeFist::FinishAnimShakeFistCB
    0x5DFD70,   -- the ped's own: restores its heading rate from its stats
}

local animWipe                     -- {told = va} once in place, {owner = name} if another mod's

-- The replacement: the same walk over the clump's animations (cdecl, the clump
-- at [esp+4]), plus the callback. Bytes from i686-w64-mingw32-as;
-- lib/fixes/test/anim_wipe_test.c runs them.
local function animWipeCode(offsetVa, listVa, entriesVa, toldVa)
    local out = {}
    local function emit(hex, dword)
        for _, b in ipairs(hexBytes(hex)) do out[#out + 1] = b end
        if dword then for _, b in ipairs(u32bytes(dword)) do out[#out + 1] = b end end
    end
    emit("8B 44 24 04")           -- mov  eax, [esp+4]       ; the clump
    emit("8B 0D", offsetVa)       -- mov  ecx, [offset]
    emit("8B 04 01")              -- mov  eax, [ecx+eax]     ; its CAnimBlendClumpData
    emit("8B 00")                 -- mov  eax, [eax]         ; the first link
    emit("85 C0")                 -- test eax, eax
    emit("74 4A")                 -- jz   done
    emit("56 57 53")              -- push esi / push edi / push ebx
                                  -- next:
    emit("8B 30")                 -- mov  esi, [eax]         ; the next link, read before anything runs
    emit("8D 78 FC")              -- lea  edi, [eax-4]       ; the animation
    emit("83 7F 30 02")           -- cmp  dword [edi+30h], 2 ; a delete callback?
    emit("75 2B")                 -- jne  kill
    emit("8B 4F 34")              -- mov  ecx, [edi+34h]     ; which one
    emit("BA", listVa)            -- mov  edx, list
    emit("8B 1D", entriesVa)      -- mov  ebx, [entries]
                                  -- scan:
    emit("85 DB")                 -- test ebx, ebx
    emit("74 19")                 -- jz   kill               ; not one of the game's: as stock
    emit("3B 0A")                 -- cmp  ecx, [edx]
    emit("74 06")                 -- je   tell
    emit("83 C2 04")              -- add  edx, 4
    emit("4B")                    -- dec  ebx
    emit("EB F2")                 -- jmp  scan
                                  -- tell:
    emit("FF 77 38")              -- push dword [edi+38h]    ; its data, the task
    emit("57")                    -- push edi
    emit("FF D1")                 -- call ecx                ; cdecl, as UpdateBlend calls it
    emit("83 C4 08")              -- add  esp, 8
    emit("FF 05", toldVa)         -- inc  dword [told]
                                  -- kill:
    emit("89 F9")                 -- mov  ecx, edi
    emit("8B 11")                 -- mov  edx, [ecx]
    emit("6A 01")                 -- push 1
    emit("FF 12")                 -- call [edx]              ; the scalar deleting destructor, as stock
    emit("89 F0")                 -- mov  eax, esi
    emit("85 C0")                 -- test eax, eax
    emit("75 BC")                 -- jnz  next
    emit("5B 5F 5E")              -- pop  ebx / pop edi / pop esi
                                  -- done:
    emit("C3")                    -- ret
    return out
end

function fixAnimationWipe()
    local head = readBytes(AW_VA, 5)
    if head and head[1] == 0xE9 then
        -- ours from an earlier run (a reload), or another mod's hook
        local block = (AW_VA + 5 + bytesU32({head[2], head[3], head[4], head[5]}) - AW_CODE) % 0x100000000
        local h = readBytes(block, 16)
        if h and bytesU32({h[1], h[2], h[3], h[4]}) == AW_MAGIC then
            animWipe = {told = block + 4}
            log(string.format("[fixes] animation wipes: already patched (block 0x%X, %d task callbacks run so far)",
                block, readU32(block + 4)))
            return true
        end
        animWipe = {owner = hookedBy(AW_VA)}
        log(string.format("[fixes] animation wipes: RpAnimBlendClumpRemoveAllAssociations (0x%X) is replaced by %s; " ..
            "standing down", AW_VA, animWipe.owner))
        return false
    end

    local found = readBytes(AW_VA, #AW_STOCK)
    if not bytesEqual(found, AW_STOCK) then
        log(string.format("[fixes] refusing the animation-wipe fix: 0x%X holds %s, expected %s",
            AW_VA, bytesToHex(found), bytesToHex(AW_STOCK)))
        return false
    end

    local p = ffi.C.VirtualAlloc(nil, AW_SIZE, 0x3000, 0x40)   -- MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE
    if p == nil then
        log(string.format("[fixes] refusing the animation-wipe fix: VirtualAlloc of %d bytes failed", AW_SIZE))
        return false
    end
    local block = tonumber(ffi.cast("uintptr_t", p))
    local header = {}
    for _, v in ipairs({AW_MAGIC, 0, #AW_CALLBACKS, block + AW_LIST}) do
        for _, b in ipairs(u32bytes(v)) do header[#header + 1] = b end
    end
    poke(block, header)
    local list = {}
    for _, cb in ipairs(AW_CALLBACKS) do
        for _, b in ipairs(u32bytes(cb)) do list[#list + 1] = b end
    end
    poke(block + AW_LIST, list)
    poke(block + AW_CODE, animWipeCode(AW_OFFSET, block + AW_LIST, block + 8, block + 4))

    writeCode(AW_VA, {0xE9, unpack(u32bytes(block + AW_CODE - (AW_VA + 5)))})
    local verified = readU8(AW_VA) == 0xE9 and (AW_VA + 5 + readU32(AW_VA + 1)) % 0x100000000 == block + AW_CODE
    if verified then animWipe = {told = block + 4} end
    log(string.format("[fixes] animation wipes: RpAnimBlendClumpRemoveAllAssociations now runs the game's %d " ..
        "task delete callbacks before deleting (block 0x%X) -- %s",
        #AW_CALLBACKS, block, verified and "jmp written" or "WRITE DID NOT STICK"))
    return verified
end

--------------------------------------------------------------------------------
-- Entity references
--------------------------------------------------------------------------------
-- CReferences::aRefs holds 3000. When the free list is empty, RegisterReference
-- prunes every ped, vehicle and object; if that frees nothing, the reference is
-- dropped and its holder is never told the entity is gone. The head load at
-- 0x571BB3 calls a block that hands over a linked reserve of 4096 when the list
-- is empty; the main loop keeps one reserve ready (64 at most) and prunes a
-- slice of the pools each frame. A port of RefPoolFix by Софтик Инквизитор
-- (blast.hk), with his permission.
local REF_VA       = 0x571BB3       -- mov eax,[CReferences::pEmptyList], in RegisterReference
local REF_HEAD     = 0xBA1768       -- CReferences::pEmptyList
local REF_PRUNE    = 0x571A90       -- CEntity::PruneReferences (thiscall)
local REF_MAGIC    = 0x66527846     -- "FxRf", the block's first dword
local REF_CODE     = 0x10           -- the block: header {magic, ready, used, made}, then the code
local REF_SIZE     = 0x40
local REF_PER      = 4096           -- references in a reserve (8 bytes each)
local REF_MAX      = 64             -- reserves at most
local REF_SLICES   = 64             -- one slice of the pools pruned per frame
local REF_STOCK    = hexBytes("A1 68 17 BA 00")
local REF_POOLS    = {              -- CPool pointer, entity size
    {0xB74490, 0x7C4},              -- peds
    {0xB74494, 0xA18},              -- vehicles
    {0xB7449C, 0x19C},              -- objects
}

local refPool                       -- {ready, used, made} vas once in place, {owner = name} if another mod's
local refCursor = 0

-- Bytes from i686-w64-mingw32-as.
local function refPoolCode(readyVa, usedVa)
    local out = {}
    local function emit(hex, dword)
        for _, b in ipairs(hexBytes(hex)) do out[#out + 1] = b end
        if dword then for _, b in ipairs(u32bytes(dword)) do out[#out + 1] = b end end
    end
    emit("A1", REF_HEAD)          -- mov  eax, [pEmptyList]
    emit("85 C0")                 -- test eax, eax
    emit("75 1E")                 -- jnz  done
    emit("A1", readyVa)           -- mov  eax, [ready]       ; a linked reserve, or 0
    emit("85 C0")                 -- test eax, eax
    emit("74 15")                 -- jz   done               ; none: as stock (prune every pool)
    emit("A3", REF_HEAD)          -- mov  [pEmptyList], eax
    emit("C7 05", readyVa)        -- mov  dword [ready], 0
    emit("00 00 00 00")
    emit("FF 05", usedVa)         -- inc  dword [used]
                                  -- done:
    emit("C3")                    -- ret                     ; eax = the head, as the mov left it
    return out
end

function fixReferencePool()
    local head = readBytes(REF_VA, 5)
    if head and head[1] == 0xE8 then
        local target = (REF_VA + 5 + bytesU32({head[2], head[3], head[4], head[5]})) % 0x100000000
        local block = target - REF_CODE
        if readU32(block) == REF_MAGIC then
            refPool = {ready = block + 4, used = block + 8, made = block + 12}
            log(string.format("[fixes] entity references: already patched (block 0x%X, %d reserves used)",
                block, readU32(block + 8)))
            return true
        end
        refPool = {owner = moduleAt(target) or string.format("code at 0x%X", target)}
        log(string.format("[fixes] entity references: the free-list read at 0x%X is hooked by %s; standing down",
            REF_VA, refPool.owner))
        return false
    end
    if not bytesEqual(head, REF_STOCK) then
        log(string.format("[fixes] refusing the entity-reference fix: 0x%X holds %s, expected %s",
            REF_VA, bytesToHex(head), bytesToHex(REF_STOCK)))
        return false
    end

    local p = ffi.C.VirtualAlloc(nil, REF_SIZE, 0x3000, 0x40)   -- MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE
    if p == nil then
        log(string.format("[fixes] refusing the entity-reference fix: VirtualAlloc of %d bytes failed", REF_SIZE))
        return false
    end
    local block = tonumber(ffi.cast("uintptr_t", p))
    local header = {}
    for _, v in ipairs({REF_MAGIC, 0, 0, 0}) do
        for _, b in ipairs(u32bytes(v)) do header[#header + 1] = b end
    end
    poke(block, header)
    poke(block + REF_CODE, refPoolCode(block + 4, block + 8))

    writeCode(REF_VA, {0xE8, unpack(u32bytes(block + REF_CODE - (REF_VA + 5)))})
    local verified = readU8(REF_VA) == 0xE8 and (REF_VA + 5 + readU32(REF_VA + 1)) % 0x100000000 == block + REF_CODE
    if verified then refPool = {ready = block + 4, used = block + 8, made = block + 12} end
    log(string.format("[fixes] entity references: past the game's 3000, reserves of %d (up to %d) " ..
        "(block 0x%X) -- %s", REF_PER, REF_MAX, block, verified and "call written" or "WRITE DID NOT STICK"))
    return verified
end

-- A linked reserve of REF_PER free references, or nil.
local function refPoolReserve()
    local p = ffi.C.VirtualAlloc(nil, REF_PER * 8, 0x3000, 0x04)   -- PAGE_READWRITE
    if p == nil then return nil end
    local base = tonumber(ffi.cast("uintptr_t", p))
    local refs = ffi.cast("uint32_t*", base)
    for i = 0, REF_PER - 2 do
        refs[2 * i] = base + 8 * (i + 1)
        refs[2 * i + 1] = 0
    end
    refs[2 * (REF_PER - 1)] = 0
    refs[2 * (REF_PER - 1) + 1] = 0
    return base
end

local pruneReferences

-- Each frame: a reserve ready if the last one went, and one slice of the pools
-- pruned so stale references go back to the free list before it runs dry.
function refPoolTick()
    if not refPool or refPool.owner then return end
    if readU32(refPool.ready) == 0 and readU32(refPool.made) < REF_MAX then
        local reserve = refPoolReserve()
        if reserve then
            ffi.cast("uint32_t*", refPool.made)[0] = readU32(refPool.made) + 1
            ffi.cast("uint32_t*", refPool.ready)[0] = reserve
        end
    end

    pruneReferences = pruneReferences or ffi.cast("void(__thiscall*)(uintptr_t)", REF_PRUNE)
    local total = 0
    for _, pool in ipairs(REF_POOLS) do
        local cp = readU32(pool[1])
        if cp ~= 0 then total = total + readU32(cp + 8) end
    end
    if total == 0 then return end
    local count = math.ceil(total / REF_SLICES)
    if refCursor >= total then refCursor = 0 end
    local base = 0
    for _, pool in ipairs(REF_POOLS) do
        local cp = readU32(pool[1])
        if cp ~= 0 then
            local objects, flags, size = readU32(cp), readU32(cp + 4), readU32(cp + 8)
            for i = math.max(refCursor - base, 0), math.min(refCursor + count - base, size) - 1 do
                if readU8(flags + i) < 0x80 then pruneReferences(objects + i * pool[2]) end
            end
            base = base + size
        end
    end
    refCursor = refCursor + count
end

--------------------------------------------------------------------------------
-- Text layouts
--------------------------------------------------------------------------------
-- lib/fixes/text_cache.dll (source in lib/fixes/native), TextCacheFix by Софтик
-- Инквизитор (blast.hk): d3dx9_25's ID3DXFont::DrawText replays strings it has
-- drawn before.
-- Option "textcache", on by default. The hooks live in the DLL, which is loaded
-- with a reference of fixes' own and never freed, so they outlive this script.
pcall(ffi.cdef, [[ void* LoadLibraryA(const char* name); ]])
pcall(ffi.cdef, [[
    typedef struct { uint32_t installed, hits, recorded, entries, mismatches, disabled; } FxTextCacheStats;
    int TextCache_Install(void* d3dx, uintptr_t* foreign);
    int TextCache_Uninstall(void);
    void TextCache_Stats(FxTextCacheStats* out);
]])
local TEXT_CACHE_DLL = getWorkingDirectory() .. "\\lib\\fixes\\text_cache.dll"
local D3DX_BUILD     = 0x423B781A

local textCacheLib
local textCacheState                -- {installed = true}, {owner = name} or {why = text}

local function loadTextCache()
    if textCacheLib then return textCacheLib end
    -- ffi.load's own reference goes when its clib is collected; the DLL holds live hooks
    local ok, handle = pcall(function() return tonumber(ffi.cast("uintptr_t", ffi.C.LoadLibraryA(TEXT_CACHE_DLL))) end)
    if not ok or not handle or handle == 0 then return nil, "cannot load " .. TEXT_CACHE_DLL end
    local okLib, lib = pcall(ffi.load, TEXT_CACHE_DLL)
    if not okLib then return nil, tostring(lib) end
    textCacheLib = lib
    return lib
end

function fixTextCache()
    if not options.textcache then return false end
    local lib, why = loadTextCache()
    if not lib then
        textCacheState = {why = why}
        log("[fixes] refusing the text cache: " .. why)
        return false
    end
    local base = getModuleHandle("d3dx9_25.dll")
    if not base or base == 0 then
        textCacheState = {why = "d3dx9_25.dll is not loaded"}
        log("[fixes] text cache: d3dx9_25.dll is not loaded, nothing to cache")
        return false
    end
    local foreign = ffi.new("uintptr_t[1]")
    local result = lib.TextCache_Install(ffi.cast("void*", base), foreign)
    if result == 1 then
        textCacheState = {installed = true}
        log("[fixes] text cache: d3dx9_25's DrawText now replays layouts it has drawn before")
        return true
    end
    if result == 2 then
        local at = tonumber(foreign[0])
        textCacheState = {owner = moduleAt(at) or string.format("code at 0x%X", at)}
        log(string.format("[fixes] text cache: d3dx9_25's font is hooked by %s; standing down", textCacheState.owner))
        return false
    end
    textCacheState = {why = string.format("d3dx9_25.dll is not build 0x%08X", D3DX_BUILD)}
    log("[fixes] refusing the text cache: " .. textCacheState.why)
    return false
end

-- Off: the stock slots go back, also when an earlier run of this script hooked them.
function stopTextCache()
    if not textCacheLib then
        local handle = getModuleHandle("text_cache.dll")
        if not handle or handle == 0 then return true end
        if not loadTextCache() then return false end
    end
    local all = textCacheLib.TextCache_Uninstall() == 1
    if textCacheState and textCacheState.installed then textCacheState = nil end
    return all
end

local function textCacheStats()
    if not textCacheLib then return nil end
    local st = ffi.new("FxTextCacheStats")
    textCacheLib.TextCache_Stats(st)
    return st
end

--------------------------------------------------------------------------------
-- Crash fixes (lib/fixes/crashfix.dll)
--------------------------------------------------------------------------------
-- MTA:SA's crash-fix hooks (GPLv3), as gtasa_crashfix (crashes.asi 2.x by
-- Whitetigerswt, GPLv3) copied them and as MTA has them now, built as a DLL
-- from lib/fixes/native/crashfix.c. Loaded once and never freed; each hook is
-- installed by number, so one another mod already hooks (crashes.asi while it
-- is installed) stands down alone, and every one logs a line. Option
-- "crashfix"; the hooks in CRASHFIX_OPTION only with their own option too.
-- docs/MEMORY_FIXES.md 30, 34.
pcall(ffi.cdef, [[
    int CrashFix_Count(void);
    const char* CrashFix_Key(int i);
    const char* CrashFix_Name(int i);
    uint32_t CrashFix_Address(int i);
    int CrashFix_SetSampSite(uint32_t va);
    void CrashFix_SetShadows(int volumetric);
    int CrashFix_Install(int i, uint32_t* found);
    int CrashFix_Uninstall(int i);
    int CrashFix_Installed(int i);
    uint32_t CrashFix_Averted(int i);
]])
local CRASHFIX_DLL = getWorkingDirectory() .. "\\lib\\fixes\\crashfix.dll"
local CRASHFIX_SAMP = string.char(0x74, 0x12, 0x8B, 0x50, 0x10, 0x8B, 0x02)   -- je / mov edx,[eax+10h] / mov eax,[edx]

local crashFixLib
local crashFixes = {}              -- key -> {name, va, state, why, seen}, in DLL order

local function loadCrashFix()
    if crashFixLib then return crashFixLib end
    local ok, handle = pcall(function() return tonumber(ffi.cast("uintptr_t", ffi.C.LoadLibraryA(CRASHFIX_DLL))) end)
    if not ok or not handle or handle == 0 then return nil, "cannot load " .. CRASHFIX_DLL end
    local okLib, lib = pcall(ffi.load, CRASHFIX_DLL)
    if not okLib then return nil, tostring(lib) end
    crashFixLib = lib
    return lib
end

-- Off unless their option is on: 19 breaks SkyGFX; MTA no longer installs 12 and 17.
local CRASHFIX_OPTION = {misc19 = "frameclone", misc12 = "oldcrashfixes", misc17 = "oldcrashfixes"}

local function crashFixWanted(key)
    if options.crashfix == false then return false end
    if CRASHFIX_OPTION[key] then return options[CRASHFIX_OPTION[key]] == true end
    return true
end

-- part: "game" (gta_sa.exe, from the start) or "samp" (samp.dll's helper,
-- once SA-MP is up). -> hooks installed or removed now.
function fixCrashes(part)
    local lib, why = loadCrashFix()
    if not lib then
        log("[fixes] refusing the crash fixes: " .. why, "warn")
        return 0
    end
    lib.CrashFix_SetShadows(options.volshadows and 1 or 0)
    local changed = 0
    for i = 0, lib.CrashFix_Count() - 1 do
        local key = ffi.string(lib.CrashFix_Key(i))
        if (key == "samp") == (part == "samp") then
            local entry = crashFixes[key] or {name = ffi.string(lib.CrashFix_Name(i)), seen = 0}
            crashFixes[key] = entry
            if part == "samp" and lib.CrashFix_Installed(i) == 0 then
                local base = getModuleHandle("samp.dll")
                local site = base and base ~= 0 and findInSections(moduleSections(base), true, CRASHFIX_SAMP)
                if site then lib.CrashFix_SetSampSite(site) end
            end
            entry.va = tonumber(lib.CrashFix_Address(i))
            if not crashFixWanted(key) then
                if lib.CrashFix_Installed(i) == 1 and lib.CrashFix_Uninstall(i) == 1 then
                    changed = changed + 1
                    log(string.format("[fixes] crash fix %s (%s): removed", key, entry.name), "info")
                end
                entry.state, entry.why = "off", nil
            elseif entry.va == 0 then
                entry.state, entry.why = "refused", "its pattern is not in samp.dll"
                log(string.format("[fixes] refusing crash fix %s (%s): %s", key, entry.name, entry.why), "info")
            else
                local found = ffi.new("uint32_t[1]")
                local r = lib.CrashFix_Install(i, found)
                local at = tonumber(found[0])
                if r == 1 then
                    changed = changed + 1
                    entry.state, entry.why = "ok", nil
                    log(string.format("[fixes] 0x%X crash fix %s (%s): hooked", entry.va, key, entry.name), "info")
                elseif r == 2 then
                    entry.state, entry.why = "already", nil
                    log(string.format("[fixes] 0x%X crash fix %s (%s): already hooked", entry.va, key, entry.name), "info")
                elseif r == 3 then
                    entry.state, entry.why = "conflict", moduleAt(at) or string.format("code at 0x%X", at)
                    log(string.format("[fixes] 0x%X crash fix %s (%s) is owned by %s; standing down",
                        entry.va, key, entry.name, entry.why), "info")
                else
                    entry.state = "refused"
                    entry.why = r == 0 and ("found " .. bytesToHex(u32bytes(at)) .. " ...") or "unreadable"
                    log(string.format("[fixes] refusing 0x%X crash fix %s (%s): %s", entry.va, key, entry.name, entry.why),
                        "warn")
                end
            end
        end
    end
    return changed
end

function setCrashFixShadows(on)
    if not crashFixLib then return false end
    crashFixLib.CrashFix_SetShadows(on and 1 or 0)
    return true
end

-- -> in place, owned elsewhere, refused, off
local function crashFixCounts()
    local n = {ok = 0, owned = 0, refused = 0, off = 0}
    for _, e in pairs(crashFixes) do
        if e.state == "ok" or e.state == "already" then n.ok = n.ok + 1
        elseif e.state == "conflict" then n.owned = n.owned + 1
        elseif e.state == "off" then n.off = n.off + 1
        elseif e.state then n.refused = n.refused + 1 end
    end
    return n
end

-- The hooks' state as one of otherFixes' ("in place", "left to X", "off ...",
-- "not in place").
function crashFixState()
    if not crashFixLib then return options.crashfix == false and "off (option crashfix)" or "not in place" end
    local n, owners, seen = crashFixCounts(), {}, {}
    for _, e in pairs(crashFixes) do
        if e.state == "conflict" and not seen[e.why] then seen[e.why] = true; owners[#owners + 1] = e.why end
    end
    table.sort(owners)
    if n.refused > 0 then return "not in place" end
    if n.owned > 0 then return "left to " .. table.concat(owners, ", ") .. (" (%d of %d hooks)"):format(n.owned, n.ok + n.owned) end
    if n.ok == 0 then return "off (option crashfix)" end
    return "in place"
end

-- A line for each hook that averted crashes since its last line, at most one
-- a minute per hook (some fire every frame on a broken model); from the main
-- loop.
local CRASHFIX_QUIET = 60
local function crashFixTick(now)
    if not crashFixLib then return end
    now = now or os.clock()
    for i = 0, crashFixLib.CrashFix_Count() - 1 do
        local e = crashFixes[ffi.string(crashFixLib.CrashFix_Key(i))]
        local n = tonumber(crashFixLib.CrashFix_Averted(i))
        if e and n ~= e.seen and (not e.said or now - e.said >= CRASHFIX_QUIET) then
            e.seen, e.said = n, now
            log(string.format("[fixes] crash fix %s (%s) averted a crash (%d so far)",
                ffi.string(crashFixLib.CrashFix_Key(i)), e.name, n), "warn")
        end
    end
end

--------------------------------------------------------------------------------
-- Animation blocks
--------------------------------------------------------------------------------
-- Force-load every animation block and hold a reference, so the streamer never
-- evicts one mid-play (peds T-posing or sliding under SA-MP). CAnimBlock is 0x20
-- bytes: char Name[16]; bool IsLoaded; int16 RefCnt at +0x12. RefCnt is checked
-- first (AddAnimBlockRef only increments: a reload would add a second ref).
local ANIM_BLOCKS       = 0xB5D4A0   -- CAnimManager::ms_aAnimBlocks
local ANIM_BLOCK_SIZE   = 0x20
local ANIM_BLOCK_REFCNT = 0x12
local NUM_ANIM_BLOCKS   = 180

local function animBlockRefCount(id)
    local ok, refs = pcall(function()
        return ffi.cast("int16_t*", ANIM_BLOCKS + id * ANIM_BLOCK_SIZE + ANIM_BLOCK_REFCNT)[0]
    end)
    if not ok then return nil end
    return tonumber(refs)
end

-- 0x48C36B must still be `lea eax,[edi+RESOURCE_ID_IFP]` (8D 87 <disp32>): the id
-- base is read from it, following anything that moved the streaming ids.
local function animBlockModelIdBase()
    local ok, isLea = pcall(function()
        return readU8(0x48C36B) == 0x8D and readU8(0x48C36B + 1) == 0x87
    end)
    if not ok or not isLea then return nil end
    return memory.read(0x48C36B + 2, 4, true)
end

function pinAnimBlocks()
    local idBase = animBlockModelIdBase()
    if not idBase then
        log("[fixes] not pinning anim blocks: 0x48C36B is not `lea eax,[edi+imm32]` any more")
        return false
    end

    local count = memory.read(0xB4EA30, 4, true)   -- CAnimManager::ms_numAnimBlocks
    if not count or count < 1 or count > NUM_ANIM_BLOCKS then
        log(string.format("[fixes] not pinning anim blocks: ms_numAnimBlocks is %s", tostring(count)))
        return false
    end

    local AddAnimBlockRef = ffi.cast("AddAnimBlockRef_t", 0x4D3FB0)
    local pinned, held = 0, 0

    -- Block 0 ("ped") is always resident, so it starts at 1.
    for id = 1, count - 1 do
        local refs = animBlockRefCount(id)
        if refs == nil then
            log(string.format("[fixes] not pinning anim blocks: cannot read RefCnt of block %d", id))
            return false
        elseif refs > 0 then
            held = held + 1          -- already referenced, do not stack another one
        else
            requestModel(idBase + id)
            AddAnimBlockRef(id)
            pinned = pinned + 1
        end
    end

    if pinned > 0 then loadAllModelsNow() end
    log(string.format("[fixes] anim blocks (base id %d): %d pinned, %d already referenced",
        idBase, pinned, held))
    return true
end

--------------------------------------------------------------------------------
-- Server data SA-MP trusts (RPCs)
--------------------------------------------------------------------------------
-- Sizes SA-MP's handlers use unchecked: the RPC is dropped before it reaches
-- them, or a dialog's list lines are cut. The checks are MiTurboFix's (public
-- domain, github.com/povargek/MiTurboFix); the menu is read in SA-MP's own
-- layout, where the enabled flags are 4-byte BOOLs. Needs SAMPFUNCS, which
-- raises onReceiveRpc.
local RPC_SHOW_DIALOG, RPC_INIT_MENU, RPC_APPLY_ANIM, RPC_APPLY_ACTOR_ANIM = 61, 76, 86, 173
local ANIM_LIB_MAX, ANIM_NAME_MAX = 15, 24          -- an IFP block name, an animation name
local MENU_MAX, MENU_ITEMS, MENU_LINE = 128, 12, 32
local DIALOG_LINE_MAX = 128
local DIALOG_TABLIST = {[4] = true, [5] = true}     -- tablist, tablist with headers: cut per cell
local DIALOG_LIST = {[2] = true, [4] = true, [5] = true}
local RPC_LOGGED = 5                                -- lines logged per kind; the rest are counted
-- A teleport to NaN or far off the map puts the ped or vehicle where the
-- streamer and collision code fail. The limits are the world's absolute ones,
-- not SetWorldBounds': a server may teleport a player outside those.
local RPC_SET_PLAYER_POS, RPC_SET_PLAYER_POS_FIND_Z, RPC_SET_VEHICLE_POS = 12, 13, 159
local POS_XY_MAX, POS_Z_MIN, POS_Z_MAX = 20000.0, -10000.0, 100000.0

local rpcCounts = {anim = 0, menu = 0, dialog = 0, pos = 0}
local RPC_KIND = {[RPC_APPLY_ANIM] = "anim", [RPC_APPLY_ACTOR_ANIM] = "anim",
                  [RPC_INIT_MENU] = "menu", [RPC_SHOW_DIALOG] = "dialog",
                  [RPC_SET_PLAYER_POS] = "pos", [RPC_SET_PLAYER_POS_FIND_Z] = "pos",
                  [RPC_SET_VEHICLE_POS] = "pos"}

local function skipBytes(bs, n) if n > 0 then raknetBitStreamIgnoreBits(bs, n * 8) end end

-- nil, or what is wrong with the animation's names
local function checkAnimation(bs)
    raknetBitStreamIgnoreBits(bs, 16)                       -- player or actor id
    local lib = raknetBitStreamReadInt8(bs)
    if lib < 1 or lib > ANIM_LIB_MAX then return ("a library name of %d bytes"):format(lib) end
    skipBytes(bs, lib)
    local name = raknetBitStreamReadInt8(bs)
    if name < 1 or name > ANIM_NAME_MAX then return ("an animation name of %d bytes"):format(name) end
end

-- nil, or what is wrong with the position
local function checkPosition(bs)
    local x, y, z = raknetBitStreamReadFloat(bs), raknetBitStreamReadFloat(bs), raknetBitStreamReadFloat(bs)
    if x ~= x or y ~= y or z ~= z then return "a NaN position" end
    if math.abs(x) > POS_XY_MAX or math.abs(y) > POS_XY_MAX or z < POS_Z_MIN or z > POS_Z_MAX then
        return ("a position off the world (%.0f, %.0f, %.0f)"):format(x, y, z)
    end
end

-- a 32-byte menu line holding its terminator
local function menuLineOk(bs)
    local terminated = false
    for _ = 1, MENU_LINE do
        if raknetBitStreamReadInt8(bs) == 0 then terminated = true end
    end
    return terminated
end

-- nil, or what is wrong with the menu
local function checkMenu(bs)
    local id = raknetBitStreamReadInt8(bs)
    if id >= MENU_MAX then return ("menu id %d"):format(id) end
    local two = raknetBitStreamReadInt32(bs)
    if two ~= 0 and two ~= 1 then return ("a column flag of %d"):format(two) end
    if not menuLineOk(bs) then return "an unterminated title" end
    skipBytes(bs, 4 * (3 + two))                            -- x, y, the column widths
    skipBytes(bs, 4 * (1 + MENU_ITEMS))                     -- enabled, and each row's
    for column = 1, 1 + two do
        if not menuLineOk(bs) then return ("an unterminated header in column %d"):format(column) end
        local rows = raknetBitStreamReadInt8(bs)
        if rows > MENU_ITEMS then return ("%d rows in column %d"):format(rows, column) end
        for row = 1, rows do
            if not menuLineOk(bs) then return ("an unterminated row %d in column %d"):format(row, column) end
        end
    end
end

-- Cuts a list dialog's long lines (a tablist's long cells) in place. -> how
-- many were cut, and the longest, or nil.
local function cutDialogLines(bs)
    raknetBitStreamIgnoreBits(bs, 16)                       -- dialog id
    local style = raknetBitStreamReadInt8(bs)
    if not DIALOG_LIST[style] then return nil end
    for _ = 1, 3 do skipBytes(bs, raknetBitStreamReadInt8(bs)) end   -- title, two buttons
    local at = raknetBitStreamGetReadOffset(bs)
    local text = raknetBitStreamDecodeString(bs, 4096)
    local sep = DIALOG_TABLIST[style] and "\t" or "\n"
    local cut, longest, parts = 0, 0, {}
    for part in (text .. sep):gmatch("(.-)" .. sep) do
        if sep == "\t" then                                 -- a cell may end a line
            local cells = {}
            for cell in (part .. "\n"):gmatch("(.-)\n") do
                if #cell > DIALOG_LINE_MAX then
                    cut, longest = cut + 1, math.max(longest, #cell)
                    cell = cell:sub(1, DIALOG_LINE_MAX)
                end
                cells[#cells + 1] = cell
            end
            part = table.concat(cells, "\n")
        elseif #part > DIALOG_LINE_MAX then
            cut, longest = cut + 1, math.max(longest, #part)
            part = part:sub(1, DIALOG_LINE_MAX)
        end
        parts[#parts + 1] = part
    end
    if cut == 0 then return nil end
    raknetBitStreamSetWriteOffset(bs, at)
    raknetBitStreamEncodeString(bs, table.concat(parts, sep))
    return cut, longest
end

local function noteRpc(kind, text)
    rpcCounts[kind] = rpcCounts[kind] + 1
    if rpcCounts[kind] <= RPC_LOGGED then
        log("[fixes] " .. text .. (rpcCounts[kind] == RPC_LOGGED and " (further ones are only counted)" or ""), "warn")
    end
end

-- false drops the RPC
local function screenRpc(id, bs)
    if id == RPC_APPLY_ANIM or id == RPC_APPLY_ACTOR_ANIM then
        local why = checkAnimation(bs)
        if why then
            noteRpc("anim", ("dropped a server %s animation with %s"):format(
                id == RPC_APPLY_ANIM and "player" or "actor", why))
            return false
        end
    elseif id == RPC_INIT_MENU then
        local why = checkMenu(bs)
        if why then
            noteRpc("menu", "dropped a server menu with " .. why)
            return false
        end
    elseif (id == RPC_SET_PLAYER_POS or id == RPC_SET_PLAYER_POS_FIND_Z or id == RPC_SET_VEHICLE_POS)
            and options.rpcpositions ~= false then
        local vehicle = id == RPC_SET_VEHICLE_POS and raknetBitStreamReadInt16(bs)
        local why = checkPosition(bs)
        if why then
            noteRpc("pos", ("dropped a server teleport of %s to %s"):format(
                vehicle and ("vehicle " .. vehicle) or "the player", why))
            return false
        end
    elseif id == RPC_SHOW_DIALOG and options.dialoglines ~= false then
        local cut, longest = cutDialogLines(bs)
        if cut then
            noteRpc("dialog", ("cut %d dialog line(s) to %d characters (the longest had %d)"):format(
                cut, DIALOG_LINE_MAX, longest))
        end
    end
    return true
end

function onReceiveRpc(id, bs)
    if not RPC_KIND[id] then return end
    local start = raknetBitStreamGetReadOffset(bs)
    local ok, keep = pcall(screenRpc, id, bs)
    raknetBitStreamSetReadOffset(bs, start)
    if not ok then log("[fixes] RPC " .. id .. " check failed: " .. tostring(keep)) return end
    if keep == false then return false end
end

local function say(text)
    sampAddChatMessage("{9ACD32}[fixes]{FFFFFF} " .. text, -1)
end

-- The fixes that are not gamePatches entries, in the order main runs them:
-- {name, state}, state "in place", "left to <mod>", "off (option <key>)" or
-- "not in place".
local otherFixes = {}

local function noteFix(name, worked, owner)
    otherFixes[#otherFixes + 1] = {name, worked and "in place" or owner and ("left to " .. owner) or "not in place"}
end

-- Another copy of a fix fixes carries. Junior_Djjr's Car Speed FPS Fix is a
-- CLEO script that rewrites fWheelFriction every frame, which wheelfps would
-- then scale a second time: found in CTheScripts' active list (0xA8B42C;
-- next +0, base IP +10h, IP +14h) by its first instruction (0A8D, the
-- handling manager operand 0x5B7273) and a 0A8C write, its IP pointed at an
-- 0A93 so CLEO ends and frees it, and fWheelFriction put back to 0.9. Only
-- while wheelfps is ours. The ASIs a fix stood down for are named in chat.
FPS.CLEO_HEAD = hexBytes("8D 0A 01 73 72 5B 00")
FPS.DUPLICATES = {["swim fps fix.asi"] = "swimfps", ["gxt_crash_fix.asi"] = "the GXT string fix"}
FPS.told = {}

-- The .cs file in cleo\ holding the script, or nil.
function FPS.cleoFile()
    if type(findFirstFile) ~= "function" then return nil end
    local dir = getGameDirectory() .. "\\cleo\\"
    local handle, name = findFirstFile(dir .. "*.cs")
    local found
    while handle and name and not found do
        local f = io.open(dir .. name, "rb")
        local head = f and f:read(7)
        if f then f:close() end
        if head == string.char(unpack(FPS.CLEO_HEAD)) then found = name end
        name = findNextFile(handle)
    end
    if handle then findClose(handle) end
    return found
end

-- Ends the Car Speed FPS Fix script if it runs. -> true when it ended one
function FPS.endCleoScript()
    if not (FPS.stubs and FPS.stubs.cleoEnd) then return false end
    local wheel = FPS.SITES[1]
    if not fpsHookTarget(wheel) then return false end
    local script, ended, guard = readU32(0xA8B42C), false, 0
    while script ~= 0 and guard < 1000 do
        guard = guard + 1
        local base = readU32(script + 0x10)
        if base ~= 0 and bytesEqual(readBytes(base, 7), FPS.CLEO_HEAD) then
            local body, writes = readBytes(base, 96), false
            for i = 8, 95 do if body[i] == 0x8C and body[i + 1] == 0x0A then writes = true end end
            if writes and readU32(script + 0x14) ~= FPS.stubs.cleoEnd then
                writeCode(script + 0x14, u32bytes(FPS.stubs.cleoEnd))
                writeCode(0xC2B9CC, hexBytes("66 66 66 3F"))   -- fWheelFriction 0.9, as the game sets it
                ended = true
            end
        end
        script = readU32(script)
    end
    if ended then
        local file = FPS.cleoFile()
        log(string.format("[fixes] wheelfps: ended Car Speed FPS Fix (CLEO%s), which rewrites fWheelFriction every " ..
            "frame; fWheelFriction back to 0.9", file and ", cleo\\" .. file or ""), "warn")
        say(string.format("Car Speed FPS Fix (CLEO) was running and is stopped: fixes does the same (/fixes wheelfps). " ..
            "Delete {6BB6E8}cleo\\%s{FFFFFF}.", file or "its .cs file"))
    end
    return ended
end

-- Once SA-MP is up: each ASI a fix stood down for, named in chat once.
function FPS.reportDuplicates()
    local owners = {}
    for option, owner in pairs(FPS.owner) do owners[#owners + 1] = {owner, option} end
    for _, p in ipairs(gamePatches) do
        if p.state == "conflict" and p.va == 0x69DB50 then owners[#owners + 1] = {p.why, "the GXT string fix"} end
    end
    for _, o in ipairs(owners) do
        local module = tostring(o[1]):match("^([^%(]-)%s*$") or ""
        local what = FPS.DUPLICATES[module:lower()]
        if what and not FPS.told[module:lower()] then
            FPS.told[module:lower()] = true
            log(string.format("[fixes] %s is left to %s; fixes carries the same fix", what, module), "warn")
            say(string.format("%s does what fixes already does (%s), so fixes left it to it. Delete it to use fixes'.",
                module, what))
        end
    end
end

-- A gta_sa.exe patch's state, as /fixes patches shows it.
local function patchState(p)
    if not optionOn(p) then return "off (option " .. p.option .. ")" end
    if p.state == "ok" then return "applied" end
    if p.state == "already" then return "already set" end
    if p.state == "conflict" then return "owned by " .. tostring(p.why) end
    if p.state == "refused" then return "refused - " .. tostring(p.why) end
    return "not applied yet"
end

local function patchCounts()
    local n = {ok = 0, already = 0, conflict = 0, refused = 0, off = 0, pending = 0}
    for _, p in ipairs(gamePatches) do
        local key = not optionOn(p) and "off" or (n[p.state or ""] and p.state) or "pending"
        n[key] = n[key] + 1
    end
    return n
end

local function patchSummary()
    local n = patchCounts()
    return ("gta_sa.exe patches: %d applied, %d already set, %d owned elsewhere, %d refused, %d off%s"):format(
        n.ok, n.already, n.conflict, n.refused, n.off, n.pending > 0 and (", %d not yet"):format(n.pending) or "")
end

-- One line once every fix has run: all the patches and fixes, counted. A
-- warning when a site was refused or a fix is not in place.
function reportStartup()
    local placed, owned, off, missing = 0, 0, 0, {}
    for _, f in ipairs(otherFixes) do
        if f[2] == "in place" then placed = placed + 1
        elseif f[2]:find("^left to ") then owned = owned + 1
        elseif f[2]:find("^off") then off = off + 1
        else missing[#missing + 1] = f[1] end
    end
    local refused = patchCounts().refused
    log(("[fixes] started - %s; other fixes: %d in place, %d left to other mods, %d off, %d not in place%s"):format(
        patchSummary(), placed, owned, off, #missing,
        #missing > 0 and (" (" .. table.concat(missing, ", ") .. ")") or ""),
        (refused > 0 or #missing > 0) and "warn" or "info")
end

--- /fixes - the options and their state; /fixes patches - every patch;
--- /fixes <option> on|off - switch one.
function fixesCommand(params)
    local key, value = tostring(params or ""):lower():match("^%s*(%S*)%s*(%S*)")
    if key == "patches" then
        say(patchSummary())
        local groups, order = {}, {}       -- a group is one line: its sites' states, counted
        for _, p in ipairs(gamePatches) do
            if p.group then
                if not groups[p.group] then groups[p.group] = {n = 0}; order[#order + 1] = p.group end
                local g, state = groups[p.group], patchState(p)
                g.n = g.n + 1
                g[state] = (g[state] or 0) + 1
            else
                say(("0x%X %s: %s"):format(p.va, p.name, patchState(p)))
            end
        end
        for _, name in ipairs(order) do
            local g, parts = groups[name], {}
            for state, count in pairs(g) do
                if state ~= "n" then parts[#parts + 1] = ("%d %s"):format(count, state) end
            end
            table.sort(parts)
            say(("%s (%d sites): %s"):format(name, g.n, table.concat(parts, ", ")))
        end
        local cf = crashFixCounts()
        say(("crash fixes (%d hooks): %d hooked, %d owned elsewhere, %d refused, %d off"):format(
            cf.ok + cf.owned + cf.refused + cf.off, cf.ok, cf.owned, cf.refused, cf.off))
        for _, f in ipairs(otherFixes) do say(("%s: %s"):format(f[1], f[2])) end
        if #otherFixes == 0 then say("other fixes: not run yet") end
        return
    end
    if OPTIONS[key] and value == "" then
        say(("%s: %s - %s"):format(key, options[key] and "on" or "off", OPTIONS[key].label))
        return
    end
    if key == "" then
        local names, line = {}, {}
        for name in pairs(OPTIONS) do names[#names + 1] = name end
        table.sort(names)
        for _, name in ipairs(names) do      -- a few to a line
            line[#line + 1] = ("%s %s"):format(name, options[name] and "on" or "off")
            if #line == 6 then say("options: " .. table.concat(line, ", ")); line = {} end
        end
        if #line > 0 then say("options: " .. table.concat(line, ", ")) end
        say("/fixes <option> says what one does; /fixes <option> on|off switches it at once.")
        say(patchSummary() .. " - /fixes patches lists them")
        if removedBuildings and removedBuildings.owner then
            say(("removed buildings: left to %s, which replaces SA-MP's code"):format(removedBuildings.owner))
        elseif removedBuildings then
            say(("removed buildings: %d stored, room for %d (SA-MP's own table holds %d)"):format(
                readU32(removedBuildings.count), removedBuildings.cap, RB_STOCK_CAP))
        end
        if textCacheState and textCacheState.owner then
            say(("text cache: left to %s, which hooks d3dx9_25's font"):format(textCacheState.owner))
        elseif textCacheStats() and textCacheStats().installed ~= 0 then
            local st = textCacheStats()
            say(("text cache: %d layouts, %d replayed, %d recorded%s"):format(st.entries, st.hits, st.recorded,
                st.disabled ~= 0 and (" - off after %d mismatches"):format(st.mismatches) or ""))
        elseif options.textcache and textCacheState and textCacheState.why then
            say("text cache: not in place - " .. textCacheState.why)
        end
        if refPool and refPool.owner then
            say(("entity references: left to %s, which hooks the game's code"):format(refPool.owner))
        elseif refPool then
            say(("entity references: %d of %d reserves of %d made, %d handed to the game"):format(
                readU32(refPool.made), REF_MAX, REF_PER, readU32(refPool.used)))
        end
        if rpcCounts.anim + rpcCounts.menu + rpcCounts.dialog + rpcCounts.pos > 0 then
            say(("server data: %d animation(s), %d menu(s) and %d teleport(s) dropped, %d dialog(s) cut"):format(
                rpcCounts.anim, rpcCounts.menu, rpcCounts.pos, rpcCounts.dialog))
        end
        if next(crashFixes) then
            local cf, averted = crashFixCounts(), 0
            if crashFixLib then
                for i = 0, crashFixLib.CrashFix_Count() - 1 do averted = averted + tonumber(crashFixLib.CrashFix_Averted(i)) end
            end
            say(("crash fixes: %d hooked, %d left to other mods (%s); %d crash(es) averted this session"):format(
                cf.ok, cf.owned, crashFixState(), averted))
        end
        if exceptionFilter and exceptionFilter.owner then
            say("exception filter: left to " .. exceptionFilter.owner)
        elseif exceptionFilter then
            say(("exception filter: %d exception(s) swallowed this session (stock crashes on the 11th)"):format(
                readU32(exceptionFilter.count)))
        end
        if animWipe and animWipe.owner then
            say(("animation wipes: left to %s, which replaces the game's code"):format(animWipe.owner))
        elseif animWipe then
            say(("animation wipes: %d task callbacks run when a ped's animations were wiped"):format(
                readU32(animWipe.told)))
        end
        return
    end
    if not OPTIONS[key] or (value ~= "on" and value ~= "off") then
        say("/fixes patches, or /fixes <option> on|off - options: " .. table.concat((function()
            local t = {}; for name in pairs(OPTIONS) do t[#t + 1] = name end; table.sort(t); return t end)(), ", "))
        return
    end
    local changed, refused = setOption(key, value == "on")
    say(("%s %s: %d site(s) changed%s."):format(key, value, changed,
        refused > 0 and (", %d left alone (another mod's bytes)"):format(refused) or ""))
end

function main()
    -- gta_sa.exe is mapped before any script runs: the code patches go in now
    -- (the video mode list, for one, is built long before SA-MP is up).
    loadOptions()
    -- An option that is off: its sites go back to stock if an earlier run patched
    -- them.
    for _, p in ipairs(gamePatches) do
        if not optionOn(p) then revertGamePatch(p) end
    end
    reportGameInitState()
    applyGamePatches("early")
    local worked = fixAnimationWipe()            -- gta_sa.exe code too: before any ped changes its skin
    noteFix("animation wipes", worked, animWipe and animWipe.owner)
    worked = fixReferencePool()
    noteFix("entity references", worked, refPool and refPool.owner)
    fixCrashes("game")                           -- gta_sa.exe code, before the game runs much of it
    if options.fogrestore then
        worked = fixFogRestore()
        noteFix("fog restore", worked, fogRestore and fogRestore.owner)
    else
        stopFogRestore()
        otherFixes[#otherFixes + 1] = {"fog restore", "off (option fogrestore)"}
    end
    for _, key in ipairs({"wheelfps", "swimfps", "floatfps"}) do
        if options[key] then
            local n = fixFrameRate(key)
            noteFix(key, n > 0, FPS.owner[key])
        else
            stopFrameRate(key)
            otherFixes[#otherFixes + 1] = {key, "off (option " .. key .. ")"}
        end
    end
    if options.positionguard then
        worked = fixPositionGuard()
        noteFix("position guard", worked, posGuard and posGuard.owner)
    else
        stopPositionGuard()
        otherFixes[#otherFixes + 1] = {"position guard", "off (option positionguard)"}
    end

    -- Past here we need the game and SA-MP actually initialised.
    while not isSampAvailable() do wait(0) end

    -- First: a server sends its removed buildings as soon as it accepts the
    -- connection.
    worked = fixRemovedBuildings()               -- samp.dll
    noteFix("removed buildings", worked, removedBuildings and removedBuildings.owner)
    worked = fixExceptionFilter()
    noteFix("exception filter", worked, exceptionFilter and exceptionFilter.owner)
    if options.framecaps then
        worked = fixFrameCap()
        noteFix("SAMP frame cap", worked, frameCap and frameCap.owner)
    else
        otherFixes[#otherFixes + 1] = {"SAMP frame cap", "off (option framecaps)"}
    end
    fixCrashes("samp")
    otherFixes[#otherFixes + 1] = {"crash fixes", crashFixState()}
    if options.textcache then                    -- d3dx9_25.dll, which samp.dll loads
        worked = fixTextCache()
        noteFix("text cache", worked, textCacheState and textCacheState.owner)
    else
        stopTextCache()
        otherFixes[#otherFixes + 1] = {"text cache", "off (option textcache)"}
    end

    applyGamePatches("late")      -- data the game writes during CGame init
    worked = isSampfuncsLoaded and isSampfuncsLoaded()
    if worked then
        log(("[fixes] server RPC checks on: animations (86, 173), menus (76), teleports (12, 13, 159, %s), list dialog lines (61, %s)"):format(
            options.rpcpositions and "on" or "off: option rpcpositions",
            options.dialoglines and "on" or "off: option dialoglines"), "info")
    else
        log("[fixes] server RPC checks are off: they need SAMPFUNCS, which is not loaded", "warn")
    end
    noteFix("server RPC checks", worked)
    sampRegisterChatCommand("fixes", fixesCommand)

    noteFix("F1 help dialog off", disableF1KeyAndDialog())   -- samp.dll
    noteFix("smoking animation", fixSmokingAnimation())
    --forceRadarVisible(true)
    --blankDebugEntityLabels()
    if sigscanOk then pcall(sigscan.flush) end   -- remember the offsets for next launch

    noteFix("animation blocks pinned", pinAnimBlocks())   -- needs the streamer and CAnimManager up
    FPS.endCleoScript()
    FPS.reportDuplicates()
    reportStartup()

    local nextFilterCheck = 0
    while true do
        wait(0)
        timed('thread:fixes/references', refPoolTick)
        local now = os.clock()
        if now >= nextFilterCheck then nextFilterCheck = now + 1; exceptionFilterTick(); PG.tick(); FPS.endCleoScript(); crashFixTick(now) end
    end
end
