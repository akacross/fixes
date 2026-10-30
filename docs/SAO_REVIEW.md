# San Andreas Online against fixes

San Andreas Online (github.com/Dryxio/san-andreas-online, `/mnt/data/Projects/samp/san-andreas-online-master`)
is a fork of mtasa-blue. Most of what it adds is its own: story missions, scripted AI peds
with ownership sync, relocated pools and stores, an extended world, a new radar and UI.
Compared 2026-10-30 with fixes 0.20.0 by every game address in `Client/multiplayer_sa` and
`Client/game_sa` that mtasa-blue does not have.

**Applied in fixes 0.21.0** (MEMORY_FIXES.md 42): the two crash fixes and the glass list.

## Worth taking

| site | what | note |
|---|---|---|
| 0x70FC87, 0x710471 | `CStencilShadows::RenderForVehicle` / `RenderForObject`: a col shadow mesh over 2048 vertices | the game allocates 2048 points per frame and transforms the col's whole shadow mesh (`CCollisionData+0x24`) into them; past that it writes off the end and crashes at 0x80D5EF. The shadows draw at FX quality medium and up; with fixes, only with `volshadows` on. SA:O skips that one shadow and jumps to each function's epilogue (0x7102D8, 0x710AB2), which only uses esp. Stock bytes; no installed mod names either site |
| 0x618370 | `IKChain_c::Init`: a bone missing from the ped's hierarchy | `RpHAnimIDGetIndex` returns -1 for it and the game indexes the frames with it before checking: crash at 0x6183CA. Head tracking and gun pointing start IK chains on every ped, so a skin with a bone missing crashes whoever sees it. SA:O walks effector to pivot through the bone table at 0x8D26D0 first and returns false (`retn 3Ch`) if one is missing. Stock bytes; fixes has nothing there |

## Smaller

| site | what | note |
|---|---|---|
| 8 calls to `CLinkList::InsertSorted` (0x733910), list 0xC88070 | the vehicle alpha-atomic list holds 20 per vehicle | it is filled and drawn per vehicle (`CRenderer::RenderOneNonRoad`); past 20 a vehicle's transparent parts draw unsorted, so glass hides what is behind it. Only a modded car with more than 20 transparent atomics reaches it. The limit adjuster's `AlphaEntityList` is for non-vehicle entities only |

## Already covered

| SA:O | fixes |
|---|---|
| DFT-30 left middle wheel named `wheel_lm` (0x4C5311) | SilentPatch fixes it (its strings hold `wheel_lm_dummy` and `wheel_lm`) |
| `CMultiplayerSA_FrameRateFixes.cpp` | an older subset of mtasa-blue's; fixes 0.20.0 has all of it and more |
| `ControlGunMove` (0x61E0C0) on a private timestep | copies what samp.dll does. With samp.dll's step (3 x 50 / fps x 0.07) the first move step stays above fixes' `aimwalk` threshold (0.1 x timestep / 1.6667) at any frame rate, so the two agree |
| coronas 4096, visible lists 8192, the entity alpha list | the limit adjuster ini: coronas 20000, visible lists and the alpha entity list unlimited |

## Not for fixes

- `ENTITY_PERFORMANCE.md`: measurements only; the one optimization it tried (AABB rejection) it rejected.
- `LIMIT_PATCHING.md`: pool, store and world-sector relocations for SA:O's extended map.
- Project2DFX far clip, vegetation LOD, cull zones, ped path patches, native world packs: features for its own map.
- The other new hooks (choking, bike jacking, jump stats, kill-ped-with-car, boat damage, damage-event copies): its scripted AI peds and their sync.
