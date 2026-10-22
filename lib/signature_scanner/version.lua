-- signature_scanner/version.lua - samp.dll build identification. Tags match
-- the rest of the repo: DLR1, R1, R2, R3, R4, R5.

local memory = require('signature_scanner.memory')
local logger = require('signature_scanner.logger')

local M = {}

M.VERSIONS = {
    UNKNOWN = 'UNKNOWN',
    DLR1    = 'DLR1',   -- 0.3.DL R1
    R1      = 'R1',     -- 0.3.7 R1
    R2      = 'R2',
    R3      = 'R3',     -- 0.3.7 R3-1, see M.LABELS
    R4      = 'R4',
    R5      = 'R5',
}
local V = M.VERSIONS

-- Real build names: 'R3' is entry point 0xCC4D0, the 0.3.7-R3-1 build
-- (md5 08cf4166).
M.LABELS = {
    DLR1 = '0.3.DL R1',
    R1   = '0.3.7 R1',
    R2   = '0.3.7 R2',
    R3   = '0.3.7 R3-1',
    R4   = '0.3.7 R4',
    R5   = '0.3.7 R5',
}

-- SAMPFUNCS supports R1, R3-1, R5 and 0.3.DL: the only builds with verified
-- offsets. R2/R4 are detected (named rather than 'UNKNOWN') and get a scan only.
M.SUPPORTED = { [V.R1] = true, [V.R3] = true, [V.R5] = true, [V.DLR1] = true }

-- AddressOfEntryPoint (RVA) -> version, confirmed against these DLLs and in game:
--   0x31DF13 -> R1   (samp.dll 2,199,552 bytes, md5 1d22eaa2...)
--   0xCBC90  -> R5   (samp.dll 1,204,224 bytes, md5 5ba5f0be...)
--   0xFDB60  -> DLR1 (samp.dll 1,466,368 bytes, md5 449e4f98...)
local entrypoint_map = {
    [0xFDB60]  = V.DLR1,
    [0x31DF13] = V.R1,
    [0x3195DD] = V.R2,
    [0xCC4D0]  = V.R3,
    [0xCBCB0]  = V.R4,
    [0xCBC90]  = V.R5,
}

-- Fallback when the entry point is unknown (patched/repacked DLL). Both fields
-- must match: R3-1 and R5 share SizeOfImage 0x27E000, and a wrong guess hands
-- out offsets 1 KB off that get NOPs written into unrelated code.
local image_map = {
    { size = 0x330000, timestamp = 0x5542F47A, version = V.R1   },
    { size = 0x27E000, timestamp = 0x5C0B4243, version = V.R3   },  -- 0.3.7-R3-1
    { size = 0x27E000, timestamp = 0x6372C39E, version = V.R5   },
    { size = 0x2BE000, timestamp = 0x5A6A3130, version = V.DLR1 },
}

-- Builds that ship packed (extra .poopN sections, entry point inside the packer
-- stub). Their runtime image is not a straight map of the file.
M.PACKED = { [V.R1] = true, [V.R2] = true }

