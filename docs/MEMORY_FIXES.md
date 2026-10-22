# fixes.lua memory patches

Every entry in the `gamePatches` table in [fixes.lua](../fixes.lua), identified
against [gta-reversed](https://github.com/gta-reversed/gta-reversed) and verified
byte-for-byte by disassembling the installed `gta_sa.exe`.

All addresses are **absolute VAs in gta_sa.exe 1.0 US** (HOODLUM, 14,383,616 bytes).
They are not rebased, so a different exe build makes every one of them wrong.
The samp.dll patches (F1 handler, radar, debug labels, smoking animation) live
in the upper half of `fixes.lua` and are resolved per build; they are not
covered here, except the removed-building table (19), a crash fix that writes
code of its own.

**Status (2026-10-21):** `fixes.lua` 0.9.0. It applies the 18 `gamePatches`
entries below, with the plate pair (13 and 14) optional and off by default
since 0.5.0, the animation-block preload (18), task-safe animation wipes (20,
since 0.7.0), which write code of their own, and since 0.8.0 the
visible-entity list bound (21), which stands down for a limit adjuster since
0.8.1. Since 0.9.0 it adds a growing entity-reference pool (22), which also writes
code of its own, and a DrawText layout cache for d3dx9_25's fonts (23, option
`textcache`, on by default). The samp.dll side resolves
each site by signature first (`lib/signature_scanner`, cache in
`config/signature_cache.lua`, log in `logs/akacross.log`, area sigscan) and
falls back to a per-build table for 0.3.DL-R1, 0.3.7-R1, R3-1 and R5. It moves
SA-MP's removed-building table (19), disables the F1 help dialog and fixes the
smoking animation (`SmokeAnimTime`/`SmokeAnimBlend`). The radar and
debug-label patches are present but switched off. The frame limiter
patches are not here: frame pacing belongs to autofps. The script is not
published and is outside the shared runtime (it prints to moonloader.log
rather than through `akacross.log`); patches stay applied on unload by design.
See [MODS.md](MODS.md).

## Phases

`gta_sa.exe` is mapped before any MoonLoader script runs, so a patch against its
code has nothing to wait for. Descriptors carry a `phase`:

- **`early`** (the default, 16 of 17) - applied at the top of `main()`, before
  the `isSampAvailable()` wait. These are `.text` / `.rdata` writes whose only
  requirement is that the exe is mapped.
- **`late`** (1 of 17, `0xC2B9CC`) - applied after the wait, because it is *data*
  the game writes during `CGame::InitialiseCoreDataAfterRW`. Run early it would
  read uninitialised memory, refuse, and then be overwritten with `0.9f` anyway.

This matters beyond tidiness: sitting behind the SA-MP wait meant the game had
already read the stock bytes at several sites. `GetVideoModeList` and
`FillAvailableVMs` both run during startup, so the aspect-ratio unlock was
landing after the mode list had been built.

### How early is "early", measured

There is a hard floor. `CStreaming::ms_bIsInitialised` (0x9654B8) is set at the
very end of `CStreaming::Init2`, and it is the dividing line for anything that
patches a constant baked into an allocation call - for example the
`ms_rwObjectInstances.Init(12000)` argument at 0x5B8E55 that
`SAMPGraphicRestore.asi` rewrites to 90000. Once the flag is set, those sites
have already been read and patching them does nothing.

`main()` probes that flag on entry. **Measured 2026-08-14: it is already set by
the time MoonLoader starts scripts.** So:

- Patches against *code the game executes repeatedly* (all 17 here) are fine -
  earlier is better, and "as soon as the script loads" is early enough.
- Patches against *constants the game reads once during init* are impossible
  from a MoonLoader script no matter how the script is structured. Those need an
  ASI, which loads at `DllMain` before the game initialises.

The probe stays in the code because it costs nothing and it is exactly the thing
that would have to change for that to stop being true, but it only logs if it
ever finds the window open.

## Gating

Nothing is written blind. Each descriptor carries the bytes it expects to find
(`from`) and the bytes it wants there (`to`), which gives three outcomes per
site:

| outcome | meaning |
|---|---|
| `already` | the site is already in the wanted state, so nothing is written. Either this script ran before, or an ASI that loads earlier made the same change. Named in the log so a site quietly owned by another mod stays visible. |
| `conflict` | the site matches a declared `conflicts` entry, or the descriptor's `standDown` names another mod that handles the same thing (21: a limit adjuster that moved the array). A known third-party patch got here first and does the same job its own way. Logged as "owned by X; standing down", not as a failure. If that mod is ever uninstalled the site reads stock again and the patch applies on the next launch. |
| `ok` | the site matched `from`, was rewritten, and read back correct. |
| `refused` | none of the above, so something patched it a way we do not recognise or the address is wrong for this exe. Left untouched and logged with found-vs-expected. |

Each phase ends with a summary line:

```
[fixes] gta_sa.exe patches (early): 14 applied, 0 already set, 2 owned elsewhere, 0 refused
[fixes] gta_sa.exe patches (late): 1 applied, 0 already set, 0 owned elsewhere, 0 refused
```

If every patch in a phase refuses, the exe is not the build these offsets came
from and the log says so explicitly rather than corrupting the process.

`from` values are the on-disk bytes of gta_sa.exe 1.0 US, except 0xC2B9CC, which
lives past `.data`'s raw size and is written at runtime by
`cHandlingDataMgr::Initialise` - so its `from` is the initialised value, `0.9f`.

### Sites other installed ASIs also touch

Found by scanning every `.asi` / `.dll` in the game folder for the little-endian
encoding of each patch address. ASIs load before MoonLoader scripts, so they
always write first.

| site | mod | what it does |
|---|---|---|
| 0x6FB9A0 | SilentPatchSA.asi | `mov byte [0x6FB9A0], 0x00` **plus** `mov dword [0x6FB97A], 0x10054C88`. Declared as a `conflicts` entry - see section 2. |
| 0x63F576 | SilentPatchSA.asi | `mov byte [0x63F576], 0x75`. Declared as a `conflicts` entry - see section 15. |
| 0x745BC9, 0x7459E1 | ~~SAMPGraphicRestore.asi~~ | It wrote `mov ecx, 0x9090` / `mov word [0x745BC9], cx` / `mov word [0x7459E1], dx` - byte for byte the same patch, so these used to land as `already`. **Removed 2026-08-14** (renamed to `.asi.disabled`), so they now land as `applied`. |
| 0x7459E1 | SilentPatchSA.asi | also writes a word here (`mov eax, 0x7459E1` / `mov [eax], si`). |
| 0x747FFE | ~~SAMPGraphicRestore.asi~~ | 6 NOPs. Adopted into `gamePatches` when the ASI was removed - see section 18. |
| 0x6D6517, 0x6D0E43 | SilentPatchSA.asi | `mov eax, 0x6D6517` / `mov word [eax], cx` and the same for 0x6D0E43, alongside redirecting the `call 0x6A2F30` at 0x6D0E53 into its own code. `cx` is not set in that instruction window, so the resulting bytes are only knowable at runtime - the log's `already in the wanted state:` line names them if they match. |

