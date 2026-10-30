-- Host test for fixes.lua: 0.5.0's optional patches - the plate pair applies
-- only while its option is on (off by default), /fixes switches it at once -
-- off writes the stock bytes back - and the choice is saved; and 0.6.0's
-- removed-buildings fix against SA-MP R5's real store and lookup bytes, at its
-- own base and relocated; and 0.7.0's animation-wipe fix, 0.8.x's visible-list
-- bound, 0.9.0's entity references, 0.10.0's occluders and 0.11.0's
-- big-message and texture-dictionary patches against gta_sa.exe 1.0 US's bytes,
-- 0.11.0's float-clock sites, per-site log lines and /fixes patches, and its RPC checks letting
-- well-formed RPCs through untouched; and 0.12.0's exception filter tail. The real
-- script runs against a fake ffi and memory backed by a table of bytes.
--
--     luajit lib/fixes/test/fixes_test.lua
--     luajit lib/fixes/test/fixes_test.lua --store-bytes rb_store.bin --wipe-bytes anim_wipe.bin
--
-- The options also write the new store's and the new wipe's machine code,
-- built for rb_store_test.c's and anim_wipe_test.c's addresses, which run
-- them for real under Wine.

-- Line-buffer stdout: piped, it flushes only at exit, so a crash's traceback
-- (stderr, unbuffered) would land above every line printed before it.
io.stdout:setvbuf('line')

local here = (debug.getinfo(1, 'S').source:sub(2):match('(.*[/\\])') or './')
local root = here .. '../../../'

local passed, failed = 0, 0
local function ok(cond, what)
    if cond then passed = passed + 1 else failed = failed + 1; print('  FAIL: ' .. what) end
end

-- game memory: only the bytes the test cares about; everything else reads 0
local mem = {}
local function put(va, ...) for i, b in ipairs({...}) do mem[va + i - 1] = b end end
local function get(va, n) local t = {} for i = 1, n do t[i] = mem[va + i - 1] or 0 end return t end
local function same(a, b) for i = 1, #b do if a[i] ~= b[i] then return false end end return true end
local SETUP, RESET = 0x6D6517, 0x6D0E43
put(SETUP, 0x75, 0x08); put(RESET, 0x75, 0x13)   -- stock: the automobile-only guards

local function getU32(va)
    return (mem[va] or 0) + (mem[va + 1] or 0) * 0x100 + (mem[va + 2] or 0) * 0x10000 + (mem[va + 3] or 0) * 0x1000000
end
local function putU32(va, v)
    v = v % 0x100000000
    for i = 0, 3 do mem[va + i] = math.floor(v / 0x100 ^ i) % 0x100 end
