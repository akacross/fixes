-- signature_scanner/cache.lua - persists resolved RVAs so a scan happens once
-- per build, not per launch. Entries are RVAs keyed by version (samp.dll is
-- relocatable, so a VA would be wrong next launch); the whole file is dropped
-- when the module's entry point / size / timestamp changes.

local logger = require('signature_scanner.logger')

local M = {}

local store = { identity = nil, entries = {} }
local path                  -- resolved absolute path
local dirty = false

local function resolve(p)
    if not p then return nil end
    if p:match('^%a:[\\/]') or p:match('^[\\/]') then return p end
    if type(getWorkingDirectory) == 'function' then
        return getWorkingDirectory() .. '\\' .. p:gsub('/', '\\')
    end
    return p
end

local function ensureDir(p)
    local dir = p:match('^(.*)[\\/][^\\/]+$')
    if not dir then return end
    if type(doesDirectoryExist) == 'function' and type(createDirectory) == 'function' then
        if not doesDirectoryExist(dir) then createDirectory(dir) end
    end
end

local function sameIdentity(a, b)
    if not a or not b then return false end
    return a.entry == b.entry and a.size == b.size and a.timestamp == b.timestamp
end

-- identity = { entry, size, timestamp } from version.detect()
function M.load(file, identity)
    path = resolve(file)
    store = { identity = identity, entries = {} }
    dirty = false
    if not path then return end

    local chunk = loadfile(path)
    if not chunk then return end                 -- missing or syntactically broken: start clean
    local ok, data = pcall(chunk)
    if not ok then
        logger.warn('cache file errored while loading; ignoring it')
        return
    end
    -- an empty file is "nothing cached yet", not corruption: no warning
    if data == nil then return end
    if type(data) ~= 'table' then
        logger.warnf('cache file returned a %s instead of a table; ignoring it', type(data))
        return
    end
    if not sameIdentity(data.identity, identity) then
        logger.info('cache is for a different samp.dll build; discarding it')
        return
    end
    store.entries = type(data.entries) == 'table' and data.entries or {}
end

local function bucket(version)
    version = version or 'UNKNOWN'
    store.entries[version] = store.entries[version] or {}
    return store.entries[version]
end

function M.get(version, name)
    local b = store.entries[version or 'UNKNOWN']
    return b and b[name] or nil
end

function M.set(version, name, rva)
    if type(rva) ~= 'number' then return end
    local b = bucket(version)
    if b[name] == rva then return end
    b[name] = rva
    dirty = true
end

function M.clear()
    store.entries = {}
    dirty = true
end

function M.save(force)
    if not path then return false end
    if not dirty and not force then return true end

    local out = { 'return {\n' }
    local id = store.identity
    if id then
        out[#out + 1] = string.format('  identity = { entry = 0x%X, size = 0x%X, timestamp = 0x%X },\n',
                                      id.entry or 0, id.size or 0, id.timestamp or 0)
    end
    out[#out + 1] = '  entries = {\n'
    local versions = {}
    for v in pairs(store.entries) do versions[#versions + 1] = v end
    table.sort(versions)
    for _, v in ipairs(versions) do
        local names = {}
        for n in pairs(store.entries[v]) do names[#names + 1] = n end
        table.sort(names)
        out[#out + 1] = string.format('    [%q] = {\n', v)
        for _, n in ipairs(names) do
            out[#out + 1] = string.format('      [%q] = 0x%X,\n', n, store.entries[v][n])
        end
        out[#out + 1] = '    },\n'
    end
    out[#out + 1] = '  },\n}\n'

    ensureDir(path)
    local f = io.open(path, 'w')
    if not f then
        logger.warnf('cannot write cache to %s', path)
        return false
    end
    f:write(table.concat(out))
    f:close()
    dirty = false
    return true
end

return M
