-- signature_scanner/init.lua
--
-- MoonLoader signature scanner for samp.dll (0.3.7 R1-R5, 0.3.DL).
--
--   local sig = require('signature_scanner').init()
--   local va  = sig.resolve('F1KeyHandler')      -- absolute address
--   local rva = sig.resolveRva('SAMP_INFO')      -- offset from samp.dll base
--
-- Call init() only after `while not isSampAvailable() do wait(0) end`: before
-- that, samp.dll may not be mapped yet and every lookup fails for no good
-- reason.

local M = {}

M.logger   = require('signature_scanner.logger')
M.memory   = require('signature_scanner.memory')
M.scanner  = require('signature_scanner.scanner')
M.patterns = require('signature_scanner.patterns')
M.version  = require('signature_scanner.version')
M.cache    = require('signature_scanner.cache')
M.resolver = require('signature_scanner.resolver')

M._VERSION = '0.2.1'

local initialised = false

-- opts = {
--   module_name = 'samp.dll',
--   logfile     = nil,   -- a file of its own, relative to <gta>/moonloader; in game the
--                        -- lines go to logs\akacross.log (area sigscan) either way
--   cache_file  = 'config/signature_cache.lua',  -- nil disables persistence
--   refresh     = false,                         -- ignore the cache this run
-- }
function M.init(opts)
    opts = opts or {}
    local moduleName = opts.module_name or 'samp.dll'

    M.logger.init(opts.logfile)

    local det = M.version.detect(moduleName, opts.refresh)
    M.detected = det

    if opts.cache_file ~= false then
        M.cache.load(opts.cache_file or 'config/signature_cache.lua', {
            entry     = det.entry,
            size      = det.size,
            timestamp = det.timestamp,
        })
    end

    M.resolver.init({ module = moduleName, version = det.version })
    initialised = true
    return M
end

local function ensure()
    if not initialised then M.init() end
end

function M.resolve(name, opts)    ensure() return M.resolver.resolve(name, opts)    end
function M.resolveRva(name, opts) ensure() return M.resolver.resolveRva(name, opts) end
function M.resolveAll(opts)       ensure() return M.resolver.resolveAll(opts)       end

-- Persist newly resolved offsets. Cheap and idempotent; a no-op when nothing
-- changed. Worth calling once after your startup resolves.
function M.flush() return M.cache.save() end

return M
