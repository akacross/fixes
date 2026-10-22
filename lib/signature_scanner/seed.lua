-- signature_scanner/seed.lua - derives a candidate signature from a known
-- address at runtime.
--
-- A dword is wildcarded only when it decodes to an address inside this
-- module's image (a relocated operand, which is what differs between builds)
-- and the bytes before it look like an opcode taking an imm32/disp32. The
-- result is validated: a non-unique match is reported as such.
--
-- A starting point to check in IDA. For several builds at once, prefer
-- tools/gen_pattern.py, which confirms a pattern on each.

local bit     = rawget(_G, 'bit') or require('bit')
local memory  = require('signature_scanner.memory')
local scanner = require('signature_scanner.scanner')
local logger  = require('signature_scanner.logger')

local M = {}

-- Opcodes whose following dword is an absolute address / displacement.
-- { [byte] = operand offset from the opcode start }
local ABS_OPERAND_1 = {
    [0xA1] = 1,   -- mov eax, ds:imm32
    [0xA3] = 1,   -- mov ds:imm32, eax
    [0x68] = 1,   -- push imm32
    [0xB8] = 1,   -- mov eax, imm32   (and B9..BF for other regs)
}
-- Opcodes that take a bare disp32 operand via a mod=00 rm=101 ModRM byte:
-- mov/cmp/test r32,[disp32] and mov [disp32],imm / inc [disp32] / etc.
local MODRM_DISP32 = {
    [0x8B] = true, [0x89] = true, [0x8A] = true, [0x88] = true,
    [0x3B] = true, [0x39] = true, [0x85] = true,
    [0xC6] = true, [0xC7] = true, [0xFF] = true,
}
local function absOperandOffset(b0, b1)
    if ABS_OPERAND_1[b0] then return ABS_OPERAND_1[b0] end
    if b0 >= 0xB8 and b0 <= 0xBF then return 1 end
    if MODRM_DISP32[b0] and b1 and bit.band(b1, 0xC7) == 0x05 then return 2 end
    return nil
end

-- rel32 branch operands (E8 call / E9 jmp / 0F 8x jcc) shift between builds as
-- well, so they are wildcarded when the target lands inside the image. Same rule
-- as tools/gen_pattern.py, so runtime and offline seeding agree.
local function relOperandOffset(b0, b1)
    if b0 == 0xE8 or b0 == 0xE9 then return 1 end
    if b0 == 0x0F and b1 and b1 >= 0x80 and b1 <= 0x8F then return 2 end
    return nil
end

-- Build a pattern for `length` bytes starting at rva.
-- Returns { pattern = , rva = , matches = , unique = } or nil, err
function M.suggest(moduleName, rva, length)
    moduleName = moduleName or 'samp.dll'
    length = length or 32
    if length < 8 or length > 256 then return nil, 'length must be 8..256' end

    local info, err = memory.getModuleInfo(moduleName)
    if not info then return nil, err end
    if not memory.rvaInImage(info, rva) or not memory.rvaInImage(info, rva + length - 1) then
        return nil, string.format('0x%X..0x%X is not inside %s', rva, rva + length - 1, moduleName)
    end

    local bytes = memory.readBytes(info.base + rva, length)
    if not bytes then return nil, 'read failed' end

    local function dwordAt(o)
        return bytes[o] + bytes[o + 1] * 0x100 + bytes[o + 2] * 0x10000 + bytes[o + 3] * 0x1000000
    end
    local function emitWildcarded(tokens, i, operandAt)
        for k = 0, operandAt - 1 do
            tokens[#tokens + 1] = string.format('%02X', bytes[i + k])
        end
        for _ = 1, 4 do tokens[#tokens + 1] = '??' end
    end

    local tokens, i = {}, 1
    while i <= length do
        local wildcarded = false

        local operandAt = absOperandOffset(bytes[i], bytes[i + 1])
        if operandAt and i + operandAt + 3 <= length then
            -- The operand is an absolute VA; in a loaded image it has already
            -- been relocated, so compare against the live base.
            if memory.rvaInImage(info, dwordAt(i + operandAt) - info.base) then
                emitWildcarded(tokens, i, operandAt)
                i = i + operandAt + 4
                wildcarded = true
            end
        end

        if not wildcarded then
            operandAt = relOperandOffset(bytes[i], bytes[i + 1])
            if operandAt and i + operandAt + 3 <= length then
                local rel = dwordAt(i + operandAt)
                if rel >= 0x80000000 then rel = rel - 0x100000000 end
                local siteRva = rva + (i - 1)
                if memory.rvaInImage(info, siteRva + operandAt + 4 + rel) then
                    emitWildcarded(tokens, i, operandAt)
                    i = i + operandAt + 4
                    wildcarded = true
                end
            end
        end

        if not wildcarded then
            tokens[#tokens + 1] = string.format('%02X', bytes[i])
            i = i + 1
        end
    end

    local pattern = table.concat(tokens, ' ')
    local matches = scanner.findAll(moduleName, pattern, { sections = { '.text' } })
    local result = {
        rva     = rva,
        pattern = pattern,
        matches = #matches,
        unique  = #matches == 1 and matches[1] == rva,
    }
    if not result.unique then
        logger.warnf('seeded pattern for 0x%X is not unique (%d match(es)); widen the length',
                     rva, #matches)
    end
    return result
end

-- Widen until unique, or give up. Returns the same shape as suggest().
function M.suggestUnique(moduleName, rva, startLength, maxLength, step)
    startLength = startLength or 24
    maxLength   = maxLength or 96
    step        = step or 8
    local last, lastErr
    for len = startLength, maxLength, step do
        local r, err = M.suggest(moduleName, rva, len)
        if not r then return nil, err end
        last, lastErr = r, err
        if r.unique then return r end
    end
    return last, lastErr or 'no unique pattern within maxLength'
end

return M
