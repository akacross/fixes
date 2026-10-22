script_name("fixes")
script_version("0.9.0")

-- /perf (akacross.perf): first, so it sees this script's waits and handlers. Inert until /perf.
do local ok, perf = pcall(require, 'akacross.perf'); if ok then perf.setup{env = getfenv(1)} end end

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
-- and DL also in game). 'R3' is R3-1. The frame-cap symbols are autofps's.
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
        print("[fixes] signature_scanner failed to initialise; using the offset table")
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
        print("[fixes] F1 key and help dialog disabling is not supported: " .. tostring(how))
        return false
    end

    if alreadyPatched(keyVa) then
        print(string.format("[fixes] F1 handler at 0x%X is already NOPed; nothing to do", keyVa))
        return true
    end

    local sane, why = looksLikeF1Block(keyVa, callVa)
    if not sane then
        print(string.format("[fixes] refusing to patch 0x%X via %s: %s", keyVa, how, tostring(why)))
        return false
    end

    memory.fill(keyVa,  0x90, F1_BLOCK_SIZE, true)
    memory.fill(callVa, 0x90, F1_CALL_SIZE,  true)

    local verified = alreadyPatched(keyVa) and readU8(callVa) == 0x90
    print(string.format("[fixes] F1 help disabled on SAMP %s via %s: handler 0x%X (rva 0x%X), call 0x%X -- %s",
        getSampVersion(), how, keyVa, keyVa - getModuleHandle("samp.dll"), callVa,
        verified and "33 bytes NOPed" or "WRITE DID NOT STICK"))
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
        print("[fixes] radar toggle patch is not supported: " .. tostring(why))
        return false
    end

    local want = forceOn and RADAR_ON or RADAR_JZ
    local other = forceOn and RADAR_JZ or RADAR_ON

    local ok, b0 = pcall(readU8, va)
    local _, b1 = pcall(readU8, va + 1)
    if not ok then
        print(string.format("[fixes] cannot read the radar branch at 0x%X; not patching", va))
        return false
    end

    if b0 == want[1] and b1 == want[2] then
        return true                                   -- already in the wanted state
    end

    -- Only ever between the two known encodings.
    if not (b0 == other[1] and b1 == other[2]) then
        print(string.format("[fixes] refusing to patch the radar branch at 0x%X via %s: found %02X %02X, expected %02X %02X",
            va, how, b0, b1, other[1], other[2]))
        return false
    end

    memory.setuint8(va,     want[1], true)
    memory.setuint8(va + 1, want[2], true)

    local verified = readU8(va) == want[1] and readU8(va + 1) == want[2]
    print(string.format("[fixes] radar %s on SAMP %s via %s: 0x%X (rva 0x%X) -- %s",
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
        print("[fixes] debug label blanking is not supported: " .. tostring(why))
        return false
    end

    local ok, first = pcall(readU8, va)
    if not ok then
        print(string.format("[fixes] cannot read the debug label string at 0x%X; not patching", va))
        return false
    end

    if first == 0 then
        print(string.format("[fixes] debug labels at 0x%X are already blank; nothing to do", va))
        return true
    end

    -- It must start with "[id: ", or this is not it.
    local expected = "[id: "
    for i = 1, #expected do
        local okByte, got = pcall(readU8, va + i - 1)
        if not okByte or got ~= expected:byte(i) then
            print(string.format("[fixes] refusing to blank 0x%X via %s: expected %q at +%d, got %s",
                va, how, expected:sub(i, i), i - 1,
                okByte and string.format("%02X", got) or "<unreadable>"))
            return false
        end
    end

    local oldProtection = memory.unprotect(va, DEBUG_LABEL_LEN)
    ffi.fill(ffi.cast("void*", va), DEBUG_LABEL_LEN, 0)
    memory.protect(va, DEBUG_LABEL_LEN, oldProtection)

    local verified = readU8(va) == 0
    print(string.format("[fixes] debug entity labels blanked on SAMP %s via %s: 0x%X (rva 0x%X) -- %s",
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
        print("[fixes] the smoking animation fix is not supported: " .. tostring(why))
        return false
    end
    local blendVa, how2, why2 = resolveSamp("SmokeAnimBlend")
    if not blendVa then
        print("[fixes] the smoking animation fix is not supported: " .. tostring(why2))
        return false
    end
    how = (how == how2) and how or (how .. "/" .. how2)

    -- The two operands belong to one call, 13 bytes apart, or neither is written.
    if blendVa ~= timeVa + SMOKE_OPERAND_GAP then
        print(string.format("[fixes] refusing the smoking animation fix via %s: operands are not %d bytes apart (0x%X vs 0x%X)",
            how, SMOKE_OPERAND_GAP, blendVa, timeVa + SMOKE_OPERAND_GAP))
        return false
    end

    local okRead, time, blend = pcall(function() return readU32(timeVa), readU32(blendVa) end)
    if not okRead then
        print(string.format("[fixes] cannot read the smoking animation operands at 0x%X; not patching", timeVa))
        return false
    end

    if time == SMOKE_TIME_WANT and blend == SMOKE_BLEND_WANT then
        print(string.format("[fixes] the smoking animation at 0x%X is already patched; nothing to do", timeVa))
        return true
    end

    local okCheck, why3 = pcall(function()
        return checkSmokeOperand(timeVa, "time", time, SMOKE_TIME_STOCK, SMOKE_TIME_WANT)
            or checkSmokeOperand(blendVa, "blend", blend, SMOKE_BLEND_STOCK, SMOKE_BLEND_WANT)
    end)
    if not okCheck or why3 then
        print(string.format("[fixes] refusing to patch 0x%X via %s: %s", timeVa, how,
            okCheck and tostring(why3) or "the site is unreadable"))
        return false
    end

    memory.setint32(timeVa,  -1,  true)
    memory.setfloat(blendVa, 4.0, true)

    local verified = readU32(timeVa) == SMOKE_TIME_WANT and readU32(blendVa) == SMOKE_BLEND_WANT
    print(string.format("[fixes] smoking animation fixed on SAMP %s via %s: time 0x%X (rva 0x%X), blend 0x%X -- %s",
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
    {   -- gHandlingDataMgr.fWheelFriction (0xC2B9C8 + 4), 0.9 -> 0.351001. Read
        -- only by CVehicle::ProcessWheel on the coasting path where it sets
        -- rolling resistance, so vehicles coast further off the throttle.
        -- `from` is the value cHandlingDataMgr::Initialise wrote, so this is the
        -- ONE entry that runs late (after CGame::InitialiseCoreDataAfterRW)
        name = "gHandlingDataMgr.fWheelFriction",
        phase = "late",
        va = 0xC2B9CC, from = hexBytes("66 66 66 3F"), to = hexBytes("75 B6 B3 3E")
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
            if found[1] == 0xE8 or found[1] == 0xE9 then
                -- the Open Limit Adjuster's "unlimited" hooks the count read
                local target = (0x553521 + 5 + readU32(0x553522)) % 0x100000000
                return string.format("%s (it hooks the store)", moduleAt(target) or string.format("code at 0x%X", target))
            end
            local array = readU32(VISIBLE_ENTITY_OPERAND)
            if array == VISIBLE_ENTITY_PTRS then return nil end
            return string.format("%s (ms_aVisibleEntityPtrs moved to 0x%X)", limitAdjusterName(), array)
        end,
        from = hexBytes("A1 44 68 B7 00 89 34 85 98 58 B7 00 40 A3 44 68 B7 00 5E C3" ..
            " 90 90 90 90 90 90 90 90 90 90 90"),
        to = hexBytes("EB 12 73 0E 90 89 34 85 98 58 B7 00 40 A3 44 68 B7 00 5E C3" ..
            " A1 44 68 B7 00 66 3D E8 03 EB E3")
    }
}

--------------------------------------------------------------------------------
-- Optional patches
--------------------------------------------------------------------------------
-- A descriptor with `option = "<key>"` applies only while that option is on
-- (config/fixes.json, {"plates": false}). /fixes lists them; /fixes <key> on|off
-- switches one at once (off writes the stock bytes back). Several descriptors
-- may share a key.
local OPTIONS = {
    plates = {default = false, label = "custom plates on bikes, boats, planes and trailers"},
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
        print("[fixes] cannot read CStreaming::ms_bIsInitialised")
        return nil
    end

    initialised = tonumber(initialised) ~= 0
    if not initialised then
        print("[fixes] CStreaming::Init2 has not run yet -- init-time constants " ..
              "(e.g. ms_rwObjectInstances.Init at 0x5B8E55) are patchable from here after all")
    end
    return initialised
end

--- Put a site back to stock if it holds ours: "reverted", "stock" (nothing of
--- ours) or "refused" (someone else's bytes).
local function revertGamePatch(p)
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
            local state = on and applyGamePatch(p) or revertGamePatch(p)
            if state == "ok" or state == "reverted" then changed = changed + 1
            elseif state == "refused" then refused = refused + 1 end
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
function applyGamePatches(phase)
    local counts = {ok = 0, already = 0, conflict = 0, refused = 0}
    local alreadySet = {}
    local considered = 0

    for _, p in ipairs(gamePatches) do
        if (p.phase or "early") == phase and optionOn(p) then
            considered = considered + 1

            local state, why = applyGamePatch(p)
            counts[state] = counts[state] + 1

            if state == "refused" then
                print(string.format("[fixes] refusing 0x%X (%s): %s", p.va, p.name, tostring(why)))
            elseif state == "conflict" then
                print(string.format("[fixes] 0x%X (%s) is owned by %s; standing down",
                    p.va, p.name, tostring(why)))
            elseif state == "already" then
                -- Named, so a site another mod owns is visible.
                alreadySet[#alreadySet + 1] = string.format("0x%X (%s)", p.va, p.name)
            end
        end
    end

    if #alreadySet > 0 then
        print(string.format("[fixes] %s: already in the wanted state: %s",
            phase, table.concat(alreadySet, ", ")))
    end

    if considered > 0 and counts.refused == considered then
        print(string.format("[fixes] every %s gta_sa.exe patch refused; this is not the 1.0 US exe the offsets came from",
            phase))
    else
        print(string.format("[fixes] gta_sa.exe patches (%s): %d applied, %d already set, %d owned elsewhere, %d refused",
            phase, counts.ok, counts.already, counts.conflict, counts.refused))
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
        print("[fixes] the removed-buildings fix is not supported: " .. tostring(why))
        return false
    end
    local lookupVa, how2, why2 = resolveSamp("RemoveBuildingLookup")
    if not lookupVa then
        print("[fixes] the removed-buildings fix is not supported: " .. tostring(why2))
        return false
    end
    how = (how == how2) and how or (how .. "/" .. how2)
    local function refuse(fmt, ...)
        print(string.format("[fixes] refusing the removed-buildings fix via %s: " .. fmt, how, ...))
        return false
    end

    -- SA-MP's code replaced by another mod (open.mp's client): not ours.
    local owner = hookedBy(storeVa + RB_HANDLER) or hookedBy(lookupVa + RB_LOOKUP_FUNC)
    if owner then
        removedBuildings = {owner = owner}
        print(string.format("[fixes] removed buildings: SA-MP's handler and lookup are replaced by %s, " ..
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
        print(string.format("[fixes] removed buildings: already moved to 0x%X (%d stored, room for %d)",
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
        print(string.format("[fixes] SA-MP had already stored %d removed buildings and run over its own " ..
            "memory; kept the first %d. Restart the game if anything misbehaves.", count, RB_STOCK_CAP))
    end

    -- The lookup first (the new table already holds the old one's entries), then
    -- the jmp into the new store.
    writeCode(lookupVa, u32bytes(tableVa))
    writeCode(storeVa, {0xE9, unpack(u32bytes(block + RB_CODE - (storeVa + 5)))})

    local verified = readU32(lookupVa) == tableVa and readU8(storeVa) == 0xE9
    if verified then removedBuildings = {count = store.count, cap = RB_CAP} end
    print(string.format("[fixes] removed buildings on SAMP %s via %s: table moved from 0x%X to 0x%X, room for %d " ..
        "(was %d), each entry kept once; %d carried over -- %s",
        getSampVersion(), how, store.table, tableVa, RB_CAP, RB_STOCK_CAP, carried,
        verified and "store and lookup rewritten" or "WRITE DID NOT STICK"))
    return verified
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
            print(string.format("[fixes] animation wipes: already patched (block 0x%X, %d task callbacks run so far)",
                block, readU32(block + 4)))
            return true
        end
        animWipe = {owner = hookedBy(AW_VA)}
        print(string.format("[fixes] animation wipes: RpAnimBlendClumpRemoveAllAssociations (0x%X) is replaced by %s; " ..
            "standing down", AW_VA, animWipe.owner))
        return false
    end

    local found = readBytes(AW_VA, #AW_STOCK)
    if not bytesEqual(found, AW_STOCK) then
        print(string.format("[fixes] refusing the animation-wipe fix: 0x%X holds %s, expected %s",
            AW_VA, bytesToHex(found), bytesToHex(AW_STOCK)))
        return false
    end

    local p = ffi.C.VirtualAlloc(nil, AW_SIZE, 0x3000, 0x40)   -- MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE
    if p == nil then
        print(string.format("[fixes] refusing the animation-wipe fix: VirtualAlloc of %d bytes failed", AW_SIZE))
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
    print(string.format("[fixes] animation wipes: RpAnimBlendClumpRemoveAllAssociations now runs the game's %d " ..
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
            print(string.format("[fixes] entity references: already patched (block 0x%X, %d reserves used)",
                block, readU32(block + 8)))
            return true
        end
        refPool = {owner = moduleAt(target) or string.format("code at 0x%X", target)}
        print(string.format("[fixes] entity references: the free-list read at 0x%X is hooked by %s; standing down",
            REF_VA, refPool.owner))
        return false
    end
    if not bytesEqual(head, REF_STOCK) then
        print(string.format("[fixes] refusing the entity-reference fix: 0x%X holds %s, expected %s",
            REF_VA, bytesToHex(head), bytesToHex(REF_STOCK)))
        return false
    end

    local p = ffi.C.VirtualAlloc(nil, REF_SIZE, 0x3000, 0x40)   -- MEM_COMMIT|MEM_RESERVE, PAGE_EXECUTE_READWRITE
    if p == nil then
        print(string.format("[fixes] refusing the entity-reference fix: VirtualAlloc of %d bytes failed", REF_SIZE))
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
    print(string.format("[fixes] entity references: past the game's 3000, reserves of %d (up to %d) " ..
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
        print("[fixes] refusing the text cache: " .. why)
        return false
    end
    local base = getModuleHandle("d3dx9_25.dll")
    if not base or base == 0 then
        textCacheState = {why = "d3dx9_25.dll is not loaded"}
        print("[fixes] text cache: d3dx9_25.dll is not loaded, nothing to cache")
        return false
    end
    local foreign = ffi.new("uintptr_t[1]")
    local result = lib.TextCache_Install(ffi.cast("void*", base), foreign)
    if result == 1 then
        textCacheState = {installed = true}
        print("[fixes] text cache: d3dx9_25's DrawText now replays layouts it has drawn before")
        return true
    end
    if result == 2 then
        local at = tonumber(foreign[0])
        textCacheState = {owner = moduleAt(at) or string.format("code at 0x%X", at)}
        print(string.format("[fixes] text cache: d3dx9_25's font is hooked by %s; standing down", textCacheState.owner))
        return false
    end
    textCacheState = {why = string.format("d3dx9_25.dll is not build 0x%08X", D3DX_BUILD)}
    print("[fixes] refusing the text cache: " .. textCacheState.why)
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
        print("[fixes] not pinning anim blocks: 0x48C36B is not `lea eax,[edi+imm32]` any more")
        return false
    end

    local count = memory.read(0xB4EA30, 4, true)   -- CAnimManager::ms_numAnimBlocks
    if not count or count < 1 or count > NUM_ANIM_BLOCKS then
        print(string.format("[fixes] not pinning anim blocks: ms_numAnimBlocks is %s", tostring(count)))
        return false
    end

    local AddAnimBlockRef = ffi.cast("AddAnimBlockRef_t", 0x4D3FB0)
    local pinned, held = 0, 0

    -- Block 0 ("ped") is always resident, so it starts at 1.
    for id = 1, count - 1 do
        local refs = animBlockRefCount(id)
        if refs == nil then
            print(string.format("[fixes] not pinning anim blocks: cannot read RefCnt of block %d", id))
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
    print(string.format("[fixes] anim blocks (base id %d): %d pinned, %d already referenced",
        idBase, pinned, held))
    return true
end

local function say(text)
    sampAddChatMessage("{9ACD32}[fixes]{FFFFFF} " .. text, -1)
end

--- /fixes - the options and their state; /fixes <option> on|off - switch one.
function fixesCommand(params)
    local key, value = tostring(params or ""):lower():match("^%s*(%S*)%s*(%S*)")
    if key == "" then
        local names = {}
        for name in pairs(OPTIONS) do names[#names + 1] = name end
        table.sort(names)
        for _, name in ipairs(names) do
            say(("%s: %s - %s"):format(name, options[name] and "on" or "off", OPTIONS[name].label))
        end
        say("/fixes <option> on|off switches one at once.")
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
        if animWipe and animWipe.owner then
            say(("animation wipes: left to %s, which replaces the game's code"):format(animWipe.owner))
        elseif animWipe then
            say(("animation wipes: %d task callbacks run when a ped's animations were wiped"):format(
                readU32(animWipe.told)))
        end
        return
    end
    if not OPTIONS[key] or (value ~= "on" and value ~= "off") then
        say("/fixes <option> on|off - options: " .. table.concat((function()
            local t = {}; for name in pairs(OPTIONS) do t[#t + 1] = name end; return t end)(), ", "))
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
    fixAnimationWipe()            -- gta_sa.exe code too: before any ped changes its skin
    fixReferencePool()

    -- Past here we need the game and SA-MP actually initialised.
    while not isSampAvailable() do wait(0) end

    -- First: a server sends its removed buildings as soon as it accepts the
    -- connection.
    fixRemovedBuildings()         -- samp.dll
    if options.textcache then fixTextCache() else stopTextCache() end   -- d3dx9_25.dll, which samp.dll loads

    applyGamePatches("late")      -- data the game writes during CGame init
    sampRegisterChatCommand("fixes", fixesCommand)

    disableF1KeyAndDialog()       -- samp.dll
    fixSmokingAnimation()
    --forceRadarVisible(true)
    --blankDebugEntityLabels()
    if sigscanOk then pcall(sigscan.flush) end   -- remember the offsets for next launch

    pinAnimBlocks()               -- needs the streamer and CAnimManager up

    while true do
        wait(0)
        refPoolTick()
    end
end
