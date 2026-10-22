-- signature_scanner/self_test.lua - in-game verification. A module, not a
-- script (MoonLoader auto-runs only files directly in moonloader/):
--
--   require('signature_scanner.self_test').run()
--
-- run() waits for SA-MP itself and never dereferences an RVA as absolute.

local sig      = require('signature_scanner')
local patterns = require('signature_scanner.patterns')
local memory   = require('signature_scanner.memory')
local seed     = require('signature_scanner.seed')
local logger   = require('signature_scanner.logger')

local M = {}

local function waitForSamp()
    if type(isSampAvailable) ~= 'function' then return true end
    while not isSampAvailable() do wait(0) end
    return true
end

function M.run(opts)
    opts = opts or {}
    waitForSamp()

    sig.init({
        module_name = 'samp.dll',
        logfile     = opts.logfile or 'logs/sigscan_selftest.log',
        cache_file  = false,          -- always scan for real during a self-test
        refresh     = true,
    })

    local det = sig.detected
    logger.infof('self-test: %s, entry=0x%X, size=0x%X, timestamp=0x%X, base=0x%X%s',
                 det.version, det.entry or 0, det.size or 0, det.timestamp or 0, det.base or 0,
                 det.packed and ', packed build' or '')

    local info = memory.getModuleInfo('samp.dll')
    for _, sec in ipairs(info.sections) do
        logger.infof('  section %-8s rva=0x%06X size=0x%06X%s%s',
                     sec.name, sec.rva, sec.size,
                     sec.code and ' code' or '', sec.discardable and ' DISCARDABLE(skipped)' or '')
    end

    local pass, fail = 0, 0
    for _, name in ipairs(patterns.names(det.version)) do
        local desc = patterns.get(det.version, name)
        local rva, source = sig.resolveRva(name, { refresh = true })
        if rva then
            local expected = desc.expect and desc.expect[det.version]
            local tag = expected and (expected == rva and ' == documented' or
                        string.format(' != documented 0x%X', expected)) or ''
            local dump = ''
            if not desc.extract and memory.rvaInImage(info, rva) then
                dump = '  bytes: ' .. memory.hexdump(info.base + rva, math.min(desc.size or 16, 16))
            end
            logger.infof('  PASS %-19s rva=0x%-8X va=0x%-8X via %s%s%s',
                         name, rva, info.base + rva, source, tag, dump)
            if expected and expected ~= rva then fail = fail + 1 else pass = pass + 1 end
        else
            fail = fail + 1
            logger.warnf('  FAIL %-19s %s', name, tostring(source))
            -- Only ever seed from a documented offset for THIS build; never from
            -- an offset belonging to another version or another module.
            local known = sig.version.getKnownRva(name, det.version)
            if known and not desc.extract then
                local s = seed.suggestUnique('samp.dll', known)
                if s then
                    logger.infof('    seeded from the known 0x%X (%d match(es), unique=%s):',
                                 known, s.matches, tostring(s.unique))
                    logger.info('    ' .. s.pattern)
                end
            end
        end
    end

    logger.infof('self-test: %d passed, %d failed', pass, fail)
    return pass, fail
end

return M
