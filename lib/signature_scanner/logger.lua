-- signature_scanner/logger.lua
-- Minimal logger. In MoonLoader the lines go through akacross.log, as area
-- `sigscan` of the calling script: logs\akacross.log takes them all, and
-- moonloader.log warnings and errors. Without it (or outside the game: the
-- host test) they are printed. An init() path adds a file of their own.
local M = {}

-- akacross.log's scope, only where it writes its file (in MoonLoader): outside
-- the game it would print warnings and errors only, and the host test wants
-- every line. Looked up once; a missing akacross.log keeps print.
local shared
do
    local ok, logM = pcall(require, 'akacross.log')
    if ok and type(logM) == 'table' and logM.scope and logM.defaultFilePath and logM.defaultFilePath() then
        shared = logM.scope('sigscan')
    end
end
local LEVELS = {['[info]'] = 'info', ['[warn]'] = 'warn', ['[error]'] = 'error'}

local logfile        -- resolved absolute path, or nil when file logging is off
local prefix = '[sigscan] '

-- io.open() relative paths resolve against the GTA root, not moonloader/.
local function resolve(path)
    if not path then return nil end
    if path:match('^%a:[\\/]') or path:match('^[\\/]') then return path end
    if type(getWorkingDirectory) == 'function' then
        return getWorkingDirectory() .. '\\' .. path:gsub('/', '\\')
    end
    return path
end

local function ensureDir(path)
    local dir = path:match('^(.*)[\\/][^\\/]+$')
    if not dir then return end
    if type(doesDirectoryExist) == 'function' and type(createDirectory) == 'function' then
        if not doesDirectoryExist(dir) then createDirectory(dir) end
    end
end

function M.init(path)
    logfile = resolve(path)
    if logfile then
        ensureDir(logfile)
        local f = io.open(logfile, 'a')
        if f then
            f:close()
        else
            logfile = nil       -- unwritable: degrade to print-only instead of failing silently
        end
    end
end

local function emit(level, msg)
    local line = prefix .. level .. ' ' .. tostring(msg)
    if shared then
        pcall(shared[LEVELS[level]], '%s', tostring(msg))
    else
        print(line)
    end
    if not logfile then return end
    local f = io.open(logfile, 'a')
    if not f then return end
    f:write(line, '\n')
    f:close()
end

function M.info(msg)  emit('[info]', msg)  end
function M.warn(msg)  emit('[warn]', msg)  end
function M.error(msg) emit('[error]', msg) end

-- Convenience: format-style logging without building strings at every call site.
function M.infof(fmt, ...)  emit('[info]',  string.format(fmt, ...)) end
function M.warnf(fmt, ...)  emit('[warn]',  string.format(fmt, ...)) end
function M.errorf(fmt, ...) emit('[error]', string.format(fmt, ...)) end

return M