--------------------------------------------------------------------------------
-- known offsets
--------------------------------------------------------------------------------
-- Every number here is an RVA (offset from getModuleHandle('samp.dll')), not an
-- absolute address: use `base + rva`. They are a cross-check and an offline
-- fallback; resolver.lua prefers a live scan.
--
-- `ShowHelpDialogCall` is the 5-byte `call` instruction (NOP it to suppress the
-- dialog); `ShowHelpDialogFunc` is its callee. NOPing 5 bytes over the callee
-- leaves a stray operand byte (R5 crashed at samp.dll+0x6BB35), so there is no
-- plain `ShowHelpDialog`: a consumer asking for it gets nil.
M.KNOWN = {
    -- Verified against samp.dll md5 1d22eaa2605717ddf215f68e861de378
    [V.R1] = {
        F1KeyHandler       = 0x713D6,
        ShowHelpDialogCall = 0x713F2,
        ShowHelpDialogFunc = 0x67450,
        SAMP_INFO          = 0x21A0F8,
        CNetGame           = 0x21A0F8,  -- same slot as SAMP_INFO
        CNetGameDtor       = 0x9380,
        FrameLimiter       = 0x9D170,
        FrameSleepStub     = 0x9D8F1,
        RadarToggleBranch  = 0x9D318,
        DebugLabelFormat   = 0xD83A8,
        ConnectDelayCmp    = 0x2D3C45,
        SmokeAnimTime      = 0xA85D5,
        SmokeAnimBlend     = 0xA85E2,
    },
    -- samp.dll md5 08cf4166d916e314ed3ee8cff2f13cca (0.3.7-R3-1); the scan
    -- reproduces fixes.lua's 0x752C6 / 0x752E2.
    [V.R3] = {
        F1KeyHandler       = 0x752C6,
        ShowHelpDialogCall = 0x752E2,
        ShowHelpDialogFunc = 0x6B3C0,
        SAMP_INFO          = 0x26E8DC,
        CNetGame           = 0x26E8DC,
        CNetGameCtor       = 0xB5F0,
        CNetGameDtor       = 0x9510,
        FrameLimiter       = 0xA14E0,
        FrameSleepStub     = 0xA1CB1,
        RadarToggleBranch  = 0xA1688,
        DebugLabelFormat   = 0xEA780,
        ConnectDelayCmp    = 0x85E3,
        SmokeAnimTime      = 0xAD4A5,
        SmokeAnimBlend     = 0xAD4B2,
    },
    -- samp.dll md5 5ba5f0be7af99dfd03fb39e88a970a2b, scanned and confirmed in game.
    [V.R5] = {
        F1KeyHandler       = 0x759D5,
        ShowHelpDialogCall = 0x759F1,
        ShowHelpDialogFunc = 0x6BB30,
        SAMP_INFO          = 0x26EB94,
        CNetGame           = 0x26EB94,
        CNetGameCtor       = 0xB930,
        CNetGameDtor       = 0x9880,
        FrameLimiter       = 0xA1C10,
        FrameSleepStub     = 0xA23E1,
        RadarToggleBranch  = 0xA1DB8,
        DebugLabelFormat   = 0xEA7D8,
        ConnectDelayCmp    = 0x8953,
        SmokeAnimTime      = 0xADD35,
        SmokeAnimBlend     = 0xADD42,
    },
    -- 0.3.DL R1, samp.dll md5 449e4f985215ffb5bffadf23551c0d50: resolved by
    -- signature in game (base 0x2B20000 -> handler 0x2B95456), then re-verified
    -- against the file.
    [V.DLR1] = {
        F1KeyHandler       = 0x75456,
        ShowHelpDialogCall = 0x75472,
        ShowHelpDialogFunc = 0x6B570,
        SAMP_INFO          = 0x2ACA24,
        CNetGame           = 0x2ACA24,
        CNetGameCtor       = 0xB610,
        CNetGameDtor       = 0x9570,
        FrameLimiter       = 0xA1A30,
        FrameSleepStub     = 0xA2201,
        RadarToggleBranch  = 0xA1BD8,
        DebugLabelFormat   = 0x11C800,
        ConnectDelayCmp    = 0x8643,
        SmokeAnimTime      = 0xAD935,
        SmokeAnimBlend     = 0xAD942,
    },
    [V.R2]   = {},
    [V.R4]   = {},
}

-- Returns rva or nil. Never returns an absolute address.
function M.getKnownRva(name, ver)
    local t = ver and M.KNOWN[ver]
    return t and t[name] or nil
end

function M.getKnownAddresses(ver)
    if ver then return M.KNOWN[ver] or {} end
    return M.KNOWN
end

--------------------------------------------------------------------------------
-- detection
--------------------------------------------------------------------------------

local cached

-- Returns { version, method, base, entry, size, timestamp, packed }
function M.detect(module_name, refresh)
    module_name = module_name or 'samp.dll'
    if cached and not refresh and cached.module == module_name then return cached end

    local info, err = memory.getModuleInfo(module_name, refresh)
    if not info then
        logger.warn(err or ('unable to locate ' .. tostring(module_name)))
        return { version = V.UNKNOWN, method = 'no-module', module = module_name }
    end

    local result = {
        module    = module_name,
        base      = info.base,
        entry     = info.entry,
        size      = info.size,
        timestamp = info.timestamp,
        version   = V.UNKNOWN,
        method    = 'unknown',
    }

    local byEntry = entrypoint_map[info.entry]
    if byEntry then
        result.version, result.method = byEntry, 'entrypoint'
    else
        for _, candidate in ipairs(image_map) do
            if candidate.size == info.size and candidate.timestamp == info.timestamp then
                result.version, result.method = candidate.version, 'size+timestamp'
                break
            end
        end
    end

    result.packed    = M.PACKED[result.version] or false
    result.label     = M.LABELS[result.version] or result.version
    result.supported = M.SUPPORTED[result.version] or false

    if result.version == V.UNKNOWN then
        logger.warnf('unrecognised %s: entry=0x%X size=0x%X timestamp=0x%X ' ..
                     '(add it to entrypoint_map; signature scanning will still work)',
                     module_name, info.entry, info.size, info.timestamp)
    else
        logger.infof('%s = %s (%s) by %s, base=0x%X%s', module_name, result.version,
                     result.label, result.method, info.base,
                     result.supported and '' or ' -- no verified offsets for this build, scan only')
    end

    cached = result
    return result
end

function M.getVersionEnum() return M.VERSIONS end

return M