Nothing else in the folder (`AimCameraShakeFix`, `#FixGameMapBuilding_byAIR`,
`flickr`, `FxLimiter`, `ImprovedStreaming.SA`, `CLEO`, `SAMPFUNCS`, `modloader`,
`samp.dll`) references any of the 17 addresses.

### Removed: SAMPGraphicRestore.asi (2026-08-14)

Renamed to `SAMPGraphicRestore.asi.disabled` in the game folder. It was 5 byte
patches plus 7 code hooks; only some of that transfers.

**Carried over:**

| addr | status |
|---|---|
| 0x745BC9, 0x7459E1 | already in `gamePatches`; now `applied` instead of `already` |
| 0x747FFE | adopted, section 17 |

**Lost, and not recoverable from a MoonLoader script:**

- **The object-instance pool** - *recovered*, see below. It rewrote the
  `ms_rwObjectInstances` link-array size at 0x5B8E55 and 0x5B8EB0 from 0x2EE0 to
  0x15F90 (`CLinkList<CEntity*>` at 0x9654F0). This is the "graphic restore" the
  name refers to: on servers streaming a lot of objects the stock pool runs out
  and entities stop rendering. It is an init-time constant, so per "How early is
  early" above it cannot be patched from here. Re-`Init`ing the list at runtime
  is not a workaround either - it would orphan the streaming links of everything
  already in the list. **Replaced by `objectpool.asi`**
  (`/mnt/data/Projects/objectpool/`), a ~4.5 KB single-purpose ASI that does only
  this, with the same validate-before-write discipline used here. Note the value
  is a **byte** count and must be a multiple of 12, or `CStreaming::Init2`'s
  free-list loop never terminates; see that project's README.
- **7 render hooks** - `Render2dStuff` (0x53E230), `CGame::RenderScene` (0x53DFE2
  and 0x53E01E), `CRenderer::RenderEverythingBarRoads` (0x553CF1),
  `CVisibilityPlugins::RenderBoatAlphaAtomics` (0x733F01), and two in `Idle()`
  (0x53EB0D, 0x53EB7E). Each jumps into ASI-resident code on the game's render
  thread; there is no Lua equivalent.

**Also gone:** it opened with
`VirtualProtect(0x401000, 0x4A3000, PAGE_EXECUTE_READWRITE)` over the game's
entire `.text` and never restored it. That is why `.text` writes here used to
work without unprotecting. `fixes.lua` no longer depends on it (see Protection
below), but any other mod or script that writes to `.text` with `unprotect =
false` was relying on it and may now fault.

The object pool is now handled by `objectpool.asi`, built from
`/mnt/data/Projects/objectpool/`. It logs one line to `objectpool.log` beside
itself per launch, and refuses (with a reason) rather than writing if the exe
does not match.

### Protection

Writes always go through `memory.setuint8(..., true)`, i.e. always unprotect.
`.text` (characteristics 0xE0000020) and `.data` (0xC0000040) are already
writable, but 0x884984 lives in `.rdata` (0x858000, 0x40000040) which has no
`IMAGE_SCN_MEM_WRITE`, and a raw store into a read-only page is a hard access
violation that `pcall` cannot catch.

Method, if you need to re-verify anything:

```bash
objdump -d -M intel --start-address=0x6E1D40 --stop-address=0x6E1D80 gta_sa.exe
```

then map the enclosing function through gta-reversed's `RH_ScopedInstall(Name, 0x...)`
lines or its `// 0x...` function comments.

---

## 1. Tail light damage index

```
0x6E1D4F   03 -> 02
```

**`CVehicle::DoVehicleLights` (0x6E1A60)**, one byte: the `push 3` immediate at
0x6E1D4E becomes `push 2`.

```
6e1d4e:  6a 03                push 3                    ; light id
6e1d50:  8d 8f a0 05 00 00    lea  ecx,[edi+0x5A0]      ; &CVehicle::m_damageManager
6e1d56:  e8 d5 03 fe ff       call 0x6C2130             ; CDamageManager::GetLightStatus
```

`eLights` is `FRONT_LEFT=0, FRONT_RIGHT=1, REAR_RIGHT=2, REAR_LEFT=3`. Scanning
the whole function for the `GetLightStatus` call pattern turns up exactly three
literal light ids plus one register form:

| site | light queried |
|---|---|
| 0x6E1D2A | `push ebp`, and ebp is provably 0 on that path -> FRONT_LEFT |
| 0x6E1CFA | 1 = FRONT_RIGHT |
| 0x6E1D4E | 3 = REAR_LEFT |
| 0x6E1D7B | 3 = REAR_LEFT |

`REAR_RIGHT` is never read. It is a copy-paste bug in the stock game: the two
rear-light checks both consult the rear-left damage state, so shooting out the
right tail light leaves it lit and shooting out the left one kills both. The
patch points the first of the two at `REAR_RIGHT`.

The comment in `fixes.lua` used to read "Skybox crash?"; that was wrong.

Independently confirmed: MTA:SA does the identical write in `CMultiplayerSA.cpp`
(twice), commented *"Fix vehicle back lights both using light state 3 (SA bug)"*,
and `crashes.asi` inherits it as `sampPatches.backLightStateFix`.

---

## 2. Wet road reflections use the far Z plane

```
0x6FB9A0   18 -> 1C
```

**`CCoronas::RenderReflections` (0x6FB630)**, one byte: the displacement of

```
6fb99e:  d9 42 18             fld DWORD PTR [edx+0x18]
```

goes from `+0x18` to `+0x1C`. `edx` is `RwEngineInstance` (`RwGlobals*`, at
0xC97B24). `RwGlobals::dOpenDevice` sits at +0x10, and `RwDevice` is
`{ gammaCorrection, fpSystem, zBufferNear, zBufferFar, fpRenderStateSet, ... }`,
so +0x18 is `zBufferNear` and +0x1C is `zBufferFar`. (The nearby
`call [edx+0x20]` throughout this file is `fpRenderStateSet`, which pins the
layout.)

That float is passed as the `recipZ` argument of
`CSprite::RenderBufferedOneXLUSprite` (0x70E4A0). Feeding the far value instead
of the near one pushes the wet-road corona reflection to the back of the depth
range so it stops z-fighting with / punching through the road it is drawn on.

**SilentPatch owns this site.** It fixes the same thing more completely:

```
mov dword [0x6FB97A], 0x10054C88   ; the disp32 of `mov edx, ds:0xC97B24` at 0x6FB978
mov byte  [0x6FB9A0], 0x00         ; fld [edx+0x18] -> fld [edx]
```

So instead of borrowing `zBufferFar`, it repoints `edx` at a struct of its own
inside SilentPatchSA.asi and reads a value it controls. That is strictly the
better fix, and it wins by load order, so this entry declares `00` as a
`conflicts` state and stands down. It only applies on an install without
SilentPatch.

---

## 3. The player loses its reserved real-time shadow slot

```
0x7069F5   74 -> EB
```

**`CRealTimeShadowManager::GetRealTimeShadow` (0x706970)**, one byte:
`74 05` (`je`) becomes `EB 05` (`jmp`), so the branch is unconditional.