end
local allocs, nextBlock = 0, 0x30000000
local unmapped                              -- va -> true: VirtualQuery reports it reserved, not committed
local thiscalls = {}   -- {function, this}
local fakeFfi = {
    cdef = function() end,
    cast = function(ctype, va)
        va = tonumber(va)
        if ctype == 'uintptr_t' or ctype == 'uint32_t' then return va end
        if ctype:find('__thiscall', 1, true) then
            return function(this) thiscalls[#thiscalls + 1] = {va, tonumber(this)} end
        end
        if ctype:find('int32_t*', 1, true) then
            return setmetatable({}, {__index = function(_, i) return getU32(va + 4 * i) end,
                __newindex = function(_, i, v) putU32(va + 4 * i, v) end})
        end
        return setmetatable({__va = va}, {__index = function(_, i) return mem[va + i] or 0 end,
            __newindex = function(_, i, v) mem[va + i] = v end})
    end,
    string = function(p, n)   -- the bytes at a cast pointer
        local va, t = p.__va, {}
        for i = 0, n - 1 do t[#t + 1] = string.char(mem[va + i] or 0) end
        return table.concat(t)
    end,
    new = function() return {[0] = 0, 0, 0, 0, 0, 0, 0} end,
    C = {VirtualQuery = function(at, mbi)   -- one committed, readable region, or reserved where told
            local va = at.__va
            mbi[0], mbi[3] = va, 0x1000
            mbi[4], mbi[5] = (unmapped and unmapped(va)) and 0x2000 or 0x1000, 0x20
            return 28
        end,
        VirtualAlloc = function(_, size, kind, protect)
        allocs = allocs + 1
        local block = nextBlock
        nextBlock = nextBlock + 0x100000
        return block
    end},
}
local files, chat, commands, printed = {}, {}, {}, {}
local loadedModules = {}   -- name -> handle, for getModuleHandle
local sampBase = 0
local env = setmetatable({
    require = function(name)
        if name == 'ffi' then return fakeFfi end
        if name == 'memory' then return {setuint8 = function(va, v) mem[va] = v end} end
        if name == 'akacross.perf' or name == 'signature_scanner' then error('not here') end
        return require(name)
    end,
    script_name = function() end, script_version = function() end,
    getWorkingDirectory = function() return 'ML' end,
    getModuleHandle = function(name)
        if name == 'samp.dll' then return sampBase end
        return loadedModules[name] or 0
    end,
    decodeJson = function(t) return files.decoded end,
    encodeJson = function(t) files.saved = {plates = t.plates}; return 'json' end,
    io = {open = function(path, mode)
        if mode == 'w' then return {write = function() end, close = function() end} end
        if files.decoded then return {read = function() return 'json' end, close = function() end} end
        return nil
    end},
    sampAddChatMessage = function(text) chat[#chat + 1] = text end,
    sampRegisterChatCommand = function(name, fn) commands[name] = fn end,
    print = function(text) printed[#printed + 1] = text end,
}, {__index = _G})
local chunk = assert(loadfile(root .. 'fixes.lua'))
setfenv(chunk, env)()

local function said(text)   -- the chat line /fixes printed with `text` in it
    for i = #chat, math.max(1, #chat - 12), -1 do if chat[i]:find(text, 1, true) then return chat[i] end end
end
print('\n== off by default: the plate guards stay stock')
-- main() would wait for SA-MP: reach its local loadOptions and run the start by hand
local loadOptions = (function()
    local i = 1
    while true do
        local name, value = debug.getupvalue(env.main, i)
        if not name then return nil end
        if name == 'loadOptions' then return value end
        i = i + 1
    end
end)()
ok(type(loadOptions) == 'function', 'main reaches loadOptions')
loadOptions()
env.applyGamePatches('early')
ok(same(get(SETUP, 2), {0x75, 0x08}) and same(get(RESET, 2), {0x75, 0x13}), 'both plate sites untouched')

print('\n== /fixes plates on: both sites at once, and saved')
env.fixesCommand('plates on')
ok(same(get(SETUP, 2), {0x90, 0x90}) and same(get(RESET, 2), {0x90, 0x90}), 'both guards NOPed')
ok(files.saved and files.saved.plates == true, 'the choice is saved')
ok(chat[#chat]:find('2 site', 1, true), 'and said: ' .. tostring(chat[#chat]))

print('\n== /fixes plates off: the stock bytes go back')
env.fixesCommand('plates off')
ok(same(get(SETUP, 2), {0x75, 0x08}) and same(get(RESET, 2), {0x75, 0x13}), 'both guards restored')
ok(files.saved.plates == false, 'saved off')

print('\n== a saved "on" applies at the next start')
files.decoded = {plates = true}
loadOptions()
env.applyGamePatches('early')
ok(same(get(SETUP, 2), {0x90, 0x90}) and same(get(RESET, 2), {0x90, 0x90}), 'applied from the saved option')

print('\n== another mod\'s bytes are left alone')
put(SETUP, 0xEB, 0x08)
env.fixesCommand('plates off')
ok(same(get(SETUP, 2), {0xEB, 0x08}), 'a site holding somebody else\'s bytes is not touched')
ok(chat[#chat]:find('left alone', 1, true), 'and said: ' .. tostring(chat[#chat]))

print('\n== /fixes lists the options; nonsense is answered, not obeyed')
local before = #chat
env.fixesCommand('')
do
    local listing = {}
    for i = before + 1, #chat do if chat[i]:find('options: ', 1, true) then listing[#listing + 1] = chat[i] end end
    local all = table.concat(listing, ' ')
    ok(#listing >= 7 and all:find('keepcars on', 1, true) and all:find('wheelfps on', 1, true) and all:find('swimfps on', 1, true) and all:find('background on, chutefps on, crashfix on', 1, true) and all:find('drift off', 1, true) and all:find('floatclock on', 1, true)
        and all:find('turnfps on', 1, true) and all:find('vehiclefps on', 1, true) and all:find('freezecar on', 1, true) and all:find('chutefps on', 1, true) and all:find('pedfps on', 1, true) and all:find('pushfps on', 1, true)
        and all:find('occluders on', 1, true) and all:find('plates o', 1, true) and all:find('textcache on', 1, true),
        'the options, six to a line, with their states: ' .. all)
    env.fixesCommand('frameclone')
    ok(chat[#chat]:find('frameclone: off - crash fix 19', 1, true), '/fixes <option> says what it does: ' .. tostring(chat[#chat]))
end
env.fixesCommand('textcache off')
ok(chat[#chat]:find('textcache off: 1 site', 1, true), '/fixes textcache off takes the hook out: ' .. tostring(chat[#chat]))
env.fixesCommand('textcache on')
ok(chat[#chat]:find('textcache on: 0 site', 1, true), 'on again: nothing to hook here (no d3dx9_25.dll): ' .. tostring(chat[#chat]))
ok(printed[#printed]:find('text cache', 1, true), 'and said why: ' .. tostring(printed[#printed]))
env.fixesCommand('plates maybe')
ok(chat[#chat]:find('on|off', 1, true), 'a bad value gets the usage')

--------------------------------------------------------------------------------
-- 0.6.0: removed buildings
--------------------------------------------------------------------------------
-- SA-MP R5's store (from its `mov ecx,[count]`) and lookup (from its
-- `mov eax,[count]`), as they sit in samp.dll at its own base 0x10000000:
-- count 0x1014FD88, table 0x1014AF68.
local STORE_R5 = {0x9D3EF, '8B 0D 88 FD 14 10 8B 54 24 20 8D 04 89 C1 E0 02 89 B0 68 AF 14 10 89 90 6C AF 14 10 ' ..
    '8B 54 24 24 89 90 70 AF 14 10 8B 54 24 28 83 C4 14 89 90 74 AF 14 10 8B 54 24 18 41 89 90 78 AF 14 10 ' ..
    '89 0D 88 FD 14 10 5E C3'}
local LOOKUP_R5 = {0x9D050, 'A1 88 FD 14 10 53 55 56 57 33 FF 85 C0 74 4D 8B 5C 24 20 8B 6C 24 1C BE 68 AF 14 10 ' ..
    '8D 64 24 00 8B 06 83 F8 FF 74 06 39 44 24 14 75 22 8B 46 0C 8B 4E 08 8B 56 04 50 8B 44 24 1C 51 52 53 55 50 ' ..
    'E8 DB 78 01 00 D8 5E 10 DF E0 F6 C4 41 7B 14 A1 88 FD 14 10'}
-- the dwords the loader relocates: store +2/+12h/+18h/+22h/+2Fh/+3Ah/+40h, lookup +1/+18h/+50h
local STORE_RELOCS, LOOKUP_RELOCS = {0x02, 0x12, 0x18, 0x22, 0x2F, 0x3A, 0x40}, {0x01, 0x18, 0x50}
local COUNT_RVA, TABLE_RVA = 0x14FD88, 0x14AF68

-- The store as i686-w64-mingw32-as assembled it, for count 0x11223344, table
-- 0x55667788 and cap 0x99AA.
local STORE_AS = '83 C4 14 8B 0D 44 33 22 11 BA 88 77 66 55 85 C9 74 2E 39 32 75 24 8B 44 24 0C 39 42 04 75 1B ' ..
    '8B 44 24 10 39 42 08 75 12 8B 44 24 14 39 42 0C 75 09 8B 44 24 18 39 42 10 74 39 83 C2 14 49 75 D2 ' ..
    '8B 0D 44 33 22 11 81 F9 AA 99 00 00 73 25 89 32 8B 44 24 0C 89 42 04 8B 44 24 10 89 42 08 8B 44 24 14 ' ..
    '89 42 0C 8B 44 24 18 89 42 10 41 89 0D 44 33 22 11 5E C3'

local function hex(s) local t = {} for b in s:gmatch('%x%x') do t[#t + 1] = tonumber(b, 16) end return t end

-- A fresh samp.dll R5 at `base`: PE header enough for getSampVersion, the two
-- sites relocated, `count` entries in the old table.
local function loadSamp(base, count)
    for k in pairs(mem) do if k >= 0x01000000 then mem[k] = nil end end
    sampBase = base
    putU32(base + 0x3C, 0x118); putU32(base + 0x118 + 40, 0xCBC90)   -- e_lfanew, entry point = R5
    local delta = base - 0x10000000
    for _, site in ipairs({{STORE_R5, STORE_RELOCS}, {LOOKUP_R5, LOOKUP_RELOCS}}) do
        local va = base + site[1][1]
        put(va, unpack(hex(site[1][2])))
        for _, o in ipairs(site[2]) do putU32(va + o, getU32(va + o) + delta) end
    end
    putU32(base + COUNT_RVA, count)
    for i = 0, math.min(count, 1000) - 1 do
        putU32(base + TABLE_RVA + i * 20, 600 + i)                       -- model
        putU32(base + TABLE_RVA + i * 20 + 8, 0xC37A01FF)                 -- y = -250.0078
    end
end

local function upvalue(fn, wanted)
    local i = 1
    while true do
        local name, value = debug.getupvalue(fn, i)
        if not name then return nil end
        if name == wanted then return value end
        i = i + 1
    end
end

local function snapshot(lo, hi)
    local t = {}
    for k, v in pairs(mem) do if k >= lo and k < hi then t[k] = v end end
    return t
end
local function unchanged(a, b)
    for k, v in pairs(a) do if b[k] ~= v then return false end end
    for k, v in pairs(b) do if a[k] ~= v then return false end end
    return true
end

for _, base in ipairs({0x10000000, 0x02B20000}) do
    local tag = ('base 0x%08X'):format(base)
    print(('\n== removed buildings, SA-MP R5 at %s: the table moves, the store jumps to ours'):format(tag))
    loadSamp(base, 3)
    allocs = 0
    local storeVa, lookupVa = base + 0x9D3EF, base + 0x9D068
    ok(env.fixRemovedBuildings() == true, tag .. ': applied')
    ok(allocs == 1, tag .. ': one block allocated')
    local block = nextBlock - 0x100000
    ok(mem[storeVa] == 0xE9 and (storeVa + 5 + getU32(storeVa + 1)) % 0x100000000 == block + 0x10,
        tag .. ': the store starts with a jmp to the block\'s code')
    ok(getU32(lookupVa) == block + 0x100, tag .. ': the lookup reads the new table')
    ok(getU32(block) == 0x62527846 and getU32(block + 4) == 5000 and getU32(block + 8) == block + 0x100
        and getU32(block + 12) == base + COUNT_RVA, tag .. ': header: magic, cap 5000, table, count')
    ok(getU32(base + COUNT_RVA) == 3, tag .. ': SA-MP\'s count is left at 3')
    local carried = true
    for i = 0, 3 * 20 - 1 do
        if (mem[block + 0x100 + i] or 0) ~= (mem[base + TABLE_RVA + i] or 0) then carried = false end
    end
    ok(carried and getU32(block + 0x100 + 2 * 20) == 602, tag .. ': the 3 stored entries are carried over')
    -- the code in the block is the assembler's, with this run's addresses
    local want = hex(STORE_AS)
    local function sub(at, v) local b = {v % 256, math.floor(v / 256) % 256, math.floor(v / 65536) % 256, math.floor(v / 16777216)}
        for i = 1, 4 do want[at + i] = b[i] end end
    sub(5, base + COUNT_RVA); sub(10, block + 0x100); sub(0x42, base + COUNT_RVA); sub(0x48, 5000); sub(0x6F, base + COUNT_RVA)
    local code = {}
    for i = 1, #want do code[i] = mem[block + 0x10 + i - 1] or 0 end
    ok(#want == 117 and same(code, want), tag .. ': the store in the block is the assembled one')
    -- the untouched rest of the stock store is still there, and the lookup's other bytes
    ok(same(get(storeVa + 5, 4), {math.floor((base + COUNT_RVA) / 0x1000000), 0x8B, 0x54, 0x24}),
        tag .. ': only 5 bytes of the store were written')   -- +5: the count address's top byte
    ok(mem[lookupVa - 1] == 0xBE and mem[lookupVa - 0x18] == 0xA1, tag .. ': only the lookup\'s operand was written')

    print(('\n== %s: a reload finds it in place and leaves it'):format(tag))
    local before = snapshot(0x01000000, 0x7FFFFFFF)
    ok(env.fixRemovedBuildings() == true, tag .. ': still fine')
    ok(allocs == 1, tag .. ': no second block')
    ok(unchanged(before, snapshot(0x01000000, 0x7FFFFFFF)), tag .. ': not a byte changed')
    ok(printed[#printed]:find('already moved', 1, true), tag .. ': and said so: ' .. tostring(printed[#printed]))
end

print('\n== /fixes shows how many are stored')
putU32(0x02B20000 + COUNT_RVA, 1242)
env.fixesCommand('')
ok(said('removed buildings: 1242 stored, room for 5000'), 'listed')

print('\n== another mod\'s jmp at the store: refused, nothing written')
loadSamp(0x10000000, 0)
put(0x10000000 + 0x9D3EF, 0xE9, 0x00, 0x10, 0x00, 0x00)
allocs = 0
local before = snapshot(0x01000000, 0x7FFFFFFF)
ok(env.fixRemovedBuildings() == false, 'refused')
ok(allocs == 0 and unchanged(before, snapshot(0x01000000, 0x7FFFFFFF)), 'nothing allocated, nothing written')
ok(printed[#printed]:find('not this script', 1, true), 'said why: ' .. tostring(printed[#printed]))

for _, case in ipairs({{'the handler', 0x9D3D0}, {'the lookup', 0x9D050}}) do
    print(('\n== open.mp\'s client replaces %s: stand down, naming the owner, nothing written'):format(case[1]))
    loadSamp(0x10000000, 0)
    put(0x10000000 + case[2], 0xE9, 0x00, 0x00, 0x10, 0x00)       -- jmp somewhere else entirely
    allocs = 0
    local snap = snapshot(0x01000000, 0x7FFFFFFF)
    ok(env.fixRemovedBuildings() == false and allocs == 0, case[1] .. ': not applied, nothing allocated')
    ok(unchanged(snap, snapshot(0x01000000, 0x7FFFFFFF)), case[1] .. ': nothing written')
    ok(printed[#printed]:find('replaced by code at 0x', 1, true) and printed[#printed]:find('standing down', 1, true),
        case[1] .. ': said so: ' .. tostring(printed[#printed]))
end
env.fixesCommand('')
ok(said('removed buildings: left to code at 0x'), '/fixes says who has it')

print('\n== a store that is not the stock code: refused')
loadSamp(0x10000000, 0)
mem[0x10000000 + 0x9D3EF + 0x2A] = 0x90                          -- the add esp,14h
allocs = 0
before = snapshot(0x01000000, 0x7FFFFFFF)
ok(env.fixRemovedBuildings() == false and allocs == 0, 'refused, nothing allocated')
ok(unchanged(before, snapshot(0x01000000, 0x7FFFFFFF)), 'nothing written')

print('\n== store and lookup naming different tables: refused')
loadSamp(0x10000000, 0)
putU32(0x10000000 + 0x9D068, 0x1014AF00)
allocs = 0
ok(env.fixRemovedBuildings() == false and allocs == 0, 'refused, nothing allocated')

print('\n== SA-MP had already overflowed: the first 1000 are kept, and it says so')
loadSamp(0x10000000, 1235)
ok(env.fixRemovedBuildings() == true, 'applied')
local block = nextBlock - 0x100000
ok(getU32(0x10000000 + COUNT_RVA) == 1000, 'count set back to 1000')
ok(getU32(block + 0x100 + 999 * 20) == 1599, 'entry 999 carried')
local warned = false
for _, line in ipairs(printed) do if line:find('already stored 1235', 1, true) then warned = true end end
ok(warned, 'and the log says so')

print('\n== not R5, no scanner: not supported, nothing written')
loadSamp(0x10000000, 0)
putU32(0x10000000 + 0x118 + 40, 0x12345)                          -- an unknown entry point
allocs = 0
ok(env.fixRemovedBuildings() == false and allocs == 0, 'not applied')
ok(printed[#printed]:find('not supported', 1, true), 'said why: ' .. tostring(printed[#printed]))

--------------------------------------------------------------------------------
-- 0.7.0: animation wipes
--------------------------------------------------------------------------------
-- RpAnimBlendClumpRemoveAllAssociations as it sits in gta_sa.exe 1.0 US, and
-- the replacement as i686-w64-mingw32-as assembled it, for offset 0x11111111,
-- list 0x22222222, entries 0x33333333 and told 0x44444444.
local AW_VA = 0x4D6C00
local AW_STOCK = '8B4424048B0D78F8B5008B04018B0085C07417568B308D48FC85C974068B116A01FF1285F68BC675EB5EC3'
local WIPE_AS = '8B4424048B0D111111118B04018B0085C0744A5657538B308D78FC837F3002752B8B4F34BA22222222' ..
    '8B1D3333333385DB74193B0A740683C2044BEBF2FF773857FFD183C408FF054444444489F98B116A01FF1289F085C075BC5B5F5EC3'
local AW_CALLBACKS = {0x678E60, 0x692550, 0x67A380, 0x61A8A0, 0x61DAE0, 0x621BE0, 0x68B7D0, 0x690C60, 0x5DFD70}
local function stockWipe()
    for k = AW_VA, AW_VA + 0x40 do mem[k] = nil end
    put(AW_VA, unpack(hex(AW_STOCK)))
end

print('\n== animation wipes: the function jumps to the block, which holds the code and the game\'s callbacks')
stockWipe()
allocs = 0
ok(env.fixAnimationWipe() == true, 'applied')
ok(allocs == 1, 'one block allocated')
local awBlock = nextBlock - 0x100000
ok(mem[AW_VA] == 0xE9 and (AW_VA + 5 + getU32(AW_VA + 1)) % 0x100000000 == awBlock + 0x10,
    'the function starts with a jmp to the block\'s code')
ok(same(get(AW_VA + 5, 38), {unpack(hex(AW_STOCK), 6)}), 'only those 5 bytes were written')
ok(getU32(awBlock) == 0x77417846 and getU32(awBlock + 4) == 0 and getU32(awBlock + 8) == #AW_CALLBACKS
    and getU32(awBlock + 12) == awBlock + 0x80, 'header: magic, nothing told yet, 9 entries, the list')
local listed = true
for i, cb in ipairs(AW_CALLBACKS) do if getU32(awBlock + 0x80 + 4 * (i - 1)) ~= cb then listed = false end end
ok(listed, 'the list holds the 9 delete callbacks, CTaskSimpleInAir\'s first')
do
    local want = hex(WIPE_AS)
    local function sub(at, v) for i = 1, 4 do want[at + i] = math.floor(v / 0x100 ^ (i - 1)) % 0x100 end end
    sub(0x06, 0xB5F878); sub(0x25, awBlock + 0x80); sub(0x2B, awBlock + 8); sub(0x48, awBlock + 4)
    ok(#want == 94 and same(get(awBlock + 0x10, 94), want), 'the code in the block is the assembled one')
end
ok(printed[#printed]:find('runs the game\'s 9 task delete callbacks', 1, true), 'logged: ' .. tostring(printed[#printed]))

print('\n== animation wipes: a reload finds it in place and leaves it')
do
    local before = snapshot(0x00400000, 0x7FFFFFFF)
    ok(env.fixAnimationWipe() == true and allocs == 1, 'still fine, no second block')
    ok(unchanged(before, snapshot(0x00400000, 0x7FFFFFFF)), 'not a byte changed')
    ok(printed[#printed]:find('already patched', 1, true), 'and said so: ' .. tostring(printed[#printed]))
end

print('\n== /fixes shows how many callbacks have run')
putU32(awBlock + 4, 3)
env.fixesCommand('')
ok(said('animation wipes: 3 task callbacks run'), 'listed')

print('\n== animation wipes: another mod\'s jmp there: stand down, naming it, nothing written')
stockWipe()
put(AW_VA, 0xE9, 0x00, 0x00, 0x10, 0x00)
allocs = 0
do
    local before = snapshot(0x00400000, 0x7FFFFFFF)
    ok(env.fixAnimationWipe() == false and allocs == 0, 'not applied, nothing allocated')
    ok(unchanged(before, snapshot(0x00400000, 0x7FFFFFFF)), 'nothing written')
    ok(printed[#printed]:find('replaced by code at 0x', 1, true) and printed[#printed]:find('standing down', 1, true),
        'said so: ' .. tostring(printed[#printed]))
end
env.fixesCommand('')
ok(said('animation wipes: left to code at 0x'), '/fixes says who has it')

print('\n== animation wipes: anything but the stock function is refused')
for _, case in ipairs({{'another plugin offset', 6, 0x79}, {'a changed loop', 0x21, 0x90}}) do
    stockWipe()
    mem[AW_VA + case[2]] = case[3]
    allocs = 0
    local before = snapshot(0x00400000, 0x7FFFFFFF)
    ok(env.fixAnimationWipe() == false and allocs == 0, case[1] .. ': refused, nothing allocated')
    ok(unchanged(before, snapshot(0x00400000, 0x7FFFFFFF)), case[1] .. ': nothing written')
end
ok(printed[#printed]:find('refusing the animation-wipe fix', 1, true), 'said why: ' .. tostring(printed[#printed]))

--------------------------------------------------------------------------------
-- 0.8.0: the visible-entity list bound
--------------------------------------------------------------------------------
-- CRenderer::AddEntityToRenderList's store, ret and the padding before
-- SetupScanLists (0x553540), as gta_sa.exe 1.0 US has them, behind the jne at
-- 0x5534DB that shares the ret.
local VIS_VA = 0x553521
local VIS_STOCK = 'A14468B7008934859858B70040A34468B7005EC39090909090909090909090'
local VIS_PATCHED = 'EB12730E908934859858B70040A34468B7005EC3A14468B700663DE803EBE3'
local function stockVis()
    for k = 0x5534D0, 0x553540 do mem[k] = nil end
    put(0x5534DB, 0x75, 0x56)
    put(VIS_VA, unpack(hex(VIS_STOCK)))
end

print('\n== visible list: the count is checked in the padding, the store stays where it was')
stockVis()
env.applyGamePatches('early')
ok(same(get(VIS_VA, 31), hex(VIS_PATCHED)), 'jmp check / jae ret / store / inc / write back / ret / check')
ok(getU32(0x553529) == 0xB75898, 'the store\'s operand (ProperShaders reads it) still names the array')
ok(same(get(0x553533, 2), {0x5E, 0xC3}) and same(get(0x5534DB, 2), {0x75, 0x56}),
    'the pop esi / ret the jne at 0x5534DB lands on is untouched, and so is the jne')
ok(mem[0x553540] == nil, 'nothing past the padding')

print('\n== visible list: a second run finds it in place')
env.applyGamePatches('early')
ok(same(get(VIS_VA, 31), hex(VIS_PATCHED)), 'unchanged')

local function findPrinted(text)
    for i = #printed, 1, -1 do if printed[i]:find(text, 1, true) then return printed[i] end end
end

print('\n== visible list: a limit adjuster moved the array: stand down, nothing written')
stockVis()
local baseline = env.applyGamePatches('early')
stockVis()
put(0x553529, 0x00, 0x10, 0x00, 0x20)   -- its own, bigger array
local counts = env.applyGamePatches('early')
ok(mem[VIS_VA] == 0xA1 and getU32(0x553529) == 0x20001000 and same(get(0x553535, 11), hex('9090909090909090909090')),
    'the store and the padding are as the adjuster left them')
ok(counts.conflict == baseline.conflict + 1 and counts.refused == baseline.refused,
    'counted as owned elsewhere, not refused')
local said = findPrinted('visible list bound')
ok(said and said:find('owned by a limit adjuster (ms_aVisibleEntityPtrs moved to 0x20001000)', 1, true)
    and said:find('standing down', 1, true), 'said so: ' .. tostring(said))

print('\n== visible list: a loaded limit adjuster is named')
stockVis()
put(0x553529, 0x00, 0x10, 0x00, 0x20)
loadedModules['$fastman92limitAdjuster.asi'] = 0x60000000
env.applyGamePatches('early')
said = findPrinted('visible list bound')
ok(said and said:find('owned by $fastman92limitAdjuster.asi', 1, true), 'named: ' .. tostring(said))
loadedModules['$fastman92limitAdjuster.asi'] = nil

print('\n== visible list: an adjuster installed but this limit left alone: the cap applies')
stockVis()
loadedModules['III.VC.SA.LimitAdjuster.asi'] = 0x60000000
env.applyGamePatches('early')
ok(same(get(VIS_VA, 31), hex(VIS_PATCHED)), 'the array is still the 1000-slot one, so it is capped')
loadedModules['III.VC.SA.LimitAdjuster.asi'] = nil

print('\n== visible list: the Open Limit Adjuster hooks the store: stand down, nothing written')
stockVis()
-- its "VisibleEntityPtrs = unlimited", as the game had it 2026-10-21: call 0x783CBF80
local OLA_HOOK = 'E85A8AE777' .. VIS_STOCK:sub(11)
put(VIS_VA, unpack(hex(OLA_HOOK)))
counts = env.applyGamePatches('early')
ok(same(get(VIS_VA, 31), hex(OLA_HOOK)), 'its call and the padding are left as they are')
ok(counts.conflict == baseline.conflict + 1 and counts.refused == baseline.refused,
    'counted as owned elsewhere, not refused')
said = findPrinted('visible list bound')
ok(said and said:find('owned by code at 0x783CBF80 (it hooks the store)', 1, true), 'said so: ' .. tostring(said))

print('\n== visible list: a different function there: refused, nothing written')
stockVis()
put(0x553535, 0xCC)
env.applyGamePatches('early')
ok(mem[VIS_VA] == 0xA1 and mem[0x553535] == 0xCC, 'left as it was')

--------------------------------------------------------------------------------
-- 0.10.0: exterior occluders
--------------------------------------------------------------------------------
-- COccluder::NearCamera starts `sub esp,0x10`; the patch makes it xor al,al / ret.
local NC_VA = 0x71F960
local function stockNearCamera() put(NC_VA, 0x83, 0xEC, 0x10, 0x0F, 0xBF) end

print('\n== occluders: on by default, NearCamera answers false')
stockNearCamera()
env.applyGamePatches('early')
ok(same(get(NC_VA, 5), {0x32, 0xC0, 0xC3, 0x0F, 0xBF}), 'xor al,al / ret, and nothing past the 3 bytes')

print('\n== occluders: /fixes occluders off puts the stock prologue back')
env.fixesCommand('occluders off')
ok(same(get(NC_VA, 3), {0x83, 0xEC, 0x10}), 'stock again')
env.fixesCommand('occluders on')
ok(same(get(NC_VA, 3), {0x32, 0xC0, 0xC3}), 'and on again')

print('\n== occluders: #FixGameMapBuilding_byAIR.asi\'s jmp there: stand down, nothing written')
stockNearCamera()
put(NC_VA, 0xE9, 0xAE, 0x19, 0x0A, 0x78, 0x90, 0x90)   -- its hook, as the game had it
local ncBefore, ncCounts = get(NC_VA, 7), nil
ncCounts = env.applyGamePatches('early')
ok(same(get(NC_VA, 7), ncBefore), 'its jmp is left')
local ncSaid
for i = #printed, 1, -1 do if printed[i]:find('NearCamera', 1, true) then ncSaid = printed[i]; break end end
ok(ncSaid and ncSaid:find('owned by code at 0x787C1313', 1, true) and ncSaid:find('standing down', 1, true),
    'named its target: ' .. tostring(ncSaid))
stockNearCamera()

--------------------------------------------------------------------------------
-- 0.11.0: MiTurboFix's game fixes
--------------------------------------------------------------------------------
-- CMessages::AddBigMessage from its unused strlen to the style load, and the
-- two struct reads in RwTexDictionaryGtaStreamRead(1), as gta_sa.exe 1.0 US has
-- them; the style check as i686-w64-mingw32-as assembled it.
local BM_VA, BM_LOAD = 0x69F2D8, 0x69F2F8
local BM_STOCK = '8A4C241433D283C4103ACA8D442404740F8DA424000000008A4801403ACA75F8'
local BM_PATCHED = '31D283C4106683BC2430030000067771' .. ('90'):rep(16)
local TXD_SITES = {0x730FE1, 0x73109B}
local function stockMiTurbo()
    put(BM_VA, unpack(hex(BM_STOCK)))
    put(BM_LOAD, 0x0F, 0xB7, 0x84, 0x24, 0x30, 0x03, 0x00, 0x00)   -- movzx eax, word [esp+330h]
    for _, va in ipairs(TXD_SITES) do put(va, 0x8B, 0x54, 0x24, 0x08, 0x52, 0x8D) end
end

print('\n== big messages: the style is checked where the unused strlen was')
stockMiTurbo()
env.applyGamePatches('early')
ok(same(get(BM_VA, 32), hex(BM_PATCHED)), 'xor edx,edx / add esp,10h / cmp word [esp+330h],6 / ja / nops')
ok(BM_VA + 16 + 0x71 == 0x69F359, 'the ja lands on the pop esi / add esp,320h / ret')
ok(same(get(BM_LOAD, 8), {0x0F, 0xB7, 0x84, 0x24, 0x30, 0x03, 0x00, 0x00}), 'the style load after it is untouched')

print('\n== texture dictionaries: both struct reads take 4 bytes')
for _, va in ipairs(TXD_SITES) do
    ok(same(get(va, 6), {0x6A, 0x04, 0x90, 0x90, 0x90, 0x8D}), ('0x%X: push 4, nothing past 5 bytes'):format(va))
end

print('\n== a second run finds them in place')
env.applyGamePatches('early')
ok(same(get(BM_VA, 32), hex(BM_PATCHED)) and same(get(TXD_SITES[2], 5), {0x6A, 0x04, 0x90, 0x90, 0x90}), 'unchanged')

print('\n== a hook at a site: stand down, naming it, nothing written')
stockMiTurbo()

--------------------------------------------------------------------------------
-- 0.18.0: CMessages::GetStringLength returns 0 for a null string
--------------------------------------------------------------------------------
local GXT_VA = 0x69DB50
local GXT_STOCK = '8B4C24048A1133C084D2740D8D6424008A51014041 84D275F7C3'
local GXT_PATCHED = '8B4C240433C085C9740980 3C0100740340EBF7C3' .. ('90'):rep(6)
local function stockGxt() put(GXT_VA, unpack(hex(GXT_STOCK))) end

print('\n== GXT strings: a null one has length 0, not a crash at 0x69DB54')
stockGxt()
env.applyGamePatches('early')
ok(same(get(GXT_VA, 26), hex(GXT_PATCHED)), 'test ecx / jz ret / cmp byte [ecx+eax],0 / je ret / inc eax / jmp')
ok(GXT_VA + 10 + 9 == GXT_VA + 19 and get(GXT_VA + 19, 1)[1] == 0xC3, 'the null jz and the end je land on the ret')
env.applyGamePatches('early')
ok(same(get(GXT_VA, 26), hex(GXT_PATCHED)), 'a second run finds it in place')

print('\n== GXT strings: gxt_crash_fix.asi\'s hook at 0x69DB54 is left to it')
stockGxt()
put(GXT_VA + 4, 0xE9); putU32(GXT_VA + 5, 0x13377000 - (GXT_VA + 9))
do
    local before = get(GXT_VA, 26)
    local from = #printed
    env.applyGamePatches('early')
    ok(same(get(GXT_VA, 26), before), 'nothing written')
    local line
    for i = from + 1, #printed do if printed[i]:find('GetStringLength', 1, true) then line = printed[i] end end
    ok(line and line:find('owned by code at 0x13377000', 1, true), 'named: ' .. tostring(line))
end
stockGxt()
put(BM_VA, 0xE9, 0x00, 0x00, 0x10, 0x00)
put(TXD_SITES[1], 0xE8, 0x00, 0x00, 0x10, 0x00)
do
    local bm, txd = get(BM_VA, 32), get(TXD_SITES[1], 5)
    env.applyGamePatches('early')
    ok(same(get(BM_VA, 32), bm) and same(get(TXD_SITES[1], 5), txd), 'both hooks are left as they were')
    local line
    for i = #printed, 1, -1 do if printed[i]:find('AddBigMessage style bound', 1, true) then line = printed[i]; break end end
    ok(line and line:find('owned by code at 0x', 1, true) and line:find('standing down', 1, true), 'named: ' .. tostring(line))
    ok(same(get(TXD_SITES[2], 5), {0x6A, 0x04, 0x90, 0x90, 0x90}), 'the other struct read is still patched')
end

print('\n== a different function at a site: refused, nothing written')
stockMiTurbo()
mem[BM_VA + 0x11] = 0xCC
env.applyGamePatches('early')
ok(mem[BM_VA] == 0x8A and mem[BM_VA + 0x11] == 0xCC, 'left as it was')
stockMiTurbo()

--------------------------------------------------------------------------------
-- 0.11.0: the game timer as a float
--------------------------------------------------------------------------------
-- 21 `fild [m_snTimeInMilliseconds]` loads read m_snTimeInMillisecondsNonClipped
-- (0xB7CB80), and the jge over their `fadd 2^32` becomes a jmp.
local clockSites = {}
for _, p in ipairs(upvalue(env.applyGamePatches, 'gamePatches')) do
    if p.option == 'floatclock' then clockSites[#clockSites + 1] = p end
end
local function stockClock()
    for _, p in ipairs(clockSites) do put(p.va, unpack(p.from)); mem[p.va + #p.from] = 0xD8 end   -- the fadd after
end

print('\n== float clock: 21 loads move to the non-clipped clock, their sign test to a jmp')
ok(#clockSites == 21, '21 sites: ' .. #clockSites)
stockClock()
-- two of the 26 left alone: CBulletInfo::Update and CAutomobile::SpawnFlyingComponent
put(0x736116, 0xDB, 0x05, 0x84, 0xCB, 0xB7, 0x00); put(0x6A8853, 0xDB, 0x05, 0x84, 0xCB, 0xB7, 0x00)
env.applyGamePatches('early')
local allMoved = true
for _, p in ipairs(clockSites) do
    local got = get(p.va, #p.from)
    local want = {unpack(p.from)}
    want[3], want[4], want[5], want[6] = 0x80, 0xCB, 0xB7, 0x00
    want[#want - 1] = 0xEB
    if not same(got, want) or mem[p.va + #p.from] ~= 0xD8 or p.from[#p.from - 1] ~= 0x7D or p.from[#p.from] ~= 0x06 then
        allMoved = false; print(('  0x%X: %s'):format(p.va, table.concat(got, ' ')))
    end
end
ok(allMoved, 'each reads 0xB7CB80, its jge is a jmp, and nothing else in or past its bytes changed')
ok(same(get(0x736116, 6), {0xDB, 0x05, 0x84, 0xCB, 0xB7, 0x00}) and same(get(0x6A8853, 6), {0xDB, 0x05, 0x84, 0xCB, 0xB7, 0x00}),
    'loads that meet the game\'s own stamps are not touched')

print('\n== float clock: a site the audio-clock script patched is its, and named')
do
    local p = clockSites[1]
    put(p.va + 2, 0xE0, 0x10, 0xB6, 0x00); mem[p.va + #p.from - 2] = 0x7D
    local before = get(p.va, #p.from)
    env.applyGamePatches('early')
    ok(same(get(p.va, #p.from), before), 'its bytes are left')
    ok(findPrinted(p.name) and findPrinted(p.name):find('owned by a timer script', 1, true), 'said: ' .. tostring(findPrinted(p.name)))

    env.fixesCommand('floatclock off')
    ok(chat[#chat]:find('20 site(s) changed, 1 left alone', 1, true), 'off puts the other 20 back: ' .. tostring(chat[#chat]))
    local stock = true
    for i = 2, #clockSites do if not same(get(clockSites[i].va, #clockSites[i].from), clockSites[i].from) then stock = false end end
    ok(stock and same(get(p.va, #p.from), before), 'stock again, the script\'s site untouched')
    env.fixesCommand('floatclock on')
    ok(chat[#chat]:find('20 site(s) changed', 1, true), 'on again: ' .. tostring(chat[#chat]))
    local from = #chat
    env.fixesCommand('patches')
    local line
    for i = from + 1, #chat do if chat[i]:find('game timer as a float', 1, true) then line = chat[i] end end
    ok(line and line:find('(21 sites): 1 owned by a timer script (the audio clock, 0xB610E0), 20 applied', 1, true), '/fixes patches counts them: ' .. tostring(line))
end
stockClock()

--------------------------------------------------------------------------------
-- 0.11.0: every patch announces itself; /fixes patches lists them
--------------------------------------------------------------------------------
local function lineAfter(from, text)
    for i = from + 1, #printed do if printed[i]:find(text, 1, true) then return printed[i] end end
end

print('\n== each site gets its own log line')
do
    local from = #printed
    stockMiTurbo(); stockGxt()
    put(TXD_SITES[2], 0x6A, 0x04, 0x90, 0x90, 0x90)                   -- as if a run before did it
    env.applyGamePatches('early')
    local applied = lineAfter(from, 'AddBigMessage style bound')
    ok(applied and applied:find('0x69F2D8 (CMessages::AddBigMessage style bound): applied, 32 byte(s) written', 1, true),
        'applied, with its byte count: ' .. tostring(applied))
    local already = lineAfter(from, 'GtaStreamRead1 struct size')
    ok(already and already:find('already in the wanted state', 1, true), 'already set, named: ' .. tostring(already))
    local considered, opts = 0, upvalue(env.fixesCommand, 'options')
    for _, p in ipairs(upvalue(env.applyGamePatches, 'gamePatches')) do
        if (p.phase or 'early') == 'early' and (p.option == nil or opts[p.option]) then considered = considered + 1 end
    end
    local perSite = 0
    for i = from + 1, #printed do if printed[i]:find('^%[fixes%] 0x%x+ %(') or printed[i]:find('^%[fixes%] refusing 0x') then perSite = perSite + 1 end end
    ok(perSite == considered, ('one line for each of the %d early sites (got %d)'):format(considered, perSite))
    ok(not lineAfter(from, 'gta_sa.exe patches ('), 'and no count in the middle of startup')
end

print('\n== /fixes patches: a summary, then every site with its state')
do
    local patches = upvalue(env.applyGamePatches, 'gamePatches')
    local from = #chat
    env.fixesCommand('patches')
    ok(chat[from + 1] and chat[from + 1]:find('gta_sa.exe patches: ', 1, true) and chat[from + 1]:find('applied', 1, true),
        'the summary first: ' .. tostring(chat[from + 1]))
    local function listed(text) for i = from + 2, #chat do if chat[i]:find(text, 1, true) then return chat[i] end end end
    local grouped = 0
    for _, p in ipairs(patches) do if p.group then grouped = grouped + 1 end end
    ok(grouped == 21 + 73 and #chat - from == #patches - grouped + 2 + 3,
        ('a line for each of the %d ungrouped sites, one for each group, the crash fixes and the other fixes'):format(
        #patches - grouped))
    ok(listed("MTA's multiplayer patches (73 sites): "), 'the MTA group\'s line')
    ok(listed('crash fixes (0 hooks): '), 'the crash fixes, before they ran')
    ok(listed('game timer as a float (21 sites): '), 'the group\'s line: ' .. tostring(listed('game timer as a float')))
    ok(listed('0x69F2D8 CMessages::AddBigMessage style bound: applied'), 'an applied site')
    ok(listed('0x73109B RwTexDictionaryGtaStreamRead1 struct size: already set'), 'an already-set site')
    ok(listed('0x6D6517 CVehicle::SetupRender plate guard: off (option plates)'), 'a site whose option is off')
    ok(listed('0x69DB50 CMessages::GetStringLength null string: applied'), 'the GXT strlen rewritten: ' ..
        tostring(listed('0x69DB50')))
    ok(listed('other fixes: not run yet'), 'and says the other fixes have not run before main has')
end

print('\n== /fixes shows the summary under the options')
do
    local from = #chat
    env.fixesCommand('')
    local line
    for i = from + 1, #chat do if chat[i]:find('gta_sa.exe patches: ', 1, true) then line = chat[i] end end
    ok(line and line:find('/fixes patches lists them', 1, true), 'with the pointer: ' .. tostring(line))
end

--------------------------------------------------------------------------------
-- 0.11.0: MiTurboFix's RPC checks let well-formed RPCs through untouched
--------------------------------------------------------------------------------
-- A byte-aligned stand-in for SAMPFUNCS's bitstreams: these RPCs carry whole
-- bytes, and an "encoded" string here is a 2-byte length and its bytes.
local function newBs() return {data = {}, rd = 0} end
local function w8(bs, v) bs.data[#bs.data + 1] = v % 256 end
local function w16(bs, v) w8(bs, v); w8(bs, math.floor(v / 256)) end
local function w32(bs, v) v = v % 0x100000000; for i = 0, 3 do w8(bs, math.floor(v / 0x100 ^ i)) end end
local function wstr(bs, s) for i = 1, #s do w8(bs, s:byte(i)) end end
local function wstr8(bs, s) w8(bs, #s); wstr(bs, s) end
local function wfixed(bs, s) for i = 1, 32 do w8(bs, s:byte(i) or 0) end end
local function wenc(bs, s) w16(bs, #s); wstr(bs, s) end
local function r8(bs) local v = bs.data[bs.rd / 8 + 1] or 0; bs.rd = bs.rd + 8; return v end
env.raknetBitStreamGetReadOffset = function(bs) return bs.rd end
env.raknetBitStreamSetReadOffset = function(bs, n) bs.rd = n end
env.raknetBitStreamIgnoreBits = function(bs, n) bs.rd = bs.rd + n end
env.raknetBitStreamReadInt8 = r8
env.raknetBitStreamReadInt16 = function(bs) return r8(bs) + r8(bs) * 256 end
env.raknetBitStreamReadInt32 = function(bs)
    local v = r8(bs) + r8(bs) * 0x100 + r8(bs) * 0x10000 + r8(bs) * 0x1000000
    return v >= 0x80000000 and v - 0x100000000 or v
end
env.raknetBitStreamDecodeString = function(bs, max)
    local t = {}
    for i = 1, math.min(env.raknetBitStreamReadInt16(bs), max - 1) do t[i] = string.char(r8(bs)) end
    return table.concat(t)
end
env.raknetBitStreamSetWriteOffset = function(bs, n) for i = #bs.data, n / 8 + 1, -1 do bs.data[i] = nil end end
env.raknetBitStreamEncodeString = wenc

-- -> what onReceiveRpc returned, whether the bitstream is byte for byte as sent,
-- and whether the read offset came back to the start
local function screen(id, bs)
    local sent = {unpack(bs.data)}
    bs.rd = 0
    local r = env.onReceiveRpc(id, bs)
    return r, #bs.data == #sent and same(bs.data, sent), bs.rd == 0
end
local function allPass(id, bs, what)
    local r, untouched, back = screen(id, bs)
    ok(r == nil and untouched and back, what .. ': passed untouched, read offset back at the start')
end

local function anim(lib, name)
    local bs = newBs()
    w16(bs, 7); wstr8(bs, lib); wstr8(bs, name)
    w32(bs, 0x40800000); for _ = 1, 4 do w8(bs, 1) end; w32(bs, 0)   -- delta, loop/lock x/lock y/freeze, time
    return bs
end

-- InitMenu as SA-MP sends it: the column flag, enabled flag and row flags are
-- 4-byte BOOLs, each line a 32-byte field
local function menu(columns, rows)
    local bs = newBs()
    w8(bs, 3); w32(bs, columns - 1); wfixed(bs, 'Weapons')
    w32(bs, 0x43200000); w32(bs, 0x43200000); w32(bs, 0x43480000)   -- x, y, width 1
    if columns == 2 then w32(bs, 0x42C80000) end                    -- width 2
    w32(bs, 1); for _ = 1, 12 do w32(bs, 1) end
    for c = 1, columns do
        wfixed(bs, c == 1 and 'Item' or 'Price')
        w8(bs, rows)
        for r = 1, rows do wfixed(bs, ('%s %d'):format(c == 1 and 'Row' or '$', r)) end
    end
    return bs
end

local function dialog(style, text)
    local bs = newBs()
    w16(bs, 12); w8(bs, style); wstr8(bs, 'Shop'); wstr8(bs, 'Buy'); wstr8(bs, 'Close'); wenc(bs, text)
    return bs
end

print('\n== RPCs: animations a server sends pass untouched')
allPass(86, anim('PED', 'WALK_civi'), 'a player animation')
allPass(173, anim('DEALER', 'DEALER_IDLE'), 'an actor animation')
allPass(86, anim(('L'):rep(15), ('N'):rep(24)), 'the longest names IFP files hold (15 and 24)')

print('\n== RPCs: menus pass untouched')
allPass(76, menu(1, 3), 'one column, 3 rows')
allPass(76, menu(2, 12), 'two columns, 12 rows each')
allPass(76, menu(1, 0), 'no rows')

print('\n== RPCs: dialogs that fit pass untouched')
allPass(61, dialog(2, 'Pistol\nShotgun\nRifle'), 'a list')
allPass(61, dialog(5, 'Item\tPrice\nPistol\t$500\nShotgun\t$1200'), 'a tablist with headers')
local cell = ('x'):rep(120)
allPass(61, dialog(4, cell .. '\t' .. cell .. '\t' .. cell .. '\n' .. cell), 'tablist rows of 360 characters, no cell over 128')
allPass(61, dialog(0, ('long message text '):rep(40)), 'a message box, whatever its length')
allPass(61, dialog(1, ''), 'an input dialog with no text')

print('\n== RPCs: other RPCs are not read at all')
do
    local bs = newBs(); w32(bs, 0xFFFFFFFF)
    bs.rd = 0
    ok(env.onReceiveRpc(93, bs) == nil and bs.rd == 0, 'a client message passes, nothing read')
end

print('\n== chutefps: the parachute script\'s per-frame constants follow the timestep')
do
    local SCRIPT, BASE, TS = 0x51000000, 0x52000000, 0xB7CB5C
    local F20, F5, F15, F001 = {0, 0, 0xA0, 0x41}, {0, 0, 0xA0, 0x40}, {0, 0, 0x70, 0x41}, {0x0A, 0xD7, 0x23, 0x3C}
    -- the loaded build's sites: {offset, opcode, the float's bytes}
    local sites = {{1003, 0x17, F20}, {1029, 0x17, F5}, {1135, 0x17, F20}, {1536, 0x13, F001}, {1578, 0x13, F001},
        {3344, 0x17, F20}, {3370, 0x17, F15}, {3578, 0x17, F20}, {3911, 0x17, F20}, {4080, 0x17, F20}, {4238, 0x17, F20}}
    local function load()
        for i = 0, 0x1800 - 1 do mem[BASE + i] = 0x90 end
        put(BASE, 0xA4, 0x03, 0x09, 0x50, 0x4C, 0x43, 0x48, 0x55, 0x54, 0x45)
        put(BASE + 985, 0x17, 0, 3, 0x15, 0, 6, 0x48, 0x8A, 0x88, 0x40)          -- /= 4.267, left alone
        for _, site in ipairs(sites) do put(BASE + site[1], site[2], 0, 3, 0x15, 0, 6, unpack(site[3])) end
    end
    load()
    putU32(0xA8B42C, SCRIPT); putU32(SCRIPT, 0); put(SCRIPT + 8, ('plchute'):byte(1, -1)); putU32(SCRIPT + 0x10, BASE)
    local function at(off) return mem[BASE + off + 6] end
    mem[TS] = 50 / 30
    env.chuteTick()
    ok(math.abs(at(1003) - 20) < 1e-9 and math.abs(at(1029) - 5) < 1e-9 and math.abs(at(1536) - 0.01) < 1e-12,
        '30 fps: the stock values, found by their instructions')
    mem[TS] = 50 / 150
    env.chuteTick()
    ok(math.abs(at(1029) - 25) < 1e-9 and math.abs(at(3370) - 75) < 1e-9, '150 fps: a turn a frame is a fifth')
    local blend = 1 - (1 - 1 / 20) ^ 0.2
    ok(math.abs(at(1003) - 1 / blend) < 1e-9 and math.abs(at(4238) - 1 / blend) < 1e-9,
        'a /20 blend: 1 - 0.95^0.2 a frame, 0.95 a 30 fps step')
    ok(math.abs(at(1578) - (1 - 0.99 ^ 0.2)) < 1e-12, 'a *0.01 blend the same way')
    ok(mem[BASE + 985 + 6] == 0x48, 'the /= 4.267 (the stick) left alone')
    env.fixesCommand('chutefps off')
    ok(math.abs(at(1029) - 5) < 1e-9 and math.abs(at(1003) - 20) < 1e-9, 'off: the stock values back')
    env.fixesCommand('chutefps on')
    load()
    put(BASE + 4238, 0x90, 0x90)                                               -- a build with one /= 20.0 fewer
    putU32(SCRIPT + 0x10, BASE + 0x2000)
    for i = 0, 0x1800 - 1 do mem[BASE + 0x2000 + i] = mem[BASE + i] end
    mem[TS] = 50 / 150
    env.chuteTick()
    ok(mem[BASE + 0x2000 + 1003 + 6] == 0x00 and mem[BASE + 0x2000 + 1029 + 6] == 0x00,
        'a build without every site: left alone')
    putU32(0xA8B42C, 0)
end

print('\n== freezecar: frozen by the server while driving, the car is held too')
do
    local CAR, PTR = 0x1234, 0x50000000
    local inCar, driver, controls, exists = true, 1, false, true
    env.PLAYER_PED, env.PLAYER_HANDLE = 1, 0
    env.isCharInAnyCar = function() return inCar end
    env.storeCarCharIsInNoSave = function() return CAR end
    env.getDriverOfCar = function() return driver end
    env.getCarPointer = function() return PTR end
    env.doesVehicleExist = function() return exists end
    env.isPlayerControlOn = function() return controls end
    local function rpc(moveable) local bs = newBs(); w8(bs, moveable); return screen(15, bs) end
    local function flags() return getU32(PTR + 0x40) end
    putU32(PTR + 0x40, 0x0001); putU32(PTR + 0x44, 0x3F800000); putU32(PTR + 0x54, 0x3F000000)
    local r, untouched, back = rpc(0)
    ok(r == nil and untouched and back, 'RPC 15 passes on to SA-MP, read offset back')
    env.freezeCarTick()
    ok(flags() == 0x2001 and getU32(PTR + 0x44) == 0 and getU32(PTR + 0x54) == 0,
        'frozen while driving: the car does not apply its speed, its move and turn speeds are 0')
    putU32(PTR + 0x48, 0x3F800000)
    env.freezeCarTick()
    ok(getU32(PTR + 0x48) == 0, 'and are kept at 0 every frame')
    rpc(1)
    ok(flags() == 0x0001, 'unfrozen: the bit back off, the rest as it was')
    putU32(PTR + 0x40, 0x2000)
    rpc(0); env.freezeCarTick(); rpc(1)
    ok(flags() == 0x2000, 'a car already held by something else stays held')
    putU32(PTR + 0x40, 0)
    rpc(0); env.freezeCarTick()
    driver = 2
    env.freezeCarTick()
    ok(flags() == 0, 'not the driver any more: let go')
    driver = 1
    env.freezeCarTick()
    ok(flags() == 0x2000, 'the driver again, still frozen: held again')
    controls = true
    env.freezeCarTick()
    ok(flags() == 0, 'the controls back without an RPC (a respawn): let go')
    controls = false
    env.freezeCarTick()
    ok(flags() == 0, 'and not held again until the server freezes again')
    rpc(0)
    env.fixesCommand('freezecar off')
    env.freezeCarTick()
    ok(flags() == 0, 'option off: not held')
    env.fixesCommand('freezecar on')
    env.freezeCarTick()
    ok(flags() == 0x2000, 'on: held')
    inCar = false
    env.freezeCarTick()
    ok(flags() == 0, 'out of the car: let go')
    rpc(1)
    env.PLAYER_PED, env.PLAYER_HANDLE = nil, nil
end

print('\n== RPCs: /fixes says nothing until something was refused')
before = #chat
env.fixesCommand('')
local listedServer = false
for i = before + 1, #chat do if chat[i]:find('server data', 1, true) then listedServer = true end end
ok(not listedServer, 'no server data line')

print('\n== RPCs: teleports on the map pass, NaN and off-world ones are dropped')
do
    local fbits = require('ffi').new('union { float f; uint32_t u; }')
    local function wf(bs, v) fbits.f = v; w32(bs, fbits.u) end
    env.raknetBitStreamReadFloat = function(bs)
        fbits.u = env.raknetBitStreamReadInt32(bs) % 0x100000000
        return fbits.f
    end
    local function pos(id, x, y, z)
        local bs = newBs()
        if id == 159 then w16(bs, 412) end
        wf(bs, x); wf(bs, y); wf(bs, z)
        return bs
    end
    allPass(12, pos(12, 1544.0, -1675.0, 13.5), 'a player teleport in Los Santos')
    allPass(13, pos(13, -2000.0, 150.0, 0.0), 'a find-Z teleport')
    allPass(159, pos(159, 2500.0, 2500.0, 10.0), 'a vehicle teleport')
    allPass(12, pos(12, 19999.0, -19999.0, 999999.0), 'a teleport just inside the absolute limits')
    allPass(12, pos(12, -2080.44, 296.46, -10000.04), 'to objects a server keeps at z -10000 (Unity Roleplay)')
    local function dropped(id, x, y, z, what)
        local r, untouched, back = screen(id, pos(id, x, y, z))
        ok(r == false and untouched and back, what .. ': dropped, read offset back at the start')
    end
    dropped(12, 0 / 0, 0.0, 0.0, 'a NaN player teleport')
    dropped(159, 100.0, 0 / 0, 5.0, 'a NaN vehicle teleport')
    dropped(13, 25000.0, 0.0, 10.0, 'a find-Z teleport past 20000')
    dropped(12, 0.0, 0.0, -2000000.0, 'a teleport far under the world')
    dropped(159, 0.0, 0.0, 1e9, 'a vehicle teleport into the sky')
    env.fixesCommand('rpcpositions off')
    allPass(12, pos(12, 0 / 0, 0.0, 0.0), 'rpcpositions off: a NaN teleport passes, for the position guard to catch')
    env.fixesCommand('rpcpositions on')
end

print('\n== RPCs: /fixes counts the teleports it dropped')
before = #chat
env.fixesCommand('')
local teleports = false
for i = before + 1, #chat do if chat[i]:find('5 teleport(s) dropped', 1, true) then teleports = true end end
ok(teleports, 'the server data line counts 5 teleports')

--------------------------------------------------------------------------------
-- 0.13.0: every patch's stock bytes are gta_sa.exe 1.0 US's
--------------------------------------------------------------------------------
print('\n== every gamePatches `from` matches the real gta_sa.exe')
do
    local f = io.open(root .. '../gta_sa.exe', 'rb')
    if not f then
        print('  (no ../gta_sa.exe here: skipped)')
    else
        local exe = f:read('*a'); f:close()
        local function u32(o) local a, b, c, d = exe:byte(o + 1, o + 4); return a + b * 256 + c * 65536 + d * 16777216 end
        local function u16(o) local a, b = exe:byte(o + 1, o + 2); return a + b * 256 end
        local pe = u32(0x3C)
        local sections = {}
        for i = 0, u16(pe + 6) - 1 do
            local sh = pe + 24 + u16(pe + 20) + i * 40
            sections[#sections + 1] = {va = 0x400000 + u32(sh + 12), raw = u32(sh + 20), size = u32(sh + 16)}
        end
        local function fileOffset(va, n)
            for _, sec in ipairs(sections) do
                if va >= sec.va and va + n <= sec.va + sec.size then return sec.raw + va - sec.va end
            end
        end
        local checked, wrong = 0, {}
        for _, p in ipairs(upvalue(env.applyGamePatches, 'gamePatches')) do
            local o = fileOffset(p.va, #p.from)
            local stock = p.fileFrom or p.fromSamp and p.to or p.from
            if o then
                checked = checked + 1
                for i = 1, #stock do
                    if exe:byte(o + i) ~= stock[i] then wrong[#wrong + 1] = ('0x%X %s'):format(p.va, p.name); break end
                end
            end
        end
        ok(checked >= 120 and #wrong == 0, ('%d checked against the file, wrong: %s'):format(checked, table.concat(wrong, '; ')))
        local guardWrong = {}
        for _, site in ipairs(upvalue(env.fixPositionGuard, 'PG').SITES) do
            local o = fileOffset(site.va, #site.from)
            for i = 1, #site.from do
                if not o or exe:byte(o + i) ~= site.from[i] then guardWrong[#guardWrong + 1] = ('0x%X'):format(site.va); break end
            end
        end
        ok(#guardWrong == 0, 'the position guard\'s two sites are the file\'s bytes: ' .. table.concat(guardWrong, ', '))
        local fpsWrong = {}
        for _, site in ipairs(upvalue(env.fixFrameRate, 'FPS').SITES) do
            local o = fileOffset(site.va, #site.from)
            for i = 1, #site.from do
                if not o or exe:byte(o + i) ~= site.from[i] then fpsWrong[#fpsWrong + 1] = ('0x%X'):format(site.va); break end
            end
        end
        ok(#fpsWrong == 0, 'the 8 frame-rate sites are the file\'s bytes: ' .. table.concat(fpsWrong, ', '))
        local function fileHas(va, bytes)
            local o = fileOffset(va, #bytes)
            if not o then return false end
            for i = 1, #bytes do if exe:byte(o + i) ~= bytes[i] then return false end end
            return true
        end
        local fogSites = upvalue(env.fixFogRestore, 'FOG_SITES')
        ok(fileHas(fogSites[1].va, fogSites[1].from) and fileHas(fogSites[2].va, fogSites[2].from),
            'the fog restore\'s two sites are the file\'s')
        local fogCodeFn = upvalue(env.fixFogRestore, 'fogRestoreCode')
        local setter, shadow = hex('8B 44 24 04 8B 4C 24 08 39 0C C5'), upvalue(fogCodeFn, 'FOG_SHADOW')
        for i = 0, 3 do setter[#setter + 1] = math.floor(shadow / 0x100 ^ i) % 0x100 end
        ok(fileHas(upvalue(fogCodeFn, 'FOG_SET'), setter), 'and RwD3D9SetRenderState compares against its shadow table')
    end
end

--------------------------------------------------------------------------------
-- 0.13.0: MTA's multiplayer patches; crashes.asi's visual and display switches
--------------------------------------------------------------------------------
local patchList = upvalue(env.applyGamePatches, 'gamePatches')
local mta = {}
for _, p in ipairs(patchList) do if p.option == 'mtapatches' then mta[#mta + 1] = p end end
local function stockMta() for _, p in ipairs(mta) do put(p.va, unpack(p.from)) end end

print('\n== MTA patches: 73 sites, applied late (SA-MP is up)')
ok(#mta == 73, '73 sites: ' .. #mta)
stockMta()
env.applyGamePatches('early')
ok(same(get(0x4629E0, 1), {0x6A}), 'not in the early phase')
env.applyGamePatches('late')
local applied = 0
for _, p in ipairs(mta) do if same(get(p.va, #p.to), p.to) then applied = applied + 1 end end
ok(applied == 73, 'all 73 in the late phase: ' .. applied)
ok(same(get(0x738F3A, 3), {0x90, 0x90, 0x90}) and same(get(0x4629E0, 1), {0xC3}), 'the satchel NOPs and the roadblock ret')

print('\n== MTA patches: where SA-MP writes a site itself, it is SA-MP\'s')
stockMta()
put(0x54F3A1, 0x70, 0x17, 0x00, 0x00)                      -- SA-MP's 6000 matrices
put(0x738F3A, 0xE9, 0x00, 0x00, 0x00, 0x10)                -- SA-MP's satchel hook
env.applyGamePatches('late')
ok(same(get(0x54F3A1, 4), {0x70, 0x17, 0x00, 0x00}), 'SA-MP\'s bigger matrix array is not shrunk to 1800')
ok(findPrinted('placeableMatrixArraySize') and findPrinted('placeableMatrixArraySize'):find('owned by samp.dll', 1, true),
    'and said so: ' .. tostring(findPrinted('placeableMatrixArraySize')))
ok(mem[0x738F3A] == 0xE9 and findPrinted('satchelCrashFix'):find('owned by code at 0x', 1, true),
    'SA-MP\'s satchel hook is left: ' .. tostring(findPrinted('satchelCrashFix')))

print('\n== the owners the first in-game run found (2026-10-25): named, not refused')
stockMta()
put(0x738F3A, 0xFF, 0x25, 0x00, 0x00, 0x50, 0x40); putU32(0x40500000, 0x13372000)   -- SA-MP's jmp [ptr]
put(0x7469A0, 0x30, 0xC0, 0x90, 0x90)                                               -- SilentPatch's xor al,al
put(0x8A5A84, 0x20, 0x00, 0x00, 0x00)                                               -- a stream.ini vehicles value
env.applyGamePatches('late')
ok(mem[0x738F3A] == 0xFF and findPrinted('satchelCrashFix'):find('owned by code at 0x13372000', 1, true),
    'an indirect jmp is a hook too, and its target named: ' .. tostring(findPrinted('satchelCrashFix')))
ok(same(get(0x7469A0, 4), {0x30, 0xC0, 0x90, 0x90}) and findPrinted('dinputNonExclusiveMouse'):find('owned by SilentPatch', 1, true),
    'SilentPatch\'s same mouse patch: ' .. tostring(findPrinted('dinputNonExclusiveMouse')))
ok(mem[0x8A5A84] == 0x20 and findPrinted('value_8A5A84_127'):find('owned by stream.ini', 1, true),
    'a value set on purpose stands down: ' .. tostring(findPrinted('value_8A5A84_127')))
put(0x8A5A84, 0x16, 0x00, 0x00, 0x00)                                               -- what CStreaming init sets
env.applyGamePatches('late')
ok(same(get(0x8A5A84, 4), {0x7F, 0x00, 0x00, 0x00}), 'the game\'s own 22 is stock: 127 is written')
env.fixesCommand('mtapatches off')
ok(same(get(0x8A5A84, 4), {0x16, 0x00, 0x00, 0x00}), 'and off puts 22 back, not the file\'s 12')
env.fixesCommand('mtapatches on')
for _, p in ipairs(patchList) do if p.va == 0x74872D then put(p.va, unpack(p.from)) end end
put(0x74872D, 0xE8); putU32(0x74872E, 0x13373000 - 0x748732)                          -- SilentPatch's call
env.applyGamePatches('early')
ok(mem[0x74872D] == 0xE8 and findPrinted('Alt+Enter windowed mode'):find('owned by code at 0x13373000', 1, true),
    'a redirected call at the windowed-mode site: ' .. tostring(findPrinted('Alt+Enter windowed mode')))

print('\n== MTA patches: /fixes mtapatches off puts them back')
stockMta()
env.applyGamePatches('late')
env.fixesCommand('mtapatches off')
local stockAgain = 0
for _, p in ipairs(mta) do if same(get(p.va, #p.from), p.from) then stockAgain = stockAgain + 1 end end
ok(stockAgain == 73 and chat[#chat]:find('73 site(s) changed', 1, true), 'all 73: ' .. tostring(chat[#chat]))
env.fixesCommand('mtapatches on')

print('\n== focusmenu: the pause menu on focus, off by default, SA-MP\'s NOPs kept')
do
    local SITE, CALL, NOPS = 0x748063, {0xE8, 0xF8, 0x3B, 0xDF, 0xFF}, {0x90, 0x90, 0x90, 0x90, 0x90}
    put(SITE, unpack(NOPS))                                 -- SA-MP's flag was 1
    env.applyGamePatches('late')
    ok(same(get(SITE, 5), NOPS), 'off by default: SA-MP\'s NOPs are left')
    env.fixesCommand('focusmenu on')
    ok(same(get(SITE, 5), CALL), 'on: the call to 0x53BC60 is back')
    env.fixesCommand('focusmenu off')
    ok(same(get(SITE, 5), NOPS), 'off: SA-MP\'s NOPs again')
    put(SITE, unpack(CALL))                                 -- SA-MP NOPed 0x7480B3 instead
    env.fixesCommand('focusmenu on')
    env.fixesCommand('focusmenu off')
    ok(same(get(SITE, 5), CALL), 'a call SA-MP left in place is not NOPed by off')
end

print('\n== keepcars: 01C3 keeps the car, on by default')
do
    local SITE, CALL, NOPS = 0x47D775, {0xE8, 0xF6, 0x8E, 0x00, 0x00}, {0x90, 0x90, 0x90, 0x90, 0x90}
    put(SITE, unpack(CALL))
    env.applyGamePatches('early')
    ok(same(get(SITE, 5), NOPS), 'on by default: the CleanUpThisVehicle call is NOPed')
    env.fixesCommand('keepcars off')
    ok(same(get(SITE, 5), CALL), 'off: the call is back')
    env.fixesCommand('keepcars on')
    ok(same(get(SITE, 5), NOPS), 'on again')
    env.fixesCommand('keepcars off')
    put(SITE, 0xE8); putU32(SITE + 1, 0x13374000 - (SITE + 5))     -- another mod's hook on the call
    local hooked = get(SITE, 5)
    env.fixesCommand('keepcars on')
    ok(same(get(SITE, 5), hooked), 'a redirected call is left to its owner')
end

print('\n== visual switches: crashes.asi\'s defaults, each its own option')
for _, p in ipairs(patchList) do
    if p.option and p.option:find('^no') or p.option == 'windowed' or p.option == 'background' then put(p.va, unpack(p.from)) end
end
env.applyGamePatches('early')
ok(same(get(0x701780, 1), {0xC3}) and same(get(0x53E1B4, 5), {0x90, 0x90, 0x90, 0x90, 0x90}),
    'heat haze and the cloud render call off by default')
ok(same(get(0x507750, 1), {0x56}) and same(get(0x7000E0, 1), {0x8B}) and same(get(0x53E227, 1), {0xE9}),
    'sound, flashes and post effects on by default')
ok(same(get(0x74872D, 2), {0x90, 0x90}) and same(get(0x561AF6, 1), {0x00}), 'windowed mode and running in the background on')
env.fixesCommand('nosound on')
ok(same(get(0x507750, 1), {0xC3}), '/fixes nosound on')
env.fixesCommand('nosound off')
ok(same(get(0x507750, 1), {0x56}), 'and off')

--------------------------------------------------------------------------------
-- 0.12.0: SA-MP's exception filter
--------------------------------------------------------------------------------
-- SA-MP R5's filter as it sits in samp.dll at 0x10000000: the push of its
-- "Warning(%s)" format, `push eax`, then the 37-byte tail from the warning call
-- (print 0x100680F0, counter 0x10125A58). The new tail as
-- i686-w64-mingw32-as assembled it, for counter 0x11223344.
local EF_STRING_RVA, EF_PUSH_RVA, EF_SITE_RVA, EF_COUNT_RVA, EF_PRINT_RVA = 0xE96D0, 0x60A47, 0x60A4D, 0x125A58, 0x680F0
local EF_TAIL = 'E8 9E 76 00 00 83 C4 14 A1 58 5A 12 10 83 F8 0A 7D 0E 40 A3 58 5A 12 10 B8 01 00 00 00 C2 0C 00 33 C0 C2 0C 00'
local EF_AS = 'E8 11 00 00 00 83 C4 14 FF 05 44 33 22 11 B8 01 00 00 00 C2 0C 00 83 3D 44 33 22 11 0A 73 05 E9 00 00 00 00 C3'
local EF_FORMAT = 'Warning(%s): Exception 0x%X at 0x%X\0'

-- samp.dll R5 at `base` with the filter in place; `tail` replaces its stock bytes
local function filterSamp(base, tail)
    loadSamp(base, 0)
    -- two sections: .text over the filter, .rdata over its string
    local nt = base + 0x118
    put(nt + 6, 2, 0); put(nt + 20, 0xE0, 0)
    for i, sec in ipairs({{0x60000, 0x60000020}, {0xE9000, 0x40000040}}) do
        local at = nt + 24 + 0xE0 + (i - 1) * 40
        putU32(at + 8, 0x1000); putU32(at + 12, sec[1]); putU32(at + 36, sec[2])
    end
    for i = 1, #EF_FORMAT do mem[base + EF_STRING_RVA + i - 1] = EF_FORMAT:byte(i) end
    put(base + EF_PUSH_RVA, 0x68); putU32(base + EF_PUSH_RVA + 1, base + EF_STRING_RVA)
    put(base + EF_PUSH_RVA + 5, 0x50)                                     -- push eax
    put(base + EF_SITE_RVA, unpack(hex(tail or EF_TAIL)))
    if not tail then
        putU32(base + EF_SITE_RVA + 9, base + EF_COUNT_RVA); putU32(base + EF_SITE_RVA + 20, base + EF_COUNT_RVA)
    end
    mem[base + EF_SITE_RVA + 37] = 0xCC
end

for _, base in ipairs({0x10000000, 0x02B20000}) do
    local tag = ('base 0x%08X'):format(base)
    print(('\n== exception filter, SA-MP R5 at %s: found by its string, the tail rewritten'):format(tag))
    filterSamp(base)
    local site, count = base + EF_SITE_RVA, base + EF_COUNT_RVA
    ok(env.fixExceptionFilter() == true, tag .. ': applied')
    local want = hex(EF_AS)
    local function sub(at, v) for i = 1, 4 do want[at + i] = math.floor((v % 0x100000000) / 0x100 ^ (i - 1)) % 0x100 end end
    sub(10, count); sub(24, count); sub(32, (base + EF_PRINT_RVA) - (site + 0x24))
    ok(same(get(site, 37), want), tag .. ': the tail is the assembled one, with this counter and print')
    ok((site + 0x24 + getU32(site + 0x20)) % 0x100000000 == base + EF_PRINT_RVA, tag .. ': its jmp reaches the print')
    ok(mem[site - 1] == 0x50 and mem[site + 37] == 0xCC, tag .. ': nothing outside the 37 bytes')
    ok(mem[site + 8] == 0xFF, tag .. ': +8, where the no-chat jump lands, starts an instruction (inc)')
    ok(printed[#printed]:find('every exception is swallowed, the first 10 still print', 1, true),
        tag .. ': said: ' .. tostring(printed[#printed]))

    print(('\n== %s: a reload finds it in place'):format(tag))
    local before = snapshot(0x01000000, 0x7FFFFFFF)
    ok(env.fixExceptionFilter() == true and unchanged(before, snapshot(0x01000000, 0x7FFFFFFF)), tag .. ': not a byte changed')
    ok(printed[#printed]:find('already patched', 1, true), tag .. ': said so')
end

print('\n== exception filter: the count is reported once it moves')
do
    local tick = upvalue(env.main, 'exceptionFilterTick')
    ok(type(tick) == 'function', 'main reaches the tick')
    local n0 = #printed
    tick()
    ok(#printed == n0, 'nothing new: nothing said')
    putU32(0x02B20000 + EF_COUNT_RVA, 12)
    tick()
    ok(#printed == n0 + 1 and printed[#printed]:find('swallowed 12 exception(s) so far', 1, true), 'said: ' .. tostring(printed[#printed]))
    tick()
    ok(#printed == n0 + 1, 'and once')
    local from = #chat
    env.fixesCommand('')
    local shown = false
    for i = from + 1, #chat do if chat[i]:find('exception filter: 12 exception(s) swallowed', 1, true) then shown = true end end
    ok(shown, '/fixes shows it')
end

for _, case in ipairs({
        {"Export's anticrasher", '90 90 90 90 90 83 C4 14 A1 58 5A 12 10 90 90 90 90 90 40 A3 58 5A 12 10 B8 01 00 00 00 C2 0C 00 33 C0 C2 0C 00',
         "Export's"},
        {"the 0.3.DL ASI's broken NOPs", '90 90 90 90 00 83 C4 14 A1 58 5A 12 10 83 F8 0A 7D 0E 40 A3 58 5A 12 10 B8 01 00 00 00 C2 0C 00 33 C0 C2 0C 00',
         'remove it'},
        {"DarkP1xel's", 'E8 9E 76 00 00 83 C4 14 ' .. ('90 '):rep(26) .. 'C2 0C 00', 'DarkP1xel'}}) do
    print(('\n== exception filter: %s: stand down, naming it, nothing written'):format(case[1]))
    filterSamp(0x10000000, case[2])
    local before = snapshot(0x01000000, 0x7FFFFFFF)
    ok(env.fixExceptionFilter() == false and unchanged(before, snapshot(0x01000000, 0x7FFFFFFF)), case[1] .. ': left as it is')
    ok(printed[#printed]:find(case[3], 1, true) and printed[#printed]:find('standing down', 1, true),
        case[1] .. ': said: ' .. tostring(printed[#printed]))
end

print('\n== exception filter: anything else is refused; no string, not supported')
filterSamp(0x10000000, EF_TAIL:gsub('7D 0E', '7D 0C'))
ok(env.fixExceptionFilter() == false and printed[#printed]:find('refusing the exception filter fix', 1, true), 'refused')
filterSamp(0x10000000)
mem[0x10000000 + EF_STRING_RVA] = 0x58
ok(env.fixExceptionFilter() == false and printed[#printed]:find('warning string is not in samp.dll', 1, true), 'not supported')
filterSamp(0x10000000)
unmapped = function(va) return va >= 0x10060000 and va < 0x10061000 end   -- .text gone, as R1's packed sections can be
ok(env.fixExceptionFilter() == false and printed[#printed]:find('nothing pushes its warning string', 1, true),
    'an unmapped section is not read: ' .. tostring(printed[#printed]))
unmapped = nil

--------------------------------------------------------------------------------
-- 0.13.0: the crash-fix hooks, through a stand-in for crashfix.dll
--------------------------------------------------------------------------------
print('\n== crash fixes: each hook installed by number, each with its line')
do
    local cf = {
        hooks = {
            {key = 'misc1', name = 'CPed shadow render', va = 0x5D9A6E, result = 1},
            {key = 'misc2', name = 'vehicle colmodel', va = 0x6B18B0, result = 3, found = 0x13371000},
            {key = 'misc4', name = 'divide by zero', va = 0x4F02D2, result = 0, found = 0x90909090},
            {key = 'misc19', name = 'RwFrameCloneHierarchy', va = 0x7F0BF7, result = 1},
            {key = 'misc12', name = 'UncompressAnimation', va = 0x4D41C5, result = 1},
            {key = 'heli1', name = 'heli takeoff', va = 0x6C4F29, result = 1, option = 'vehiclefps'},
            {key = 'glass2', name = 'vehicle glass', va = 0x734092, result = 1, option = 'glasslist'},
            {key = 'pedskin', name = 'ped skin', va = 0x7335B0, result = 1, option = 'pedmatrix'},
            {key = 'swimpitch1', name = 'dive pitch', va = 0x68A6BD, result = 1, option = 'swimfps'},
            {key = 'samp', name = 'SA-MP helper', va = 0, result = 1},
        },
        installs = {}, uninstalls = {}, shadows = nil, sampSite = nil,
    }
    local lib = {
        CrashFix_Count = function() return #cf.hooks end,
        CrashFix_Key = function(i) return cf.hooks[i + 1].key end,
        CrashFix_Name = function(i) return cf.hooks[i + 1].name end,
        CrashFix_Address = function(i) return cf.hooks[i + 1].va end,
        CrashFix_SetSampSite = function(va) cf.sampSite = va; cf.hooks[#cf.hooks].va = va; return 1 end,
        CrashFix_SetShadows = function(on) cf.shadows = on end,
        CrashFix_Install = function(i, found)
            local h = cf.hooks[i + 1]
            cf.installs[#cf.installs + 1] = h.key
            found[0] = h.found or 0
            if h.result == 1 then h.on = true end
            return h.result
        end,
        CrashFix_Uninstall = function(i) local h = cf.hooks[i + 1]; cf.uninstalls[#cf.uninstalls + 1] = h.key; h.on = false; return 1 end,
        CrashFix_Installed = function(i) return cf.hooks[i + 1].on and 1 or 0 end,
        CrashFix_Averted = function(i) return cf.hooks[i + 1].averted or 0 end,
        CrashFix_Option = function(i) return cf.hooks[i + 1].option or 'crashfix' end,
    }
    local oldLoad, oldLL, oldString = fakeFfi.load, fakeFfi.C.LoadLibraryA, fakeFfi.string
    fakeFfi.load = function() return lib end
    fakeFfi.C.LoadLibraryA = function() return 1 end
    fakeFfi.string = function(p, n) if type(p) == 'string' then return p end return oldString(p, n) end

    local from = #printed
    ok(env.fixCrashes('game') == 5, 'misc1, heli1, glass2, pedskin and swimpitch1 installed')
    ok(table.concat(cf.installs, ' ') == 'misc1 misc2 misc4 heli1 glass2 pedskin swimpitch1',
        'misc1, 2 and 4 tried; misc19 and misc12 wait for their options, samp for SA-MP; heli1 under vehiclefps, glass2 glasslist,'
        .. ' pedskin pedmatrix, swimpitch1 swimfps')
    ok(lineAfter(from, 'pedskin') and lineAfter(from, 'pedskin'):find('0x7335B0 ped fix pedskin (ped skin): hooked', 1, true),
        'a ped fix says so: ' .. tostring(lineAfter(from, 'pedskin')))
    ok(lineAfter(from, 'heli1') and lineAfter(from, 'heli1'):find('0x6C4F29 frame-rate fix heli1 (heli takeoff): hooked', 1, true),
        'a frame-rate row says so: ' .. tostring(lineAfter(from, 'heli1')))
    ok(lineAfter(from, 'glass2') and lineAfter(from, 'glass2'):find('0x734092 glass list glass2 (vehicle glass): hooked', 1, true),
        'and a glass list row: ' .. tostring(lineAfter(from, 'glass2')))
    ok(cf.shadows == 0, 'blob shadows: volshadows is off')
    ok(lineAfter(from, 'misc1') and lineAfter(from, 'misc1'):find('0x5D9A6E crash fix misc1 (CPed shadow render): hooked', 1, true),
        'hooked: ' .. tostring(lineAfter(from, 'misc1')))
    ok(lineAfter(from, 'misc2'):find('is owned by code at 0x13371000; standing down', 1, true),
        'another hook: ' .. tostring(lineAfter(from, 'misc2')))
    ok(lineAfter(from, 'misc4'):find('refusing 0x4F02D2 crash fix misc4', 1, true) and lineAfter(from, 'misc4'):find('found 90 90 90 90', 1, true),
        'other bytes: ' .. tostring(lineAfter(from, 'misc4')))
    ok(env.crashFixState() == 'not in place', 'a refused hook: not in place')

    print('\n== crash fixes: the SA-MP helper, found by its pattern once SA-MP is up')
    filterSamp(0x10000000)
    put(0x10060100, 0x74, 0x12, 0x8B, 0x50, 0x10, 0x8B, 0x02)
    env.fixCrashes('samp')
    ok(cf.sampSite == 0x10060100 and cf.installs[#cf.installs] == 'samp', 'its site set, then installed: ' .. tostring(cf.sampSite))

    print('\n== crash fixes: averted crashes are reported once each')
    local tick = upvalue(env.main, 'crashFixTick')
    local n0 = #printed
    tick()
    ok(#printed == n0, 'nothing averted: nothing said')
    cf.hooks[1].averted = 2
    tick()
    ok(#printed == n0 + 1 and printed[#printed]:find('crash fix misc1 (CPed shadow render) averted a crash (2 so far)', 1, true),
        'said: ' .. tostring(printed[#printed]))
    tick()
    ok(#printed == n0 + 1, 'and once')
    cf.hooks[1].averted = 5
    tick(1000)
    ok(#printed == n0 + 2 and printed[#printed]:find('(5 so far)', 1, true), 'more, said: ' .. tostring(printed[#printed]))
    cf.hooks[1].averted = 9
    tick(1030)
    ok(#printed == n0 + 2, 'more within a minute: not yet')
    tick(1061)
    ok(#printed == n0 + 3 and printed[#printed]:find('(9 so far)', 1, true), 'a minute on, the total: ' .. tostring(printed[#printed]))
    cf.hooks[7].averted = 1
    tick(2000)
    ok(#printed == n0 + 4 and printed[#printed]:find('glass list glass2 (vehicle glass) grew for a car with more see-through parts (1 so far)', 1, true)
        and not printed[#printed]:find('averted', 1, true), 'a glass list that grew is no crash: ' .. tostring(printed[#printed]))
    cf.hooks[8].averted = 3
    tick(3000)
    ok(#printed == n0 + 5 and printed[#printed]:find("ped fix pedskin (ped skin) caught a ped's broken bones (3 so far)", 1, true)
        and not printed[#printed]:find('averted', 1, true), 'and a ped left out: ' .. tostring(printed[#printed]))

    print('\n== crash fixes: options')
    local function installedSince(mark, key)
        for i = mark + 1, #cf.installs do if cf.installs[i] == key then return true end end
    end
    local mark = #cf.installs
    env.fixesCommand('frameclone on')
    ok(installedSince(mark, 'misc19'), '/fixes frameclone on installs hook 19')
    mark = #cf.installs
    env.fixesCommand('oldcrashfixes on')
    ok(installedSince(mark, 'misc12'), '/fixes oldcrashfixes on installs hook 12')
    env.fixesCommand('oldcrashfixes off')
    ok(cf.uninstalls[#cf.uninstalls] == 'misc12', 'and off takes it out')
    cf.uninstalls = {}
    cf.hooks[3].result = 1
    env.fixesCommand('crashfix off')
    ok(table.concat(cf.uninstalls, ' ') == 'misc1 misc19 samp', '/fixes crashfix off removes each one that is ours, '
        .. 'not the frame-rate rows: ' .. table.concat(cf.uninstalls, ' '))
    env.fixesCommand('vehiclefps off')
    ok(cf.uninstalls[#cf.uninstalls] == 'heli1', '/fixes vehiclefps off takes its rows out')
    mark = #cf.installs
    env.fixesCommand('vehiclefps on')
    ok(installedSince(mark, 'heli1'), 'and on puts them back')
    env.fixesCommand('glasslist off')
    ok(cf.uninstalls[#cf.uninstalls] == 'glass2' and env.crashFixState('glass') == 'off (option glasslist)',
        '/fixes glasslist off takes the glass list out: ' .. env.crashFixState('glass'))
    mark = #cf.installs
    env.fixesCommand('glasslist on')
    ok(installedSince(mark, 'glass2'), 'and on puts it back')
    env.fixesCommand('pedmatrix off')
    ok(cf.uninstalls[#cf.uninstalls] == 'pedskin' and env.crashFixState('ped') == 'off (option pedmatrix)',
        '/fixes pedmatrix off takes the ped fixes out: ' .. env.crashFixState('ped'))
    mark = #cf.installs
    env.fixesCommand('pedmatrix on')
    ok(installedSince(mark, 'pedskin'), 'and on puts them back')
    env.fixesCommand('swimfps off')
    ok(cf.uninstalls[#cf.uninstalls] == 'swimpitch1', "/fixes swimfps off takes its DLL rows out with fixes.lua's")
    mark = #cf.installs
    env.fixesCommand('swimfps on')
    ok(installedSince(mark, 'swimpitch1'), 'and on puts them back')
    env.fixesCommand('crashfix on')
    env.fixesCommand('volshadows on')
    ok(cf.shadows == 1, '/fixes volshadows on reaches the DLL')
    env.fixesCommand('volshadows off')
    env.fixesCommand('frameclone off')
    local fromChat = #chat
    env.fixesCommand('patches')
    local line
    for i = fromChat + 1, #chat do if chat[i]:find('crash fixes (', 1, true) then line = chat[i] end end
    ok(line and line:find('(6 hooks): 3 hooked, 1 owned elsewhere, 0 refused, 2 off', 1, true), '/fixes patches: ' .. tostring(line))
    local frLine
    for i = fromChat + 1, #chat do if chat[i]:find('frame-rate fixes (', 1, true) then frLine = chat[i] end end
    ok(frLine and frLine:find('(2 sites): 2 in place, 0 owned elsewhere, 0 refused, 0 off', 1, true),
        'and the frame-rate rows apart: ' .. tostring(frLine))
    local pedLine
    for i = fromChat + 1, #chat do if chat[i]:find('ped fixes (', 1, true) then pedLine = chat[i] end end
    ok(pedLine and pedLine:find('(1 sites): 1 in place, 0 owned elsewhere, 0 refused, 0 off', 1, true)
        and env.crashFixState('ped') == 'in place', 'and the ped fixes: ' .. tostring(pedLine))
    ok(env.crashFixState('frame') == 'in place', 'their state apart too')
    local glassLine
    for i = fromChat + 1, #chat do if chat[i]:find('glass list (', 1, true) then glassLine = chat[i] end end
    ok(glassLine and glassLine:find('(1 sites): 1 in place, 0 owned elsewhere, 0 refused, 0 off', 1, true)
        and env.crashFixState('glass') == 'in place', 'and the glass list: ' .. tostring(glassLine))
    fromChat = #chat
    env.fixesCommand('')
    local status
    for i = fromChat + 1, #chat do if chat[i]:find('crash fixes: ', 1, true) then status = chat[i] end end
    ok(status and status:find('; 9 crash(es) averted', 1, true),
        'the glass list\'s growth and the peds left out are not counted as crashes: ' .. tostring(status))

    fakeFfi.load, fakeFfi.C.LoadLibraryA, fakeFfi.string = oldLoad, oldLL, oldString
end

--------------------------------------------------------------------------------
-- 0.22.0: /fixes drift, a line every 30 s of the frame rate and what grows
--------------------------------------------------------------------------------
print('\n== drift: frame times and counts, compared over a session')
do
    local D = upvalue(env.driftFrame, 'DRIFT')
    local now, lines = 0, {}
    D.clock = function() return now end
    D.write = function(line) lines[#lines + 1] = line end
    D.EVERY = 1
    env.fixesCommand('drift report')
    ok(chat[#chat]:find('drift is off', 1, true), 'off: says how to start it: ' .. tostring(chat[#chat]))
    local POOL, FLAGS = 0x40000000, 0x40001000
    putU32(0xB7449C, POOL); putU32(POOL + 4, FLAGS); putU32(POOL + 8, 10)
    for i = 0, 9 do mem[FLAGS + i] = i < 3 and 1 or 0x80 end
    putU32(0x8E4CB4, 200 * 1048576); putU32(0xA9AE80 + 0x14, 37)
    env.sampIs3dTextDefined = function(id) return id < 5 end
    env.fixesCommand('drift on')
    ok(lines[1] and lines[1]:find('fixes drift: a line every 1 s', 1, true), 'on: a session header: ' .. tostring(lines[1]))
    env.driftFrame()
    while #lines < 2 do now = now + 4; env.driftFrame() end
    ok(lines[2]:find('fps=250 ms=4.00', 1, true) and lines[2]:find('worst=4.0 slow=0', 1, true),
        'a second of 4 ms frames: 250 fps: ' .. lines[2])
    ok(lines[2]:find('objects=3/10', 1, true) and lines[2]:find('streamMB=200', 1, true) and lines[2]:find('fx=37', 1, true)
        and lines[2]:find('labels=5', 1, true) and not lines[2]:find('peds=', 1, true),
        'the counts that can be read, and not the ones that cannot: ' .. lines[2])
    env.fixesCommand('drift report')
    ok(chat[#chat]:find('too early, 1 line', 1, true), 'one line: too early to compare: ' .. tostring(chat[#chat]))
    for i = 3, 5 do mem[FLAGS + i] = 1 end
    putU32(0xA9AE80 + 0x14, 30)
    while #lines < 3 do now = now + 5; env.driftFrame() end
    ok(lines[3]:find('fps=200', 1, true) and lines[3]:find('objects=6/10', 1, true), 'the next second: 200 fps, 6 objects: ' .. lines[3])
    now = now + 25; env.driftFrame()
    now = now + 3000; env.driftFrame()
    while #lines < 4 do now = now + 5; env.driftFrame() end
    ok(lines[4]:find('worst=25.0 slow=1 paused=3.0s', 1, true), 'a 25 ms frame: the worst and a hitch; a 3 s gap: '
        .. 'a pause, not a frame: ' .. lines[4])
    local from = #chat
    env.fixesCommand('drift report')
    local report = table.concat(chat, ' ', from + 1)
    ok(report:find('250 -> ', 1, true) and report:find('grew most: objects 3 -> 6', 1, true) and report:find('fell: fx 37 -> 30', 1, true),
        'the report: fps then, now, what grew and fell: ' .. report)
    for _ = 1, 10 do now = now + 5; env.driftFrame() end
    env.fixesCommand('textcache off')
    ok(lines[#lines]:find('/fixes textcache off (fps since the last line: 200)', 1, true),
        'an option switched: its own line, with the frame rate before it: ' .. lines[#lines])
    env.fixesCommand('textcache on')
    env.fixesCommand('drift off')
    ok(lines[#lines]:find('stopped', 1, true), 'off: a last line')
    local n = #lines
    now = now + 5000; env.driftFrame()
    ok(#lines == n, 'and no more')
    putU32(0xB7449C, 0)
    env.sampIs3dTextDefined = nil
end

--------------------------------------------------------------------------------
-- 0.13.0: SA-MP's frame cap (autofps's patches, in fixes too)
--------------------------------------------------------------------------------
-- SA-MP R5's frame limiter from its entry (+0x00..+0x25), its default sleep at
-- +0x182 and the Sleep(1) stub GTA's spin calls, as samp.dll has them.
local FC_BASE, FC_RVA, FC_SLEEP_RVA = 0x10000000, 0xA1C10, 0xA23E0
local FC_ENTRY = '51 56 57 8B F9 E8 D6 CB F8 FF 8B F0 A1 94 06 15 10 85 C0 75 2C B8 00 00 80 3F A3 5C CB B7 00 A3 58 CB B7 00 C7 05'
local function frameCapSamp()
    loadSamp(FC_BASE, 0)
    put(FC_BASE + FC_RVA, unpack(hex(FC_ENTRY)))
    put(FC_BASE + FC_RVA + 0x182, 0x52, 0xFF, 0x15, 0x54, 0x52, 0x0E, 0x10)
    put(FC_BASE + FC_SLEEP_RVA, 0x6A, 0x01, 0xFF, 0x15, 0x54, 0x52, 0x0E, 0x10)
end
local function frameCapReset()   -- main()'s state, as a new session has it
    local i = 1
    while true do
        local name = debug.getupvalue(env.fixFrameCap, i)
        if not name then break end
        if name == 'frameCap' then debug.setupvalue(env.fixFrameCap, i, nil) end
        i = i + 1
    end
end

print('\n== frame cap: the limiter dispatches on the Frame Limiter setting, its sleeps off')
frameCapSamp(); frameCapReset()
local va = FC_BASE + FC_RVA
allocs = 0
ok(env.fixFrameCap() == true and allocs == 1, 'applied, one stub')
local fcStub = nextBlock - 0x100000
ok(mem[va + 0x0C] == 0xE9 and (va + 0x0C + 5 + getU32(va + 0x0D)) % 0x100000000 == fcStub, 'a jmp at +0x0C to the stub')
ok(same(get(va + 0x11, 2), {0x90, 0x90}) and same(get(va + 0x13, 2), {0x75, 0x2C}), 'the jmp\'s tail NOPed, the jne kept')
ok(same(get(fcStub, 9), {0x80, 0x3D, 0x94, 0x67, 0xBA, 0x00, 0x00, 0x75, 0x05}), 'cmp byte [0xBA6794], 0 / jne')
ok((fcStub + 14 + getU32(fcStub + 10)) % 0x100000000 == va + 0x15 and (fcStub + 19 + getU32(fcStub + 15)) % 0x100000000 == va + 0x11,
    'off -> the init arm (+0x15), on -> the stock test (+0x11)')
ok(same(get(va + 0x1A, 5), {0x90, 0x90, 0x90, 0x90, 0x90}) and same(get(va + 0x1F, 1), {0xA3}), 'the timestep store NOPed, the next kept')
ok(same(get(va + 0x182, 7), {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90}), 'the default sleep NOPed')
ok(mem[FC_BASE + FC_SLEEP_RVA + 1] == 0x00 and mem[FC_BASE + FC_SLEEP_RVA] == 0x6A, 'SA-MP\'s Sleep(1) is Sleep(0)')
ok(printed[#printed]:find('dispatches on the Frame Limiter setting', 1, true), 'said: ' .. tostring(printed[#printed]))
do
    local before = snapshot(0x01000000, 0x7FFFFFFF)
    ok(env.fixFrameCap() == true and allocs == 1 and unchanged(before, snapshot(0x01000000, 0x7FFFFFFF)), 'again: nothing changes')
end

print('\n== frame cap: /fixes framecaps off puts the hook and the store back')
env.fixesCommand('framecaps off')
ok(same(get(va, 0x26), hex(FC_ENTRY)), 'the entry is stock again: ' .. tostring(chat[#chat]))
env.fixesCommand('framecaps on')
ok(mem[va + 0x0C] == 0xE9, 'and on again')

print('\n== frame cap: autofps (or crashes.asi) hooked it first: stand down, the sleeps still go')
frameCapSamp(); frameCapReset()
put(0x40600000, 0x80, 0x3D, 0x94, 0x67, 0xBA, 0x00, 0x00)                    -- a stub of autofps's shape
put(va + 0x0C, 0xE9); putU32(va + 0x0D, 0x40600000 - (va + 0x11))
allocs = 0
ok(env.fixFrameCap() == false and allocs == 0, 'not applied, nothing allocated')
ok(printed[#printed]:find('hooked by a frame-cap stub at 0x40600000', 1, true) and printed[#printed]:find('standing down', 1, true),
    'named: ' .. tostring(printed[#printed]))
ok(same(get(va + 0x1A, 5), {0xA3, 0x5C, 0xCB, 0xB7, 0x00}), 'the timestep store is the owner\'s business')
ok(same(get(va + 0x182, 7), {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90}) and mem[FC_BASE + FC_SLEEP_RVA + 1] == 0x00,
    'the default sleep and Sleep(0): the same bytes either way')
frameCapSamp(); frameCapReset()
put(va + 0x0C, 0xE9); putU32(va + 0x0D, 0x13374000 - (va + 0x11))
env.fixFrameCap()
ok(printed[#printed]:find('hooked by code at 0x13374000', 1, true), 'another hook is named by its target')

print('\n== frame cap: not the stock shape: refused, nothing written')
frameCapSamp(); frameCapReset()
mem[va + 0x15] = 0xCC
do
    local before = snapshot(0x01000000, 0x7FFFFFFF)
    ok(env.fixFrameCap() == false and unchanged(before, snapshot(0x01000000, 0x7FFFFFFF)), 'refused, nothing written')
    ok(printed[#printed]:find('refusing the SAMP frame cap removal: +0x15', 1, true), 'said why: ' .. tostring(printed[#printed]))
end

--------------------------------------------------------------------------------
-- 0.15.0: the fog restore (fogrestore.asi's, in fixes too)
--------------------------------------------------------------------------------
-- The two sites in CGame::RenderScene as gta_sa.exe 1.0 US has them, and the
-- routine as i686-w64-mingw32-as assembled it.
local FOG_A, FOG_B = 0x53DFE2, 0x53E01E
local FOG_A_STOCK, FOG_B_STOCK = '6A 00 B9 40 42 BB 00', 'A0 28 C7 C7 00'
local FOG_AS = '9C60A1B092C9005083F001A3B092C900681C000000B8D0C27F00FFD083C408A1E092C9005083F001A3E092C9006822000000' ..
    'B8D0C27F00FFD083C408A1E892C9005083F001A3E892C9006823000000B8D0C27F00FFD083C408A13096C9005083F001A33096C900' ..
    '688C000000B8D0C27F00FFD083C408619DC3'
local function fogSession()   -- a new session: fixes' state gone, the sites as they are
    for _, wanted in ipairs({'fogRestore', 'fogBlock'}) do
        local i = 1
        while true do
            local name = debug.getupvalue(env.fixFogRestore, i)
            if not name then break end
            if name == wanted then debug.setupvalue(env.fixFogRestore, i, nil) end
            i = i + 1
        end
    end
end
local function fogStock() put(FOG_A, unpack(hex(FOG_A_STOCK))); put(FOG_B, unpack(hex(FOG_B_STOCK))) end
local function jmpTarget(va) return mem[va] == 0xE9 and (va + 5 + getU32(va + 1)) % 0x100000000 or nil end

print('\n== fog restore: the routine is the assembler\'s; each entry calls it, replays its site and goes back')
do
    local build = upvalue(env.fixFogRestore, 'fogRestoreCode')
    local code, entries = build(0x40000000)
    local routine = hex(FOG_AS)
    ok(same(code, routine), 'the routine')
    local function at(va) return code[va - 0x40000000 + 1] end
    local function rel(va) return (va + 5 + (at(va + 1) + at(va + 2) * 0x100 + at(va + 3) * 0x10000 + at(va + 4) * 0x1000000))
        % 0x100000000 end
    ok(entries[1] == 0x40000000 + #routine and entries[2] == entries[1] + 17, 'the entries follow it: ' .. tostring(entries[1]))
    ok(at(entries[1]) == 0xE8 and rel(entries[1]) == 0x40000000 and at(entries[2]) == 0xE8 and rel(entries[2]) == 0x40000000,
        'each calls the routine')
    local a, b = {}, {}
    for i = 1, 7 do a[i] = at(entries[1] + 4 + i) end
    for i = 1, 5 do b[i] = at(entries[2] + 4 + i) end
    ok(same(a, hex(FOG_A_STOCK)) and same(b, hex(FOG_B_STOCK)), 'then its site\'s bytes')
    ok(at(entries[1] + 12) == 0xE9 and rel(entries[1] + 12) == FOG_A + 7 and at(entries[2] + 10) == 0xE9
        and rel(entries[2] + 10) == FOG_B + 5 and #code == entries[2] + 15 - 0x40000000, 'then jmps back past it')
end

print('\n== fog restore: both sites hooked, logged')
fogSession(); fogStock()
allocs = 0
ok(env.fixFogRestore() == true and allocs == 1, 'applied, one block')
local fogCode = nextBlock - 0x100000
local fogEntryA, fogEntryB = jmpTarget(FOG_A), jmpTarget(FOG_B)
ok(fogEntryA == fogCode + #hex(FOG_AS) and fogEntryB == fogEntryA + 17, 'each site jmps to its entry')
ok(same(get(FOG_A + 5, 2), {0x90, 0x90}), 'the 7-byte site\'s tail NOPed')
ok(same(get(fogCode, #hex(FOG_AS)), hex(FOG_AS)), 'the routine is in the block')
ok(printed[#printed]:find('0x53DFE2, 0x53E01E fog restore', 1, true) and printed[#printed]:find('twice a frame', 1, true),
    'said: ' .. tostring(printed[#printed]))
do
    local before = snapshot(0x00400000, 0x7FFFFFFF)
    ok(env.fixFogRestore() == true and allocs == 1 and unchanged(before, snapshot(0x00400000, 0x7FFFFFFF)), 'again: nothing changes')
end

print('\n== fog restore: /fixes fogrestore off puts both sites back; on reuses the block')
env.fixesCommand('fogrestore off')
ok(same(get(FOG_A, 7), hex(FOG_A_STOCK)) and same(get(FOG_B, 5), hex(FOG_B_STOCK)), 'both stock: ' .. tostring(chat[#chat]))
env.fixesCommand('fogrestore on')
ok(jmpTarget(FOG_A) == fogEntryA and jmpTarget(FOG_B) == fogEntryB and allocs == 1, 'hooked again, nothing allocated')

print('\n== fog restore: a reload finds the hooks an earlier run left')
fogSession()
ok(env.fixFogRestore() == true and allocs == 1 and jmpTarget(FOG_A) == fogEntryA, 'taken as ours, nothing allocated')
ok(printed[#printed]:find('an earlier run', 1, true), 'said so: ' .. tostring(printed[#printed]))
ok(env.stopFogRestore() == true and same(get(FOG_A, 7), hex(FOG_A_STOCK)) and same(get(FOG_B, 5), hex(FOG_B_STOCK)),
    'and an earlier run\'s hooks come out when the option is off')

print('\n== fog restore: fogrestore.asi (another hook) at either site: stand down, nothing written')
for _, site in ipairs({FOG_A, FOG_B}) do
    fogSession(); fogStock()
    put(site, 0xE9); putU32(site + 1, 0x13375000 - (site + 5))
    allocs = 0
    local before = snapshot(0x00400000, 0x7FFFFFFF)
    ok(env.fixFogRestore() == false and allocs == 0 and unchanged(before, snapshot(0x00400000, 0x7FFFFFFF)),
        ('0x%X hooked: not applied, the other site left stock'):format(site))
    ok(printed[#printed]:find('hooked by code at 0x13375000; standing down', 1, true), 'named: ' .. tostring(printed[#printed]))
end

print('\n== fog restore: ENB or SkyGFX owns the fog: stand down')
fogSession(); fogStock()
env.getGameDirectory = function() return 'GAME' end
env.doesFileExist = function(path) return path == 'GAME\\skygfx.ini' end
allocs = 0
ok(env.fixFogRestore() == false and allocs == 0 and same(get(FOG_A, 7), hex(FOG_A_STOCK)), 'skygfx.ini: not applied')
ok(printed[#printed]:find('skygfx.ini is here', 1, true), 'said: ' .. tostring(printed[#printed]))
env.getGameDirectory, env.doesFileExist = nil, nil

print('\n== fog restore: a site that is not stock and no hook: refused, nothing written')
fogSession(); fogStock()
mem[FOG_B + 1] = 0x29
do
    local before = snapshot(0x00400000, 0x7FFFFFFF)
    ok(env.fixFogRestore() == false and unchanged(before, snapshot(0x00400000, 0x7FFFFFFF)), 'refused, nothing written')
    ok(printed[#printed]:find('refusing the fog restore: 0x53E01E is A0 29', 1, true), 'said why: ' .. tostring(printed[#printed]))
end
fogSession(); fogStock()

--------------------------------------------------------------------------------
-- 0.18.0: the position guard (its code runs in position_guard_test.sh)
--------------------------------------------------------------------------------
local PG_ADD, PG_RAA = 0x544A30, 0x542560
local PG_ADD_STOCK, PG_RAA_STOCK = '64A100000000', '83EC2C558BE9'
local PG = upvalue(env.fixPositionGuard, 'PG')
local function guardSession()   -- a new session: fixes' state gone, the sites as they are
    local i = 1
    while true do
        local name = debug.getupvalue(env.fixPositionGuard, i)
        if not name then break end
        if name == 'posGuard' then debug.setupvalue(env.fixPositionGuard, i, nil) end
        i = i + 1
    end
    PG.block = nil
end
local function guardStock() put(PG_ADD, unpack(hex(PG_ADD_STOCK))); put(PG_RAA, unpack(hex(PG_RAA_STOCK))) end

print('\n== position guard: each entry calls the routine, replays its site and goes back')
do
    local code, entries = PG.code(0x40000000)
    local function at(va) return code[va - 0x40000000 + 1] end
    local function rel(va) return (va + 5 + (at(va + 1) + at(va + 2) * 0x100 + at(va + 3) * 0x10000 + at(va + 4) * 0x1000000))
        % 0x100000000 end
    local routine = 0x40000000 + PG.ROUTINE
    ok(#code < PG.BLOCK_SIZE and at(routine) == 0x9C and at(routine + 1) == 0x60, 'the routine after the slots, inside the block')
    ok(at(entries[1]) == 0xE8 and rel(entries[1]) == routine and at(entries[2]) == 0xE8 and rel(entries[2]) == routine,
        'each entry calls it')
    local a, b = {}, {}
    for i = 1, 6 do a[i] = at(entries[1] + 4 + i); b[i] = at(entries[2] + 4 + i) end
    ok(same(a, hex(PG_ADD_STOCK)) and same(b, hex(PG_RAA_STOCK)), 'then its site\'s bytes')
    ok(at(entries[1] + 11) == 0xE9 and rel(entries[1] + 11) == PG_ADD + 6 and at(entries[2] + 11) == 0xE9
        and rel(entries[2] + 11) == PG_RAA + 6, 'then jmps back past them')
end

print('\n== position guard: both sites hooked, logged')
guardSession(); guardStock()
allocs = 0
ok(env.fixPositionGuard() == true and allocs == 1, 'applied, one block')
local pgEntryA, pgEntryB = jmpTarget(PG_ADD), jmpTarget(PG_RAA)
ok(pgEntryA and pgEntryB and mem[PG_ADD + 5] == 0x90 and mem[PG_RAA + 5] == 0x90, 'each site jmps to its entry, its 6th byte NOPed')
ok(printed[#printed]:find('0x544A30, 0x542560 position guard', 1, true), 'said: ' .. tostring(printed[#printed]))
do
    local before = snapshot(0x00400000, 0x7FFFFFFF)
    ok(env.fixPositionGuard() == true and allocs == 1 and unchanged(before, snapshot(0x00400000, 0x7FFFFFFF)), 'again: nothing changes')
end

print('\n== position guard: /fixes positionguard off puts both back; on reuses the block')
env.fixesCommand('positionguard off')
ok(same(get(PG_ADD, 6), hex(PG_ADD_STOCK)) and same(get(PG_RAA, 6), hex(PG_RAA_STOCK)), 'both stock: ' .. tostring(chat[#chat]))
env.fixesCommand('positionguard on')
ok(jmpTarget(PG_ADD) == pgEntryA and jmpTarget(PG_RAA) == pgEntryB and allocs == 1, 'hooked again, nothing allocated')

print('\n== position guard: a reload finds the hooks an earlier run left')
guardSession()
ok(env.fixPositionGuard() == true and allocs == 1 and jmpTarget(PG_ADD) == pgEntryA, 'taken as ours, nothing allocated')
ok(printed[#printed]:find('an earlier run', 1, true), 'said so: ' .. tostring(printed[#printed]))
ok(upvalue(env.fixPositionGuard, 'posGuard').block == pgEntryA - pgEntryA % 0x10000, 'its counters found at the block\'s base')
ok(env.stopPositionGuard() == true and same(get(PG_ADD, 6), hex(PG_ADD_STOCK)) and same(get(PG_RAA, 6), hex(PG_RAA_STOCK)),
    'and an earlier run\'s hooks come out when the option is off')

print('\n== position guard: another hook at either site: stand down, nothing written')
for _, site in ipairs({PG_ADD, PG_RAA}) do
    guardSession(); guardStock()
    put(site, 0xE9); putU32(site + 1, 0x13376000 - (site + 5))
    allocs = 0
    local before = snapshot(0x00400000, 0x7FFFFFFF)
    ok(env.fixPositionGuard() == false and allocs == 0 and unchanged(before, snapshot(0x00400000, 0x7FFFFFFF)),
        ('0x%X hooked: not applied, the other site left stock'):format(site))
    ok(printed[#printed]:find('hooked by code at 0x13376000; standing down', 1, true), 'named: ' .. tostring(printed[#printed]))
end

print('\n== position guard: a site that is not stock and no hook: refused, nothing written')
guardSession(); guardStock()
mem[PG_RAA + 2] = 0x30
do
    local before = snapshot(0x00400000, 0x7FFFFFFF)
    ok(env.fixPositionGuard() == false and unchanged(before, snapshot(0x00400000, 0x7FFFFFFF)), 'refused, nothing written')
    ok(printed[#printed]:find('refusing the position guard: 0x542560 is 83 EC 30', 1, true), 'said why: ' .. tostring(printed[#printed]))
end
guardSession(); guardStock()

print('\n== position guard: the log names the last bad position, at most every 10 s')
do
    ok(PG.f32(0x469C4000) == 20000 and PG.f32(0xC61C4000) == -10000 and PG.f32(0x7F800000) == math.huge, 'f32: 20000, -10000, inf')
    local nan = PG.f32(0x7FC00000)
    ok(nan ~= nan and PG.f32(0) == 0, 'f32: NaN, 0')
    local BLOCK, CAR = 0x06000000, 0x06100000
    local i = 1
    while true do
        local name = debug.getupvalue(env.fixPositionGuard, i)
        if not name then break end
        if name == 'posGuard' then debug.setupvalue(env.fixPositionGuard, i, {block = BLOCK}) end
        i = i + 1
    end
    putU32(BLOCK, 1180); putU32(BLOCK + 4, 0)
    putU32(BLOCK + PG.SAMPLE, CAR)
    putU32(BLOCK + PG.SAMPLE + 4, 0x46C35000); putU32(BLOCK + PG.SAMPLE + 8, 0x44FA0000); putU32(BLOCK + PG.SAMPLE + 12, 0x41200000)
    put(CAR + 0x22, 0x90, 0x01); put(CAR + 0x36, 0x02)          -- model 400, a car
    putU32(0xB7CD98, 0)
    PG.seen, PG.nextLog, PG.lastAt = {0, 0}, nil, nil
    local from = #printed
    PG.tick()
    local line = printed[#printed]
    ok(#printed == from + 1 and line:find('1180 bad position(s) put back', 1, true)
        and line:find('last: car model 400 at 0x6100000, 25000, 2000, 10', 1, true), 'said: ' .. tostring(line))
    putU32(BLOCK, 2360)
    PG.tick()
    ok(#printed == from + 1, 'not again within 10 s')
    PG.nextLog = os.clock() - 1
    putU32(BLOCK + PG.SAMPLE + 4, 0x7FC00000)
    PG.tick()
    ok(#printed == from + 2 and printed[#printed]:find('2360', 1, true) and printed[#printed]:find('nan', 1, true),
        'after 10 s, with a NaN shown as such: ' .. tostring(printed[#printed]))
    guardSession()
end

--------------------------------------------------------------------------------
-- 0.18.0: frame-rate fixes (their stubs run in fps_fixes_test.sh)
--------------------------------------------------------------------------------
local FPS = upvalue(env.fixFrameRate, 'FPS')
local function fpsStock() for _, site in ipairs(FPS.SITES) do put(site.va, unpack(site.from)) end end
local function fpsSession() FPS.block, FPS.stubs, FPS.owner = nil, nil, {} end
local function callTarget(va) return (mem[va] == 0xE8 or mem[va] == 0xE9) and (va + 5 + getU32(va + 1)) % 0x100000000 or nil end

print('\n== wheelfps: the five fWheelFriction loads call the wheel stub')
fpsSession(); fpsStock()
allocs = 0
ok(env.fixFrameRate('wheelfps') == 5 and allocs == 1, 'five sites, one block')
local wheelStub = FPS.stubs.wheel
local called = 0
for _, site in ipairs(FPS.SITES) do
    if site.option == 'wheelfps' and mem[site.va] == 0xE8 and callTarget(site.va) == wheelStub and mem[site.va + 5] == 0x90 then
        called = called + 1
    end
end
ok(called == 5, 'each a call to it, its 6th byte NOPed (' .. called .. ')')
ok(same(get(wheelStub, 6), hex('D905CCB9C200')), 'the stub loads fWheelFriction itself')
ok(same(get(0x68A4FC, 18), hex('D95C241CD9442414D8C9D95C2420D84C2418')), 'swimfps untouched')
do
    local before = snapshot(0x00400000, 0x7FFFFFFF)
    ok(env.fixFrameRate('wheelfps') == 5 and allocs == 1 and unchanged(before, snapshot(0x00400000, 0x7FFFFFFF)), 'again: nothing changes')
end

print('\n== swimfps and floatfps share the block')
ok(env.fixFrameRate('swimfps') == 3 and env.fixFrameRate('floatfps') == 1 and allocs == 1, 'no second block')
ok(mem[0x68A4FC] == 0xE9 and callTarget(0x68A4FC) == FPS.stubs.swim and same(get(0x68A501, 13), hex(('90'):rep(13))),
    'the swim site jmps (18 bytes: the rest NOPed)')
ok(mem[0x68A42B] == 0xE8 and callTarget(0x68A42B) == FPS.stubs.dive, 'the -0.1 site calls the dive stub')
ok(mem[0x68A4CA] == 0xE8 and callTarget(0x68A4CA) == FPS.stubs.lift and mem[0x68A4CF] == 0x90,
    'the underwater buoyancy site calls the lift stub, its 6th byte NOPed')
ok(same(get(FPS.stubs.lift, 6), hex('D905CC088700')), 'the lift stub loads the 0.01 itself')
ok(mem[0x6C27AE] == 0xE8 and callTarget(0x6C27AE) == FPS.stubs.float, 'the buoyancy site calls the float stub')

print('\n== off puts each option\'s sites back, and only those')
ok(env.stopFrameRate('swimfps') == 3 and same(get(0x68A4FC, 18), hex('D95C241CD9442414D8C9D95C2420D84C2418'))
    and same(get(0x68A42B, 6), hex('D80DF48E8500')) and same(get(0x68A4CA, 6), hex('D805CC088700')), 'swimfps stock')
ok(callTarget(0x6D6E69) == wheelStub and callTarget(0x6C27AE) == FPS.stubs.float, 'the others stay')
env.fixesCommand('wheelfps off')
ok(same(get(0x6D6E69, 6), hex('D905CCB9C200')), '/fixes wheelfps off: ' .. tostring(chat[#chat]))
env.fixesCommand('wheelfps on')
ok(callTarget(0x6D6E69) == wheelStub and allocs == 1, 'on again: the same stubs')

print('\n== a reload takes an earlier run\'s hooks')
fpsSession()
ok(env.fixFrameRate('wheelfps') == 5 and allocs == 1 and callTarget(0x6D6E69) == wheelStub, 'taken as ours, nothing allocated')
ok(env.stopFrameRate('wheelfps') == 5 and same(get(0x6D76CD, 6), hex('D905CCB9C200')), 'and comes out when off')

print('\n== another mod\'s hook at a site stands the option down')
fpsSession(); fpsStock()
put(0x6D767F, 0xE8); putU32(0x6D7680, 0x13378000 - 0x6D7684)
allocs = 0
do
    local before = snapshot(0x00400000, 0x7FFFFFFF)
    ok(env.fixFrameRate('wheelfps') == 0 and allocs == 0 and unchanged(before, snapshot(0x00400000, 0x7FFFFFFF)),
        'not applied, nothing written')
    ok(printed[#printed]:find('hooked by code at 0x13378000; standing down', 1, true) and
        FPS.owner.wheelfps == 'code at 0x13378000', 'named: ' .. tostring(printed[#printed]))
end
fpsSession(); fpsStock()

print('\n== Car Speed FPS Fix (CLEO): nothing done unasked; stop, keep and a confirmed remove')
do
    -- what identifies it: 0A8D reading 0x5B7273 first, an 0A8C write later
    local cs = '\141\10\1\115\114\91\0\4\4\4\0\3\0\0' .. '\1\0\4\0' .. ('\0'):rep(30) ..
        '\140\10\3\0\0\4\4\3\4\0\4\0' .. '\2\0\1\235\255\255\255' .. '\147\10'
    local CS_BASE, SCRIPT_A, SCRIPT_B, OTHER = 0x07000000, 0x07100000, 0x07100100, 0x07200000
    for i = 1, #cs do mem[CS_BASE + i - 1] = cs:byte(i) end
    put(OTHER, 0x01, 0x00, 0x04, 0x00)                                -- another script: wait 0
    local function script(at, nextScript, base, ip)
        putU32(at, nextScript); putU32(at + 0x10, base); putU32(at + 0x14, ip)
    end
    local function running()
        script(SCRIPT_A, SCRIPT_B, OTHER, OTHER)
        script(SCRIPT_B, 0, CS_BASE, CS_BASE + 0x15)                  -- after its wait 0
        putU32(0xA8B42C, SCRIPT_A)
        put(0xC2B9CC, 0xC5, 0x20, 0x70, 0x3E)                         -- what it wrote last (0.2345)
    end
    local dir = os.tmpname(); os.remove(dir); os.execute('mkdir -p "' .. dir .. '/cleo"')
    local NAME = 'Car Speed FPS Fix (Junior_Djjr).cs'
    local function writeFile() local out = assert(io.open(dir .. '/cleo/' .. NAME, 'wb')); out:write(cs); out:close() end
    writeFile()
    local other = assert(io.open(dir .. '/cleo/other.cs', 'wb')); other:write('\1\0\4\0'); other:close()
    env.getGameDirectory = function() return dir end
    local listing
    env.findFirstFile = function()
        listing = {}
        for _, n in ipairs({'other.cs', NAME}) do
            local f = io.open(dir .. '/cleo/' .. n, 'rb'); if f then f:close(); listing[#listing + 1] = n end
        end
        return 1, table.remove(listing, 1)
    end
    env.findNextFile = function() return table.remove(listing, 1) end
    env.findClose = function() end
    local function unix(path) return (path:gsub('\\', '/')) end
    local envIo = rawget(env, 'io')                                   -- the options file's fake, if any
    env.io = setmetatable({open = function(path, mode)
        if path:find('cleo', 1, true) then return io.open(unix(path), mode) end
        return (envIo or io).open(path, mode)
    end}, {__index = envIo or io})
    env.os = setmetatable({rename = function(a, b) return os.rename(unix(a), unix(b)) end}, {__index = os})
    local function said(text, from) for i = from + 1, #chat do if chat[i]:find(text, 1, true) then return chat[i] end end end
    local opts = upvalue(env.fixesCommand, 'options')

    fpsSession(); fpsStock(); running()
    opts.wheelfps = true
    ok(env.fixFrameRate('wheelfps') == 5, 'wheelfps ours')
    local from = #chat
    FPS.cleoTick()
    ok(getU32(SCRIPT_B + 0x14) == CS_BASE + 0x15 and same(get(0xC2B9CC, 4), {0xC5, 0x20, 0x70, 0x3E}),
        'found: the script and its value untouched')
    ok(same(get(0x6D6E69, 6), hex('D905CCB9C200')) and FPS.owner.wheelfps == 'Car Speed FPS Fix (CLEO)',
        'wheelfps stood down, so nothing is scaled twice')
    ok(said('Car Speed FPS Fix (CLEO) is running', from) and said('/fixes cleo stop', from), 'asked in chat')
    local asked = #chat
    FPS.cleoTick()
    ok(#chat == asked, 'asked once')

    print('\n== /fixes cleo stop: ended for this session, wheelfps on')
    env.fixesCommand('cleo stop')
    ok(getU32(SCRIPT_B + 0x14) == FPS.stubs.cleoEnd and same(get(FPS.stubs.cleoEnd, 2), {0x93, 0x0A}),
        'its IP at an 0A93: CLEO ends and frees it')
    ok(getU32(SCRIPT_A + 0x14) == OTHER, 'another script untouched')
    ok(same(get(0xC2B9CC, 4), hex('6666663F')) and callTarget(0x6D6E69) == FPS.stubs.wheel, 'fWheelFriction 0.9, wheelfps on')
    ok(chat[#chat]:find('stopped for this session', 1, true) and chat[#chat]:find(NAME, 1, true), 'said: ' .. chat[#chat])
    local f = io.open(dir .. '/cleo/' .. NAME, 'rb')
    ok(f ~= nil, 'its file is still there'); if f then f:close() end

    print('\n== /fixes cleo keep: kept, wheelfps off and saved')
    fpsSession(); fpsStock(); running(); FPS.cleoState = nil
    opts.wheelfps = true; env.fixFrameRate('wheelfps'); FPS.cleoTick()
    local encode, savedAll = env.encodeJson, nil
    env.encodeJson = function(t) savedAll = t; return encode(t) end
    env.fixesCommand('cleo keep')
    env.encodeJson = encode
    ok(opts.wheelfps == false and savedAll and savedAll.wheelfps == false, 'wheelfps off, saved')
    ok(getU32(SCRIPT_B + 0x14) == CS_BASE + 0x15, 'the script keeps running')
    from = #chat
    FPS.cleoTick()
    ok(#chat == from, 'and is not asked about again')

    print('\n== /fixes cleo remove: explained first, done only with yes in time, renamed not deleted')
    FPS.cleoState = nil; env.fixesCommand('wheelfps on'); running(); FPS.cleoTick()
    from = #chat
    env.fixesCommand('cleo remove yes')
    ok(said('first, then /fixes cleo remove yes', from) and getU32(SCRIPT_B + 0x14) == CS_BASE + 0x15,
        'yes before asking: refused, nothing done')
    env.fixesCommand('cleo remove')
    ok(said('Nothing is deleted', from) and said('within 30 seconds', from), 'explained, with the 30 seconds')
    local f2 = io.open(dir .. '/cleo/' .. NAME, 'rb')
    ok(f2 ~= nil, 'nothing done yet'); if f2 then f2:close() end
    FPS.removeUntil = os.clock() - 1
    env.fixesCommand('cleo remove yes')
    ok(said('first, then /fixes cleo remove yes', #chat - 1), 'too late: refused')
    env.fixesCommand('cleo remove')
    env.fixesCommand('cleo remove yes')
    local gone, kept = io.open(dir .. '/cleo/' .. NAME, 'rb'), io.open(dir .. '/cleo/' .. NAME .. '.disabled', 'rb')
    ok(gone == nil and kept ~= nil, 'renamed to .cs.disabled, the bytes kept'); if kept then ok(kept:read('*a') == cs, 'unchanged'); kept:close() end
    ok(getU32(SCRIPT_B + 0x14) == FPS.stubs.cleoEnd and opts.wheelfps, 'and stopped, wheelfps on')
    ok(chat[#chat]:find('.disabled', 1, true), 'said: ' .. chat[#chat])
    from = #chat
    env.fixesCommand('cleo remove')
    ok(said('No Car Speed FPS Fix .cs file', from), 'nothing left to remove')

    env.getGameDirectory, env.findFirstFile, env.findNextFile, env.findClose, env.os = nil, nil, nil, nil, nil
    env.io = envIo
    os.execute('rm -rf "' .. dir .. '"')
    putU32(0xA8B42C, 0); FPS.cleoState = nil
end

print('\n== an ASI a fix stood down for is named in chat once')
do
    fpsSession(); fpsStock()
    FPS.owner.swimfps, FPS.owner.floatfps = 'Swim FPS Fix.asi', 'Swim FPS Fix.asi'
    FPS.told = {}
    local fromChat = #chat
    FPS.reportDuplicates()
    local said = 0
    for i = fromChat + 1, #chat do if chat[i]:find('Swim FPS Fix.asi does what fixes already does', 1, true) then said = said + 1 end end
    ok(said == 1, 'Swim FPS Fix.asi: one line for both options (' .. said .. ')')
    FPS.owner.wheelfps = 'code at 0x13378000'
    FPS.reportDuplicates()
    ok(#chat == fromChat + 1, 'an unknown hook is not a duplicate, and nothing twice')
    fpsSession()
end
fpsSession(); fpsStock()

--------------------------------------------------------------------------------
-- 0.9.0: entity references
--------------------------------------------------------------------------------
-- The head load in CEntity::RegisterReference, and the block's code as
-- i686-w64-mingw32-as assembled it for ready 0x11111111 and used 0x22222222.
local REF_VA = 0x571BB3
local REF_AS = 'A16817BA0085C0751EA11111111185C07415A36817BA00C7051111111100000000FF0522222222C3'
local refBuild = upvalue(env.fixReferencePool, 'refPoolCode')
ok(same(refBuild(0x11111111, 0x22222222), hex(REF_AS)), 'the block\'s code is the assembler\'s')

print('\n== entity references: the head load calls the block')
for k = REF_VA, REF_VA + 4 do mem[k] = nil end
put(REF_VA, 0xA1, 0x68, 0x17, 0xBA, 0x00)
allocs = 0
ok(env.fixReferencePool() == true and allocs == 1, 'applied, one block')
local refBlock = nextBlock - 0x100000
ok(mem[REF_VA] == 0xE8 and (REF_VA + 5 + getU32(REF_VA + 1)) % 0x100000000 == refBlock + 0x10,
    'a call to the block\'s code, in the same 5 bytes')
ok(getU32(refBlock) == 0x66527846 and getU32(refBlock + 4) == 0 and getU32(refBlock + 8) == 0,
    'header: magic, no reserve yet, none used')
ok(same(get(refBlock + 0x10, 40), refBuild(refBlock + 4, refBlock + 8)), 'the code names this block\'s fields')

print('\n== entity references: each frame keeps a reserve ready and prunes a slice')
-- pools: 2 peds (one free), no vehicles, 64 objects
local PEDS, OBJS = 0x40000000, 0x40100000
local function pool(at, objects, flags, size, used)
    putU32(at, objects); putU32(at + 4, flags); putU32(at + 8, size)
    for i = 0, size - 1 do mem[flags + i] = used(i) and 0x01 or 0x81 end
end
putU32(0xB74490, PEDS); pool(PEDS, 0x41000000, PEDS + 0x100, 2, function(i) return i == 1 end)
putU32(0xB74494, 0)
putU32(0xB7449C, OBJS); pool(OBJS, 0x42000000, OBJS + 0x100, 64, function() return true end)
thiscalls = {}
allocs = 0
env.refPoolTick()
local reserve = getU32(refBlock + 4)
ok(allocs == 1 and reserve ~= 0 and getU32(refBlock + 12) == 1, 'one reserve made and ready')
ok(getU32(reserve) == reserve + 8 and getU32(reserve + 8 * 4094) == reserve + 8 * 4095
    and getU32(reserve + 8 * 4095) == 0, 'linked: each to the next, the last to nothing')
-- 66 entities in 64 slices: 2 a frame; the first slice is the two peds, one free
ok(#thiscalls == 1 and thiscalls[1][1] == 0x571A90 and thiscalls[1][2] == 0x41000000 + 0x7C4,
    'PruneReferences on the used ped only')
thiscalls = {}
env.refPoolTick()
ok(#thiscalls == 2 and thiscalls[1][2] == 0x42000000 and thiscalls[2][2] == 0x42000000 + 0x19C,
    'the next slice starts the objects')
ok(allocs == 1, 'no second reserve while one is ready')

print('\n== entity references: when the game takes the reserve, the next frame makes another')
putU32(refBlock + 4, 0); putU32(refBlock + 8, 1)   -- what the block's code does
env.refPoolTick()
ok(allocs == 2 and getU32(refBlock + 4) ~= 0 and getU32(refBlock + 12) == 2, 'a second reserve')
putU32(refBlock + 12, 64); putU32(refBlock + 4, 0)
env.refPoolTick()
ok(allocs == 2 and getU32(refBlock + 4) == 0, 'none past 64')

print('\n== entity references: a reload finds its block')
ok(env.fixReferencePool() == true and allocs == 2, 'reattached, nothing allocated')
ok(printed[#printed]:find('already patched', 1, true), 'said so: ' .. tostring(printed[#printed]))

print('\n== entity references: RefPoolFix.asi\'s call there: stand down, nothing written')
put(REF_VA, 0xE8, 0x00, 0x00, 0x00, 0x10)
local before = get(REF_VA, 5)
allocs = 0
ok(env.fixReferencePool() == false and allocs == 0, 'not applied')
ok(same(get(REF_VA, 5), before), 'its call is left')
ok(printed[#printed]:find('hooked by code at 0x', 1, true) and printed[#printed]:find('standing down', 1, true),
    'said so: ' .. tostring(printed[#printed]))
thiscalls = {}
env.refPoolTick()
ok(#thiscalls == 0, 'and the frame tick does nothing')

print('\n== entity references: anything but the stock load is refused')
put(REF_VA, 0x8B, 0x0D, 0x68, 0x17, 0xBA)
allocs = 0
ok(env.fixReferencePool() == false and allocs == 0, 'refused')
ok(printed[#printed]:find('refusing the entity-reference fix', 1, true), 'said why: ' .. tostring(printed[#printed]))

--------------------------------------------------------------------------------
-- 0.10.1: lines go to akacross.log when the shared runtime is installed
--------------------------------------------------------------------------------
print('\n== with akacross.log: every line goes to the logger as fixes, none to print')
do
    local logged, setupName, leveled, all = {}, nil, {}, {}   -- all: both kinds, in order
    local function at(level) return function(_, text)
        leveled[#leveled + 1] = {level, text}; all[#all + 1] = {level, text}
    end end
    local fakeLog = {setup = function(o)
        setupName = o.name
        return {line = function(text) logged[#logged + 1] = text; all[#all + 1] = {nil, text} end,
            scope = function() return {info = at('info'), warn = at('warn')} end}
    end}
    local printedBefore = #printed
    local env2 = setmetatable({
        require = function(name)
            if name == 'akacross.log' then return fakeLog end
            return env.require(name)
        end,
    }, {__index = env})
    setfenv(assert(loadfile(root .. 'fixes.lua')), env2)()
    for k = 0x571BB3, 0x571BB7 do mem[k] = nil end
    put(0x571BB3, 0x8B, 0x0D, 0x68, 0x17, 0xBA)   -- not the stock load: refused, and said why
    env2.fixReferencePool()
    ok(setupName == 'fixes', 'the logger is set up under the name fixes')
    ok(#logged == 1 and logged[1]:find('refusing the entity-reference fix', 1, true), 'the line reached the logger: ' .. tostring(logged[1]))
    ok(#printed == printedBefore, 'and not print')

    -- the startup count's level is what happened, not its words ("0 refused")
    local function summary()
        env2.reportStartup()
        local last = leveled[#leveled]
        return last and last[2]:find('^started %- ') and last or nil
    end
    local others = upvalue(env2.reportStartup, 'otherFixes')
    others[1] = {'animation wipes', 'in place'}; others[2] = {'removed buildings', 'left to omp-client.dll'}
    -- every site stock, so nothing refuses
    for _, p in ipairs(upvalue(env2.applyGamePatches, 'gamePatches')) do put(p.va, unpack(p.from)) end
    local c = env2.applyGamePatches('early')
    local line = summary()
    ok(line and c.refused == 0 and line[1] == "info" and not line[2]:find('[fixes]', 1, true),
        'the count is info when nothing was refused: ' .. tostring(line and line[2]))
    ok(line and line[2]:find('other fixes: 1 in place, 1 left to other mods, 0 off, 0 not in place', 1, true),
        'and counts the other fixes')
    stockVis()
    put(0x553535, 0xCC)   -- a different function at the visible-list site: refused
    c = env2.applyGamePatches('early')
    line = summary()
    ok(c.refused > 0 and line and line[1] == 'warn', 'a warning when a site was refused')
    stockVis()
    env2.applyGamePatches('early')
    others[3] = {'smoking animation', 'not in place'}
    line = summary()
    ok(line and line[1] == 'warn' and line[2]:find('1 not in place (smoking animation)', 1, true),
        'and when a fix is not in place, naming it: ' .. tostring(line and line[2]))
    for i = #others, 1, -1 do others[i] = nil end

    print('\n== startup: the count comes once, last, after every fix has run')
    env2.isSampAvailable = function() return true end
    env2.isSampfuncsLoaded = function() return true end
    env2.wait = function() coroutine.yield() end
    local from = #all
    local co = coroutine.create(env2.main)
    local okRun, err = coroutine.resume(co)
    ok(okRun and coroutine.status(co) == 'suspended', 'main runs up to its frame loop: ' .. tostring(err))
    local counts, partial = 0, 0
    for i = from + 1, #all do
        if all[i][2]:find('^started %- ') then counts = counts + 1 end
        if all[i][2]:find('gta_sa.exe patches (', 1, true) then partial = partial + 1 end
    end
    ok(counts == 1 and all[#all][2]:find('^started %- '), 'one count, and it is the last line')
    ok(partial == 0, 'no count in the middle of startup')
    local fromChat = #chat
    env2.fixesCommand('patches')
    local listedOthers = 0
    for _, name in ipairs({'animation wipes', 'entity references', 'removed buildings', 'text cache',
            'server RPC checks', 'F1 help dialog off', 'smoking animation', 'animation blocks pinned', 'fog restore',
            'position guard'}) do
        for i = fromChat + 1, #chat do if chat[i]:find(name .. ': ', 1, true) then listedOthers = listedOthers + 1; break end end
    end
    ok(listedOthers == 10, ('/fixes patches lists all 10 other fixes with their state (%d)'):format(listedOthers))
    stockVis()
end

-- --store-bytes <file>: the store for rb_store_test.c (count 0x20000000,
-- table 0x20001000, cap 8). --wipe-bytes <file>: the animation-wipe code for
-- anim_wipe_test.c (the real plugin offset 0xB5F878, its block at 0x20000000).
-- --filter-bytes <file>: the exception filter's tail for exception_filter_test.c,
-- built for SA-MP R5 at its preferred base (the harness moves the counter).
-- --fog-bytes <file>: the fog routine, which runs anywhere, for
-- crashfix_dll_test.c. --guard-bytes <file>: the position guard's block (its
-- counters, slots, routine and entries) built at 0x20000000 with the player
-- pointer at 0x20000F00, for position_guard_test.c. --fps-bytes <file>: the
-- frame-rate stubs built at 0x20000000, for fps_fixes_test.c.
for i = 1, #arg do
    if arg[i] == '--filter-bytes' and arg[i + 1] then
        local build = upvalue(env.fixExceptionFilter, 'exceptionFilterCode')
        local bytes = build(0x10000000 + EF_SITE_RVA, 0x10000000 + EF_COUNT_RVA, 0x10000000 + EF_PRINT_RVA)
        local f = assert(io.open(arg[i + 1], 'wb'))
        for _, b in ipairs(bytes) do f:write(string.char(b)) end
        f:close()
        print(('\nwrote the exception filter tail (%d bytes) to %s'):format(#bytes, arg[i + 1]))
    end
end
for i = 1, #arg do
    if arg[i] == '--wipe-bytes' and arg[i + 1] then
        local build = upvalue(env.fixAnimationWipe, 'animWipeCode')
        local bytes = build(0xB5F878, 0x20000080, 0x20000008, 0x20000004)
        local f = assert(io.open(arg[i + 1], 'wb'))
        for _, b in ipairs(bytes) do f:write(string.char(b)) end
        f:close()
        print(('\nwrote the animation-wipe code (%d bytes) to %s'):format(#bytes, arg[i + 1]))
    end
end
for i = 1, #arg do
    if arg[i] == '--fog-bytes' and arg[i + 1] then
        local build = upvalue(env.fixFogRestore, 'fogRestoreCode')
        local code, entries = build(0)
        local f = assert(io.open(arg[i + 1], 'wb'))
        for j = 1, entries[1] do f:write(string.char(code[j])) end
        f:close()
        print(('\nwrote the fog routine (%d bytes) to %s'):format(entries[1], arg[i + 1]))
    end
end
for i = 1, #arg do
    if arg[i] == '--guard-bytes' and arg[i + 1] then
        local PG = upvalue(env.fixPositionGuard, 'PG')
        local code = PG.code(0x20000000, 0x20000F00)   -- the player pointer inside the block
        local f = assert(io.open(arg[i + 1], 'wb'))
        for _, b in ipairs(code) do f:write(string.char(b)) end
        f:close()
        print(('\nwrote the position guard (%d bytes) to %s'):format(#code, arg[i + 1]))
    end
end
for i = 1, #arg do
    if arg[i] == '--fps-bytes' and arg[i + 1] then
        local FPS = upvalue(env.fixFrameRate, 'FPS')
        local code = FPS.code(0x20000000,           -- the game globals at +F00h, the jmp back to +F80h
            {timestep = 0x20000F00, friction = 0x20000F04, minus = 0x20000F08, back = 0x20000F80, lift = 0x20000F0C})
        local f = assert(io.open(arg[i + 1], 'wb'))
        for _, b in ipairs(code) do f:write(string.char(b)) end
        f:close()
        print(('\nwrote the frame-rate stubs (%d bytes) to %s'):format(#code, arg[i + 1]))
    end
end
for i = 1, #arg do
    if arg[i] == '--store-bytes' and arg[i + 1] then
        local build = upvalue(env.fixRemovedBuildings, 'removedBuildingsStore')
        local bytes = build(0x20000000, 0x20001000, 8)
        local f = assert(io.open(arg[i + 1], 'wb'))
        for _, b in ipairs(bytes) do f:write(string.char(b)) end
        f:close()
        print(('\nwrote the store (%d bytes) to %s'):format(#bytes, arg[i + 1]))
    end
end

print(('\nfixes: %d passed, %d failed'):format(passed, failed))
os.exit(failed == 0 and 0 or 1)
