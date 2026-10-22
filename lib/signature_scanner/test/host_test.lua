-- signature_scanner/test/host_test.lua
--
-- Runs the real scanner against a real samp.dll, outside the game.
--
-- It maps a samp.dll file into memory the way the Windows loader would (headers
-- plus each section at its RVA), stubs getModuleHandle(), and then calls the
-- production code path. No mocks of the scanner itself, so a pass here means the
-- pattern compiler, section walker, scan loop, operand decoding and version
-- detection all work on genuine bytes.
--
-- Relocations are deliberately NOT applied, which exercises the ImageBase
-- fallback in resolver.extract().
--
-- Usage (from the moonloader folder):
--   luajit lib/signature_scanner/test/host_test.lua <path-to-samp.dll> [...]

-- Line-buffer stdout: piped, it flushes only at exit, so a crash's traceback
-- (stderr, unbuffered) would land above every line printed before it.
io.stdout:setvbuf('line')

local ffi = require('ffi')
local bit = require('bit')

local here = arg[0]:match('^(.*)[/\\][^/\\]+$') or '.'
package.path = table.concat({
    here .. '/../../?.lua',
    here .. '/../../?/init.lua',
    package.path,
}, ';')

--------------------------------------------------------------------------------
-- map a PE file into memory
--------------------------------------------------------------------------------

local function u16(s, o) return s:byte(o + 1) + s:byte(o + 2) * 0x100 end
local function u32(s, o)
    return s:byte(o + 1) + s:byte(o + 2) * 0x100 + s:byte(o + 3) * 0x10000 + s:byte(o + 4) * 0x1000000
end

local function mapImage(path)
    local f = assert(io.open(path, 'rb'), 'cannot open ' .. path)
    local data = f:read('*a')
    f:close()

    local e_lfanew = u32(data, 0x3C)
    assert(u32(data, e_lfanew) == 0x00004550, 'not a PE file')
    local fileHeader = e_lfanew + 4
    local numSections = u16(data, fileHeader + 2)
    local sizeOfOpt   = u16(data, fileHeader + 16)
    local opt         = fileHeader + 20
    local sizeOfImage   = u32(data, opt + 56)
    local sizeOfHeaders = u32(data, opt + 60)

    local buf = ffi.new('uint8_t[?]', sizeOfImage)
    ffi.copy(buf, data:sub(1, sizeOfHeaders), sizeOfHeaders)

    local sh = opt + sizeOfOpt
    for i = 0, numSections - 1 do
        local s = sh + i * 40
        local vsize = u32(data, s + 8)
        local rva   = u32(data, s + 12)
        local rsize = u32(data, s + 16)
        local rptr  = u32(data, s + 20)
        local n = math.min(rsize, vsize > 0 and vsize or rsize)
        if n > 0 and rptr > 0 then
            ffi.copy(buf + rva, data:sub(rptr + 1, rptr + n), n)
        end
    end

    return buf, tonumber(ffi.cast('uintptr_t', buf)), #data
end

--------------------------------------------------------------------------------
-- run
--------------------------------------------------------------------------------

local paths = {}
for i = 1, #arg do paths[#paths + 1] = arg[i] end
if #paths == 0 then
    io.stderr:write('usage: luajit host_test.lua <samp.dll> [more...]\n')
    os.exit(2)
end

local failures = 0