```
706980:  cmp  al,0x3                  ; CEntity::m_nType == ENTITY_TYPE_PED
...
706987:  mov  edx,[ebp+0x598]         ; CPed::m_nPedType
70698f:  sete dl                      ; dl = (pedType == PED_TYPE_PLAYER1)
...
7069f3:  test dl,dl
7069f5:  je   0x7069FC                ; -> scan m_apShadows[1..] for a free slot
7069f7:  mov  esi,[ecx+0x4]           ; -> take m_apShadows[0] unconditionally
```

Stock behaviour: any ped whose `m_nPedType` is `PED_TYPE_PLAYER1` (0) is handed
the dedicated shadow `m_apShadows[0]`; everything else searches the remaining 15
slots for a free one. Under SA-MP more than one ped can end up with that ped
type, and they then all claim slot 0 and stomp each other's shadow. The patch
sends every entity through the free-slot search.

---

## 4 and 5. Shadows ignore the "do not cast shadows on me" flag

```
0x70A83B   0F 85 FE 00 00 00 -> 90 90 90 90 90 90   (CastRealTimeShadowSectorList)
0x70A4CB   0F 85 43 01 00 00 -> 90 90 90 90 90 90   (CastPlayerShadowSectorList)
```

Six NOPs each, over an identical `jne rel32` in two near-identical functions:
**`CShadows::CastRealTimeShadowSectorList` (0x70A7E0)** and
**`CShadows::CastPlayerShadowSectorList` (0x70A470)**.

```
70a4c7:  f6 46 1e 01          test BYTE PTR [esi+0x1E],0x1
70a4cb:  0f 85 43 01 00 00    jne  <skip this entity>
```

`CEntity::m_nFlags` is at +0x1C, so byte +0x1E bit 0 is flag bit 16,
`m_bDontCastShadowsOn`. NOPing the branch makes ped and vehicle shadows land on
entities that normally reject them. The preceding `m_bUsesCollision` test and
the following area-code test are left alone.

---

## 6. Headlight beams leave backface culling off

```
0x6E1425   02 -> 01
```

**`CVehicle::DoHeadLightBeam` (0x6E0E20)**, one byte: the last render-state
restore in the function,

```
6e1424:  6a 02                push 2       ; rwCULLMODECULLBACK
6e1426:  6a 14                push 0x14    ; rwRENDERSTATECULLMODE
6e1428:  ff 50 20             call [eax+0x20]
```

becomes `push 1` = `rwCULLMODECULLNONE`. The beam itself is drawn with culling
disabled; stock code restores `CULLBACK` on the way out, the patch leaves
culling off for whatever renders next.

This one is a deliberate behaviour change rather than a bug fix. If two-sided
geometry starts showing through after headlights are on screen, this is the
patch to revert first.

---

## 7. Disable mip filtering for trilinear textures

```
0x884984   02 00 00 00 -> 00 00 00 00   (the one site in a read-only section)
```

Not code. 0x884950 in `.rdata` is a table of `{ magMinFilter, mipFilter }`
D3DTEXF pairs indexed by `RwTextureFilterMode`, consumed by the D3D9 texture
bind path around 0x7FD5E0:

```
7fd5e5:  mov eax,[edx*8+0x884950]   ; -> RwD3D9SetSamplerState(stage, D3DSAMP_MAGFILTER, ...)
7fd5f7:  mov edx,[ecx*8+0x884950]   ; -> RwD3D9SetSamplerState(stage, D3DSAMP_MINFILTER, ...)
7fd609:  mov ecx,[eax*8+0x884954]   ; -> RwD3D9SetSamplerState(stage, D3DSAMP_MIPFILTER, ...)
```

0x884984 is entry 6 (`rwFILTERLINEARMIPLINEAR`, trilinear), second field, stock
value 2 = `D3DTEXF_LINEAR`. Writing 0 = `D3DTEXF_NONE` turns mipmapping off for
every texture using that filter mode: sharper distant textures, more aliasing.

`crashes.asi` has the same write as its `fixblackroads` toggle
(`MemPut<BYTE>(0x884984, 0)`), which corroborates the identification from the
other direction - distant roads rendering black on some GPUs is the classic
mip-selection artifact. It writes one byte where this writes four; equivalent,
because the upper three of that dword are already zero.

---

## 8. Wheel friction constant

```
0xC2B9CC   66 66 66 3F -> 75 B6 B3 3E   (0.9f -> 0.351001f)
```

Not code. 0xC2B9C8 is `gHandlingDataMgr`, and +0x4 is `fWheelFriction`, set to
`0.9f` by `cHandlingDataMgr::Initialise` (the store is visible at 0x5BFAAE
inside `CGame::InitialiseCoreDataAfterRW`, 0x5BFA90). The patch overwrites it
with `0.351001`.

Only `CVehicle::ProcessWheel` (0x6D6C00) reads it, and only on the coasting path
(no throttle, no brake) where it sets the rolling-resistance brake force:

```cpp
if (IsBike())        brake = fWheelFriction * 0.6f / (mass + 200.0f);
else if (IsSubPlane()) brake = 0.0f;
else                 brake = fWheelFriction / mass;
```

Lowering it to ~39% of stock means vehicles coast noticeably further when you
let off the gas.

Because this is a plain data write and not a code patch, it has to happen after
`cHandlingDataMgr::Initialise` has run. It is the only `phase = "late"`
descriptor, applied after the `isSampAvailable()` wait; every other entry patches
code and goes in immediately.

---

## 9. Remove the follow-car camera timestep clamp

```
0x524FD7   D9 05 24 86 85 00 D8 1D 5C CB B7 00 DF E0 F6 C4 41
           75 08 D9 05 24 86 85 00 EB 06   ->   27x 90
```

**`CCam::Process_FollowCar_SA` (0x5245B0)**, 27 NOPs covering exactly this run:

```
524fd7:  d9 05 24 86 85 00    fld   DWORD PTR ds:0x858624   ; 1.0f
524fdd:  d8 1d 5c cb b7 00    fcomp DWORD PTR ds:0xB7CB5C   ; CTimer::ms_fTimeStep
524fe3:  df e0                fnstsw ax
524fe5:  f6 c4 41             test  ah,0x41
524fe8:  75 08                jne   0x524FF2
524fea:  d9 05 24 86 85 00    fld   DWORD PTR ds:0x858624   ; push 1.0f
524ff0:  eb 06                jmp   0x524FF8
524ff2:  d9 05 5c cb b7 00    fld   DWORD PTR ds:0xB7CB5C   ; push ms_fTimeStep
524ff8:  d9 c9                fxch  st(1)
524ffa:  d8 a6 bc 00 00 00    fsub  DWORD PTR [esi+0xBC]
525000:  d8 f1                fdiv  st,st(1)
```

That is `max(1.0f, CTimer::ms_fTimeStep)` used as the divisor of an angle delta.
Execution now falls through the NOPs into 0x524FF2, so the real `ms_fTimeStep`
is always used. Above 50 fps `ms_fTimeStep` drops below 1.0 and the stock clamp
makes the chase camera lag behind the car; without it the camera tracks at the
same rate regardless of framerate.

The NOP run is byte-exact (6+6+2+3+2+6+2 = 27 = 0x1B) and ends cleanly on the
`fld` at 0x524FF2.

