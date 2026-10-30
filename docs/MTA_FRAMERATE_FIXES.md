# MTA's frame-rate fixes against fixes

MTA:SA keeps its high-fps fixes in `Client/multiplayer_sa/CMultiplayerSA_FrameRateFixes.cpp`
(mtasa-blue as of 2026-10, `/mnt/data/Projects/samp/mtasa-blue-master`): 38 hooks, 23
byte writes and 10 call redirects. They work two ways: physics the game applies once per
step is scaled by `timestep / 1.6667` (stock at 30 fps), and effects the game spawns once
per frame run only on a 30 Hz tick. Compared 2026-10-29 with fixes 0.19.0.

**Applied in fixes 0.20.0** (MEMORY_FIXES.md 40, 41): both gaps and every row below
except the two under "not to take", with MTA's chassis and bike-clamp hooks corrected.

Every site below is stock in gta_sa.exe 1.0 US (HOODLUM) and ends on an instruction
boundary (the water-cannon fx site covers padding after a short jmp; the air-resistance
hook never returns into the bytes it takes). `cBuoyancy::AddSplashParticles` starts with
HOODLUM's `nop / jmp 0x401C83`, a stub holding its `sub esp,0E0h`: the same thing, so
MTA's hook ports against those bytes. No installed mod (samp.dll, SAMPFUNCS, SilentPatch,
ProperShaders, the limit adjuster, CLEO, MoonLoader) names any of the sites.

## What fixes has

| fixes | MTA | note |
|---|---|---|
| gamePatches chase camera, 0x524FD7 | the same NOP | same |
| `wheelfps`, 0x6D6E69 and four more | the rapid-stop fix's wheel friction | same sites; MTA pairs it with air resistance (below) |
| `swimfps`, 0x68A4FC + dive 0x68A42B | swimming resistance + dive speed | same job; MTA also compensates the underwater buoyancy (below) |
| `floatfps`, 0x6C27AE | cBuoyancy damping, 0x6C27B7 (peds) | another way to keep a surfacing ped up: one or the other, not both |

## Missing

Gaps in what fixes has:

| site | what | note |
|---|---|---|
| 0x68A4CA | the underwater swim state's buoyancy (+0.01) | `swimfps` scales the whole target, z too, by 1.6667 / timestep, so underwater a swimmer rises with 4x the buoyancy at 120 fps; MTA scales the 0.01 back (#3344) |
| 0x544D29 | `CPhysical::ApplyAirResistance`, 0.99 per step on the turn speed | the move speed already uses `pow(..., timestep)`; the turn speed does not, so a spin dies faster at high fps. MTA's rapid-stop fix does it with the wheel friction |

Physics scaled by the timestep:

| site | what |
|---|---|
| 0x6811E9, 0x68128A, 0x68131B | climbing over objects kills the player (MTA #602): the climb reads a constant 1.6667. The third is the get-to-position timer: with the constant it runs out at high fps and the climb aborts with a push back, so fixes takes only the first two |
| 0x6DC23F, 0x6D2771 | boats: `ProcessBoatControl` and `ApplyBoatWaterResistance` (slow boats) |
| 0x6C4F13 | helicopter takeoff acceleration (RC helis 465/501 their own rate) |
| 0x61E4F2, NOP 0x61E5E4 | moving while aiming: the move threshold scaled, `m_MoveCmd`'s reset gone (camera jitter) |
| 0x52C723 | `CCamera::Process` shake speed |
| 0x716BA6 | moving fog speed |
| 0x71AABF, 0x71AAEA, 0x71AB29 | falling glass shards spinning and flying too fast |
| 0x6F42D5, 0x6F4422 | a swinging chassis (`DOOR_EXTRA_CHASSIS`) rolling too hard |
| 0x6B1D34, 0x6B997C, 0x6B99C5; NOP 0x6B1AF5, 0x6B9850, 0x6B361C, 0x6BC18F; the 0.005 at 0x6B33F8, 0x6B340E, 0x6B3424, 0x6BC103, 0x6BC119, 0x6BC12B | cars and bikes going to sleep after 10 still frames (41 ms at 240 fps) with the suspension still settling, and the damping that zeroes their fall speed. MTA scales from 0.0045; the exe's constant (0x858B4C) is 0.005 |

Effects spawned once per frame, gated to 30 Hz:

| site | what |
|---|---|
| 0x6D2D50, 0x6AA78A | wheel dirt, water and sand |
| 0x6DE240 | exhaust |
| 0x6C34E0, 0x6DD130, 0x6DDF60 | boat splashes, rain splashing on vehicles |
| 0x72AAA8 | rain |
| 0x68AD3B, 0x68AC31 | swimming effects and bubbles |
| 0x6CA937, 0x6CBE4B, 0x6C939A | stunt-plane smoke trails, damaged-plane particles |
| 0x738C63 | rocket smoke |
| 0x72A29B, 0x72A37B, 0x729437 | the water cannon's reach, how often it pushes peds, its particles |
| 0x59E420 | broken objects despawning early |
| 0x5700F5 | the money counter's animation |
| 0x5E7181 | particles of a headless ped (MTA's setPedHeadless; nothing for SA-MP) |

The tick: MTA's `CTimer::Update` hook (0x561C5D) makes `CTimer::m_FrameCounter` itself
count at 30 Hz. ProperShaders reads `m_FrameCounter` (eight references), so a port keeps
its own 30 Hz flag and counter and leaves the game's alone; the water cannon's push
(0x72A37B), which reads the counter, reads ours.

Larger:

| site | what |
|---|---|
| 10 calls to `CPhysical::ApplyCollision` (0x54BDB2 ...) and `ApplyCollisionAlt` (0x54C9FA, 0x54CAC2) | a ped pushing an unoccupied car: an impulse every contact frame makes it roll away at high fps; MTA caps it by momentum (a C function) |

Not to take as it is:

| site | why |
|---|---|
| 0x73DC3D `CWeapon::Update` | "invisible extinguisher, spraycan and flamethrower particles": in the FIRING state it shrinks the time to the next shot every frame for every weapon but the camera, so at high fps weapons leave FIRING sooner. On SA-MP that is a fire-rate change; if wanted, only for weapons 37, 41 and 42, and tested |