for _, path in ipairs(paths) do
    print(('='):rep(78))
    print('file: ' .. path)

    local keepAlive, base, filesize = mapImage(path)   -- keepAlive must outlive the run
    _G.getModuleHandle = function(name)
        if name and name:lower() == 'samp.dll' then return base end
        return 0
    end

    -- Fresh module state per file.
    for name in pairs(package.loaded) do
        if name:match('^signature_scanner') then package.loaded[name] = nil end
    end

    local sig      = require('signature_scanner')
    local patterns = require('signature_scanner.patterns')
    local memory   = require('signature_scanner.memory')

    sig.init({ module_name = 'samp.dll', cache_file = false, refresh = true })
    local det = sig.detected
    print(string.format('mapped at 0x%X, file size %d bytes', base, filesize))
    print(string.format('detected: %s (by %s) entry=0x%X size=0x%X timestamp=0x%X',
                        det.version, det.method, det.entry, det.size, det.timestamp))

    local info = memory.getModuleInfo('samp.dll')
    local names = {}
    for _, sec in ipairs(info.sections) do
        names[#names + 1] = string.format('%s%s%s', sec.name,
            sec.code and '(code)' or '', sec.discardable and '(discardable)' or '')
    end
    print('sections: ' .. table.concat(names, ' '))

    local scanned = memory.getScanRanges(info)
    local scannedNames = {}
    for _, sec in ipairs(scanned) do scannedNames[#scannedNames + 1] = sec.name end
    print('default scan set: ' .. table.concat(scannedNames, ' '))

    for _, name in ipairs(patterns.names(det.version)) do
        local desc = patterns.get(det.version, name)
        local t0 = os.clock()
        local rva, source = sig.resolveRva(name, { refresh = true })
        local ms = (os.clock() - t0) * 1000
        local expected = desc.expect and desc.expect[det.version]
        if not rva then
            failures = failures + 1
            print(string.format('  FAIL  %-19s %s', name, tostring(source)))
        elseif expected and expected ~= rva then
            failures = failures + 1
            print(string.format('  FAIL  %-19s rva=0x%X, expected 0x%X', name, rva, expected))
        else
            print(string.format('  ok    %-19s rva=0x%-8X via %-6s %s(%.0f ms)',
                  name, rva, source, expected and 'matches documented value ' or 'undocumented ', ms))
        end
    end

    -- fields.lua is generated from the SFAPI headers, which are a *claim*. This
    -- checks it against the binary: whatever field the code dereferences most
    -- often right after loading g_SAMP has to be `pools` (it is the gateway to
    -- every pool). Catches a stale fields.lua and a wrong version tag alike.
    local fields = require('signature_scanner.fields')
    local poolsOff = fields.offset('stSAMP', 'pools', det.version)
    if poolsOff then
        local slot = sig.resolveRva('CNetGame', { refresh = true })
        local imm = info.imageBase + slot         -- the link-time VA baked into code
        local immPat = string.format('%02X %02X %02X %02X',
            imm % 256, math.floor(imm / 256) % 256,
            math.floor(imm / 65536) % 256, math.floor(imm / 16777216) % 256)
        local tally, best, bestN = {}, nil, 0
        for _, rva in ipairs(sig.scanner.findAll('samp.dll', immPat, { sections = { '.text' }, limit = 4096 })) do
            local p = memory.bytePtr(info.base + rva + 4)
            -- mov r32, [r32 + disp32]  ->  8B /r with mod=10
            if p[0] == 0x8B and bit.band(p[1], 0xC0) == 0x80 then
                local d = p[2] + p[3] * 0x100 + p[4] * 0x10000 + p[5] * 0x1000000
                tally[d] = (tally[d] or 0) + 1
                if tally[d] > bestN then best, bestN = d, tally[d] end
            end
        end
        if best == poolsOff then
            print(string.format('  ok    fields.lua: pools at +0x%X is the most-dereferenced field (%d sites)',
                  poolsOff, bestN))
        else
            failures = failures + 1
            print(string.format('  FAIL  fields.lua says pools is at +0x%X, but the binary favours +0x%X (%d sites)',
                  poolsOff, best or -1, bestN))
        end
    end

    -- CNetGame and SAMP_INFO are the same pointer slot, resolved by two
    -- signatures that share no bytes. If they ever disagree, one of them is
    -- matching the wrong site.
    local cn = sig.resolveRva('CNetGame', { refresh = true })
    local si = sig.resolveRva('SAMP_INFO', { refresh = true })
    if cn and si and cn == si then
        print(string.format('  ok    CNetGame == SAMP_INFO (0x%X, two independent signatures)', cn))
    else
        failures = failures + 1
        print(string.format('  FAIL  CNetGame (%s) != SAMP_INFO (%s)',
              cn and ('0x%X'):format(cn) or 'nil', si and ('0x%X'):format(si) or 'nil'))
    end

    -- Regression guard for the wildcard bug: 33 tokens, 12 of them wildcards.
    -- The old parser dropped the wildcards and compiled this to a contiguous
    -- 21-byte literal, which matches nothing.
    local scanner = require('signature_scanner.scanner')
    local c = scanner.compile('A1 ?? ?? ?? ?? 85 C0 74 18 6A 70 FF D6 66 85 C0 74 0F 8B 0D ?? ?? ?? ?? 85 C9 74 05 E8 ?? ?? ?? ??')
    if c.length ~= 33 then
        failures = failures + 1
        print(string.format('  FAIL  pattern compiler produced length %d, expected 33', c.length))
    else
        print('  ok    pattern compiler keeps wildcards (length 33, not 21)')
    end
    if not pcall(scanner.compile, 'A1 ZZ 85') then
        print('  ok    pattern compiler rejects malformed tokens')
    else
        failures = failures + 1
        print('  FAIL  pattern compiler accepted a malformed token')
    end

    -- seed.lua: derive a pattern from a known offset and check it round-trips.
    local seed = require('signature_scanner.seed')
    local knownRva = sig.version.getKnownRva('F1KeyHandler', det.version)
    if knownRva then
        local s, err = seed.suggestUnique('samp.dll', knownRva)
        if s and s.unique then
            print(string.format('  ok    seed re-derives F1KeyHandler at 0x%X uniquely', knownRva))
        else
            failures = failures + 1
            print(string.format('  FAIL  seed could not re-derive 0x%X uniquely (%s)',
                  knownRva, s and (s.matches .. ' matches') or tostring(err)))
        end
    end

    -- cache.lua: RVAs must survive a save/load round-trip, and a cache written
    -- for a different build must be rejected.
    local cache = require('signature_scanner.cache')
    local tmp = os.tmpname()
    os.remove(tmp)                  -- start from "no cache file", as a first run would
    local identity = { entry = det.entry, size = det.size, timestamp = det.timestamp }
    cache.load(tmp, identity)
    cache.set(det.version, 'F1KeyHandler', 0x759D5)
    cache.save(true)
    cache.load(tmp, identity)
    local roundTrip = cache.get(det.version, 'F1KeyHandler') == 0x759D5
    -- The "cache is for a different samp.dll build" line below is the assertion
    -- passing, not a problem.
    cache.load(tmp, { entry = 0xDEAD, size = 1, timestamp = 2 })
    local rejected = cache.get(det.version, 'F1KeyHandler') == nil
    os.remove(tmp)
    if roundTrip and rejected then
        print('  ok    cache round-trips RVAs and rejects a foreign build')
    else
        failures = failures + 1
        print(string.format('  FAIL  cache round-trip=%s foreign-build-rejected=%s',
              tostring(roundTrip), tostring(rejected)))
    end

    keepAlive = keepAlive   -- keep the mapping alive until here
end

print(('='):rep(78))
if failures == 0 then
    print('all checks passed')
    os.exit(0)
end
print(failures .. ' check(s) failed')
os.exit(1)
