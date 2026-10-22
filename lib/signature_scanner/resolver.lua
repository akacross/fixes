-- signature_scanner/resolver.lua
-- Descriptor-driven address resolution: cache -> live scan -> known-offset table.

local scanner  = require('signature_scanner.scanner')
local patterns = require('signature_scanner.patterns')
local versionM = require('signature_scanner.version')
local memory   = require('signature_scanner.memory')
local cache    = require('signature_scanner.cache')
local logger   = require('signature_scanner.logger')

local M = {}

local state = {
    module  = 'samp.dll',
    version = versionM.VERSIONS.UNKNOWN,
}

function M.init(opts)
    opts = opts or {}
    state.module  = opts.module or 'samp.dll'
    state.version = opts.version or versionM.VERSIONS.UNKNOWN
end

--------------------------------------------------------------------------------
-- extraction
--------------------------------------------------------------------------------

local function extract(info, matchRva, desc)
    local site = matchRva + (desc.offset or 0)
    local kind = desc.extract

    if not kind then return site end

    local siteVa = info.base + site

    if kind == 'rel32' then
        local targetVa = scanner.rel32Target(siteVa)
        if not targetVa then return nil, 'rel32 read failed' end
        local rva = targetVa - info.base
        if not memory.rvaInImage(info, rva) then
            return nil, string.format('rel32 target 0x%X is outside the image', rva)
        end
        return rva
    end

    if kind == 'imm32va' then
        local va = scanner.imm32(siteVa)
        if not va then return nil, 'imm32 read failed' end
        -- The operand is an absolute VA baked in at link time against ImageBase.
        -- If the module got relocated, the loader already fixed it up in memory,
        -- so subtracting the live base is correct either way.
        local rva = va - info.base
        if not memory.rvaInImage(info, rva) then
            -- Fall back to the link-time base, in case this dword was somehow
            -- not relocated.
            local alt = va - info.imageBase
            if memory.rvaInImage(info, alt) then return alt end
            return nil, string.format('imm32 0x%X maps to no section', va)
        end
        return rva
    end

    if kind == 'imm32' then
        local v = scanner.imm32(siteVa)
        if not v then return nil, 'imm32 read failed' end
        return v
    end

    return nil, 'unknown extract kind: ' .. tostring(kind)
end

--------------------------------------------------------------------------------
-- resolution
--------------------------------------------------------------------------------

-- Returns rva, source  ('cache' | 'scan' | 'known')  or nil, error
function M.resolveRva(name, opts)
    opts = opts or {}
    local version = opts.version or state.version
    local moduleName = opts.module or state.module

    if not opts.refresh then
        local hit = cache.get(version, name)
        if hit then return hit, 'cache' end
    end

    local info, err = memory.getModuleInfo(moduleName)
    if not info then return nil, err end

    local desc = patterns.get(version, name)
    if desc then
        local matchRva, count = scanner.findUnique(moduleName, desc.pattern, { sections = desc.sections })
        if matchRva then
            local rva, xerr = extract(info, matchRva, desc)
            if rva then
                local expected = desc.expect and desc.expect[version]
                if expected and expected ~= rva then
                    logger.warnf('%s: scan found 0x%X but %s is documented at 0x%X ' ..
                                 '(trusting the scan; update patterns.lua if the build changed)',
                                 name, rva, version, expected)
                end
                cache.set(version, name, rva)
                return rva, 'scan'
            end
            logger.warnf('%s: matched at 0x%X but extraction failed: %s', name, matchRva, tostring(xerr))
        elseif count == 0 then
            logger.warnf('%s: pattern not found in %s (%s)', name, moduleName, tostring(version))
        else
            logger.warnf('%s: pattern matched %d times in %s; refusing an ambiguous result',
                         name, count, moduleName)
        end
    end

    local known = versionM.getKnownRva(name, version)
    if known then
        logger.infof('%s: using the known %s offset 0x%X', name, version, known)
        return known, 'known'
    end

    return nil, string.format('%s: no signature and no known offset for %s', name, tostring(version))
end

-- Absolute address, ready for ffi.cast / memory.* . nil if unresolved.
function M.resolve(name, opts)
    local rva, source = M.resolveRva(name, opts)
    if not rva then return nil, source end
    local info = memory.getModuleInfo(opts and opts.module or state.module)
    if not info then return nil, 'module vanished' end
    return info.base + rva, source
end

-- Resolve everything known for the current build. Returns { [name] = {rva, va, source} }
function M.resolveAll(opts)
    local version = (opts and opts.version) or state.version
    local out = {}
    for _, name in ipairs(patterns.names(version)) do
        local rva, source = M.resolveRva(name, opts)
        out[name] = { rva = rva, source = source }
        if rva then
            local info = memory.getModuleInfo((opts and opts.module) or state.module)
            out[name].va = info and (info.base + rva) or nil
        end
    end
    return out
end

function M.flush() return cache.save() end

return M
