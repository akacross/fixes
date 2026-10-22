-- signature_scanner/gta_offsets.lua - gta_sa.exe absolute addresses, kept here
-- so they are never mistaken for samp.dll offsets:
--   * absolute addresses in gta_sa.exe (not relocated in practice), used directly
--   * all past samp.dll's SizeOfImage (0x27E000 on R5, 0x330000 on R1), so as
--     samp RVAs they would land outside the image
--
-- Each entry is checked against /mnt/data/Projects/gta-reversed-master.

local M = {}

M.module = 'gta_sa.exe'

M.addresses = {
    -- CAnimManager -- source/game_sa/Animation/AnimManager.{h,cpp}
    AddAnimBlockRef  = 0x4D3FB0,   -- RH_ScopedInstall(AddAnimBlockRef, 0x4D3FB0)
    ms_numAnimBlocks = 0xB4EA30,   -- StaticRef<uint32>(0xB4EA30)

    -- CRadar -- source/game_sa/Radar.h
    ms_RadarTrace    = 0xBA86F0,   -- StaticRef<std::array<tRadarTrace, MAX_RADAR_TRACES>>(0xBA86F0)

    -- CWorld -- source/game_sa/World.h
    Players          = 0xB7CD98,   -- StaticRef<CPlayerInfo[MAX_PLAYERS]>(0xB7CD98)

    -- platform -- source/app/platform/win/WinPs.cpp
    PsGlobal         = 0xC8CF88,   -- StaticRef<psGlobalType>(0xC8CF88); holds the HWND

    -- CControllerConfigManager::AffectPadFromKeyBoard -- 0x531140;
    -- lib/mimgui/init.lua NOPs 5 bytes at +0x15 to swallow keyboard input
    AffectPadFromKeyBoard = 0x531140,
}

-- Code sites existing scripts patch (gta_sa.exe instruction addresses), listed
-- so they are not mistaken for samp symbols. See fixes.lua and nocamrestore.lua.
M.patchSites = {
    'fixes.lua      -- 0x6E1D4F 0x6FB9A0 0x7069F5 0x70A83B 0x70A4CB 0x6E1425 0x884984',
    'fixes.lua      -- 0x53E94C 0xC2B9CC 0x524FD7 0x736F88 0x745BC9 0x7459E1',
    'fixes.lua      -- 0x6D6517 0x6D0E43 0x63F576 0x460773 0x48C36B',
    'nocamrestore   -- 0x5231A6 0x52322D 0x5233BA',
    'lib/mimgui     -- 0x531155',
}

return M