---

## 10. Boat and aircraft explosions expire 3 seconds sooner

```
0x736F88   B8 0B 00 00 -> 00 00 00 00
```

**`CExplosion::AddExplosion` (0x736A50)**, in the
`EXPLOSION_BOAT` / `EXPLOSION_AIRCRAFT` case. The write lands on the disp32 of

```
736f85:  8d 84 01 b8 0b 00 00  lea eax,[ecx+eax*1+0xBB8]
```

which is `CTimer::GetTimeInMS() + lifetime + 3000`, stored to
`CExplosion::m_nExpireTime` (+0x20). Zeroing 0xBB8 makes it
`GetTimeInMS() + lifetime`.

`CExplosion::Update` keeps the explosion alive, and keeps its particle and light
work running, until `m_nExpireTime`. This is the single longest-lived explosion
type in the game (the other cases use 750 or 4250 ms), so this trims 3 seconds
of per-explosion work off every destroyed boat and aircraft.

---

## 11 and 12. Unlock non-standard aspect ratios

```
0x745BC9   7A 6D -> 90 90   (GetVideoModeList)
0x7459E1   7A 71 -> 90 90   (FillAvailableVMs)
```

Two bytes each, over the final rejection branch of an identical aspect-ratio
whitelist in **`GetVideoModeList` (0x745AF0)** and
**`FillAvailableVMs` (0x745920, the startup video-mode dialog)**.

Both compute `height / width` and compare it against four constants:

| address | value | ratio |
|---|---|---|
| 0x858C98 | 0.8 | 5:4 |
| 0x858F34 | 0.75 | 4:3 |
| 0x858CC8 | 0.6 | 5:3 |
| 0x8652DC | 0.625 | 16:10 |

Anything else is dropped. 16:9 (0.5625) is not on the list, which is why stock
SA hides widescreen modes. The last comparison ends in

```
745bc9:  7a 6d                jp <reject this mode>
```

and both patches NOP exactly that `jp`, so every mode the driver reports is
accepted. In the dialog the surviving modes fall into the branch that appends
the `(WIDESCREEN)` suffix to the `"%lu x %lu x %lu %s"` label.

`GetVideoModeList` still enforces the separate 640x480 minimum and the VRAM
budget check at 0x745BF6; neither is touched.

`crashes.asi` carries the same pair in its `display.allResolutions` module,
guarded by the same expected bytes `7A 6D` / `7A 71`, and `SAMPGraphicRestore.asi`
(installed here) writes exactly `90 90` to both - so these normally land as
`already`.

---

## 13 and 14. Custom licence plates on every vehicle type

```
0x6D6517   75 08 -> 90 90   (SetupRender)
0x6D0E43   75 13 -> 90 90   (ResetAfterRender)
```

Two NOPs each over the same guard in **`CVehicle::SetupRender` (0x6D64F0)** and
**`CVehicle::ResetAfterRender` (0x6D0E20)**:

```
6d0e38:  8b 86 90 05 00 00    mov  eax,[esi+0x590]   ; CVehicle::m_nVehicleType
6d0e3e:  83 c4 0c             add  esp,0xC
6d0e41:  85 c0                test eax,eax
6d0e43:  75 13                jne  <skip>            ; != VEHICLE_TYPE_AUTOMOBILE
```

which is the `if (IsAutomobile())` around
`CAutomobile::CustomCarPlate_BeforeRenderingStart` (0x6A2F00) and
`CustomCarPlate_AfterRenderingStop` (0x6A2F30). NOPing both calls the plate
material swap for bikes, trailers, planes, boats and so on.

This is safe despite the `CAutomobile` naming: both helpers only touch
`CVehicleModelInfo::m_pPlateMaterial` and `CVehicle::m_pCustomCarPlate`
(+0x588), which is a `CVehicle` member, not a `CAutomobile` one. The two sites
must be patched together or the plate texture is never restored and the ref
count leaks.

---

## 15. Never drag a live driver out on entry

```
0x63F576   74 -> EB
```

**`CTaskComplexEnterCar` (`TASK_SIMPLE_CAR_GET_IN` case, 0x63F3AF)**, one byte:
`74 3A` (`je`) becomes `EB 3A` (`jmp`).

```
63f56d:  e8 fe 0b fa ff       call 0x5E0170          ; CPed::IsAlive(m_Car->m_pDriver)
63f572:  84 c0                test al,al
63f574:  6a 34                push 0x34              ; sizeof(CTaskComplexLeaveCar)
63f576:  74 3a                je   0x63F5B2
```

The stock code picks between two `CTaskComplexLeaveCar` constructors for the
displaced driver:

```cpp
drvr->IsAlive() && drvr->m_fHealth > 0.f
    ? new CTaskComplexLeaveCar{ car, door, 0, false, true }   // forced get-out
    : new CTaskComplexLeaveCar{ car, door, 0 }                // plain get-out
```

Forcing the jump always selects the three-argument form, i.e. the
`bSensibleLeaveCar` / `bForceGetOut` pair is never set. Live drivers get the
same ordinary leave-car task a dead one would.

**SilentPatch owns this site** too, with `mov byte [0x63F576], 0x75`. That is
`je` -> `jne`, i.e. it **inverts** the branch rather than forcing it, so a live
driver and a dead one swap which constructor they get instead of both taking the
same one. The two are mutually exclusive and SilentPatch wins by load order, so
this entry declares `75` as a `conflicts` state and stands down.

---

## 16. Hospital restart point buffer overflow

```
0x460773   66 89 0D 6C 32 A4 00 -> 90 90 90 90 90 90 90
```

**`CRestart::AddHospitalRestartPoint` (0x460730)**, seven NOPs over the final
store:

```
460730:  66 8b 0d 6c 32 a4 00  mov cx,[0xA4326C]      ; NumberOfHospitalRestarts
...
460769:  66 41                 inc cx
...
460773:  66 89 0d 6c 32 a4 00  mov [0xA4326C],cx      ; write back
```

The function has no bounds check and `MAX_RESTART_POINTS` is 10:

| array | address | size | what follows |
|---|---|---|---|
| `HospitalRestartWhenToUse` | 0xA432C0 | 40 | `HospitalRestartHeadings` |
| `HospitalRestartHeadings` | 0xA432E8 | 40 | (8 bytes slack) `HospitalRestartPoints` |
| `HospitalRestartPoints` | 0xA43318 | 120 | `PoliceRestartPoints` at 0xA43390 |

Call 11 and up writes straight past the end of each array into the next one, and
past `HospitalRestartPoints` into `PoliceRestartPoints`. SA-MP reaches this
through `SetSpawnInfo`, so a server that resets spawn info repeatedly walks the
counter up without limit.

Killing the write-back pins the index at whatever it currently is (0 in
practice), so every call rewrites slot 0 and nothing overflows. The trade-off is
that hospital restart points effectively stop registering, which is a non-issue
under SA-MP because the server owns respawns.

---

## 17. WM_ACTIVATE ForegroundApp writeback (ex-SAMPGraphicRestore)

```
0x747FFE   89 1D 1C 62 8D 00 -> 90 90 90 90 90 90
```

