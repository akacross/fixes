-- signature_scanner/example_usage.lua - a module, not a script.
--
--   local example = require('signature_scanner.example_usage')
--   example.disableF1()          -- from inside main(), after isSampAvailable()

local sig = require('signature_scanner')

local M = {}

-- fixes.lua:disableF1KeyAndDialog() resolved by signature, so every build works.
function M.disableF1()
    local memoryLib = require('memory')

    sig.init()

    local keyVa  = sig.resolve('F1KeyHandler')
    local callVa = sig.resolve('ShowHelpDialogCall')
    if not keyVa or not callVa then
        print('[example] could not resolve the F1 handler; nothing patched')
        return false
    end

    memoryLib.fill(keyVa, 0x90, 28, true)
    memoryLib.fill(callVa, 0x90, 5, true)
    sig.flush()                       -- remember the offsets for next launch

    print(string.format('[example] F1 help disabled (%s: handler 0x%X, call 0x%X)',
                        sig.detected.version, keyVa, callVa))
    return true
end

-- Read the g_SAMP / SAMP_INFO pointer.
function M.getSampInfo()
    sig.init()
    local slotVa = sig.resolve('SAMP_INFO')
    if not slotVa then return nil end
    local ptr = sig.memory.readU32(slotVa)      -- the slot holds the struct pointer
    if not ptr or ptr == 0 then return nil end
    return ptr
end

-- Dump everything the current build resolves to, for eyeballing in the log.
function M.dump()
    sig.init()
    for name, r in pairs(sig.resolveAll()) do
        if r.rva then
            print(string.format('[example] %-20s rva=0x%X va=0x%X (%s)', name, r.rva, r.va, r.source))
        else
            print(string.format('[example] %-20s unresolved: %s', name, tostring(r.source)))
        end
    end
end

return M
