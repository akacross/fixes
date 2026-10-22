-- signature_scanner/memory.lua - module discovery, PE image introspection and
-- bounded memory reads. Two units, never mixed:
--   * VA  -- absolute address in the process (what ffi/memory.* take)
--   * RVA -- offset from the module base (what patterns/caches store)

local ffi_ok, ffi = pcall(require, 'ffi')
local bit = rawget(_G, 'bit') or require('bit')
local logger = require('signature_scanner.logger')

local M = {}
M.available = ffi_ok

-- IMAGE_SCN_* section characteristics we care about
local SCN_CNT_CODE       = 0x00000020
local SCN_MEM_DISCARDABLE = 0x02000000
local SCN_MEM_EXECUTE    = 0x20000000
local SCN_MEM_READ       = 0x40000000

--------------------------------------------------------------------------------
-- raw reads
--------------------------------------------------------------------------------

local function u8(va)  return ffi.cast('uint8_t*',  va)[0] end
local function u16(va) return ffi.cast('uint16_t*', va)[0] end
local function u32(va) return tonumber(ffi.cast('uint32_t*', va)[0]) end

function M.readU8(va)  if not ffi_ok then return nil end return u8(va) end
function M.readU16(va) if not ffi_ok then return nil end return u16(va) end
function M.readU32(va) if not ffi_ok then return nil end return u32(va) end

function M.readS32(va)
    if not ffi_ok then return nil end
    return tonumber(ffi.cast('int32_t*', va)[0])
end

-- Pointer to raw bytes: no copy (a Lua table of a 2.5 MB image is ~80 MB).
function M.bytePtr(va)
    if not ffi_ok then return nil end
    return ffi.cast('const uint8_t*', va)
end

-- Small copies only (pattern seeding, hex dumps). len is capped on purpose.
function M.readBytes(va, len)
    if not ffi_ok or not va or not len or len <= 0 or len > 0x10000 then return nil end
    local p = ffi.cast('const uint8_t*', va)
    local out = {}
    for i = 0, len - 1 do out[i + 1] = p[i] end
    return out
end

--------------------------------------------------------------------------------
-- VirtualQuery guard (optional)
--------------------------------------------------------------------------------
-- Section headers say what should be mapped; VirtualQuery says what is. The
-- packed builds' (R1/R2) DISCARDABLE .poopN sections can be gone by scan time.
--
-- mimgui and others cdef much of Win32 into the shared ffi namespace, so a
-- plain cdef can throw "attempt to redefine": the declaration and first call
-- are probed, and the guard disables itself if anything looks wrong.

local vq, mbi_ct
if ffi_ok then
    local function probe(typename)
        local ok, ct = pcall(ffi.typeof, typename)
        return ok and ct or nil
    end
    mbi_ct = probe('MEMORY_BASIC_INFORMATION')
    if not mbi_ct then
        pcall(ffi.cdef, [[
            typedef struct {
                void*    BaseAddress;
                void*    AllocationBase;
                uint32_t AllocationProtect;
                size_t   RegionSize;
                uint32_t State;
                uint32_t Protect;
                uint32_t Type;
            } sigscan_mbi_t;
        ]])
        mbi_ct = probe('sigscan_mbi_t')
    end
    if mbi_ct then
        pcall(ffi.cdef, 'size_t VirtualQuery(const void*, void*, size_t);')
        local ok, fn = pcall(function() return ffi.C.VirtualQuery end)
        if ok and fn then
            local buf = mbi_ct()
            local ok2 = pcall(fn, ffi.cast('const void*', buf), buf, ffi.sizeof(mbi_ct))
            if ok2 then vq = fn end
        end
    end
end

local MEM_COMMIT     = 0x1000
local PAGE_NOACCESS  = 0x01
local PAGE_GUARD     = 0x100

-- true when readable, and also when the guard is unavailable (the caller
-- still has the section bounds, the primary safety net)
function M.isReadable(va, size)
    if not vq or not ffi_ok then return true end
    local mbi = mbi_ct()
    local remaining, addr = size, va
    while remaining > 0 do
        local ok = pcall(vq, ffi.cast('const void*', addr), mbi, ffi.sizeof(mbi_ct))
        if not ok then return true end
        if mbi.State ~= MEM_COMMIT then return false end
        local prot = tonumber(mbi.Protect)
        if prot == PAGE_NOACCESS or bit.band(prot, PAGE_GUARD) ~= 0 then return false end
        local region = tonumber(mbi.RegionSize)
        if region <= 0 then return false end
        local base = tonumber(ffi.cast('uintptr_t', mbi.BaseAddress))
        local avail = base + region - addr
        if avail <= 0 then return false end
        remaining = remaining - avail
        addr = addr + avail
    end
    return true
end

--------------------------------------------------------------------------------
-- module info / PE parsing
--------------------------------------------------------------------------------

local cache = {}    -- [lowercase name] = info