Adopted from `SAMPGraphicRestore.asi` when that ASI was removed. It NOPs
`mov [ForegroundApp], ebx` in `WndProc`'s `WM_ACTIVATE` handler, on the
*activating* branch (`ebx` = 1), so that path stops setting the flag.

**Recorded as-was, not as-understood.** `ForegroundApp` (0x8D621C, initial value
1) has four writers and the ASI touched only this one:

| addr | message | writes |
|---|---|---|
| 0x747FB0 | `WM_ACTIVATE` (deactivating) | `0` |
| **0x747FFE** | `WM_ACTIVATE` (activating) | `1` - this patch |
| 0x748054 | `WM_KILLFOCUS` | `0` |
| 0x748068 | `WM_SETFOCUS` | `1` |

So this is *not* a "keep running in background" patch, whatever its reputation:
`WM_KILLFOCUS` still clears the flag on alt-tab and `WM_SETFOCUS` still restores
it. The plausible reading is the menu re-activation path (`WM_SETFOCUS` sets
`m_bActivateMenuNextFrame`), but that is a guess. Drop the entry if alt-tab
behaviour gets worse rather than better.

Readers, for reference: `IsForegroundApp` (0x746060), `WasForegroundAppLastFrame`
(0x746080), and the background-sleep check in `WinMain` at 0x748A89.

---

## 18. Preload every animation block

`pinAnimBlocks()`. Not a patch, a runtime forced load: request the streaming
model for every anim block, take a reference so the streamer can never evict it,
and block on `loadAllModelsNow()` until they are in.

- 0x4D3FB0 is `CAnimManager::AddAnimBlockRef(int32 index)`. Its whole body is
  `mov eax,[esp+4] / shl eax,5 / add eax,0xB5D4A0 / inc word [eax+0x12]`, which
  is where the gate's constants come from: `ms_aAnimBlocks` at 0xB5D4A0,
  `sizeof(CAnimBlock)` 0x20, `RefCnt` at +0x12 as an `int16`.
- 0xB4EA30 is `CAnimManager::ms_numAnimBlocks`, sanity-bounded against
  `NUM_ANIM_BLOCKS` (180).
- 0x48C36B is `lea eax,[edi+0x63E7]`, so `+2` reads the disp32 `0x63E7` = 25575
  = `RESOURCE_ID_IFP`, the streaming model-id base for IFP blocks. The gate
  checks the `8D 87` opcode first and then trusts the disp32, so the base
  follows anything that rewrote the streaming id layout instead of being
  hardcoded.

Index 0 is skipped because block 0 ("ped") is always resident anyway.

`AddAnimBlockRef` only ever increments, so the gate reads `RefCnt` first and
skips any block that already holds one. Without that, a script reload would
stack a second reference on all 180 blocks, and every reload after that another.

Cost is memory and a longer load; the payoff is that no animation can be
missing at the moment a script or the server plays it, which is the usual cause
of SA-MP peds T-posing or sliding.

---

## 19. SA-MP's removed-building table overflow (samp.dll)

`fixRemovedBuildings()`. Not a byte patch: it allocates a block, writes a new
store into it and redirects SA-MP to it. Addresses below are R5 at its
preferred base 0x10000000. The code is identical on 0.3.DL-R1, R1 and R3-1;
only the addresses differ (`sampFallback` and
`lib/signature_scanner/patterns.lua` have them for all four builds).

**The bug.** `RemoveBuildingForPlayer` lands in SA-MP's RPC handler at
0x1009D3D0. It first calls 0x1009D180, which removes the matching buildings
already in the world. Then it appends {model, x, y, z, radius} to a table so
buildings loaded later are hidden too:

```
1009d3ef:  8b 0d 88 fd 14 10   mov  ecx,[0x1014FD88]        ; count
1009d3f9:  8d 04 89            lea  eax,[ecx+ecx*4]
1009d3fc:  c1 e0 02            shl  eax,2                   ; count * 20
1009d3ff:  89 b0 68 af 14 10   mov  [eax+0x1014AF68],esi    ; model, then x/y/z/radius at +4..+10h
...
1009d426:  41                  inc  ecx
1009d42d:  89 0d 88 fd 14 10   mov  [0x1014FD88],ecx
```

The table is 1000 entries (0x1014AF68 + 1000 * 20 = 0x1014FD88), so the count
sits right after it. There is no bounds check. Entry 1000 writes its model
over the count, which `inc ecx` then rewrites, so the writes stay contiguous.
They run on through SA-MP's globals; entry 1027's Y coordinate lands on the
object pointer at 0x1014FFAC. The per-frame call at 0x100A26F7 passes that
pointer as `this` to 0x10012A50, whose first read
(`mov eax,[ebx]`, 0x10012A80) faults on the coordinate. Nothing ever resets
the count, so a reconnect in the same game run appends the server's removals
a second time.

**The only users of the table.** Every relocation into 0x1014AF68..0x1014FD8B
on all four builds is one of 11:
- the store's seven: count at +2/+40h, table at +12h, +18h, +22h, +2Fh, +3Ah;
- the lookup's three;
- the end compare of the array that precedes the table (`cmp ebp,0x1014AF68`
  at 0x1009CFAD), which does not read it.

The lookup is 0x1009D050 (model, x, y, z) -> 1 when removed. SA-MP's hook on
`CFileLoader::LoadObjectInstance` (0x538090) calls it from 0x100A4AB1 and
swaps a removed instance's model for 19300 (0x4B64). It reads the table base
from `mov esi,0x1014AF68` (operand at 0x1009D068) and the count at 0x1009D050
and 0x1009D09F.

**The fix.**

| where | stock | patched |
|---|---|---|
| store, 0x1009D3EF | `8B 0D 88 FD 14 10` (`mov ecx,[count]`) | `E9 <rel32>`: jmp to the block's code (5 bytes; the rest of the stock store is never reached) |
| lookup operand, 0x1009D068 | `68 AF 14 10` | the new table's address |

The block is one `VirtualAlloc` (PAGE_EXECUTE_READWRITE), never freed:

| offset | what |
|---|---|
| +0x00 | header: magic 0x62527846 ("FxRb"), cap, table address, count address |
| +0x10 | the new store, 117 bytes |
| +0x100 | the table: 5000 entries of 20 bytes |

The jmp enters the new store with the stack the stock store had. The world-
removal call's 5 args are still pushed (the stock code dropped them with
`add esp,14h` too), then come the saved esi, the return address and the
handler's args; esi is the model. It:

1. drops those args;
2. scans the stored entries for one equal to this removal in all five fields,
   and returns if it finds one;
3. returns if the count has reached the cap. The world removal has already
   run, so only buildings that stream in later are affected;
4. appends the entry at `table + count * 20` and increments SA-MP's count;
5. pops esi and returns, as the stock store did.

It uses only eax, ecx and edx. The count stays at 0x1014FD88, so the lookup
reads the same number it always did. The bytes come from
`i686-w64-mingw32-as`; the source is in the comments of
`removedBuildingsStore()`.

