script_name("camnan_test")
script_author("akacross")

--[[
    /camnan puts NaN in the active camera where the fall at 12:28 on 1 Nov
    left it, and says how many frames it took fixes' camnan to set it up again.

    /camnan stuck [seconds] takes the camnan hook out first (the stock bytes
    back at 0x526FC0, the rest of crashfix and its line-of-sight guard left
    in), poisons the camera, waits (5 s by default) to show whether it stays
    broken, then puts the hook back and says how fast camnan cleared it.

    /camnan force poisons it with camnan not in: the camera may stay broken
    until a change of camera mode (a respawn, a car).

    MoonLoader loads only the scripts in moonloader\: copy it there to run it.
]]

local ffi = require 'ffi'

local CAMERA = 0xB6F028             -- TheCamera
local ACTIVE = CAMERA + 0x59        -- m_nActiveCam
local CAMS, CAM_SIZE = CAMERA + 0x174, 0x238
local HOOK = 0x526FC0               -- CCam::Process: E9 when camnan is in
local STOCK = {0xDB, 0x05, 0x34, 0xFE, 0xB6, 0x00}  -- fild dword [0xB6FE34]
-- The floats that were NaN in the camera after the fall: alpha and beta speeds,
-- true beta and alpha, the horizontal angle (the mouse adds to it every frame),
-- source and up speeds, front, source, source before looking behind, up,
-- and the target's history
local POISON = {0x40, 0x44, 0xA0, 0xA4, 0xBC}
for _, r in ipairs({{0x124, 0x12C}, {0x13C, 0x144}, {0x190, 0x1BC}, {0x1D8, 0x1EC}}) do
    for o = r[1], r[2], 4 do POISON[#POISON + 1] = o end
end
local FLOATS = {{0x2C, 0x110}, {0x118, 0x120}, {0x124, 0x208}, {0x220, 0x22C}}

local function say(text) sampAddChatMessage("{9ACD32}[camnan]{FFFFFF} " .. text, -1) end

local function activeCam() return CAMS + ffi.cast("uint8_t*", ACTIVE)[0] * CAM_SIZE end

local function getBytes(at, n)
    local t = {}
    for i = 1, n do t[i] = readMemory(at + i - 1, 1, true) end
    return t
end

local function setBytes(at, bytes)
    for i, b in ipairs(bytes) do writeMemory(at + i - 1, 1, b, true) end
end

-- -> the offsets in the camera that are NaN or infinite
local function badFloats(cam)
    local bad = {}
    for _, r in ipairs(FLOATS) do
        for o = r[1], r[2] - 4, 4 do
            local v = ffi.cast("float*", cam + o)[0]
            if v ~= v or v == math.huge or v == -math.huge then bad[#bad + 1] = o end
        end
    end
    return bad
end

local function badList(bad)
    local list = {}
    for i = 1, math.min(#bad, 8) do list[i] = ("0x%X"):format(bad[i]) end
    return table.concat(list, " ") .. (#bad > 8 and " ..." or "")
end

local function poison()
    local cam = activeCam()
    local nan = 0 / 0
    for _, o in ipairs(POISON) do ffi.cast("float*", cam + o)[0] = nan end
    say(("NaN in camera %d (mode %d), %d floats bad"):format((cam - CAMS) / CAM_SIZE,
        ffi.cast("uint16_t*", cam + 0x0C)[0], #badFloats(cam)))
    return cam
end

-- -> the frame the camera had no bad float, or nil within `frames`
local function watch(cam, frames)
    for frame = 1, frames do
        wait(0)
        if #badFloats(cam) == 0 then return frame end
    end
end

local busy, saved = false, nil      -- saved: the hook's bytes while /camnan stuck has it out

local function hookBack()
    if saved then setBytes(HOOK, saved); saved = nil end
end

local function stuck(seconds)
    saved = getBytes(HOOK, #STOCK)
    setBytes(HOOK, STOCK)
    local cam = poison()
    local start, frames = os.clock(), 0
    while os.clock() - start < seconds do
        wait(0)
        frames = frames + 1
        if #badFloats(cam) == 0 then
            hookBack()
            return say(("the game cleared it by itself after %d frame(s), camnan out: not the stuck camera"):format(frames))
        end
    end
    local bad = badFloats(cam)
    say(("still %d bad after %d s with camnan out (%s); putting it back"):format(#bad, seconds, badList(bad)))
    hookBack()
    local frame = watch(cam, 120)
    say(frame and ("camnan set it up again %d frame(s) after it went back in"):format(frame)
        or ("camnan back in, still %d bad after 120 frames (%s)"):format(#badFloats(cam), badList(badFloats(cam))))
end

local function trigger(arg)
    if busy then return say("still watching the last one") end
    local hooked = readMemory(HOOK, 1, true) == 0xE9
    local secs = arg:match("^stuck%s*(%d*)$")
    if secs then
        if not hooked then return say("camnan is not in (stock bytes at 0x526FC0): nothing to take out") end
    elseif not hooked and arg ~= "force" then
        return say("camnan is not in (stock bytes at 0x526FC0): the camera may stay broken. /camnan force to do it anyway")
    end
    lua_thread.create(function()
        busy = true
        if secs then
            stuck(tonumber(secs) or 5)
        else
            local cam = poison()
            local frame = watch(cam, 120)
            if frame then
                say(hooked and ("camnan set it up again after %d frame(s); fixes.log has the line within a minute"):format(frame)
                    or ("the game cleared it by itself after %d frame(s): not the stuck camera"):format(frame))
            else
                say(("still %d bad after 120 frames (%s): enter a car or respawn to reset it"):format(
                    #badFloats(cam), badList(badFloats(cam))))
            end
        end
        busy = false
    end)
end

function onScriptTerminate(s)
    if s == thisScript() then hookBack() end
end

function main()
    while not isSampAvailable() do wait(100) end
    sampRegisterChatCommand("camnan", trigger)
    wait(-1)
end
