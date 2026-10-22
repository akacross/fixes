-- Host test for fixes.lua: 0.5.0's optional patches - the plate pair applies
-- only while its option is on (off by default), /fixes switches it at once -
-- off writes the stock bytes back - and the choice is saved; and 0.6.0's
-- removed-buildings fix against SA-MP R5's real store and lookup bytes, at its
-- own base and relocated; and 0.7.0's animation-wipe fix, 0.8.x's visible-list
-- bound and 0.9.0's entity references against gta_sa.exe 1.0 US's bytes. The real
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
        return setmetatable({}, {__index = function(_, i) return mem[va + i] or 0 end,
            __newindex = function(_, i, v) mem[va + i] = v end})
    end,
    C = {VirtualAlloc = function(_, size, kind, protect)
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
ok(#chat > before + 1 and chat[before + 1]:find('plates: ', 1, true) and chat[before + 2]:find('textcache: on', 1, true),
    'the list names plates, then textcache, with their states')
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

-- --store-bytes <file>: the store for rb_store_test.c (count 0x20000000,
-- table 0x20001000, cap 8). --wipe-bytes <file>: the animation-wipe code for
-- anim_wipe_test.c (the real plugin offset 0xB5F878, its block at 0x20000000).
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