**Gating.**
- A jmp at the first byte of the handler (store - 0x1F, the same on all four
  builds) or of the lookup means another mod has replaced that code.
  open.mp's client (`omp-client.dll`) replaces both and keeps its own table.
  The fix stands down without writing anything, logs the owning module
  (`GetModuleHandleExA` on the jmp target), and `/fixes` names it. This
  script's own patch never touches those first bytes.
- The lookup is matched first: around its operand, both `mov eax,[count]`
  reads and the fixed bytes. It names the count and the table it reads now.
- If the store already starts with `E9`, the patch counts as ours only when
  the lookup reads `jmp target - 0x10 + 0x100`, the block's magic matches and
  its count address agrees. Only then is the block read. Otherwise it refuses
  without touching anything.
- Otherwise the whole 70-byte stock store must match, naming the same count
  and table as the lookup, with the count 20000 bytes past the table.
- The block is filled and the existing entries copied. Past 1000, SA-MP has
  already overrun its globals: the first 1000 are kept, the count is set to
  1000, and the log says to restart the game.
- Then the lookup operand is written, then the jmp, and both are read back.

It runs right after `isSampAvailable()`. SA-MP starts connecting the moment it
is up, and a server sends its removals only once it has accepted the
connection.

**Cost.** The lookup is a linear scan per loaded instance, as before; it only
gets longer if a server really sends more than 1000 different removals.
Repeats are not stored, so reconnecting to the same server does not make the
table grow.

**Tests.**
- `lib/fixes/test/fixes_test.lua`: the real script against R5's real store
  and lookup bytes, at 0x10000000 and relocated to 0x02B20000. It covers the
  carried entries, the block's code (equal to the assembler's), a reload,
  another mod's jmp, a changed store, mismatched tables, an already-overflowed
  table and an unknown build.
- `lib/fixes/test/rb_store_test.c`: the store as real x86 code under Wine.
  Given R5's samp.dll, it also runs SA-MP's own handler and lookup with the
  world-removal call NOPed, stock and then patched.

## 20. Wiping a ped's animations leaves its tasks pointing at freed memory

`fixAnimationWipe()`. Like 19, not a byte patch: it allocates a block,
writes a replacement function into it and makes the stock function jump to it.
It runs in the early phase, right after the byte patches.

**The bug.** A task that plays an animation keeps a pointer to the
`CAnimBlendAssociation` and registers a delete callback that clears it.
`CTaskSimpleInAir` (falling) does `m_pAnim->SetDeleteCallback(DeleteAnimCB,
this)`, and `DeleteAnimCB` (0x678E60) sets `m_pAnim = nullptr` when the
animation is its own. The only caller of delete callbacks in SA is
`CAnimBlendAssociation::UpdateBlend`, for an auto-remove animation that has
faded out. The destructor (0x4CECF0) frees the blend nodes, unlinks and drops
the anim block ref. Unlike III's and VC's, it calls nothing.

`RpAnimBlendClumpRemoveAllAssociations` (0x4D6C00) deletes every animation on
a clump through the destructor:

```
4d6c00:  8b 44 24 04          mov  eax,[esp+4]           ; the clump
4d6c04:  8b 0d 78 f8 b5 00    mov  ecx,[0xB5F878]        ; the anim plugin's offset
4d6c0a:  8b 04 01             mov  eax,[ecx+eax]         ; CAnimBlendClumpData
4d6c0d:  8b 00                mov  eax,[eax]             ; first link
...
4d6c14:  8b 30                mov  esi,[eax]             ; next link
4d6c16:  8d 48 fc             lea  ecx,[eax-4]           ; the association
4d6c1d:  8b 11                mov  edx,[ecx]
4d6c1f:  6a 01                push 1
4d6c21:  ff 12                call [edx]                 ; scalar deleting destructor
```

It has two callers. `ClumpAnimDestruct` (0x4D6110) is the plugin destructor
for any clump that is destroyed: a ped going away, or `CPed::SetModelIndex`
(`CEntity::SetModelIndex` deletes and recreates the RW object), which is
what a skin change does. `CPlayerPed::SetInitialState` (0x60CDCB) calls it
too, after `FlushIntelligence` has deleted the tasks. A task still alive
when the wipe runs keeps its pointer. The next write through it lands in
whatever reuses the block.

Seen 2026-10-12 (`sampcmd-crash-20261012-210727.dmp`). Reconnecting from
open.mp's in-game browser deleted the player's car and the player fell. After
the reconnect, `CTaskSimpleInAir::MakeAbortable` with
`ABORT_PRIORITY_IMMEDIATE` did its
`m_BlendDelta = -8.0f; m_Flags |= AUTO_REMOVE; SetFinishCallback(DefaultAnimCB);`.
That landed in a 64-byte heap block that had since become a car model's
material list, as entries 7, 12, 13 and 14: -8.0, 1, 0x4D1390 and 0. Drawing
that car crashed in `RpMatFXMaterialGetEffects` (gta_sa.exe+0x41214A,
reading 0xC100001C). Of all the code that writes -8.0 at +1Ch and then calls
`SetFinishCallback` (0x4CEBE0) with `DefaultAnimCB`, only `MakeAbortable`
writes exactly those four fields. The other three sites are
`CTaskComplexInAirAndLand::CreateNextSubTask` and `CTaskSimpleDuck`'s
destructor and `MakeAbortable`. The wipe in between was most likely the new
server setting the skin. open.mp's `SetPlayerSkin` docs warn about skin
changes while crouching (`CTaskSimpleDuck`) or animating.

**The fix.**

| where | stock | patched |
|---|---|---|
| 0x4D6C00 | `8B 44 24 04 8B` (`mov eax,[esp+4]`) | `E9 <rel32>`: jmp to the block's code (5 bytes; the rest of the stock function is never reached) |

The block is one `VirtualAlloc` (PAGE_EXECUTE_READWRITE) of 0x100 bytes, never
freed:

| offset | what |
|---|---|
| +0x00 | header: magic 0x77417846 ("FxAw"), callbacks run so far, entries, list address |
| +0x10 | the replacement, 94 bytes |
| +0x80 | the list: the 9 delete callbacks below |

The replacement walks the list the way the stock function does: next link
first, then the destructor through the vtable. Before each delete it checks
for a delete callback (`+30h == 2`) whose function (`+34h`) is on the list.
If there is one, it calls it, `cb(assoc, data)` cdecl, as `UpdateBlend` does,
while the animation is still whole, and counts it. Everything else is deleted
exactly as stock. It saves esi, edi and ebx.

The list is every delete callback gta_sa.exe registers: each `SetDeleteCallback`
(0x4CEBC0) call site, except the 47 that pass `DefaultAnimCB` (a bare `ret`)
and one that passes a register. Each is 3 to 27 instructions without a call.
Each only clears its object's pointer or sets a flag:

| callback | owner |
|---|---|
| 0x678E60 | `CTaskSimpleInAir::DeleteAnimCB`: `m_pAnim = 0` if it is this one |
| 0x692550 | `CTaskSimpleDuck`: clears the duck or move animation, by anim id |
| 0x67A380 | `CTaskSimpleClimb::DeleteAnimCB` |
| 0x61A8A0 | `CTaskSimpleAnim::FinishRunAnimCB`: finished, `m_pAnim = 0` |
| 0x61DAE0 | `CTaskSimpleFight`: clears whichever of its two it is |
| 0x621BE0 | registered at 0x627C1E: clears +2Ch if it is this one |
| 0x68B7D0 | `CTaskSimpleArrestPed::FinishAnimArrestPedCB` |
| 0x690C60 | `CTaskSimpleShakeFist::FinishAnimShakeFistCB` |
| 0x5DFD70 | the ped's own: heading rate back from its stats |

