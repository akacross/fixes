-- signature_scanner/scanner.lua - IDA-style pattern compilation and scanning
-- over PE section ranges. Results are RVAs: samp.dll gets relocated.

local ffi_ok, ffi = pcall(require, 'ffi')
local memory = require('signature_scanner.memory')
local logger = require('signature_scanner.logger')

local M = {}

M.MAX_MATCHES = 64      -- hard stop; a pattern with more hits than this is junk

--------------------------------------------------------------------------------
-- pattern compilation
--------------------------------------------------------------------------------
-- Wildcards live in a parallel mask array (`t[#t+1] = nil` is a no-op in Lua
-- and would drop the hole); a bad token raises instead of becoming a wildcard.

local compiledCache = {}

function M.compile(pattern)
    if type(pattern) == 'table' and pattern.__compiled then return pattern end
    if type(pattern) ~= 'string' then
        error('signature_scanner: pattern must be a string, got ' .. type(pattern), 2)
    end
    local hit = compiledCache[pattern]
    if hit then return hit end

    local bytes, mask = {}, {}
    for token in pattern:gmatch('%S+') do
        if token == '?' or token == '??' then
            bytes[#bytes + 1] = 0
            mask[#mask + 1]   = 0
        elseif token:match('^%x%x?$') then
            bytes[#bytes + 1] = tonumber(token, 16)
            mask[#mask + 1]   = 0xFF
        else
            error(string.format('signature_scanner: bad pattern token %q in %q', token, pattern), 2)
        end
    end

    local n = #bytes
    if n == 0 then error('signature_scanner: empty pattern', 2) end
    if mask[1] == 0 then
        -- A leading wildcard costs a full extra scan pass for nothing; the
        -- caller almost always meant to anchor on the first opcode.
        logger.warnf('pattern starts with a wildcard, which slows scanning: %s', pattern)
    end

    -- Anchor on the first fixed byte so the inner loop rejects in one compare.
    local anchor = 0
    for i = 1, n do
        if mask[i] == 0xFF then anchor = i - 1 break end
    end

    local c = {
        __compiled = true,
        text   = pattern,
        length = n,
        anchor = anchor,
        anchorByte = bytes[anchor + 1],
    }
    if ffi_ok then
        c.bytes = ffi.new('uint8_t[?]', n, bytes)
        c.mask  = ffi.new('uint8_t[?]', n, mask)
    else
        -- Keep the fallback 0-based too, so every consumer indexes identically.
        local b0, m0 = {}, {}
        for i = 1, n do b0[i - 1], m0[i - 1] = bytes[i], mask[i] end
        c.bytes, c.mask = b0, m0
    end

    compiledCache[pattern] = c
    return c
end

--------------------------------------------------------------------------------
-- scanning
--------------------------------------------------------------------------------

-- Scan one contiguous range. `base` is the VA the range starts at.
local function scanRange(base, size, c, out, limit)
    local p = memory.bytePtr(base)
    if not p then return end
    local n, anchor, ab = c.length, c.anchor, c.anchorByte
    local bytes, mask = c.bytes, c.mask
    local last = size - n
    local i = 0
    while i <= last do
        if p[i + anchor] == ab then
            local ok = true
            for k = 0, n - 1 do
                if mask[k] ~= 0 and p[i + k] ~= bytes[k] then ok = false break end
            end
            if ok then
                out[#out + 1] = base + i
                if #out >= limit then return end
            end
        end
        i = i + 1
    end
end

-- Returns a list of RVAs.
-- opts = { sections = {'.text'}, limit = n, module = 'samp.dll' }
function M.findAll(moduleName, pattern, opts)
    opts = opts or {}
    local matches = {}
    if not ffi_ok then
        logger.error('ffi unavailable; cannot scan')
        return matches
    end

    local info, err = memory.getModuleInfo(moduleName)
    if not info then
        logger.warn(err or ('no module info for ' .. tostring(moduleName)))
        return matches
    end

    local c = M.compile(pattern)
    local limit = opts.limit or M.MAX_MATCHES
    local ranges = memory.getScanRanges(info, opts.sections)
    if #ranges == 0 then
        logger.warnf('no scannable sections in %s', info.name)
        return matches
    end

    local vas = {}
    for _, sec in ipairs(ranges) do
        scanRange(info.base + sec.rva, sec.size, c, vas, limit)
        if #vas >= limit then break end
    end

    for i = 1, #vas do matches[i] = vas[i] - info.base end   -- VA -> RVA
    return matches
end

-- Exactly-one-match lookup. Returns rva or nil, plus the match count so callers
-- can tell "not found" from "ambiguous".
function M.findUnique(moduleName, pattern, opts)
    local matches = M.findAll(moduleName, pattern, opts)
    if #matches == 1 then return matches[1], 1 end
    if #matches == 0 then return nil, 0 end
    return nil, #matches
end

-- Scan a plain byte buffer (Lua array or cdata pointer). Used by the host-side
-- tests and by tooling; returns a 0-based index.
function M.findInBuffer(buf, size, pattern)
    local c = M.compile(pattern)
    local n, anchor, ab = c.length, c.anchor, c.anchorByte
    local bytes, mask = c.bytes, c.mask
    local isTable = type(buf) == 'table'
    local function at(i) return isTable and buf[i + 1] or buf[i] end
    for i = 0, size - n do
        if at(i + anchor) == ab then
            local ok = true
            for k = 0, n - 1 do
                if mask[k] ~= 0 and at(i + k) ~= bytes[k] then ok = false break end
            end
            if ok then return i end
        end
    end
    return nil
end

--------------------------------------------------------------------------------
-- operand decoding
--------------------------------------------------------------------------------
-- A match is an instruction's address, not what it touches. These helpers,
-- driven by a descriptor's `extract`, give the global a mov reads or the
-- function a call targets.

-- rel32 branch target (E8 call / E9 jmp): site points at the opcode byte.
function M.rel32Target(va)
    if not ffi_ok then return nil end
    local rel = memory.readS32(va + 1)
    if not rel then return nil end
    return va + 5 + rel
end

-- Absolute imm32 embedded in an instruction (e.g. `A1 xx xx xx xx` /
-- `8B 0D xx xx xx xx`): site points at the first operand byte.
function M.imm32(va)
    if not ffi_ok then return nil end
    return memory.readU32(va)
end

function M.resolveCall(va)
    if memory.readU8(va) ~= 0xE8 then return nil end
    return M.rel32Target(va)
end

function M.resolveJmp(va)
    if memory.readU8(va) ~= 0xE9 then return nil end
    return M.rel32Target(va)
end

return M
