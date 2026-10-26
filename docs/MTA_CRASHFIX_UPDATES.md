# MTA's crash fixes since gtasa_crashfix

gtasa_crashfix (crashes.asi 2.x, the source of fixes' `crashfix.dll`, see
MEMORY_FIXES.md 30) copied MTA:SA's `CMultiplayerSA_CrashFixHacks.cpp` years
ago, at about 1300 lines. The current mtasa-blue
(`/mnt/data/Projects/samp/mtasa-blue-master`, no git history in the copy)
has 4380, and more fixes in their own files. Compared 2026-10-25: hook bodies
normalised (spacing, hex style, `CRASH_AVERTED` vs `push n / call
CrashAverted`, MTA's test-only `SIMULATE_ERROR` blocks) and diffed.

**Applied in fixes 0.14.0:** the revisions below and the 48 hooks marked
port, with the changes MEMORY_FIXES.md 34 lists where MTA's code is wrong
(GetCurrentTranslation's return, FindObjectsKindaCollidingSectorList's FPU
stack, misc48's eax).

## What MTA revisited in crashfix's hooks

| hook | MTA now | for crashfix.dll |
|---|---|---|
| misc1, 2, 4, 6-11, 13, 14, 27, frame | the same code | nothing |
| misc5 | the same, reading the model-info array through a variable | nothing |
| misc16, 18, 20, 22-26, 28, freeze15, misc17/19/24 (labels) | the same (only test blocks and US/EU label names differ) | nothing |
| misc3 (`ComputeAnimID` with no vehicle) | **removed** | fixes' corrected version could go too |
| misc12 (`UncompressAnimation`, Mantis #5530) | **not installed** (commented out, no reason given) | consider off by default |
| misc17 (`RwFrameForAllObjects` objectList) | **not installed** | consider off by default |
| misc19 (`RwFrameAddChild`) | defined, **not installed** | already off (`frameclone`) |
| misc21 (`FinishAnimFallOutCB`) | **reworked**: no registry of live associations; checks the task's vtable is `CTaskSimpleCarFallOut`'s and clears a vehicle pointer MTA's pool does not know | replace the registry and its two hooks with the task check (the game's vehicle pool instead of MTA's) |
| misc29 (`IsSoundBankLoaded`) | **reworked**: bank `< 0` or `>=` the slot count (`[ecx+0Ch]`), not only -1 | take the range check |
| CopyAnimation | moved to `CMultiplayerSA_FixBadAnimId.cpp`, plus #1109: anim 224 (weapon fire) in a group without it is switched to the Python group | take #1109 |
| misc14, misc16 return paths | still MTA's original `add esp,N / ret` | keep fixes' unwinding versions |

## New in MTA

Verdict: **port** (a check on game code, the same for SA-MP), **startup**
(runs before any MoonLoader script, needs an ASI), **MTA** (needs MTA's own
systems or is diagnostics), **covered** (fixes already does it another way).

| fix | site | what | verdict |
|---|---|---|---|
| CMatrix::UpdateRwMatrix | 0x59AD70 | unaligned or invalid RwMatrix* | port |
| CPlaceable::AllocateMatrix x2 | 0x54F5A6, 0x54F5C5 | zero the attach-matrix fields of a new CMatrixLink | port |
| misc43, 44, 45 | 0x4E028C, 0x4E0347, 0x4E02E8 | `CAEMP3BankLoader::GetSoundBuffer`: slot id out of range, impossible counts, size past the slot buffer | port |
| misc47, 48, 49 | 0x4DFE7D, 0x4DFF78, 0x4E0094 | `CAEMP3BankLoader::Service`: bank sizes from the stream header clamped to the buffer | port |
| misc30, misc32 | 0x4CEBE8, 0x4CEA80 | `CAnimBlendAssociation::SetFinishCallback` / `SetCurrentTime` on a bad `this` | port |
| misc39 | 0x4C7510 | `CVehicleModelInfo::FindTextureCB`: inlined strcpy into 32 bytes (0xC0000409), reimplemented | port |
| misc33, 35, 36, 37 | 0x7F39F0, 0x7F374A, 0x7F3A09, 0x7F39B3 | texture dictionary lookups on bad dictionaries, list entries or names | port |
| misc34 | 0x5D9CB2 | car env-map pipeline, null env-map plugin data | port |
| misc38, VBInstV3d/Morph/Weights, DynVBCreate | 0x7C91C0, 0x752AD0, 0x753B60, 0x752320, 0x7F5A36 | null vertex buffers when video memory runs out (MTA also frees memory; that part is MTA's; IndicesRemap, 0x754C80, is not installed) | port the checks |
| CEntity::GetBoundRect | 0x534131 | null colmodel (MTA only logs it) | MTA |
| Interior_c::Init | 0x593BF0 | the furniture object counter is never reset | port |
| CAnimManager::CreateAnimAssocGroups | 0x4D3D52 | missing model info or RwObject (CreateAssociations, 0x4CE2F7, is defined but not installed) | port |
| CTaskComplexCarSlowBeDraggedOut(AndStandUp)::CreateFirstSubTask | 0x6485AC, 0x648AAF | the vehicle is gone while the driver is dragged out | port |
| RwMatrixMultiply | 0x7F18B0 | null third argument | port |
| CAnimBlendNode::GetCurrentTranslation | 0x4CFCB5 | invalid end keyframe index | port |
| CTrain::ProcessControl | 0x6F8F89 | rail distance wrap can loop forever | port |
| CPlaceName::Process | 0x571F39 | `bInVehicle` set with no vehicle | port |
| CWorld::FindObjectsKindaCollidingSectorList | 0x56508C | null model info or colmodel | port |
| RpClumpForAllAtomics, RpAnimBlendClumpGetFirstAssociation, CAnimManager::BlendAnimation | 0x749B70, 0x4D6A70, 0x4D4610 | null clump | port |
| FxPrim_c::Enable | 0x4A9F50 | null `this` | port |
| CFire::ProcessFire | 0x53A6FC | null `attachedTo` (MTA #1757) | port |
| CColModel::MakeMultipleAlloc | 0x40F740 | corrupt collision data before the copy | port |
| misc50, 51, 52 | 0x156650D, 0x156658A, 0x15665C9 | `AreTexturesUsedByRequestedModels`: null model info | port |
| CEventScanner::ScanForEvents | 0x6080B2 | a ped's contact entity destroyed (escalators) | port |
| CStreaming GetNextFileOnCd / ConvertBufferToObject | 0x408F6E, 0x40C8CE | a TXD freed while its request is queued | port |
| CPathFind node loader | 0x156F966 | unchecked malloc for path nodes | port |
| CVisibilityPlugins Get/SetClumpAlpha | 0x732B20, 0x732B00 | an entity with no RwObject | port |
| CStreaming::AreAnimsUsedByRequestedModels | 0x407AD5 | never unload IFP blocks | covered (`pinAnimBlocks`) |
| FxSystemBP_c::Load | 0x5C0A15 | emitters without a main texture | startup |
| CVehicleModelInfo::LoadVehicleColours x2 | 0x5B6B20, 0x5B6CAA | bad carcols.dat names | startup |
| CFileLoader::LoadVehicleObject sscanf | (call) | uninitialised wheel scale on Win11 24H2 | startup |
| CVehicle_AddUpgrade, CVolumetricShadowMgr Render/Update, printf | | records for MTA's crash reports | MTA |
| train crossing barrier x3 | 0x59F7A8, 0x5A0C34, 0x5A0C54 | detaches the barrier so objects 1373/1374 can be made apart | MTA (a feature) |

Other files with fixes of the same kind: `CMultiplayerSA_FixBadAnimId.cpp`
(`GetAnimHierarchyFromSkinClump` 0x734A5D, a bad RpHAnimHierarchy: MTA only
logs it),
`CMultiplayerSA_FixLineOfSightArgs.cpp` (`ProcessLineOfSight` 0x56BA00 and
`GetIsLineOfSightClear` 0x56A490 with positions out of range: port),
`CMultiplayerSA_FixMallocAlign.cpp` (`CMemoryMgr::MallocAlign/FreeAlign`,
0x72F4C0/0x72F4F0: a port, but a large one), and
`CMultiplayerSA_ProjectileCollisionFix.cpp` (load time: startup). The
destructor, RW-resource and clothes hooks feed MTA's own systems.

Each port needs its stock bytes checked against gta_sa.exe, and SilentPatch
and SA-MP checked for the same site, as for the existing hooks.