Every task that registers one resets it in its destructor
(`SetDeleteCallback`/`SetFinishCallback(DefaultAnimCB)`). So a callback still
registered when the wipe runs names a live task, as it must for
`UpdateBlend` already. Finish callbacks are not run: of the 150 sites, several
callbacks call into vehicle and door code or start other animations. A
callback that is not on the list (a mod's) is not run either.

**Gating.**
- An `E9` at 0x4D6C00 whose target - 0x10 holds the magic is this script's
  block from an earlier run: left as it is.
- Any other `E9` there is another mod's hook. The fix stands down, logs the
  owning module and `/fixes` names it.
- Otherwise the whole 43-byte stock function must match. It contains
  0xB5F878, the plugin offset's address, so it also pins gta_sa.exe 1.0 US.

**Not covered.** Tasks whose animation callback is a finish callback
(`CTaskSimpleRunNamedAnim`, i.e. SA-MP's `ApplyAnimation`) still lose their
animation silently on a skin change. Their pointer is left as stock.

**Tests.**
- `lib/fixes/test/fixes_test.lua`: the real script against the real 43
  stock bytes. It covers the block's code (equal to the assembler's), the
  list, a reload, another mod's jmp and two changed functions.
- `lib/fixes/test/anim_wipe_test.c`: the replacement as real x86 code under
  Wine, on animations laid out like the game's:
  - all deleted once, in order;
  - a listed delete callback runs right before its delete;
  - a mod's callback and a listed finish callback do not run;
  - registers and stack are intact, 1000 times over.

  Given gta_sa.exe it also runs the stock function, which leaves the falling
  task's pointer on the deleted animation. Then it runs the patch with the
  game's own `DeleteAnimCB` bytes, which clear it.

---

## 21. More than 1000 visible entities overwrite the renderer's counts

```
0x553521   A1 44 68 B7 00 89 34 85 98 58 B7 00 40 A3 44 68 B7 00 5E C3 90 90 90 90 90 90 90 90 90 90 90
        -> EB 12 73 0E 90 89 34 85 98 58 B7 00 40 A3 44 68 B7 00 5E C3 A1 44 68 B7 00 66 3D E8 03 EB E3
```

**The bug.** `CRenderer::AddEntityToRenderList` (0x5534B0) ends with an
unchecked store into `ms_aVisibleEntityPtrs` (0xB75898, 1000 slots):

```
553521:  a1 44 68 b7 00          mov  eax,[0xB76844]          ; ms_nNoOfVisibleEntities
553526:  89 34 85 98 58 b7 00    mov  [eax*4+0xB75898],esi
55352d:  40                      inc  eax
55352e:  a3 44 68 b7 00          mov  [0xB76844],eax
553533:  5e c3                   pop  esi / ret
553535:  90 x 11                 padding before SetupScanLists (0x553540)
```

The counts sit right after the array: `ms_nNoOfVisibleSuperLods` (0xB76838),
`ms_nNoOfInVisibleEntities`, `ms_nNoOfVisibleLods` (0xB76840) and the entity
count itself (0xB76844). Entry 1002 writes a building pointer into the LOD
count, and the next pass over `ms_aVisibleLodPtrs` walks off its 1000 slots.

Crash 2026-10-18 15:19 (15 h into a session, aiming and turning on a dense
map): `gta_sa.exe+0x153265` in `RenderOneNonRoad`, reading `[0x39A3D70A+0x36]`.
The entity was a float. ProperShaders' LOD loop (`propershaders.asi+0x20990`)
was at index 2008, because the LOD count held a CBuilding pointer. The dump had
1007 entries, every one a CBuilding (vtable 0x8585C8).

**The fix.** The count is checked in the padding, and a full list is left
alone. The entities past 1000 are dropped for that frame instead of
overwriting the counts:

```
553521:  eb 12                   jmp  553535
553523:  73 0e                   jae  553533                  ; flags from the check
553525:  90                      nop
553526:  89 34 85 98 58 b7 00    mov  [eax*4+0xB75898],esi    ; stock, same address
55352d:  40                      inc  eax
55352e:  a3 44 68 b7 00          mov  [0xB76844],eax
553533:  5e c3                   pop  esi / ret               ; stock
553535:  a1 44 68 b7 00          mov  eax,[0xB76844]
55353a:  66 3d e8 03             cmp  ax,1000
55353e:  eb e3                   jmp  553523
```

**Layout constraints.**
- The store stays at 0x553526. ProperShaders reads the array's address from
  its operand (0x553529), and the LOD array's from 0x5534F5. A first version
  moved the store, and ProperShaders then read `0x0D730000` as the list and
  crashed at `propershaders.asi+0x66134` 11 s into a session.
- `pop esi / ret` stays at 0x553533: `jne 0x553533` at 0x5534DB lands there.
- 0x553521 is still the branch target from 0x5534E2 and 0x5534EB.
- SA-MP's hook at the function's start (`jmp [0x5534A6]` to
  samp.dll+0xA5110) is outside the range.

**With a limit adjuster.** A cap of 1000 would undo an adjuster that raises
this limit, so the descriptor's `standDown` checks two things before writing
anything. If either one shows, the patch counts as owned elsewhere:

- **The store is hooked.** The Open Limit Adjuster's `VisibleEntityPtrs =
  unlimited` replaces the `mov eax,[count]` at 0x553521 with a call into
  itself (`E8 5A 8A E7 77`, to `III.VC.SA.LimitAdjuster.asi+0xBF80`, seen
  2026-10-21). The module is named from the call's target:

  ```
  [fixes] 0x553521 (CRenderer::AddEntityToRenderList visible list bound) is owned by
  III.VC.SA.LimitAdjuster.asi (it hooks the store); standing down
  ```

- **The array has moved.** An adjuster that relocates the array rewrites
  every operand that names 0xB75898, including the store's at 0x553529. It is
  named by `getModuleHandle` on the adjusters' usual file names, otherwise
  logged as "a limit adjuster".

An adjuster that is installed but leaves this limit alone changes neither, so
the cap applies. The check runs when fixes starts. Adjusters patch at load,
before any script runs.

**Not covered.** The LOD store at 0x5534EB (`ms_aVisibleLodPtrs`, also 1000
slots) is still unchecked. It has no padding to grow into; it did not
overflow in the dump.

**Tests.** `lib/fixes/test/fixes_test.lua`: the real descriptor against the
stock bytes. It covers the 31 bytes, the store's operand, the shared ret and
the jne, nothing past 0x55353F and a second run. With a moved array it stands
down: nothing is written, it is counted as owned elsewhere, and a loaded
adjuster is named. The Open Limit Adjuster's real hook bytes stand it down too. An adjuster that leaves the operand stock still gets the
cap, and a different function in the range is refused.

---

## 22. The entity-reference pool runs out at 3000

`fixReferencePool()` and `refPoolTick()`, a port of RefPoolFix.asi. Like 19
and 20, it writes code of its own: a 0x40-byte block holding
`{magic "FxRf", ready, used, made}` and the hook.

```
0x571BB3   A1 68 17 BA 00 -> E8 <block+0x10>      mov eax,[pEmptyList] -> call
```

**The bug.** A `CEntity*` that must be cleared when its entity is deleted is
registered through `CEntity::RegisterReference` (0x571B70). The game links a
`CReference {next, CEntity**}` from `CReferences::aRefs` (0xB9B9A8, 3000 of
them) into the entity's list. When the free list (`pEmptyList`, 0xBA1768) is
empty, it calls `PruneReferences` on every ped, vehicle and object. If that
frees nothing, the reference is not registered. The holder keeps the
pointer, nobody clears it when the entity goes, and the next use of it is a
use-after-free. A server with many objects reaches 3000.

**The fix.** The head load becomes a call into the block:

```
mov  eax,[0xBA1768]       ; pEmptyList
test eax,eax
jnz  done
mov  eax,[ready]          ; a linked reserve, or 0
test eax,eax
jz   done                 ; none: as stock, prune every pool
mov  [0xBA1768],eax
mov  dword [ready],0
inc  dword [used]
done: ret                 ; eax = the head, as the stock mov left it
```

Only EAX changes, as with the stock `mov`; the `test eax,eax / jne` after it
reads it as before. fixes' main loop calls `refPoolTick()` every frame:
- With `ready` empty and fewer than 64 made, it allocates a reserve of 4096
  references (32 KB) and links it.
- It calls `PruneReferences` (thiscall 0x571A90) on one 64th of the
  ped/vehicle/object pools' used slots, so stale references go back to the
  free list before it runs dry.

References handed out from a reserve go back onto the same free list, which
is only ever a linked list (`CReferences::Init`, `RemoveReferencesToPlayer`,
`PruneAllReferencesInWorld` and `ResolveReferences` only follow `next`).

**Gating.**
- A call at 0x571BB3 to code whose block starts with the magic is this
  script's, from an earlier run: reattached as it is.
- Any other call there is another module's: RefPoolFix.asi, named by
  `moduleAt`. The fix stands down, and the frame tick does nothing.
- Otherwise the five stock bytes must match.

`/fixes` shows reserves made and handed over.

**Tests.** `lib/fixes/test/fixes_test.lua`: the block's code equals the
assembler's; the call; the frame tick, where the reserve is linked, only used
slots are pruned, the slices advance, there is no second reserve while one is
ready, a new one is made once it is taken, and none past 64; a reload;
RefPoolFix.asi's call; non-stock bytes.

---

## 23. SA-MP's text is laid out again on every frame

`lib/fixes/text_cache.dll`, TextCacheFix.asi's cache built as a DLL that fixes
loads. The source is `lib/fixes/native/text_cache.cpp` and `build.sh` builds
it with i686-w64-mingw32-g++, static. It is option `textcache`, on by default.
`/fixes textcache off` puts the stock slots back at once.

It hooks five slots of d3dx9_25's `CFont` vtable: RVA 0x1580, for the build
with timestamp 0x423B781A.

| slot | method | hook |
|---|---|---|
| 2 | `Release` | at refcount 0, drops that font's entries |
| 14 / 15 | `DrawTextA` / `DrawTextW` | the cache |
| 16 / 17 | `OnLostDevice` / `OnResetDevice` | drops everything, then stock |

**What it does.** Every chat line, nametag and label goes through
`ID3DXFont::DrawText` every frame. That lays the string out each time and
draws one sprite quad per glyph.

The key is the font, the format, the box's size (only where it clips or
wraps), A or W, sprite or not, and the text. The first time a key is seen it
is drawn as stock. The second time it is measured (`DT_CALCRECT`) and drawn
through a stand-in sprite that records each `Draw`. After that the recorded
glyphs are replayed, moved to the box's new origin.

These are always drawn as stock:
- `DT_MODIFYSTRING`;
- an aligned format (centre, right, vcentre, bottom) that clips;
- a box past 2^21;
- more than 1024 characters;
- a recording that called anything but `Draw` with the call's own colour.

Every 256th hit is drawn for real and compared; three mismatches turn the
cache off.

**Exports** (cdecl; fixes declares them in its "Text layouts" section):
- `TextCache_Install(d3dxBase, &foreign)` returns 1 when hooked (also when it
  already is). It returns 0 for another d3dx9_25 build. It returns 2 when a
  slot points outside the module, i.e. another hook such as TextCacheFix.asi:
  `foreign` gets that address, and fixes names the module with `moduleAt`
  and stands down.
- `TextCache_Uninstall()` puts the stock slots back when all five still hold
  its hooks, and returns 1. With a hook stacked on top it changes nothing and
  returns 0.
- `TextCache_Stats(&s)` gives installed, hits, recorded, entries, mismatches
  and disabled, for the `/fixes` line.

**Loading.** fixes calls `LoadLibraryA` itself and never frees the DLL;
`ffi.load` is only used for the symbols. The hooks therefore do not depend on
the Lua state: a fixes reload finds them in place (`Install` returns 1) and
nothing has to unhook on terminate. With the option off at start,
`stopTextCache()` unhooks a DLL an earlier run left hooked.

**Why not Lua.** A LuaJIT port (ffi callbacks, JIT off) was built and
measured first, against the real d3dx9_25 under Wine. It costs about what the
stock layout does, because each replay is an interpreted callback plus one
ffi call per glyph.

Per frame of 125 DrawText calls (chat with shadows, 40 moving nametags with
shadows, 5 strings that change every frame, 10 own-sprite labels), median of
2000 frames:

| renderer | stock | Lua port | text_cache.dll / TextCacheFix.asi |
|---|---|---|---|
| wined3d, DrawText only | 3.4-5.0 ms | 3.0-3.5 ms | 0.77 ms / 0.77 ms |
| DXVK (Proton, RTX 3070), DrawText | 2.98-3.03 ms | - | 0.22-0.26 ms |
| DXVK, whole frame | 3.07-3.14 ms | - | 0.30-0.34 ms |

Under wined3d the sprite's `End` gets slower once text is drawn faster
(5.2 -> 10.6 ms), which makes its whole-frame numbers mislead. Under DXVK,
which the game uses, `End` is unchanged (about 80 us).

**Tests.** `lib/fixes/test/text_cache_dll_test.sh` builds
`text_cache_dll_test.c` and runs it against the real d3dx9_25.dll in a
throwaway prefix. It renders with Proton's DXVK when present, else wined3d.
It checks:
- a foreign slot: `Install` returns 2 with its address;
- the five slots point into the DLL, and nothing else moved;
- a second `Install` is a no-op;
- 600 frames: replays with zero mismatches against the real draw;
- `Uninstall` restores all 18 slots, and `Install` works again;
- a hook stacked on top is left alone;
- fixes.lua's own cdef, read from the file and called from the game's
  lua51.dll.

`--bench` adds the timing runs, and `TEXTCACHEFIX_ASI=<path>` adds the ASI
for comparison.