local function parsePE(base)
    if not ffi_ok then return nil, 'ffi unavailable' end
    if u16(base) ~= 0x5A4D then return nil, 'no MZ header' end        -- 'MZ'
    local e_lfanew = u32(base + 0x3C)
    if e_lfanew == 0 or e_lfanew > 0x1000 then return nil, 'bad e_lfanew' end
    local nt = base + e_lfanew
    if u32(nt) ~= 0x00004550 then return nil, 'no PE signature' end   -- 'PE\0\0'

    local fileHeader = nt + 4
    local numSections = u16(fileHeader + 2)
    local timestamp   = u32(fileHeader + 4)
    local sizeOfOpt   = u16(fileHeader + 16)

    local opt = fileHeader + 20
    if u16(opt) ~= 0x010B then return nil, 'not a PE32 image' end      -- IMAGE_NT_OPTIONAL_HDR32_MAGIC
    local entry            = u32(opt + 16)
    local imageBase        = u32(opt + 28)
    local sectionAlignment = u32(opt + 32)
    local sizeOfImage      = u32(opt + 56)

    local sections, byName = {}, {}
    local sh = opt + sizeOfOpt
    for i = 0, numSections - 1 do
        local s = sh + i * 40
        local name = ffi.string(ffi.cast('const char*', s), 8):gsub('%z+$', '')
        local vsize = u32(s + 8)
        local rva   = u32(s + 12)
        local rsize = u32(s + 16)
        local chars = u32(s + 36)
        local span  = vsize > 0 and vsize or rsize
        local sec = {
            name        = name,
            rva         = rva,
            size        = span,
            rawSize     = rsize,
            chars       = chars,
            code        = bit.band(chars, SCN_CNT_CODE) ~= 0 or bit.band(chars, SCN_MEM_EXECUTE) ~= 0,
            readable    = bit.band(chars, SCN_MEM_READ) ~= 0,
            discardable = bit.band(chars, SCN_MEM_DISCARDABLE) ~= 0,
        }
        sections[#sections + 1] = sec
        byName[name] = sec
    end

    return {
        entry            = entry,
        imageBase        = imageBase,
        size             = sizeOfImage,
        sectionAlignment = sectionAlignment,
        timestamp        = timestamp,
        sections         = sections,
        section          = byName,
    }
end

M.parsePE = parsePE

-- MoonLoader's getModuleHandle() global (there is no
-- moonloader.getModuleAddress / getModuleSize).
local function moduleBase(name)
    if type(getModuleHandle) ~= 'function' then return nil end
    local ok, base = pcall(getModuleHandle, name)
    if not ok then return nil end
    base = tonumber(base)
    if not base or base == 0 then return nil end
    return base
end

-- Returns { name, base, size, entry, timestamp, sections = {...} } or nil, err
function M.getModuleInfo(name, refresh)
    name = name or 'samp.dll'
    local key = name:lower()
    if not refresh and cache[key] then
        local cached = cache[key]
        if moduleBase(name) == cached.base then return cached end
    end

    local base = moduleBase(name)
    if not base then return nil, 'module not loaded: ' .. tostring(name) end

    local pe, err = parsePE(base)
    if not pe then return nil, string.format('%s: %s', name, tostring(err)) end

    local info = {
        name      = name,
        base      = base,
        size      = pe.size,
        entry     = pe.entry,
        imageBase = pe.imageBase,
        timestamp = pe.timestamp,
        sections  = pe.sections,
        section   = pe.section,
        relocated = base ~= pe.imageBase,
    }
    cache[key] = info
    return info
end

-- Ranges to scan, as { {rva=, size=, name=} }, never outside a real section.
--   names == nil        -> every executable, non-discardable section
--   names == {'.text'}  -> only those sections
function M.getScanRanges(info, names)
    local out = {}
    if not info then return out end
    if names then
        for _, n in ipairs(names) do
            local sec = info.section[n]
            if sec and sec.size > 0 then
                out[#out + 1] = sec
            else
                logger.warnf('section %s not present in %s', tostring(n), tostring(info.name))
            end
        end
    else
        for _, sec in ipairs(info.sections) do
            if sec.code and not sec.discardable and sec.size > 0 then
                out[#out + 1] = sec
            end
        end
    end

    local usable = {}
    for _, sec in ipairs(out) do
        if M.isReadable(info.base + sec.rva, sec.size) then
            usable[#usable + 1] = sec
        else
            logger.warnf('skipping %s in %s: range not committed/readable', sec.name, tostring(info.name))
        end
    end
    return usable
end

function M.rvaToVa(info, rva) return info.base + rva end
function M.vaToRva(info, va)  return va - info.base end

-- Is `rva` inside a mapped section of this image? Used to sanity-check anything
-- decoded out of an instruction before it gets dereferenced.
function M.rvaInImage(info, rva)
    if not info or not rva or rva < 0 then return false end
    for _, sec in ipairs(info.sections) do
        if rva >= sec.rva and rva < sec.rva + sec.size then return true end
    end
    return false
end

function M.hexdump(va, len)
    local b = M.readBytes(va, len or 16)
    if not b then return '<unreadable>' end
    local parts = {}
    for i = 1, #b do parts[i] = string.format('%02X', b[i]) end
    return table.concat(parts, ' ')
end

return M
