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

**Status (2026-11-15):** `fixes.lua` 0.26.0. It applies the 137 `gamePatches`
entries below, with the plate pair (13 and 14) optional and off by default
since 0.5.0, the animation-block preload (18), task-safe animation wipes (20,
since 0.7.0), which write code of their own, and since 0.8.0 the
visible-entity list bound (21), which stands down for a limit adjuster since
0.8.1. Since 0.9.0 it adds a growing entity-reference pool (22), which also writes
code of its own, and a DrawText layout cache for d3dx9_25's fonts (23, option
`textcache`, on by default). Since 0.10.0 exterior occluders are off (24,
option `occluders`, on by default). Since 0.11.0 it carries MiTurboFix's
fixes: a GameText style bound (25), the texture-dictionary struct size (26)
and checks on four server RPCs (27, the dialog part as option `dialoglines`,
on by default); its hospital restart patch was already 16. Also since
0.11.0, 21 loads of the game timer as a float read a clock the server cannot
set (28, option `floatclock`, on by default). Since 0.12.0 SA-MP's exception
filter swallows every exception instead of crashing on the eleventh, as the
anticrashers by Export and DarkP1xel do, but keeps printing the first ten (29). Since 0.13.0
it carries gtasa_crashfix (crashes.asi): MTA's crash-fix hooks in
`lib/fixes/crashfix.dll` (30, option `crashfix`), MTA's 73 multiplayer patches
(31, option `mtapatches`) and its visual and display switches (32). The samp.dll side resolves
each site by signature first (`lib/signature_scanner`, cache in
`config/signature_cache.lua`, log in `logs/fixes.log`, area sigscan) and
falls back to a per-build table for 0.3.DL-R1, 0.3.7-R1, R3-1 and R5. It moves
SA-MP's removed-building table (19), disables the F1 help dialog and fixes the
smoking animation (`SmokeAnimTime`/`SmokeAnimBlend`). The radar and
debug-label patches are present but switched off. Since 0.13.0 it also
carries autofps's frame-cap patches (33, option `framecaps`); with autofps
running, whichever hooks SA-MP's limiter first owns it. Since 0.14.0 the
crash-fix hooks follow MTA's current code (30) and carry 48 of MTA's newer
ones (34). Since 0.15.0 it carries fogrestore.asi's fix: the game's fog is
sent again twice a frame, after SA-MP leaves it stale (35, option
`fogrestore`, on by default). Since 0.16.0 the RPC checks (27) also drop
server teleports to NaN or off the world. Since 0.17.0 the pause menu can
open again when the window gets focus back (36, option `focusmenu`, off by
default). Since 0.18.0 opcode 01C3 keeps the car it is given (37, option
`keepcars`, on by default) and a position guard puts back NaN, inf and
off-map positions in gta_sa.exe itself (38, option `positionguard`, on by
default). Since 0.18.0 `CMessages::GetStringLength` returns 0 for a null
string instead of crashing (39), and the coasting slow-down, swimming and
buoyancy scale with the frame rate (40, options `wheelfps`, `swimfps`,
`floatfps`, on by default) in place of entry 8. Since 0.19.0 a running Car
Speed FPS Fix script is only asked about (`/fixes cleo`). Since 0.20.0 MTA's
frame-rate fixes are in crashfix.dll (41, options `turnfps`, `pedfps`,
`vehiclefps`, `effectsfps`, `pushfps`, on by default). Since 0.21.0 it
carries San Andreas Online's two crash fixes and its growing vehicle glass
list (42, option `glasslist`, on by default). Since 0.22.0 it carries
FramerateVigilante's frame-rate fixes that MTA's lack and MTA's two ped
matrix checks (43, options `sirenfps` and `pedmatrix`, on by default; the
rest under `vehiclefps` and `swimfps`), the parachute at any fps (44,
option `chutefps`, on by default) and `/fixes drift`
(docs/PERF.md). Since 0.23.0 the sky is drawn dithered, without colour bands
(45, option `skygrad`, on by default), and `deathtrace` (46, off by default)
writes the last 3 s before each death to `logs\death.log`; the Frame Limiter no
longer resets the device to switch vsync (47, option `novsyncreset`, on by
default). `faultlog` (48, on by default) writes every crash-type exception,
with the return addresses on its stack, to `logs\faults.log` as it happens. Since 0.26.0
parked cars skip their collision step when it would change nothing (55, option
`parkedcol`, on by default), and `drawcount` (54, off by default) counts the
device's draw calls by what is drawn. Since 0.27.0 it carries HighFpsFixes'
frame-rate fixes that it lacked and stands its own down where that mod is
installed (56, options `timerfps`, `camerafps` and `weaponfps`, on by
default; the rest under the other frame-rate options). It is published as a
plain repo, https://gitlab.com/akacross/fixes (no updater, release zip per
tag). It is outside the shared runtime except for two optional pieces:
since 0.10.1 its lines go through `akacross.log` under the name `fixes`, to
`logs/fixes.log` of their own (started over at every launch, nothing kept;
levelled by wording; only warnings and errors also reach
moonloader.log; since 0.10.2
the patch summaries and the F1 line are info unless something was refused or
a write did not stick), and
`akacross.perf` times its waits, with the reference-pool tick as the row
`thread:fixes/references`. Without the runtime, as in the public repo, it
writes plain timed lines to `logs/fixes.log`, started over at every launch
(moonloader.log only if the file cannot be opened). Patches stay applied on unload by design.
See [MODS.md](MODS.md).

## Phases

`gta_sa.exe` is mapped before any MoonLoader script runs, so a patch against its
code has nothing to wait for. Descriptors carry a `phase`:

- **`early`** (the default, 54 of 128) - applied at the top of `main()`, before
  the `isSampAvailable()` wait. These are `.text` / `.rdata` writes whose only
  requirement is that the exe is mapped.
- **`late`** (74 of 128: `0xC2B9CC` and MTA's 73, section 31) - applied after the wait, because it is *data*
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

Since 0.11.0 every site gets its own line as it is applied, the way the
samp.dll patches and the code-writing fixes announce themselves. The counts
come once, as the last line, after every fix has run (the per-phase counts
before it landed in the middle of startup and covered only the byte patches):

```
[fixes] 0x69F2D8 (CMessages::AddBigMessage style bound): applied, 32 byte(s) written
[fixes] 0x6E1D4F (CVehicle::DoVehicleLights rear-right tail light): already in the wanted state
[fixes] 0x6FB9A0 (CCoronas::RenderReflections depth) is owned by SilentPatch; standing down
...
[fixes] server RPC checks on: animations (86, 173), menus (76), teleports (12, 13, 159), list dialog lines (61, on)
...
[fixes] started - gta_sa.exe patches: 13 applied, 4 already set, 3 owned elsewhere, 0 refused, 2 off;
        other fixes: 7 in place, 1 left to other mods, 0 off, 0 not in place
```

Applied, already set and owned lines are info; a refused site is a warning,
and so is the last line when a site was refused or a fix is not in place (it
names those). The other fixes are the animation wipes (20), entity
references (22), removed buildings (19), text cache (23), the server RPC
checks (27, which need SAMPFUNCS), the F1 and smoking patches, and the
animation-block preload (18). If every site of a phase refuses, the warning
that this is not the 1.0 US exe is still logged at once.

In game, `/fixes` shows the same summary under the options (with how many
are off by option), and `/fixes patches` lists every site with its state
(`applied`, `already set`, `owned by X`, `refused - why`, `off (option X)`,
`not applied yet` before the late phase), then each of the other fixes with
its state (`in place`, `left to X`, `off (option X)`, `not in place`).

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
| 0x6D6517, 0x6D0E43 | SilentPatchSA.asi | `mov eax, 0x6D6517` / `mov word [eax], cx` and the same for 0x6D0E43, alongside redirecting the `call 0x6A2F30` at 0x6D0E53 into its own code. `cx` is not set in that instruction window, so the resulting bytes are only knowable at runtime - the site's `already in the wanted state` line names them if they match. |

Nothing else in the folder (`AimCameraShakeFix`, `#FixGameMapBuilding_byAIR`,
`flickr`, `FxLimiter`, `ImprovedStreaming.SA`, `CLEO`, `SAMPFUNCS`, `modloader`,
`samp.dll`) references any of the 17 addresses. The sites 25 and 26 patch
(0x69F2D8-0x69F2F7, 0x730FE1, 0x73109B) are referenced by none of them either;
samp.dll, CLEO and MoonLoader name 0x69F2B0 only to call `AddBigMessage`.

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

## 8. Wheel friction constant (gone since 0.18.0)

Until 0.17.0 this wrote `0.351001` over `gHandlingDataMgr.fWheelFriction`
(0xC2B9CC, `0.9f`), which coasts as the stock 0.9 does at about 77 fps and at
no other frame rate. 0.18.0 drops it for `wheelfps` (40), which scales the
game's own value by the timestep where it is read.

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

MiTurboFix makes the same patch once SA-MP is up, so with both installed this
lands as `already`.

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

It hooks five slots of d3dx9_25's `CFont` vtable: RVA 0x1580 in Microsoft's
build (timestamp 0x423B781A). Any other build (Wine's builtin, as a prefix
without Microsoft's DLL has it: Unity RP's) is found through a font the DLL
makes on RwD3D9's device (0xC97C28) with the module's own `D3DXCreateFontA`,
then released; its vtable must lie in the module. Wine's `DrawTextA` calls
`DrawTextW` back through the vtable, so a draw already inside the cache goes
straight to the stock one. A sprite-less draw is cached only in Microsoft's
build, whose font keeps its own sprite at +0xC; Wine's makes a new one per
call, so there it is drawn as it was.

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
Each mismatch is written to `logs\textcache.log`: the text, font, format, the
rectangle and colour now and as recorded, the x87 control word and sprite
transform at both times, and every glyph that moved.

Aligned text (centre, right, vcentre, bottom) is keyed on the parity of the
rectangle's four edges and the sign of `left + right` and `top + bottom` too,
and a layout is replayed only for a rectangle of the class it was recorded
in: the text wider or narrower than the rectangle, and than the edges' sum,
on each axis. Wine's d3dx9_25 (what Unity RP's prefix runs) places aligned
text by halving such differences, truncated toward zero, a centred line at
`(left + right - its width) / 2`: a line sat 1 px off when the rectangle
changed sign, a 3D label recorded off the left edge and replayed on screen
(both captures, 2026-11-12; the cache turned itself off within minutes of a
long session). A centred block that straddles zero on an axis is drawn for
real. Against Wine's build: 32 mismatches in 59,000 replays before, none in
124,000 after. Microsoft's build centres each line inside the block and
never mismatched.

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

---

## 24. The game's occluders hide objects a server maps behind them

```
0x71F960   83 EC 10 -> 32 C0 C3       sub esp,0x10 -> xor al,al / ret
```

Option `occluders`, on by default. It replaces #FixGameMapBuilding_byAIR.asi
(by AIR, blast.hk), which does the same through a hook: its `jmp` at
0x71F960 calls the original, throws the answer away and returns false.

**The bug.** `COcclusion::ProcessBeforeRendering` (0x7201C0) keeps the
map's exterior occluders in two lists. `COccluder::NearCamera` (0x71F960) is
true within 250 units of the camera, past the occluder's half length. Its
answer moves an occluder from the far list to the near list (0x72022E) and
back (0x720347). Only near occluders become active and cull what is behind
them. They describe the stock map, so on a server with custom mapping they
hide the server's objects behind a wall that may not exist any more: new
buildings and objects vanish depending on where the camera is.

**The fix.** `NearCamera` returns false, so no exterior occluder ever joins
the near list and none culls. Both callers only test `al`
(`test al,al / jz|jnz`), and it is a thiscall with no arguments, so
`xor al,al / ret` is the whole function. Interior occluders are processed
separately (0x7203FC) and keep working.

**Cost.** Everything an exterior occluder would have hidden is drawn. On
stock-map areas that is more geometry; `/fixes occluders off` puts the stock
prologue back at once.

**Gating.** A `call`/`jmp` rel32 at 0x71F960 is another mod's hook (AIR's
ASI while installed): the patch counts as owned elsewhere and names the
module. Otherwise the three stock bytes must match.

**Tests.** `lib/fixes/test/fixes_test.lua`: the patch on by default, nothing
past three bytes, `/fixes occluders off` and on again, and AIR's real hook
bytes standing it down.

## 25. A GameText style past 6 writes outside BIGMessages

```
0x69F2D8   8A 4C 24 14 33 D2 83 C4 10 3A CA 8D 44 24 04 74 0F
           8D A4 24 00 00 00 00 8A 48 01 40 3A CA 75 F8
        -> 31 D2 83 C4 10 66 83 BC 24 30 03 00 00 06 77 71  + 16 NOPs
```

From MiTurboFix (public domain, https://github.com/povargek/MiTurboFix),
which hooks `CMessages::AddBigMessage` (0x69F2B0) and drops the call instead.

**The bug.** `AddBigMessage(text, duration, style)` indexes `BIGMessages`
(0xC1A970, 7 entries of 0xC0 bytes) with the 16-bit style and no check:

```
69F2F8:  movzx eax, word [esp+330h]     ; style
69F300:  mov   ecx, [esp+32Ch]          ; duration
69F307:  lea   eax, [eax+eax*2]
69F30A:  shl   eax, 6                   ; * 0xC0
69F30D:  mov   [eax+0xC1A978], ecx      ; ... and eleven more stores
```

SA-MP passes a server's GameText style straight in, so any style past 6
writes the message over the game data after the array, and the client
crashes or misbehaves later.

**The fix.** Between the two helper calls and the style load sits a strlen
loop over the message (0x69F2D8-0x69F2F7) whose result `eax` is overwritten
by the `movzx` before anything reads it. Its 32 bytes now hold:

```
69F2D8:  xor  edx, edx                   ; as stock: edx = 0 is stored below
69F2DA:  add  esp, 10h                   ; as stock: the helpers' args
69F2DD:  cmp  word [esp+330h], 6
69F2E6:  ja   69F359                     ; pop esi / add esp,320h / ret
69F2E8:  16 x nop                        ; into the style load at 0x69F2F8
```

Styles 0-6 run as before; anything larger returns without writing. It
covers every caller, not only SA-MP's GameText.

**Gating.** A `call`/`jmp` rel32 at 0x69F2D8 is another mod's hook and is
named; otherwise the 32 stock bytes must match. A hook at the function's
entry (MiTurboFix's) leaves this site stock, so both apply.

**Tests.** `fixes_test.lua`: the patched bytes against the assembler's, the
`ja` landing on the epilogue, the style load after the site untouched, a
second run, a hook standing it down, a changed site refused.

## 26. A texture dictionary's struct size is trusted

```
0x730FE1   8B 54 24 08 52 -> 6A 04 90 90 90     mov edx,[esp+8] / push edx -> push 4
0x73109B   8B 54 24 08 52 -> 6A 04 90 90 90
```

From MiTurboFix, which hooks `CTxdStore::LoadTxd(int, const char*)`
(0x7320B0), opens the file and refuses it unless the first child of the
dictionary chunk is a 4-byte struct.

**The bug.** `RwTexDictionaryGtaStreamRead` (0x730FC0) finds the struct
chunk and reads it into a 4-byte local (texture count, device id) with the
size from the file:

```
730FD5:  call RwStreamFindChunk(stream, rwID_STRUCT, &size, &version)
730FE1:  mov  edx, [esp+8]               ; size, from the file
730FE5:  push edx
730FE6:  lea  eax, [esp+8]               ; the 4-byte local
730FEC:  call RwStreamRead(stream, &local, size)
730FF1:  mov  ecx, [esp+14h]
730FF8:  cmp  eax, ecx                    ; read == size, else return NULL
```

A size over 4 overwrites the stack frame. `RwTexDictionaryGtaStreamRead1`
(0x731070), the first half of a dictionary streamed in two parts, has the
same read at 0x73109B.

**The fix.** Both reads ask for 4 bytes. The `read == size` check that
follows already exists, so any size but 4 now returns NULL and the
dictionary fails to load, as a damaged file does. Every real TXD's struct is
4 bytes, so nothing that loads today changes. This sits under
`CTxdStore::LoadTxd` (0x731DD0, which 0x7320B0 calls), so it covers the file
MiTurboFix checks, and streamed dictionaries too, without opening anything.

**Gating.** A `call`/`jmp` rel32 at either site is named; otherwise the five
stock bytes must match.

**Tests.** `fixes_test.lua`: both sites, nothing past five bytes.

## 27. Server RPCs whose sizes SA-MP trusts

Checks from MiTurboFix's RPC hooks, in `onReceiveRpc` (SAMPFUNCS raises it).
They run before SA-MP's handlers and are the same on every build. Only these
seven ids are read; the read offset is put back afterwards.

| RPC | id | check | otherwise |
|---|---|---|---|
| ApplyAnimation | 86 | library name 1-15 bytes, animation name 1-24 | dropped |
| ApplyActorAnimation | 173 | the same | dropped |
| InitMenu | 76 | menu id < 128, column flag 0 or 1, every 32-byte line (title, headers, rows) holds its terminator, at most 12 rows per column | dropped |
| ShowDialog | 61 | list styles (2, 4, 5): lines at most 128 characters; tablists (4, 5) per cell | cut, the rest passes |
| SetPlayerPos, SetPlayerPosFindZ | 12, 13 | x, y, z are numbers; \|x\|, \|y\| at most 20000, \|z\| at most 1000000 | dropped |
| SetVehiclePos | 159 | the same, after the vehicle id | dropped |

**Why.** SA-MP hands the animation names to its script-command builder,
which copies them into a fixed buffer. Its menu pool holds 128 menus and
accepts id 128 (`cmp al,80h / ja`, R5). R3-1 and DL also take any row count
and copy menu lines without a bound; R5 checks both. 15 and 24 are the
longest block and animation names an IFP file holds. A teleport to NaN or
far off the map puts the ped or vehicle where the streamer and collision
code fail (InvalidPositionFix.lua's check). Its limits are the world's
absolute ones, not SetWorldBounds': a server may teleport a player outside
those, and stale bounds from the last server would drop valid teleports.

**Changed from MiTurboFix.**

- InitMenu is read in SA-MP's layout: the column flag, the enabled flag and
  the twelve row flags are 4-byte BOOLs (R5's handler reads each with
  `push 20h`). MiTurboFix reads the enabled flags as one byte each, so every
  field after them is read from the wrong place and valid menus can be
  dropped. The column-flag check is ours.
- MiTurboFix cuts every list line to 128 characters. A tablist row is several
  cells, and SA-MP itself copies each cell up to 128 (R5, 0x1006F7C0), so
  here tablists are cut per cell and valid rows keep all their columns. Plain
  lists are cut per line, as MiTurboFix does. R5's own list parser copies up
  to 256 per line, so this can shorten a long valid line: option
  `dialoglines`, on by default, `/fixes dialoglines off` stops it.

**Logging.** Each drop or cut is a warning naming what was wrong; after 5 of
a kind they are only counted. `/fixes` shows the counts once there are any
(`server data: N animation(s), N menu(s) and N teleport(s) dropped, N dialog(s) cut`).

**Tests.** `fixes_test.lua` checks that well-formed RPCs pass byte for byte
untouched with the read offset back at the start: player and actor
animations (with the longest names), menus with one and two columns and up
to 12 rows, list, tablist (rows of 360 characters, no cell over 128),
message-box and input dialogs, and an RPC that is not one of the seven.
Teleports on the map (player, find-Z, vehicle, just inside the limits) pass
untouched; NaN and off-world ones are dropped with the read offset back, and
`/fixes` counts them. Bad animation, menu and dialog data is not tested yet.

## 28. The game timer as a float

```
21 sites   DB 05 84 CB B7 00 ... 7D 06  ->  DB 05 80 CB B7 00 ... EB 06
           fild [m_snTimeInMilliseconds] ... jge  ->  fild [m_snTimeInMillisecondsNonClipped] ... jmp
```

Option `floatclock`, on by default. From a Lua script by an unnamed author
whose notes say: "Fixes the loss of precision where the code converts
`CTimer::m_snTimeInMilliseconds` to a float (`fild dword ptr
[m_snTimeInMilliseconds]`), by redirecting it to the audio clock. The server
can set `m_snTimeInMilliseconds` to anything with the onUpdateGlobalTimer
RPC. Example: several bullets flew into one point." This port keeps the idea
and changes the clock, the sign test and the set of sites (below).
`/fixes patches` shows the 21 as one line.

**The bug.** SA-MP's RPC 60 handler (R5 `0x100A1750`) writes the server's
value into `m_snTimeInMilliseconds`, masked to 30 bits. D3D runs the FPU at
float precision, so every float computed from that timer moves in steps of
its ulp: 2 ms past 2^24 ms (4.7 hours), 128 ms at 2^30. SA-MP's
InterpolateCamera* use `CCamera::VectorMoveLinear`/`VectorTrackLinear`, so
camera moves step at a few frames a second; shakes and FOV lerps likewise;
and the weapon spread, `sin(t * 2pi/1000)` in `CWeapon::FireInstantHit`,
repeats the same offset for every shot inside one step.

**The clock.** `m_snTimeInMillisecondsNonClipped` (0xB7CB80) is advanced by
`CTimer::UpdateVariables` with the same step as the game timer (without the
300 ms cap on a long frame), stops when the game pauses, follows the time
scale, starts at 1 and is written by nothing but CTimer: no reference in
samp.dll, omp-client.dll, SAMPFUNCS, CLEO, MoonLoader or SilentPatch. It
counts from game start, so it reaches 2 ms steps only after 4.7 hours of play.
The script's clock, `CAESmoothFadeThread::m_currentTime` (0xB610E0, the
smooth-fade thread's copy of the audio time), keeps running in a pause,
ignores slow motion and is refreshed by a `Service(); Sleep(1)` loop: 15.6 ms
steps on Windows without a 1 ms timer period.

**The sign test.** MSVC converts a uint32 with `fild`, then `test reg,reg /
jge +6 / fadd [0x858C54]` (2^32), where `reg` is the game timer loaded
separately. The new clock is far below 2^31, so each site's `jge` becomes a
`jmp`; the script left it testing the game timer.

**Which loads.** gta_sa.exe 1.0 US has 47 `fild [0xB7CB84]`; the script lists
46 (it misses 0x516415). A load can move only when its float never meets a
stamp kept on the game's own clock: it is a sin/cos phase, or every writer and
reader of the stamp it makes is in the list. Every access to each stamp was
found by disassembling the whole `.text`; the fields are written nowhere else
(`CCamera::InitialiseScriptableComponents` sets them to -1.0, which is in the
past on either clock).

| site | function | the float | here |
|---|---|---|---|
| 0x50D166 | `CCamera::VectorMoveLinear` | writes move start/end (+0xCBC/+0xCC0) | yes |
| 0x50D1D6 | `CCamera::VectorTrackLinear` | writes track start/end (+0xC5C/+0xC60) | yes |
| 0x50D254 | `CCamera::AddShakeSimple` | writes shake start/end (+0xC94/+0xC98) | yes |
| 0x516415 | shake start (0x516400) | writes shake start/end | yes (not in the script) |
| 0x50D286 | `CCamera::LerpFOV` | writes zoom start/end (+0xCA4/+0xCA8) | yes |
| 0x474876, 0x4748A6 | script "track/move finished" (TheCamera) | reads +0xC60 / +0xCC0 | yes |
| 0x516446 | `CCamera::ProcessVectorTrackLinear` | reads +0xC5C/+0xC60 | yes |
| 0x5164A6 | `CCamera::ProcessVectorMoveLinear` | reads +0xCBC/+0xCC0 | yes |
| 0x516506 | `CCamera::ProcessFOVLerp` | reads +0xCA4/+0xCA8 | yes |
| 0x51A6F6, 0x52BA31 | camera shake, `CCamera::Process` | reads +0xC94/+0xC98 | yes |
| 0x739DB3 | `CShotInfo::AddShot` | writes `m_DestroyTime` (its only writer) | yes |
| 0x739EB6 | `CShotInfo::Update` | reads `m_DestroyTime` (its only reader) | yes |
| 0x67F7DC, 0x67F885 | `CTaskSimpleJetPack::RenderJetPack` | t / 79.6 -> sin, cos (flame) | yes |
| 0x6802F6 | `CTaskSimpleJetPack::ProcessPed` | sin (wobble) | yes |
| 0x703A46, 0x703B92 | `CPostEffects::UnderWaterRipple` | sin (ripple) | yes |
| 0x7400E7, 0x74057F | `CWeapon::FireInstantHit` | sin(t * 2pi/1000) (spread) | yes |
| 0x736116 | `CBulletInfo::Update` | vs `m_nDestroyTime`, which `AddBullet` makes from the integer clock + 1000 | no |
| 0x7376D8 | `CExplosion::Update` | vs `m_nExpireTime`, made from the integer clock | no |
| 0x736EE8, 0x736FA1 | `CExplosion::AddExplosion` | `m_nCreatedTime`, next to that expiry; no reader found | no |
| 0x503BF3, 0x503CAA | `CAEWeaponAudioEntity` | a sound's `ClientVariable`, read back as `(uint32)v + 820 < GetTimeInMS()` | no |
| 0x539FC2 | `CFireManager` | an integer stamp (`int(t + x)`) at fire +0x18 | no |
| 0x6200F6, 0x620130 | `CTaskSimpleThrowControl` | an integer stamp at ped +0x744 | no |
| 0x6A8853, 0x6A8881 | `CAutomobile::SpawnFlyingComponent` | `m_nRemovalTime`, an integer stamp (0x6A88A4 writes it from the integer clock) | no |
| 0x71BC1F | `CGlass::GeneratePanesForWindow` | a pane's integer created time | no |
| 0x41BC3B, 0x431422 | (unreversed) | an integer stamp, `int(t - a * b)` | no |
| 0x6DC18D | `CVehicle::ProcessBoatControl` | a stamp in +0x498, which other vehicles use as `m_f2ndSteerAngle` | no |
| 0x51D78E | `CCam::Process_Fixed` | minus an integer stamp (`fisub`) | no |
| 0x512F47, 0x518885, 0x5192C3 | arrest and wasted cameras | statics 0xB6EC60/0xB70024/0xB70054; 0xB70024 is only read and 0xB70054 only written by address | no: not shown closed |
| 0x50A285, 0x51787F, 0x5179E5, 0x517BF5, 0x517F4E, 0x51D3E5, 0x522CA2 | `CIdleCam` | 12 float time fields, some written in unreversed code | no: not shown closed |

Left out of the port: the script's `alwaysPerfectShooting`, which feeds the
spread a different value.

**Gating.** Each site's bytes from the `fild` through its `jge` must be stock;
the script's variant (operand 0xB610E0, everything else stock) counts as
owned by it, and `/fixes floatclock off` leaves those alone. Nothing runs per
frame.

**Tests.** `lib/fixes/test/fixes_test.lua`: all 21 move with nothing else in
or after their bytes changed; two of the 26 left alone stay stock; a site the
script patched is named and left; `/fixes floatclock off` and on; the group's
line in `/fixes patches`.

## 29. SA-MP's exception filter gives up after ten (samp.dll)

```
R5 0x60A4D (37 bytes)   call print / add esp,14h / mov eax,[count] / cmp eax,10 / jge zero / inc / store / mov eax,1 / ret 0Ch / zero: xor eax,eax / ret 0Ch
                     -> call shown / add esp,14h / inc dword [count] / mov eax,1 / ret 0Ch / shown: cmp dword [count],10 / jae done / jmp print / done: ret
```

SA-MP runs game opcodes (ScriptCommand) and ten other calls of its own
under `__except(filter(code, info, name))`. The filter (R5 0x609B0) prints
"Warning(opcode 0x%X): Exception 0x%X at 0x%X" (or "Warning(%s)...") to the
chat and returns 1, `EXCEPTION_EXECUTE_HANDLER`, while its counter (R5
0x10125A58) is under ten; the eleventh exception gets 0 and the game
crashes. With no chat yet the entry jumps straight to the counter (R5
`je 0x10060A55`, +8 from the call). The add_car_component opcode has a path
of its own that prints and returns 1 every time.

**The anticrashers** (`/mnt/data/Projects/samp/AntiCrasher`) all patch this
filter:

| file | by | build | what it writes |
|---|---|---|---|
| anticrasher037.txt / .cs (CLEO) | Export | 0.3.7 R1 | NOPs the warning call (+0x5CF2C, 5) and `cmp/jge` (+0x5CF39, 5): every exception swallowed, nothing printed |
| anticrasher037.asi | Export | R1 | the same four writes, then two chat lines |
| anticrasher_03DL.txt / .cs (CLEO) | Export | 0.3.DL | the same at +0x604CC / +0x604D9 |
| anticrasher_03DL.asi | Export | DL | **broken**: all four writes go to +0x604CC, so only 4 of the call's 5 bytes are NOPs; the leftover `00` and the next bytes decode as `add [ebx+0x70A114C4], al`, which eats the `add esp,14h`: the first exception crashes |
| AntiCrasher0.3.7-R4.txt / .CS (CLEO) | DarkP1xel | R4 | 26 NOPs from the counter load (+0x60A05) through `xor eax,eax`: the warning still prints and the filter returns whatever the print left in `eax` |

The compiled .cs files match their sources; the rest of each file is the
compiler's table of global variable names.

**The fix.** Every exception is swallowed, as the anticrashers do, but the
first ten still print, as stock, so the chat still says which opcode failed.
The 37-byte tail is rewritten in place: the warning call now calls into the
bytes the dead `xor eax,eax / ret 0Ch` held, which print only while the
counter is under ten (a tail jmp to the print, which returns to the
`add esp,14h`). The counter is incremented on every exception, so it is the
session's total. +8 still starts an instruction (`inc dword [count]`), so the
entry's no-chat jump lands where it did. Nothing is allocated.

**Finding it.** The warning call is the `push eax / call` right after the
push of "Warning(%s): Exception 0x%X at 0x%X", one string and one push in
every build: R1 0x5CF2C, R3-1 0x602CC, R4 0x609FD, R5 0x60A4D, DL 0x604CC.
The call's bytes are not used to find it, so a site another tool patched is
still recognised. Only sections `VirtualQuery` reports committed and readable
are read (the string in data, the push in code): R1/R2's packed sections can
be unmapped by then, and a fault in `ffi.string` is not a Lua error.

**Gating.** The whole stock tail must match, with both counter operands the
same. Ours (a reload) is reported with its count. Export's NOPs and
DarkP1xel's stand down, named; the broken DL ASI's stands down with a
warning to remove it. Anything else is refused.

**Reporting.** Once a second the main loop reads the counter; a rise is a
warning line ("SA-MP has swallowed N exception(s) so far"), and `/fixes`
shows the session's total.

**Tests.** `lib/fixes/test/fixes_test.lua`: the tail against the assembler's
at SA-MP's own base and relocated, nothing outside its 37 bytes, a reload,
the three anticrashers' patches standing it down, a changed tail refused, no
string, an unmapped section left unread, and the count in the log and
`/fixes`. `exception_filter_test.c` runs
SA-MP R5's real filter under Wine (samp.dll loaded without DllMain, off its
preferred base, its print redirected): stock swallows ten and refuses the
eleventh; patched, 30 exceptions are all swallowed, ten printed, 30 counted,
the "%s" branch and the no-chat path go through the tail, add_car_component
is unchanged, and the stack and callee-saved registers come back intact.

## 30. MTA's crash-fix hooks (lib/fixes/crashfix.dll)

From gtasa_crashfix (`crashes.asi` 2.x by Whitetigerswt, GPLv3), whose
hooks are MTA:SA's (multitheftauto/mtasa-blue, GPLv3); fixes is GPLv3 too.
Since 0.14.0 they follow MTA's current code (docs/MTA_CRASHFIX_UPDATES.md),
and MTA's newer hooks are in the same DLL (34).
Option `crashfix`, on by default. The hooks are native code, so they live in
`lib/fixes/crashfix.dll` (source `lib/fixes/native/crashfix.c`, built by
`build.sh` with mingw), loaded once like the text cache and never freed.
fixes installs them one by one (`CrashFix_Install`): each site's stock bytes
must match first, another mod's `jmp` or `call` there (rel32, or through a
pointer as SA-MP's are) is named and left alone, and
every hook logs its own line. While `crashes.asi` is installed it owns all of
these sites, so every one stands down until it is removed.

| key | site | what it stops |
|---|---|---|
| misc1 | 0x5D9A6E | ped shadow render with no shadow data |
| misc2 | 0x6B18B0 | vehicle without colmodel, coldata or suspension lines |
| misc4 | 0x4F02D2 | division by zero in `CAESoundManager::Service` |
| misc5 | 0x5DF949 | ped model info without a model |
| misc6, 11, 13 | 0x4D1750, 0x4D2C62, 0x4D464E | bad animation pointers |
| misc12 | 0x4D41C5 | a bad animation in `UncompressAnimation`: option `oldcrashfixes`, off (MTA no longer installs it) |
| misc7 | 0x417BF8 | missing colmodel |
| misc8 | 0x73485D | missing 2d-effect plugin |
| misc9 | 0x738B64 | invalid projectile |
| misc10 | 0x5334FE | invalid vector pointer (output zeroed) |
| misc14 | 0x4DD4B5 | frontend audio event before its volume table exists |
| freeze15 | 0x156CDAE | the loading thread's unbounded wait (now 4 s + 1 s) |
| misc16 | 0x5E5815 | `CPed::PlayFootSteps` with no first association |
| misc17 | 0x7F120E | a missing frame member: option `oldcrashfixes`, off (MTA no longer installs it) |
| misc24 | 0x7F0DC8 | `_RwFrameForAllChildren` on a missing frame |
| misc18 | 0x4C7DAD | `GetWheelPosn` with no wheel frame (zero position) |
| misc19 | 0x7F0BF7 | `RwFrameCloneHierarchy` bad data: option `frameclone`, off (breaks SkyGFX) |
| misc20 | 0x54F3B0 | `CPlaceable::RemoveMatrix` on null |
| misc21 | 0x648EE0 | `FinishAnimFallOutCB` on data that is not a fall-out task; a vehicle the pool does not hold is dropped |
| misc22 | 0x4CEF08 | `CAnimBlendAssociation::Init` with a bad node array |
| misc23 | 0x6E3D10 | a door index past 15 |
| misc25 | 0x646026 | door-open callback with no vehicle |
| misc26 | 0x739FA0 | `CShotInfo::Update` past its array |
| misc27 | 0x6377FB | die-in-car task with no vehicle (counted) |
| misc28 | 0x44A4FD | garage door with no door object |
| misc29 | 0x4E022C | `IsSoundBankLoaded` with a slot below 0 or past the slot count |
| copyanim | 0x4CE130 | an animation id with no hierarchy (nearest valid one); weapon fire (224) from a group without it, from the Python group (MTA #1109) |
| frame | 0x4C53C0 | a missing vehicle frame (a neighbouring id) |
| fxquality, shadow | 0x49EA50, 0x70BD9B | blob shadows for peds and five vehicles; option `volshadows`. `shadow`'s jump sits in the padding and the entry at 0x70BDA0 hops back to it: ProperShaders replaces the call at 0x70BDA4 on its first frame |
| impact | 0x49F3E8 | sand and dust bullet impacts at most every 500 ms |
| mouse | 0x745423 | no `SetCursorPos` while another window has focus |
| samp | samp.dll, by pattern | SA-MP's association helper on a bad pointer |

**Changed from crashes.asi** (whose versions are live in the game while it
is installed):

- misc3 is gone, as in MTA.
- misc14 and misc16 returned without unwinding their function's exception
  frame and saved registers; they now return the way the function does.
- misc21 is MTA's current test: the callback's data must have
  `CTaskSimpleCarFallOut`'s vtable (0x86EFD0), and a vehicle pointer the
  game's vehicle pool (0xB74494) does not hold, alive, is cleared (the
  callback handles none). crashes.asi checked a registry of live
  associations it never filled.
- misc29 compared a byte with 0xFFFF; it is MTA's range check.
- misc5 reads the model infos through 0x403DA7 (34).
- `frame` calls the original through its own entry, not through the hook.
- the SA-MP check also refuses a bad pointer at +10h.

**Reporting.** Each hook has its own counter in the DLL; the main loop logs
a rise ("crash fix X averted a crash (N so far)"; camnan says the camera was
set up again), at most once a minute per hook (some fire on every frame for
one broken model), and `/fixes` shows the session's total. `/fixes patches` has one line for all of them.

**Tests.** `fixes_test.lua` drives the Lua side against a stand-in for the
DLL: one line per hook (hooked, owned elsewhere, refused), misc19, 12 and 17
only with their options, the SA-MP site found by pattern, the averted counts
and their minute, and the options. `crashfix_dll_test.sh` runs the DLL's
code against gta_sa.exe's own (34).

## 31. MTA's multiplayer patches

73 byte patches, option `mtapatches`, on by default, applied in the late
phase (SA-MP is up), as crashes.asi applies them when samp.dll is loaded:
single-player systems a server does not want (population, wanted level,
roadblocks, gang wars, conversations, replays, idle camera and animations,
stats messages, ambient traffic streaming) and multiplayer fixes (satchels,
drive-by fall-off, the first bullets after a reload, paintjob colours,
trains, garages, DirectInput mouse, model loading priority, bird height,
interior music, clouds). `/fixes patches` shows them as one line.

Every `from` is gta_sa.exe 1.0 US's own bytes. SA-MP R5 writes a few of
these sites itself: where it wrote the same value they land as already set;
its satchel hook (0x738F3A) and its bigger values at 0x550FBA (5000) and
0x54F3A1 (6000 matrices, where MTA's would shrink it to 1800) are SA-MP's
and stand down. 0.3.DL writes its own at samp.dll+0xAA208..0xAA2D9:
210000 at 0x550F82, 50000 at 0x550FBA, 20000 matrices at 0x54F3A1; they
stand down the same way. SA-MP's satchel hook is a `jmp [ptr]` (FF 25), so `hookOwner`
reads indirect jumps and calls too. Other mods' (the first in-game run,
2026-10-25): SilentPatch writes the same non-exclusive mouse at 0x7469A0
(`xor al,al` for MTA's `mov al,0`) and stands down, named. 0x8A5A84 is
`CStreaming::desiredNumVehiclesLoaded`: the exe file holds 12, but the
game's own init sets 22 (0x5B8E6E) and stream.ini's `vehicles` key may set
another (0x5BCD9C), all before MoonLoader starts. So since 0.18.0 its stock is
22 (`fileFrom` keeps the file's 12 for the exe check) and any other value
stands down as set on purpose. Until 0.17.0 the 22 was put down to
ProperShaders.asi, which only maps one of its setting ids to the address; with
it unloaded the site was refused as not stock.

Not ported: the main.scm and loading-screen skips (they run before any
script), 0xBAB318 (crashes.asi's vsync byte, a display change; 0x53E94C,
the other frame-sleep byte, is section 33's GTA frame delay), 0x717180 (it
writes the bytes already there), and 0x6E1D4F (entry 1).

**Tests.** All 73 apply late and not early, SA-MP's values and hook stand
down, `/fixes mtapatches off` restores all 73; and every `gamePatches` entry's
`from` is checked against the real gta_sa.exe file.

## 32. crashes.asi's visual and display switches

Each is an option with crashes.asi's default (they match the user's
crashes.cfg except where noted):

| option | default | site |
|---|---|---|
| noheathaze | on | 0x701780 ret |
| noclouds | on | 0x53E1B4, the cloud render call |
| noflashes | off | 0x7000E0 ret (crashes.asi's default is on; the user's cfg keeps flashes) |
| nosound, novehiclelighting, nospecular, notargetblip, nointeriorreflections, nopostfx | off | 0x507750, 0x5D9A8F, 0x5D9ABE, 0x53E1EC, 0x555854, 0x53E227 |
| windowed | on | 0x74872D, Alt+Enter windowed mode |
| background | on | 0x53E9B3 and 0x561AF6: the game keeps running unfocused and in the pause menu |

Its black-roads fix is entry 7 and its resolution unlock 11 and 12. A call
another mod redirected at one of these sites is that mod's: SilentPatch
redirects the windowed-mode call at 0x74872D, so with it installed
`windowed` stands down and SilentPatch's own windowed mode is the one that
works.

**Not ported:** fast loading and the intro/legal skips, multiple instances
and the main.scm skip (all run before any script: they need an ASI); the
FPS-limit unlock is section 33; SA-MP's connect delay (0.3x/0.3z only); the
brightness override (a numeric setting; off in the user's crashes.cfg); the
F6+Esc mouse fix; and its crash handler, minidumps and watchdog
(diagnostics, not fixes).

## 33. SA-MP's and GTA's frame caps (autofps's patches)

```
samp.dll FrameLimiter +0Ch   A1 <lastRecalc> 85 C0  ->  E9 <stub> 90 90
                     +1Ah   A3 5C CB B7 00         ->  5 NOPs   (ms_fTimeStep store)
                     +182h  52 FF 15 <Sleep>       ->  7 NOPs   (the 100 fps sleep)
samp.dll FrameSleepStub     6A 01                  ->  6A 00    (Sleep(1) -> Sleep(0))
0x53E94A                    83 F8 0E 72 E1         ->  83 F8 00 72 E1   (GTA's 14 ms spin)
```

Option `framecaps`, on by default: autofps's port of crashes.asi's
`disableFPSLock` + `FPSStrafeHook` and its `frameSleepOff2`, carried here as
well, so the frame caps come off without autofps. SA-MP's limiter sleeps to
~100 fps and every 250 ms writes 50/fps over `CTimer::ms_fTimeStep` (wrong
strafing speed when frame times are not capped). The hook at +0Ch goes to a
19-byte stub that reads GTA's Frame Limiter setting (0xBA6794): off goes to
the init arm at +15h (no measuring, no timestep write, no sleep), on to the
stock test at +11h. The stub's `cmp` sets the flags the `jne` at +13h runs
on. With the in-game Frame Limiter on, nothing changes; off, the game is
uncapped (autofps then paces it, if installed).

SA-MP's sites are per build (R1, R3-1, R5, DL, autofps's verified table, not
the signature scanner: its pattern covers bytes this hook overwrites), and
the function's shape is checked first. SA-MP 0.3.DL writes `cmp eax, 2` at
0x53E94A (samp.dll+0xAA583, a 2 ms wait); fixes writes its 0 over that and
puts DL's 2 back when `framecaps` goes off. A hook already at +0Ch is its owner's:
autofps's stub (the same shape, not in a module) or crashes.asi's; fixes then
writes only the default sleep and Sleep(0), the same bytes either way, and
autofps does the same when fixes was first. `/fixes framecaps off` puts the
hook and the timestep store back; the sleeps stay off until a restart.

**Tests.** `fixes_test.lua` on SA-MP R5's real bytes: the jmp, the stub and
its two targets, the NOPs, Sleep(0), a reload changing nothing, the option off
and on, autofps's stub and another hook standing it down (sleeps still set),
a wrong shape refused; the GTA delay's `from` against the real gta_sa.exe,
and written over DL's 2, which goes back off.

## 34. MTA's newer crash fixes

MTA added these after the copy gtasa_crashfix made (docs/MTA_CRASHFIX_UPDATES.md
has the review). They are in `lib/fixes/crashfix.dll` with the others (30),
under option `crashfix`, each installed, logged and standing down on its own.
Every site's bytes were checked against gta_sa.exe and every return address
against MTA's. In game (2026-10-26, SA-MP R5) two stand down: misc34 to
SilentPatch, which writes its own jmp there, and blendanim to samp.dll, which
hooks `CAnimManager::BlendAnimation` with `jmp [0x4D4609]` (its pointer in
the padding before the function). colalloc goes in; SilentPatch only reads
that thunk.

| keys | sites | what they stop |
|---|---|---|
| matrix, matmul | 0x59AD70, 0x7F18B0 | `UpdateRwMatrix` into a null, low or unaligned matrix; `RwMatrixMultiply` with a null one (dst returned) |
| allocmatrix1, 2 | 0x54F5A6, 0x54F5C5 | a matrix handed out with its old attach matrix |
| misc43, 44, 45 | 0x4E028C, 0x4E0347, 0x4E02E8 | `GetSoundBuffer`: a slot out of range, impossible slot data, a sound past its slot's buffer |
| misc47, 48, 49 | 0x4DFE7D, 0x4DFF78, 0x4E0094 | `CAEMP3BankLoader::Service`: stream sizes clamped to the buffer and the slot |
| misc30, misc32 | 0x4CEBE8, 0x4CEA80 | `SetFinishCallback` on null; `SetCurrentTime` on null or with no hierarchy |
| misc33, 35, 36, 37 | 0x7F39F0, 0x7F374A, 0x7F3A09, 0x7F39B3 | texture dictionaries: a bad dictionary or name, an entry that can't be read, a list head that does not link back |
| misc39 | 0x4C7510 | `CVehicleModelInfo::FindTextureCB` copying a long remap name into 32 bytes |
| misc34 | 0x5D9CB2 | the car env-map pipeline with no env-map data |
| misc38, vbv3d, vbmorph, vbweights, dynvb | 0x7C91C0, 0x752AD0, 0x753B60, 0x752320, 0x7F5A36 | vertex buffers that failed to lock or be created (out of video memory) |
| translation | 0x4CFCB5 | `GetCurrentTranslation` past the key frames |
| assocgroups | 0x4D3D52 | anim groups for a model not loaded |
| draggedout, draggedout2 | 0x6485AC, 0x648AAF | a jacked driver's tasks once the vehicle is gone |
| train | 0x6F8F89 | the rail distance wrap that never ends with no track length |
| placename | 0x571F39 | the zone name for a ped in a vehicle with none |
| colliding | 0x56508C | `FindObjectsKindaCollidingSectorList` on a model with no colmodel |
| allatomics, firstassoc, blendanim | 0x749B70, 0x4D6A70, 0x4D4610 | a null clump |
| getalpha, setalpha | 0x732B20, 0x732B00 | clump alpha with no clump (whole functions) |
| fxprim | 0x4A9F50 | `FxPrim_c::Enable` on null (whole function) |
| fire | 0x53A6FC | a fire put out, or attached to nothing, while it burns (MTA #1757) |
| furniture | 0x593BF0 | the interior furniture counter, never reset |
| colalloc | 0x40F740 | shadow data no model has, before `CCollisionData::Copy` |
| misc50, 51, 52 | 0x156650D, 0x156658A, 0x15665C9 | `AreTexturesUsedByRequestedModels` on a model with no model info |
| txdnext, txdconvert | 0x408F6E, 0x40C8CE | a texture dictionary freed while its request is queued |
| contact | 0x6080B2 | a ped's contact entity destroyed under it |
| pathnodes | 0x156F966 | path nodes that failed to allocate |
| los, losclear | 0x56BA00, 0x56A490 | a line of sight from or to outside the world (moved next to the other end): \|x\|, \|y\| over 16000 as MTA's, \|z\| over 1000000 (MTA's -5000 tripped at a server's z -10000; only x and y enter the sector walk) |
| camnan | 0x526FC0 | a camera with a NaN or an infinity in its floats (it feeds it back to itself every frame, nothing is drawn and every line of sight starts nowhere): the bad floats zeroed, `CCam::Init` as on a change of mode with the mode kept, the source 4 behind and 1 above its target (or the player's ped) |
| followtask | 0x523E6B | `CCam::Process_FollowPed_SA` asks the followed ped for its active primary task (0x681720) and reads its type without a null check: spectating a ped with none faulted every frame (SA-MP swallowed it as s007, 512 times on 2026-11-18, and the camera was left half done). With none it goes on past the use-gun check (0x523EA0), as for another task |

**SA-MP moves the model infos.** SA-MP R5 copies
`CModelInfo::ms_modelInfoPtrs` (0xA9B0C8) to its own 65535-entry array and
rewrites the 707 instructions that read it, after checking each one's first
byte (so a hooked site keeps the old array). Every hook that reads the array
reads where it is now, through the displacement at 0x403DA7 (one of those
instructions, which no hook touches), and the five whose own bytes hold the
displacement (misc5, assocgroups, colliding, misc50-52) accept it rewritten.

**Changed from MTA**, where its code is wrong for the game or for SA-MP:

- translation returned with `ret 8` from inside the function's frame (three
  registers and 18h of locals still pushed); it leaves through the
  function's own exit (0x4CFD7B), and checks the index against the
  sequence's frame count rather than 10000.
- colliding skipped to the loop's end with the distance still on the FPU
  stack; it pops it first.
- misc48 left its clamp in eax, which the code after it stores as the slot's
  sound id (-1); misc47 and misc48 keep eax.
- FindTextureCB is the game's lookup with the remap copy bounded; MTA's also
  walks parent dictionaries.
- train wraps by the track length the game's loop uses, with `fmodf`.
- RwMatrixMultiply checks all three matrices and returns dst.
- placename clears bInVehicle in its copy of the flags, not on the ped.
- the vertex-buffer checks are MTA's, without its streaming-memory relief.

Not taken: the CEntity::GetBoundRect and GetAnimHierarchyFromSkinClump hooks
(MTA logs there, nothing more), CVehicle::AddUpgrade, the volumetric shadow
zones and printf (MTA's crash reports), FxSystemBP_c::Load,
LoadVehicleColours and the 24H2 sscanf fix (startup, before any script),
the train crossing barriers (an MTA feature), AreAnimsUsedByRequestedModels
(fixes pins the blocks, 18), VBInstIndicesRemap and CreateAssociations
(defined in MTA, not installed) and MallocAlign.

**Tests.** `crashfix_dll_test.sh` builds `crashfix.c` into a test DLL and runs
it inside a copy of gta_sa.exe whose entry point only loads it and which
imports only KERNEL32, in a throwaway Wine prefix: every hook goes in on the
stock bytes and comes out leaving them, a moved model-info array is
accepted, SA-MP's `jmp [ptr]` and a `call [ptr]` are another mod's hook, and the game's own texture-dictionary, sound-bank, clump-alpha,
FxPrim, matrix, vertex-instancing, translation and fall-out code runs
through the hooks with good data and with the bad data each one is for; the
C parts (#1109, the vehicle pool, collision data, the rail wrap, lines of
sight) directly. 60 checks.

## 35. SA-MP leaves the game's fog stale (fogrestore.asi)

```
0x53DFE2  6A 00 B9 40 42 BB 00   ->  E9 <entry A> 90 90   (CGame::RenderScene, after RenderEverythingBarRoads)
0x53E01E  A0 28 C7 C7 00         ->  E9 <entry B>         (after RenderFadingInEntities)
```

RenderWare keeps each D3D render state it has sent at `0xC991D0 + state*8`
(the value, then a queued flag) and `RwD3D9SetRenderState` (0x7FC2D0) sends
only a change: `cmp [eax*8+0xC991D0], ecx / je`, then the value, then the
state into the queue at 0xC98B40 (count 0xC9A5EC) that the next draw
flushes. SA-MP sets the device's fog itself, so the cache still says fog is
as the game left it and the game's fog never goes back.

Option `fogrestore`, on by default: fogrestore.asi's fix (built in this
workspace from SAMPGraphicRestore's idea), carried here so the ASI can go.
A 121-byte routine keeps every register and the flags and, for
FOGENABLE (0x1C), FOGCOLOR (0x22), FOGTABLEMODE (0x23) and FOGVERTEXMODE
(0x8C), reads the value the game holds, writes it back with bit 0 flipped so
the compare misses, and calls the setter with the real value: the game's own
path, no device pointer and no caps logic (the inactive one of the two fog
modes is a no-op). Each site's entry calls it, replays the five or seven
bytes it took and jumps back. Both sites are checked before either is
written, and a hook that does not stick puts both back.

Another hook at either site keeps both: fogrestore.asi hooks them at
startup, so with it installed fixes names it and stands down. So do
`enbseries.ini`, `enbseries\enbseries.ini`, `enbsuite.ini` or `skygfx.ini` in
the game folder, as fogrestore.asi did (a shader mod there owns the fog;
ProperShaders keeps the game's distance fog). A reload finds the hooks an
earlier run left. `/fixes fogrestore off` puts both sites back.

**Tests.** `fixes_test.lua`: the routine against i686-w64-mingw32-as, the
entries' calls, replayed bytes and jumps, both sites hooked and logged, off
and on (the block reused), a reload, fogrestore.asi's hook at either site, a
SkyGFX config, a site not stock, and the sites and the setter's compare
against the real gta_sa.exe. `crashfix_dll_test.sh` runs the routine (from
`fixes_test.lua --fog-bytes`) through the game's own setter under Wine: each
value kept, each state queued once, the registers back.

## 36. The pause menu on focus (sampSetFocusFix)

```
0x748063  90 90 90 90 90  ->  E8 F8 3B DF FF   (WndProc, WM_SETFOCUS: call 0x53BC60)
```

On WM_SETFOCUS the game sets `ForegroundApp` (0x8D621C) and calls 0x53BC60,
which sets `FrontEndMenuManager.m_bActivateMenuNextFrame` (0xBA677B) unless a
menu is already up or switching: the pause menu opens on the next frame
(gta-reversed `WndProc.cpp`, `case WM_SETFOCUS`). SA-MP NOPs that call at
load while its flag at samp.dll+0x26EB28 is 1, and NOPs 0x7480B3 instead
otherwise (R5, the routine at samp.dll+0xAAF50, beside its other 5-byte
NOPs). Option `focusmenu`, off by default, puts the call back in the late
phase, after SA-MP's write; that is all sampSetFocusFix.lua did.

It is a behaviour switch, not a crash fix: with it on, alt-tabbing back into
the game opens the pause menu, as single-player does. `from` is SA-MP's NOPs
(`fromSamp`), so the exe check compares `to` with gta_sa.exe. A site SA-MP
left stock is already in the wanted state, and off (`revertOnlyOurs`) writes
the NOPs back only where fixes wrote the call, so it never NOPs a call SA-MP
kept.

**Tests.** `fixes_test.lua`: off by default leaves SA-MP's NOPs, on writes
the call, off puts the NOPs back, and a call SA-MP left in place survives on
and off. The real gta_sa.exe holds the call at 0x748063.

## 37. MARK_CAR_AS_NO_LONGER_NEEDED deletes a server's car (01C3)

```
0x47D775  E8 F6 8E 00 00  ->  90 90 90 90 90   (01C3's call to CTheScripts::CleanUpThisVehicle)
```

Opcode 01C3 is case 17 of `CRunningScript::ProcessCommands400To499`
(`movzx ecx, [ecx+0x47DFE0]` / `jmp [ecx*4+0x47DF58]`, entry 0x47D75A). It
collects the car handle, calls `CTheScripts::CleanUpThisVehicle` (0x486670),
which unlocks the car and makes it a random vehicle, then drops it from the
script's mission cleanup. The population code deletes random cars, so a
server's car vanishes after you get out of it, and the upgrade parts SA-MP
keeps for it are left behind. With the call NOPed the car stays the server's;
the opcode still reads its parameter and returns as before (the `push eax`
before the call and the `add esp,4` after it balance). Option `keepcars`, on
by default; a call redirected by another mod is left to it.

"Unload Shenanigans.lua" (a loose MoonLoader script) did this by writing
`90 90 90 90` over the jump table entry, so any 01C3 jumped to 0x90909090.
What runs 01C3 on the car is not known: neither samp.dll nor the scripts in
this install contain the opcode.

**Tests.** `fixes_test.lua`: on by default NOPs the call, off puts it back,
on again, and a redirected call is left alone. The real gta_sa.exe holds the
call at 0x47D775.

## 38. NaN, inf and off-map positions (the position guard)

```
0x544A30  64 A1 00 00 00 00  ->  E9 <entry A> 90   (CPhysical::Add: mov eax, fs:[0])
0x542560  83 EC 2C 55 8B E9  ->  E9 <entry B> 90   (CPhysical::RemoveAndAdd: sub esp,2Ch / push ebp / mov ebp,ecx)
```

**The bug.** A position that is NaN, infinite or far off the map (a server
command that passes `nan` or 2147483647 through to SetPlayerPos, a player's
sync, a script) puts the ped where the streamer and the world's sector maths
fail: `CWorld::GetSectorX` is `floor(x / 50 + 60)` to an int, so NaN becomes
INT_MIN and the sector loops index outside the arrays, the camera follows
NaN and the screen washes white, and the loading screen never ends. There is
no way back once it happens.

**Where.** The check sits in the game, not SA-MP: whatever wrote the
position, samp.dll R1-R5 and DL, open.mp, an RPC or a sync packet, Arizona's
SEF login or a script, every ped, car and object that moves passes one of
these. SA-MP's teleports take the entity out with `CWorld::Remove`, write the
matrix and put it back with `CWorld::Add`, which calls the virtual
`CPhysical::Add`; a moving entity is re-sectored by `CPhysical::RemoveAndAdd`
each frame it moves. `CPlaceable::SetPosn` and the `Teleport` overrides were
not usable: samp.dll calls neither, and `SetPosn` is inlined at most sites.

**The check** (ecx = the entity, every register and the flags kept): the
position is `m_matrix->pos` (+0x30) or, with no matrix, `m_placement` (+4).
|x| and |y| at most 20000, |z| at most 1000000, each by the float's bits
(`and 7FFFFFFFh / cmp / ja`: one unsigned compare also catches NaN and inf).
Each good position the check sees on the player's ped (`[0xB7CD98]`,
`CWorld::Players[0].m_pPed`) or its vehicle (`m_pVehicle`, +0x58C) is kept
in a slot, so a bad one on either puts the last good one back: the teleport
does not happen, as with the RPC check. Anything else takes the RenderWare
frame's (`m_pRwObject` +0x18, its frame +4, the frame's position +0x40, the
last one drawn) if that one is good, else 0, 0, 0. SA-MP updates the frame
in the same teleport, which is why the player, before the slots, landed at
0, 0, 0. The move and turn speeds (+0x44-0x5B) are zeroed. A good position
with a NaN or inf speed has the speeds zeroed. Both are counted; the log
says when either count rises. The block: the two counters, the ped's slot
(+8: entity, x, y, z), the vehicle's (+0x18), then the code at +0x30.

z was limited to -10000..100000 until 0.19.0. Unity Roleplay keeps objects at
z -10000.03 and below (models 3053, 3054 at -2080, 296); each frame one moved
the guard found it bad, its frame bad too, and sent it to 0, 0, 0: about 1100
a second. Height does not enter the sector maths (only x and y do), so z is
now only bounded at 1000000, which still stops a z of 2147483647. The log
line, at most every 10 s while the count rises, gives the rate and the last
bad position: the entity's kind and model, whether it is the player's, and
x, y, z (the block's +28h; the code at +40h).

The server RPC check (27) still drops a NaN or off-world teleport before
SA-MP moves anything, so the player stays where they were; the guard catches
every other way in. Option `rpcpositions` (on by default) is that RPC check:
`/fixes rpcpositions off` lets such a teleport through, to test the guard.

**Tests.** `position_guard_test.sh` runs the routine on fake entities under
Wine (35): good positions and the limits kept, NaN, inf, x 25000, z -20000
and z 2147483647 put back from the frame, a bad frame or none gives 0, 0, 0,
no matrix reads `m_placement`, NaN and inf speeds zeroed (a huge finite one
kept); the player's ped stays where it was when its frame is bad too, takes
the latest good position after a legal teleport, its vehicle likewise,
anything else falls back to the frame or 0, 0, 0; every register and the
flags kept. `fixes_test.lua`: the entries call
the routine, replay their site and jump back; both sites hooked, off puts
them back and on reuses the block, a reload takes an earlier run's hooks and
finds its counters, another hook stands it down, an unknown site is refused;
the two sites are the real gta_sa.exe's bytes.

## 39. A null GXT string (CMessages::GetStringLength)

```
0x69DB50  8B 4C 24 04 8A 11 33 C0 84 D2 74 0D 8D 64 24 00 8A 51 01 40 41 84 D2 75 F7 C3
      ->  8B 4C 24 04 33 C0 85 C9 74 09 80 3C 01 00 74 03 40 EB F7 C3 90 90 90 90 90 90
```

`CMessages::GetStringLength(str)` (cdecl, a strlen for GXT text) reads
`[ecx]` at 0x69DB54 before anything checks it, so a null string (a missing
GXT key, a server's bad text) crashes there. The function is rewritten in
place: `mov ecx,[esp+4] / xor eax,eax / test ecx,ecx / jz ret / cmp byte
[ecx+eax],0 / je ret / inc eax / jmp` - the same length, 0 for null. No hook,
no block. gxt_crash_fix.asi does this with a MinHook trampoline at 0x69DB54
that returns without setting eax, so its caller gets whatever eax held; with
it installed (a jmp at 0x69DB54) this stands down and names it.

**Tests.** `fixes_test.lua`: the stock bytes become the rewrite, a second run
finds it, gxt_crash_fix's hook is left alone and named.

## 40. Frame rate: coasting, swimming, buoyancy

The game applies these once per physics step without the timestep, tuned at
30 fps (a timestep of 1.6667; 50 fps is 1.0). Each site becomes a `call` (one
a `jmp`) to a stub in one shared block (four constants, then the stubs), each
scaling by `timestep / 1.6667`, so 30 fps is stock. A stub starts with a
game-address instruction, which is how a reload knows its own hooks.

```
wheelfps   0x6D6E69 0x6D6EA8 0x6D767F 0x6D76AB 0x6D76CD   fld [fWheelFriction] -> call wheel / nop
swimfps    0x68A4FC (18 bytes)                             -> jmp swim, back to 0x68A50E
           0x68A42B  fmul [0x858EF4] (-0.1)                -> call dive / nop
           0x68A4CA  fadd [0x8708CC] (+0.01)               -> call lift / nop
floatfps   0x6C27AE  fmul [ms_fTimeStep]                   -> call float / nop
```

**wheelfps.** With neither throttle nor brake, `CVehicle::ProcessWheel`
(0x6D6C00) and `ProcessBikeWheel` (0x6D73B0) take
`brake = fWheelFriction / mass` (times 0.6, over mass + 200, for bikes) and
clamp the wheel's forward force to it; `ApplyMoveForce(speed * mass *
direction)` then applies it with no timestep. So the same slow-down comes
every step: at 60 fps twice as much a second as at 30, at 144 nearly five
times, and a car let off the throttle stops dead at high fps and rolls at
low. The stub returns `fWheelFriction * timestep * 0.6`, read at the use, so a
value handling.cfg or another mod sets is scaled too. Junior_Djjr's Car Speed
FPS Fix (CLEO) wrote `0.9 * timestep / 1.6667` over the global every script
frame instead: a frame late, and it replaced any other value. `fWheelFriction` is
`gHandlingDataMgr + 4` (0xC2B9CC); a limit adjuster that moves the handling
manager (Open Limit Adjuster 2.0's `HandlingEntries`) rewrites the five loads
to its copy, so the address is taken from the loads themselves (all five must
agree, or from the stub's head when they are already ours), and the stub, the
sites' bytes and the CLEO stop's 0.9 use it.

**swimfps.** `CTaskSimpleSwim::ProcessSwimmingResistance` (0x68A1D0) blends
the ped's move speed toward a target with `1 - pow(0.9, timestep)`, which is
frame-rate independent, but the target is the swim animation's movement for
this frame, so at high fps each frame's share is smaller and you swim slowly.
The swim stub multiplies the target's x, y and z by `1.6667 / timestep`,
keeping the x87 stack as the game leaves it (`z * blend * k` over the pow).
The vertical branch at 0x68A422 (the target z from the animation) ends in
`fmul [-0.1]`; the dive stub multiplies that by `timestep / 1.6667`, so the
swim stub's k cancels there and that branch stays stock. Swim FPS Fix.asi's
two hooks; it wrote the -0.1 into a float of its own each frame, the stub
computes it at the use. Since 0.20.0 the lift stub does the same for the
underwater state's constant buoyancy (`fadd [0.01]` at 0x68A4CA, which every
dive ends in), as MTA does after its dive fix (#3344): without it a swimmer
under water rose with 1.6667 / timestep times the buoyancy, 4x at 120 fps.
The lift stub sits after the others, and a reload writes the block again, so
an earlier run's block gains it.

**floatfps.** `cBuoyancy::CalcBuoyancyForce` (0x6C2750) is already
`immersion * buoyancy * timestep`. Swim FPS Fix.asi replaces that timestep
with `t * (t / 1.5 + 1)`, t = timestep / 1.6667: stock at 30 fps, less lift
above it (at 60 fps 0.667 instead of 0.833), which keeps a faster swimmer at
the surface. It is empirical and touches everything that floats, boats and
cars in water too, so it is its own option.

**Other copies.** The Car Speed FPS Fix script would make wheelfps scale
twice (it rewrites the global wheelfps scales). At startup and once a second
CTheScripts' active list (0xA8B42C; next +0, base IP +10h, IP +14h; CLEO puts
its scripts there too) is walked for a script whose code starts `8D 0A 01 73
72 5B 00` (0A8D reading the handling manager operand 0x5B7273) and writes
memory (0A8C). Nothing is done to it, or to its file, unasked: wheelfps
stands down and chat asks once.

- `/fixes cleo stop` points its IP at an `93 0A` at the end of the block, so
  CLEO's own 0A93 handler ends and frees it on its next run, puts
  fWheelFriction back to the game's 0.9 and turns wheelfps on. This session
  only: CLEO loads it again next launch.
- `/fixes cleo keep` keeps it and turns wheelfps off (saved).
- `/fixes cleo remove` says what it would do; only `/fixes cleo remove yes`
  within 30 seconds stops it and renames its file in `cleo\` to
  `.cs.disabled`, so CLEO no longer loads it. Nothing is deleted.

If the script stops running on its own while asked about, wheelfps goes back
on. Swim FPS Fix.asi and gxt_crash_fix.asi, when a fix stood down for them,
are named in chat once SA-MP is up; nothing is done to them.

**Tests.** `fps_fixes_test.sh` runs the stubs under Wine with real x87 maths
(16): the wheel brake at 30, 60 and 144 fps and over another value, the dive
and float stubs at 30 and 60, the swim stub's three products, the pow left
under them, nothing else on the stack and esp back, and 30 fps stock.
`fixes_test.lua`: each site calls or jumps to its stub, the options share
one block, off restores only that option's sites, a reload takes its own
hooks, another hook stands the option down; the 8 sites are the real
gta_sa.exe's bytes. A script with Car Speed FPS Fix's head and an 0A8C write
in a fake script list, its file in a cleo folder: found, it and its value are
left alone, wheelfps stands down and chat asks, once. Stop ends it (its IP at
the 0A93, another script untouched), puts 0.9 back and turns wheelfps on, the
file still there. Keep turns wheelfps off, saved, and asks no more. Remove
refuses `yes` before it has explained, and after 30 seconds; confirmed, it
renames the file to .cs.disabled with its bytes intact. Swim FPS Fix.asi is
named once for its two options, an unknown hook not at all.

## 41. MTA's frame-rate fixes (crashfix.dll)

MTA:SA's `CMultiplayerSA_FrameRateFixes.cpp` (GPLv3), compared in
docs/MTA_FRAMERATE_FIXES.md, in `lib/fixes/native/framerate.c`, which
`crashfix.c` includes: one DLL, and every row installs, stands down and logs
like a crash fix ("frame-rate fix KEY ... hooked"), under the option the DLL
names for it (`CrashFix_Option`). Besides jmp hooks the table now has NOP
runs, pointer swaps (an operand repointed at a constant of ours) and call
redirects; each checks the stock bytes first and uninstall writes back what
it found. `/fixes patches` counts them apart from the crash fixes, and the
startup line has "frame-rate fixes (MTA)". Physics is scaled by
`timestep / 1.6667`, stock at 30 fps; effects run on a 30 Hz tick of ours.

| option | rows | what |
|---|---|---|
| `turnfps` | turndamping 0x544D29 | `CPhysical::ApplyAirResistance` multiplies the turn speed by 0.99 every step (the move speed already uses `pow(..., timestep)`): `pow(0.99, timestep / 1.6667)`, so a spin dies as fast at any fps |
| `pedfps` | climb1-2 0x6811E9, 0x68128A; aimwalk 0x61E4F2, aimreset 0x61E5E4; fallstep 0x67FB35, chokestep 0x620520, climbstep 0x681319, fightstep 0x6299A4, stancestep 0x6878F8 (ours) | the climb's pull to the hold reads a constant 1.6667 as its timestep, so it takes the same time at any fps instead of a huge speed (climbing over objects killing the player, MTA #602); moving while aiming compares against 0.1 scaled and keeps its move command (camera jitter); a knocked-down ped's down time (`CTaskSimpleFall::ProcessPed`, 500 ms for the player, 0x8D2EF4) lost each frame's fraction of a ms to `_ftol`, so it lasted longer at high fps (~830 ms at 600) and forever above 1000; it now takes whole ms with the fraction carried, one step a frame (`m_FrameCounter`) for every fall. The same step (`cf_ms_step`) counts down a choke from a spray can, teargas or extinguisher, the climb's get-to-position timer, a fighter's idle counter and the player's fight-stance timer, each called in place of its fld/fmul/fmul/`_ftol` |
| `vehiclefps` | boatcontrol 0x6DC23F, boatwater 0x6D2771; heli1-2 0x6C4F29, 0x6C4F37; chassis1-2 0x6F42D5, 0x6F434E, doorangle 0x6F4422 (every part since 0.22.0, 43); carsleep 0x6B1D34, carstill1-2 0x6B1C2C, 0x6B1C63 (ours), bikesleep 0x6B997C, bikeclamp 0x6B99BD; carsettle 0x6B1AF5, bikesettle 0x6B9850, carfall 0x6B361C, bikefall 0x6BC18F, bikestill 0x6BC15D (ours); gearstep 0x6D05E0, gearinertia 0x6D0880, gearsmooth1-2 0x6D08F0, 0x6D08F8; still1-6; carburn 0x6A7126, heliburn 0x6A723E, rcburn 0x6A726F, bikeburn 0x6BBD04, boatburn 0x6F1E88, alarmstep 0x6D2207, enginedrain 0x6A761F, jumpout1-6 0x6D2110.. (ours) | boats, helicopter takeoff, the swinging chassis, and parked cars settling: the still frames before a car sleeps scale with the frame rate (10 at 30 fps, up to 240), the stillness threshold with them (from the exe's 0.005), no sleep on a car's first frame, and the still damping keeps its fall speed. A burning car, aircraft, bike or boat counts to 5000 ms by `cf_ms_step` (stock it lost each frame's fraction: 5.8 s at 144 to 288 fps, never above 1000), the car alarm likewise; a wrecked engine's 2 health a frame (`ProcessCarOnFireAndExplode`, engine status above 225, which only `FuckCarCompletely` sets) is scaled by timestep / 1.6667; `CanPedJumpOutCar`, run each frame by `CTaskSimpleCarWaitToSlowDown`, slows a stuck car by 0.9 a frame, now `pow(0.9, timestep / 1.6667)` (jumpout1 computes it, jumpout2-6 point the other five operands at it) |
| `effectsfps` | tick 0x561C5D; shake 0x52C723, movingfog 0x716BA6, glass1-3; 19 gates; fuelstep 0x7379F7, fuelfx 0x737A31 (ours) | the camera shake, moving fog and glass shards scaled; wheel dirt and sand, exhaust, boat and rain splashes, rain, swimming effects and bubbles, plane smoke and damage particles, rocket smoke, the water cannon (its reach, its push, its particles), broken objects despawning and the money counter only on a 30 Hz frame. A vehicle explosion's burning fuel (`CExplosion::Update`, 3 fx a frame while its fuel timer is under 200 ms) spawns on a 30 Hz frame, and the timer takes `cf_ms_step` |
| `pushfps` | push1-8, pushalt1-2: the 10 calls to `CPhysical::ApplyCollision` (0x548680) and its other form (0x544D50) | a ped's push on a car with no driver is held to what momentum allows (ped mass x10 against the car's), its tilt and lift kept out |

**The tick.** `CTimer::Update`'s frame counter increment is replayed, then
`cf_tick` sets `cf_frame_due` when 33 ms of game time have passed (a step of
33 ms, resynced after 66) and counts those frames in `cf_frame30`. MTA makes
`CTimer::m_FrameCounter` itself count at 30 Hz instead; ProperShaders reads
that counter (eight references), so fixes leaves it alone and the water
cannon's push reads `cf_frame30`. With the tick not in place (or taken out)
`cf_frame_due` is 1 and every gate passes, as stock.

**Changed from MTA**:

- 0x6F4354 jumps to 0x6F42DB, inside MTA's 14-byte chassis hook, so the
  driverless chassis got NOPs (no impulse) and a value left on the x87 stack;
  fixes hooks the two constants (0x6F42D5, 0x6F434E) and keeps the shared
  tail.
- MTA's 16-byte bike clamp covers 0x6B99CE, which the `!idle` path's `je`
  lands on: in NOPs, so that path no longer clamped and the count could wrap
  at 255. fixes takes 0x6B99BD..0x6B99D5 and does
  `if (!idle || count > limit) count = limit`.
- The helicopter's two rates are hooked at their `fadd`s, not as one 42-byte
  block.
- Each gate keeps the flags and every register (`pushfd`, a memory compare);
  MTA's use a register each.
- The rain gate leaves through the function's own `add esp,84h / ret`
  (0x72AF5E).
- The aiming threshold is stored and compared from memory as the last x87
  instruction; MTA's ended with `fxch / fstp`, which leave C0..C3 undefined.
- The stillness threshold scales the exe's 0.005 (MTA scales 0.0045, not
  stock at 30 fps).
- MTA's third climb site (0x68131B) is the get-to-position timer
  (`m_GetToPosCounter += GetTimeStepInMS()`, abort past 1000 ms, then a push
  back). With the constant it counted 33 ms a frame, so at 300 fps it ran out
  in 0.1 s and the climb aborted with the ped pushed back from the object
  (seen 2026-10-29). It keeps the real timestep; the pull, now as at 30 fps,
  needs real time there. climbstep gives it the carried step.
- `bikestill` (0x6BC15D, ours). With no throttle and every move speed under
  the threshold, `CBike::ProcessControl` zeroes the move speed and the yaw
  each frame. A roll about the wheels needs that move speed, so on a ridden
  bike the upright spring swung it about its centre with nothing damping it:
  a 0.5 s swing that grew for seconds, until the move speed passed the
  threshold (recorded at 300 fps with tests\physrec.lua; ten times the swing
  with the stock threshold). With a driver the row asks whether the bike is
  rolling or pitching (a turn speed over 0.001 at 30 fps, scaled like the
  threshold): at rest those are zeroed too; rolling, nothing is zeroed and
  the wheels damp it. Without a driver the code is stock, so a parked bike
  still tips.
- `gearstep`, `gearinertia`, `gearsmooth1-2` (ours). `cTransmission::
  CalculateDriveAcceleration` holds the engine back by `1 - (ratio - last
  ratio) * inertia`, where ratio is the speed's place in the gear: a change
  since the last frame, which shrinks with the frame. It then smooths that
  with 0.85 a frame. At 850 fps a Savanna reached 20 m/s 12% sooner than at
  30 (tests\physrec.lua runs, 2026-11-13). The change now counts per 30 fps
  frame (times 1.6667 / timestep) and the smoothing is
  pow(0.85, timestep / 1.6667); gearstep works both out at the function's
  start, where the FPU is empty.
- `carstill1-2` (ours). With `carsleep` a parked car needs about 80 still
  frames in a row at 250 fps before fake physics, and GTA zeroes the count
  on any frame that fails its test: the smoothed move speed (+0x74) against
  0.003 (0.006 wrecked) times the timestep, the smoothed turn speed (+0x80)
  against ProcessControl's limit times the timestep. Both are speeds; their
  limits shrank with the timestep, so a parked car's suspension jiggle failed
  most frames at 250 fps and parked cars never slept: in LS 25-28 of 33
  abandoned cars a frame ran full collision (drawcount's parked line,
  2026-11-14); with `carsleep` held out `parkedcol` skipped 12-15. The two
  `CTimer::ms_fTimeStep` operands (0x6B1C2C, 0x6B1C63) point at 1.6667: the
  limits a 30 fps frame uses. The distance test (+0xD4 against [esp+10h]) is
  left as it is.
- Not taken: `CVehicle::ProcessWheel`'s skid test (a `wheelskid` row from
  2026-11-13, removed). It compares the slip with the frame's adhesion, which
  is timestep-scaled while the slip is not, so a fixed slip skids at high fps.
  But the slip is not fixed: each frame the wheels push back all of it while
  they hold, so it grows only by what the turn asks in one frame, which scales
  with the timestep too. Driven in a closed loop (crashfix_dll_test) the
  stock test skids the same share of frames at 30, 144 and 850 fps. Scaling
  the slip down cut it a second time: a Sultan under full gas through a long
  turn skidded 5.75 s at 30 fps and 0.01 s at 511-611 fps, kept grip, turned
  324 deg against 302 and ended 27-30 m from the 30 fps run (physrec,
  2026-11-14). The donut and burnout gaps it was made for came from the
  burnout turn force (the `burnout` row).
- Not taken: `CWeapon::Update`'s shot timing (0x73DC3D), which shortens
  every weapon's firing state at high fps (a fire-rate change on SA-MP), and
  the headless-ped particles (MTA's `setPedHeadless`).

**Tests.** `crashfix_dll_test.sh`: every row goes in on the stock bytes and
comes out leaving them; the tick, the sleep frames and threshold, the turn
damping and the push cap run directly; heli1, boatwater, chassis1, aimwalk
and shake run through their hooks with the game's own constants, their way
back made a `ret`; the wheel-dirt gate returns 0 on a frame not due and runs
the game's code on a due one; the game's own `CTaskSimpleFall::ProcessPed`
ends a 500 ms down time in 32, 500 and never frames at 60, 600 and 2000 fps
stock, and in 30, 300 and 1000 (each within 1) with fallstep, the same step
off two falls in a frame, nothing off for a NaN timestep. The nine call rows
call `cf_step_call`, a burn timer counts 1000 ms in 600 frames at 600 fps; the
game's own `CVehicle::ProcessCarAlarm` ends a 1000 ms alarm in 63, 1000 and
never frames at 60, 600 and 2000 fps stock, 60, 600 and 2000 with alarmstep;
fuelstep, fuelfx, enginedrain and jumpout1 run through their hooks.
`fixes_test.lua`:
a frame-rate row follows its option, not `crashfix`, and is counted apart.

## 42. San Andreas Online: shadow meshes, IK chains, the vehicle glass list

San Andreas Online (Dryxio/san-andreas-online, an MTA fork, GPLv3) adds two
crash fixes and a growing alpha list MTA lacks (docs/SAO_REVIEW.md). All
three are rows of crashfix.dll: they check the stock bytes, stand down for
another hook and log like the others.

| key | site | what |
|---|---|---|
| `stencilveh`, `stencilobj` | 0x70FC87, 0x710471 | `CStencilShadows::RenderForVehicle` / `RenderForObject` transform the col's whole shadow mesh (`CCollisionData+0x24` vertices) into room for 2048 points; past that they write off the end and crash at 0x80D5EF. More than 2048 (unsigned) leaves through the function's epilogue (0x7102D8, 0x710AB2), at the same stack depth: the only call between leaves 8 bytes for its `ret 8`. The vehicle hook stores the pending float first and runs `test eax,eax` last, for the `jle` after. Only reached with volumetric shadows on (`volshadows`); off, GetFxQuality returns 0 to the whole shadow manager |
| `ikchain` | 0x6183B2 | `IKChain_c::Init` indexes the ped's frames with `RpHAnimIDGetIndex`, -1 for a bone the skin lacks, and crashes at 0x6183CA (head tracking, gun pointing). SA:O checks at the entry; fixes checks where the game looks the effector up, after its own early returns, so only a chain the game would have built is counted: every bone from the effector to the pivot through `BoneNodeManager_c::ms_boneInfos` (0x8D26D0) must be in the hierarchy, or the function returns false (0x6184A3) and `IKChainManager_c::AddIKChain` puts the chain back |
| `glass1`-`glass6` (option `glasslist`) | 0x733D35, 0x734092, 0x734141, 0x734211, 0x734356, 0x734482 | the six calls to `CLinkList::InsertSorted` for `CVisibilityPlugins::m_alphaList` (0xC88070), which `CRenderer::RenderOneNonRoad` clears and draws per vehicle: 20 links, and past them the game draws a part at once, unsorted. A full list gets as many links again (at least 20) from a pool of 980 in the DLL, never freed; the growth counts on the row of the site that asked (its return address). Once the pool is gone the game's fallback stands |

**Changed from SA:O**: the IK check sits after the game's own checks (above);
the glass links come from a static pool instead of the heap, so there is no
Initialise or Shutdown hook; the world-entity alpha list (0x733DF5,
0x7345F2) is left to the limit adjuster's `AlphaEntityList`, which hooks
those two calls.

**Not taken**: the DFT-30's left middle wheel (SilentPatch fixes it), SA:O's
frame-rate file (an older subset of MTA's, 41) and its features.

**Tests.** `crashfix_dll_test.sh`: each row in and out on the stock bytes;
the vehicle shadow hook runs at 0, 100 and 5000 vertices and the object
one at 2048, 2049 and a count read as negative, the float and flags checked; `cf_ik_bones_valid` against a
fake hierarchy with the right arm (25 to 21) whole, a bone missing, no pivot,
a bone the table lacks and no hierarchy, and the hook through both ways out
with the stack as it was; the glass list filled through the game's own
`InsertAtomicIntoSortedList`: 20 on the game's links, 20 more on the 21st,
40 more on the 41st, still sorted, another list left alone, the pool's end.
`fixes_test.lua`: a glass row follows `glasslist`, is counted on its own
line and its growth is not logged or counted as a crash.

## 43. FramerateVigilante's frame-rate fixes, MTA's ped matrix checks

FramerateVigilante (Junior_Djjr, MIT) fixes high-fps bugs MTA's file (41)
does not; `lib/fixes/native/vigilante.c` has the ones SA-MP reaches. MTA's
two checks on a ped's bone matrices sit outside its crash-fix file, in
`CMultiplayerSA.cpp` and `CMultiplayerSA_Rendering.cpp`;
`lib/fixes/native/pedmatrix.c` has them. Both are crashfix.dll rows: stock
bytes checked, another hook stood down for, a line each.

| option | key | site | what |
|---|---|---|---|
| `vehiclefps` | `doorangle` | 0x6F4422 | `CDoor::Process` adds a part's angular speed to its angle once a frame: every door, bonnet, boot and ladder swung faster at high fps. 41's row scaled the chassis alone; it now scales every part by `timestep / 1.6667` |
| `vehiclefps` | `doordamp1`-`4` | 0x6F4391, 0x6F43A1, 0x6F43CA, 0x6F43D2 | the same function's damping once a frame: 0.98 a boat's part, 0.92 the fire truck's ladder, 0.97 a bonnet and any other part, each `pow(rate, timestep / 1.6667)` (a part settled too fast at high fps) |
| `vehiclefps` | `burnout` | 0x6A5073 | `CAutomobile::ProcessCarWheelPair` turns a car in a burnout every frame with `ApplyTurnForce(contact point, min(1, 3000 / turn mass) * -0.002 * turn mass * steer * right)`, no timestep. The contact point is passed as the force, so even with no steering it twists the car about its centre of mass: at 830 fps a straight burnout pivoted 115 deg against 15 at 30, crept past the burnout's 0.01 limit and dropped in and out (physrec, 2026-11-13). The call's force is scaled, which scales the whole turn (FramerateVigilante, and this row until then, scaled only the steering part) |
| `swimfps` | `swimpitch1`-`3` | 0x68A6BD, 0x68A735, 0x68A7C0 | `ProcessSwimmingResistance` damps the dive's pitch rate (`m_fStateChanger`) by 0.95 a frame while adding to it per second: `pow(0.95, timestep / 1.6667)` |
| `swimfps` | `swimsurface` | 0x68A7E8 | its pull to the water level is capped at `timestep * 0.1` a step, a smaller speed at high fps; the operand reads 1.6667, so the cap is 0.1667 at any fps |
| `sirenfps` | `siren` | 0x6E0961 | `CVehicle::ProcessSirenAndHorn` calls a horn press a tap, which switches the siren, when it lasts at most 3 of pad 0's last 5 frames: 100 ms at 30 fps, 12 ms at 240, so at high fps a tap only honks. A press under 150 ms is a tap and one held 150 ms is the horn, timed per vehicle in a table of 16: SA-MP runs the function for other players' cars with their pads. The site is HOODLUM's `nop / nop / jmp` stub; the hook leaves by the game's three ways out |
| `pedmatrix` | `pedanim` | 0x7C5A5C | `RpHAnimHierarchyUpdateMatrices`, before it puts a bone's keyframe matrix (at `[esp+54h]`) under its parent: a rotation element of 2 or more, an offset past 10, NaN or inf zeroes it, so the bone and its children shrink to nothing instead of stretching across the screen (MTA's `CheckAnimMatrix`) |
| `pedmatrix` | `pedskin` | 0x7335B0 | `CVisibilityPlugins::RenderPedCB`: a ped atomic whose skin matrices (its RpHAnimHierarchy, at the skin plugin's offset 0xC978A4) hold NaN, inf or anything past 100000 is not drawn and returns its atomic as a drawn one does. Unity Roleplay's z -10000 is well inside |

`pedanim` and `pedskin` count what they catch; their lines are info ("ped
fix pedskin ... caught a ped's broken bones"), not crashes, and `/fixes
patches` and the startup summary show them as "ped fixes". The frame-rate
rows count nothing.

**Changed from FramerateVigilante**: each damping is `pow`, not its linear
`1 - (1 - rate) * timestep * 0.6`; the burnout is scaled after the game's
`min`, where FramerateVigilante scales the 3000 inside it (a car under 3000
turn mass, most of them, then scaled by less); its third pitch site,
0x68A7BD, is the `fld` before the `fmul`, and its code goes on inside the
`fmul`; the siren's press time is kept per vehicle, not once.

**Changed from MTA**: a zeroed bone sits at its parent's origin (MTA's puts
it 1 above); the skin check tests every element (MTA's tests y where three
of z's upper bounds should be).

**Not taken**: the rifle's aim-walk step (0x61E0CA): samp.dll points
`ControlGunMove`'s timestep operand (0x61E0C2) at its own value, and
`pedfps`' `aimwalk` handles the threshold. The on-rails wheel spin
(0x6B523F..0x6B5269): only `STATUS_SIMPLE` cars, which SA-MP has none of.
The fps limit, refresh rate and the automatic limits for cutscenes,
missions, minigames, schools and the pause menu: single-player, and frame
caps are 33's.

**Tests.** `crashfix_dll_test.sh`: each row in and out on the stock bytes;
`cf_damping` at 30 and 60 fps and with no timestep; `doorangle`, the four
dampings, `burnout` and `swimpitch1` through their hooks with the game's
constants, one x87 value left; `swimsurface`'s operand; `cf_siren_horn`
through a tap, a 150 ms hold and two cars pressed apart, and the hook to
each of the game's ways out; `cf_ped_anim` with a good bone, a rotation of
2.5, an offset of 10.5 and NaN, and through its hook (the matrix at
`[esp+54h]`, eax kept); `cf_ped_skin_ok` with z -10000, 200000, NaN and no
hierarchy, and through its hook both ways. `fixes_test.lua`: a ped row
follows `pedmatrix` and is counted and logged on its own; `swimfps` switches
its DLL rows with its fixes.lua sites.

## 44. The parachute spins at high fps

The parachute is not engine code: `player_parachute.scm` in
`data\script\script.img` (`script_name 'PLCHUTE'`) runs once a frame while
the player has one, and steers with values tuned at 30 fps. Falling, the
heading turns by `roll / 5` a frame, and roll and pitch move 1/20 of the way
to the stick a frame; open, the heading turns by `roll / 15` and the sink rate
moves 1/20 of the way to its target; the falling speed moves 1% of the way. At
144 fps the turn is almost five times the 30 fps one and the rest settle
faster: the player spins and speeds off. The canopy's opening runs on the
script's millisecond timer and is fine.

While PLCHUTE runs (found in `CTheScripts::pActiveScripts`, 0xA8B42C, by its
name at +8; its code at +10h), fixes rewrites its 11 such constants every
frame from the timestep, k = timestep / 1.6667 (at most 4):

| instruction | how many | stock | as |
|---|---|---|---|
| `21@ /= 5.0`, `21@ /= 15.0` (the turn) | 1, 1 | 5, 15 | divided by k |
| `x /= 20.0` (roll, pitch, sink rate) | 7 | 1/20 a frame | 1 - (1 - 1/20)^k |
| `21@ *= 0.01` (falling speed) | 2 | 0.01 a frame | 1 - 0.99^k |

so a second of play does what a second at 30 fps did. The game loads a
build of the script 216 bytes longer than the PC `script.img` holds (ten
`09C6` instructions with four arguments), so the sites are found in the
loaded code by their whole instructions (`op 00 03 <local> 00 06 <float>`)
once per load, after `script_name 'PLCHUTE'` at its start, and only if all
11 are there (986 / 1003 for the first in the two builds). Off, the stock
values go back. The log says "chutefps: the parachute steers as at 30 fps"
the first time it writes; holding the parachute with nothing to fix logs why
once, with the running scripts.

**Tests.** `fixes_test.lua`: a running PLCHUTE laid out as the loaded build:
30 fps leaves the stock values, 150 fps a fifth of the turn and the blends
as above, the stick's `/= 4.267` untouched, off puts them back, a build with
one site fewer is left alone. The scan was also run over the code saved from
the game and over the PC file: 7, 1, 1 and 2 in both.

## 45. The sky's colour bands (skygrad)

`CClouds::RenderSkyPolys` (0x714650) draws the sky as six quads in the
cloud temp buffer (top colour to bottom colour, bottom colour to the below
horizon grey) and ends with a jump to `CBrightLights::RenderOutGeometryBuffer`
(0x7154A8 `E9 A3 CC 00 00`). The colours blend smoothly between vertices,
but the backbuffer holds 8 bits a channel: a sky whose top and bottom colours
differ by a few dozen steps shows each step as a line across the screen.

skygrad.asi (FraGae) redraws the sky with a vertex and pixel shader that
round each pixel up or down against a grain texture. `sky.c` (option
`skygrad`, on by default) does the same with its own code: the row
`skydither` takes the jump, and `cf_sky_flush` draws the buffer the game
built (same quads, same colours) with

- a `vs_2_0` that transforms by `_RwD3D9D3D9ViewTransform` (0xC9BC80) times
  `_RwD3D9D3D9ProjTransform` (0x8E2458), the matrices the game's own draw
  uses, and passes the colour as a texture coordinate (full precision);
- a `ps_2_0` that takes c * 255 to floor(c * 255) + 1 where a per-pixel
  threshold (interleaved gradient noise of the pixel position, from
  `RsGlobal`'s size) is under its fraction, else to the floor.

Everything goes through the game's RwD3D9 wrappers (`_rwD3D9SetFVF`
0x7F9F30, `_rwD3D9SetVertexShader` 0x7F9FB0, `_rwD3D9SetPixelShader`
0x7F9FF0, `_rwD3D9SetVertexShaderConstant` 0x7FACA0,
`_rwD3D9DrawIndexedPrimitiveUP` 0x7FA1F0, which flushes RW's render state
queue), so RW's caches match the device after; the shaders are set back to
none and the buffer emptied, as the stock flush does. The shaders are made
with `RwD3D9CreateVertexShader` / `CreatePixelShader` (0x7FAC60, 0x7FACC0)
on the first sky with a device; if the card refuses them the stock flush
draws every sky and the log says so once.

skygrad.asi itself replaces the call to `RenderSkyPolys` at 0x53D81D and
never reaches 0x7154A8, so with it installed fixes' row stays in but does
nothing.

**Tests.** `crashfix_dll_test.c`, under Wine on a real D3D9 device put where
RwD3D9 keeps it: a 64x256 sky from (40, 90, 170) to (52, 120, 210) through
the hook and the game's wrappers. Drawn the stock way every row is one
value and its mean is up to 0.55 of a step off the gradient; dithered, every
row's mean is within 0.05 and every pixel within 1. The shaders are made
once, the buffer is emptied and RwD3D9's shader and FVF caches match the
device after; with nothing in the buffer the stock flush runs and no
shaders are made.

## 46. What killed the player (deathtrace)

A diagnostic, not a fix: bots on SA-MP jack a car from the passenger side
(sprint, then F) and the driver dies as he falls out. The jack itself is the
game's: `CTaskComplexCarSlowBeDraggedOut` (0x64C8B0) with the jacker's own
door gives `CTaskSimpleCarFallOut` (`CAR_FALLOUT_LHS`), and nothing on that
path takes health (the quick jack, `IsCarQuickJackPossible` 0x64EF00,
always returns false). So the death is damage dealt around it, and which
damage decides the patch.

With option `deathtrace` on, `deathFrame` keeps 3 s of the player's health
(ped +540h), armour (+548h), ped state (+530h, 54 die, 55 dead), vehicle
(+46Dh bit 0, +58Ch), primary task trees (+47Ch intelligence, slots at +4,
each task's type and subtask through its vtable +10h, +8h) and position;
`onReceivePacket` and `onReceiveRpc` add the sync packets (200, 203, 206,
207, 209, 210, 211) with their sender and every RPC with its first word, and
`onSendRpc` the damage SA-MP reports (115) and the death (53). When the
player dies the history is frozen with what the game blamed: the last damage
weapon (+760h) and entity (+764h), the last collision's intensity (+D8h) and
entity (+DCh), each named as a SA-MP player or vehicle with its driver. The
report goes to `logs\death.log` when SA-MP sends RPC 53, or 3 s after
without one, as one time-ordered timeline. Bitstreams are read and put back.
The ped is read at most 60 times a second, and always on the frame it dies
in; the task-call function types are made once (LuaJIT never frees one
parsed from a string, and one per task per frame overflowed its table).

**Tests.** `fixes_test.lua`: a jacked driver: the car drive task, an
EnterVehicle RPC and a vehicle sync from player 12, on foot with `823>834`
(be dragged out, car fall out), dead with weapon 49 by that player and a
532.5 collision with his car, then RPC 115 and RPC 53: one report, in
order, nothing older than 3 s, the read positions untouched; a second death
with no RPC 53 is written after 3 s; off, nothing is kept.

## 47. The Frame Limiter's device Reset

`psCameraShowRaster` (0x745240) flips with `rwRASTERFLIPWAITVSYNC` while the
Frame Limiter is on or a loading screen shows. `_rwD3D9CameraShowRaster`
(0x7F99B0) then switches the PresentationInterval (0xC9C074; the device is
made with `D3DPRESENT_INTERVAL_IMMEDIATE`, 0x80000000) to ONE, or back, with
a full Reset (vtable +40h), and checks nothing: when the Reset fails,
GetBackBuffer leaves 0xC97C30 NULL and the Release after it reads address 0
at 0x7F9A3C. Seen 2026-10-31 with ProperShaders under DXVK: switching the
Frame Limiter on, ProperShaders.log "Device Reset failed (0x8876086C)", then
the crash. A window resize fails the same way in `_rwD3D9CameraClear`
(0x7F7C82); that one is not covered here.

Option `novsyncreset` (on by default) writes a jump at 0x7F99E5 (`74 13 81
F9 00 00 00 80` -> `E9 F6 00 00 00 90 90 90`) to 0x7F9AE0, the flip with the
interval as it is: the device keeps the interval it was made with and is
never Reset for vsync. The Frame Limiter still caps the frame rate (33); it
no longer turns vsync on.

**Tests.** `fixes_test.lua`: on by default over the stock bytes, the jump
lands on 0x7F9AE0, off puts the switch back, another mod's bytes there are
left alone.

## 48. The faults SA-MP swallows (faultlog)

A diagnostic, not a fix. samp.dll runs `CGame::Process` (0x53BEE0) inside an
exception guard tagged `s007` (0.3.DL-R1: samp.dll+0xA24D7), and the opcodes
the server sends under `opcode`. Its filter (samp.dll+0x60470 on DL) prints
`Warning(%s): Exception 0x%X at 0x%X` and swallows the exception; fixes'
exception filter fix swallows every one, not ten. A fault at the same place
every frame then aborts the rest of the world update every frame: peds and
vehicles stop, the renderer and SA-MP's chat go on, and MoonLoader's scripts
stop too (fixes' once-a-second "swallowed N" line never came, with the count
rising). Seen 2026-11-08 on Unity RP: ten
warnings at 0x749E75 (`RpAtomicClone`, reading a bad atomic) after driving a
hotwired car, and nothing logged who called it.

With option `faultlog` on, `lib/fixes/crashfix.dll` (native/faults.c) adds a
vectored exception handler, first in line, that only watches: for an access
violation, illegal or privileged instruction, integer divide by zero, array
bounds or in-page error it copies the code, address, what it touched, the
registers and up to 1 KB of stack into a ring of 32, and returns
`EXCEPTION_CONTINUE_SEARCH`. The same address again is counted, not copied.
A thread of the DLL's own appends new records to `logs\faults.log` every
250 ms, so a fault that stops every script is still written: each address as
module+offset (with the absolute address), and the stack's return addresses,
a dword into executable memory right after a call instruction (rel32,
register, memory and SIB forms). Repeats get an `again` line at most every
5 s; after 64 faults only counts. fixes' "SA-MP has swallowed N" warning then
points at the log.

A fault inside `lua51.dll` also records where Lua was: the key being read, the
first 40 bytes of the table in `ebp`, and up to 6 Lua frames (chunk name, line,
the function's first line), walked from the interpreter's BASE in `edx` and
next instruction in `esi` with every read checked by `VirtualQuery` first.

Every fault also records the text the registers and the first 128 stack dwords
point at (at most 12 strings of 240 bytes, as `text ebx: "..."`). A crash in
MoonLoader's own error reporting, for one, loses the error otherwise: its text is
only in a register by then.

**Tests.** `crashfix_dll_test.c`, under Wine against gta_sa.exe's own code:
`CAtomicModelInfo::CreateInstance` (0x4C4530) on a model whose atomic is bad,
with no RenderWare engine, faults in `RpAtomicCreate` (0x749C5D, reading
address 0); the log names that, then the callers in order (0x749E66 in
`RpAtomicClone`, 0x4C454F in `CreateInstance`, the test), counts the second
fault at the same address, ignores a software exception, and after Stop sees
nothing. `fixes_test.lua`: started at load with the log beside the others, a
second start is a no-op, `/fixes faultlog off` and `on` stop and start it, the
swallowed count names the log, and an older DLL without the exports is said
and left alone. `faults_lua_test.sh`, in the game's own `lua51.dll` under Wine:
a table with a garbage node pointer faults at `lua51.dll+0x1CA3`, and the log
has the key, that pointer and the three Lua frames with their lines.

## 49. The dialog scrollbar's thumb drag divides by zero (samp.dll)

DXUT's `CDXUTScrollBar`, dragging the thumb, computes
`start + (thumbTop - trackTop + maxThumb / (maxFirst * 2)) * maxFirst / maxThumb`.
`maxFirst = end - start - pageSize` is 0 for a list that fits its page and
`maxThumb` (track minus thumb height) can be: a press where the hidden thumb
was and a mouse move fault inside the window procedure, and the game spins on
the fault (seen 2026-11-12 on R5, `samp.dll+0x88A11`, eax = ecx = 0x112,
edi = ebp = 0).

`fixDialogScrollbar()` hooks the 8 bytes of the first division
(`mov eax,ecx / cdq / lea ebp,[edi+edi] / idiv ebp`) with a stub that tests
edi and ecx: either 0 and it loads the position as it stands and goes to the
store at +0x1B. Signature `ScrollbarDragDivide`; the 36 bytes are the same in
all four DLLs (R1 0x843EB, R3-1 0x882FB, R5 0x88A0B, DL 0x8848B).

**Tests.** `fixes_test.lua`: another shape refused with nothing allocated; the
jmp and three NOPs, the rest untouched; the stub's bytes and both ways back;
asked again, no second block.

## 50. Anisotropic filtering (option anisotropic)

The game attaches RenderWare's RpAnisot plugin (0x748F70) and never uses it:
one byte a texture, at the offset in 0xC92478, set to 1 by the constructor at
0x749020. `RwD3D9SetTexture` (0x7FDF15) reads it: over 1 it sets
`D3DSAMP_MAXANISOTROPY` and filter 7 of its table (anisotropic min and mag,
linear mips).

`fixAnisotropic(on)` writes the level, 16 or the device's most
(`D3DCAPS9.MaxAnisotropy`, 0xC9BF6C), into the constructor's operand
(0x74902D), then into every texture of every loaded dictionary
(`CTxdStore`'s pool, 0xC8800C). Off writes 1 to both. Textures outside the
pool that exist already (SA-MP's and scripts' own) keep what they have.
Mipmaps are not made: a texture with none gets the filter and nothing to
blend between.

**Tests.** `fixes_test.lua`: refused on a device with none; 8 and 16; both
loaded textures set and a free slot skipped; off puts 1 back; another
instruction at the site refused.


## 51. Running behind another window (option unfocused)

`WinMain` sleeps 100 ms a pass and skips the frame while `ForegroundApp`
(0x8D621C) is 0. `WndProc` writes it on `WM_ACTIVATE` deactivating (0x747FB0,
0), `WM_KILLFOCUS` (0x748054, 0) and `WM_SETFOCUS` (0x748068, 1), so the game
freezes whenever another window has focus. 17's NOP of the activating write
(0x747FFE) does not change that.

`fixUnfocused(on)` points those three `mov dword [ForegroundApp], imm32` at a
flag in a 0x40-byte block of its own ("FxFg", the flag, a stub at +0x10), so
`ForegroundApp` stays 1. Idle centres the cursor every frame outside menus
(`psMouseSetPos`, `SetCursorPos` at 0x74542B), which would drag the cursor
back into the game while you work elsewhere: that call becomes a call to the
stub, which skips `SetCursorPos` while the flag is 0. In focus the cursor is
centred as before, so it cannot drift out of a borderless window.

Sound still pauses on `WM_ACTIVATE`. `CPad::UpdateMouse` now runs unfocused;
DirectInput's foreground mouse reads nothing then. Off by default; exclusive
fullscreen loses its device on alt-tab, so it is for borderless or windowed.
Not taken from the usual "antiafk" script: it also NOPs the `je` after
`DoRWStuffStartOfFrame_Horizon` (0x53EA88), drawing a frame whose
`RsCameraBeginUpdate` failed, and it NOPs the cursor call outright; its off
branch writes each site's bytes into the other.

**Tests.** `fixes_test.lua`: off by default; on sends the three writes to the
flag and the cursor call to the stub, whose bytes are checked; off puts all
four back; on again reuses the block; a hook at the cursor call is refused.

## 52. SA-MixSets' fixes (options smoothshadows, weaponicon, scrollreload, doubleskid, boatradio, boatfoam)

From Junior_Djjr's SA-MixSets (MIT, github.com/CanerKaraca23/SA-MixSets).
Of its 284 options most are preferences (densities, draw distances, effect
sizes, weather); these are its fixes `fixes` did not have and SA-MP allows.
Five more were already here: the mouse after alt-tab (crash fix `mouse`),
DirectInput's mouse (SilentPatch), paintjobs turning white and lights with
the engine off (MTA's), the display modes' aspect filter.

- `smoothshadows` (0x70C6A9, 0x70C2D6, fixes.lua). A car's headlight glow on
  the ground and its shadow are stored with a 0.4 and a 0.1 snap, so they
  move in steps; 0 makes them follow the car.
- `weaponicon` (0x58D94D, 0x58D896, fixes.lua). The HUD fist and weapon
  icons are drawn 58 wide against 47 high; both read the 47 beside it.
- `scrollreload` (0x60B4FA, fixes.lua, off by default). The mouse wheel no
  longer cancels a reload; MixSets enables it on SA-MP only for roleplay
  servers.
- `doubleskid` (calls at 0x6AB136, 0x6AB1FD, 0x6AB2B7, mixsets.c). A vehicle
  with handling's double rear wheels (model flag 0x20000000) laid one
  skidmark and one dust trail per side; each rear wheel's call is made again
  0.45 to the outside along the car's right, its id moved so the marks stay
  apart.
- `boatradio` (0x6DF4E7, mixsets.c). Changing the radio blends the car's
  tune-radio animation even on a boat's driver, breaking the boat pose; in
  the boat pose (animation 81) nothing is blended.
- `boatfoam` (0x6EA260, mixsets.c). A boat's wake is as bright at night as by
  day. Once a frame, at RenderWakeSegment's start, its four outer alpha
  multipliers (0.4, 1, 1, 0.4) follow the water colour:
  ((R + G + B + A) / 255 / 4)^2 + 0.1, times 2, at most 1. MixSets reads that
  2 from its ini (no default given); with timecyc's water, noon reaches the
  stock wake and 22:00 about 56%. Turning it off puts the stock values back.

**Tests.** `crashfix_dll_test.sh`: every row goes in on the stock bytes and
out leaving them; the wake at noon and 22:00 water and put back; with
AddSingleWheelParticles recorded, a car gets one call, a truck's front
wheel one, its rear wheels two, the second 0.45 along the car's right and to
its own side. `fixes_test.lua`: the options listed.

## 53. Model loading timed (option streamtime)

A diagnostic, not a fix: step 1 of `docs/ASYNC_STREAMING_PLAN.md`. Off by
default. The IMG read is already on `CdStreamThread` (0x406560); what the
game's thread still does is timed with QueryPerformanceCounter by three entry
hooks in `lib/fixes/native/stream.c` (crashfix.dll):

- `streamconvert` (0x40C6B0, 7 bytes) `CStreaming::ConvertBufferToObject`:
  the parse and the D3D textures. It ends at 0x40C6B7, well before
  `txdconvert`'s site at 0x40C8CE.
- `streamfinish` (0x408CB0, 8 bytes) `CStreaming::FinishLoadingLargeFile`: a
  big model's second half.
- `streamall` (0x40EA10, 5 bytes) `CStreaming::LoadAllRequestedModels`: the
  whole drain, its wait for the disk included, and who called it (the return
  address as module+offset, for SA-MP's callers). In this exe the entry is a
  jump to 0x40A45E, which holds the first instruction and jumps back to
  0x40EA15; the wrapper goes on there.

Each hook jumps to a C wrapper that calls the game's function past the
replaced bytes. All three are cdecl and return in `eax`, passed back whole.
The wrappers use integers only, so the x87 state stays the game's. Sums are
kept per kind (convert, finish), per type by the id ranges the function
branches on (dff under 20000, txd under 25000, col under 25255, ifp
25575-25754, other: IPLs, paths, recordings, scripts) and per id.

`logs\stream.log` gets a line per file and per drain, buffered and written
once a second from fixes' loop (and when 64 KB fill):

    +5123.456 f8811 convert txd 20431 3.214 ms 512 KB
    +5130.001 f8812 convert dff 1234 0.412 ms 96 KB in loadall
    +5131.900 f8812 loadall 9.870 ms, 7 file(s) 6.120 ms, from <module>+0x<offset> (0x<address>)

`+` is ms since the header, `f` the game's frame counter (lines with the same
frame add up to that frame's cost), KB the file's size in the IMG. A drain's
time less its files' is the wait for the disk. `/fixes streamtime report`
prints and appends a summary: count, total, mean and worst by kind and type,
the drains, and the 12 ids that took longest in all. Turning the option off
writes the summary too.

With `drift` also on, each 30 s line gains, after `streamMB`: `streamms` (ms
a frame converting and finishing), `streamn` (files), `streamworst` and
`streamworstid` (the longest file and its id), `loadalls` and `loadallms`.

**Tests.** `crashfix_dll_test.sh`: the rows go in on the stock bytes and out;
the entry hook ends before `txdconvert`; with the game's bodies replaced by
stand-ins that sleep, calls through 0x40C6B0, 0x408CB0 and 0x40EA10 return
`eax` whole and keep esi, edi, ebx, and are counted, timed and typed; the log's
lines, the `in loadall` mark, the drain's caller and the summary.
`fixes_test.lua`: off by default; on starts the log and installs the hooks;
the read's sums since the last read; drift's fields; the report; off; an older
DLL.

## 54. Draw calls by category (option drawcount)

A diagnostic, not a fix: which part of the scene the frame's draw calls come
from, before anything is merged. Off by default. `lib/fixes/native/drawcount.c`
(crashfix.dll).

The exe draws from some 250 sites through the device's vtable, so the count is
taken there: slots 81-84 (`DrawPrimitive`, `DrawIndexedPrimitive`,
`DrawPrimitiveUP`, `DrawIndexedPrimitiveUP`) of the `IDirect3DDevice9` at
0xC97C28 point at counters that pass the call on. SA-MP's and the overlays'
draws on the same device are counted too. Turning the option off puts the
slots back unless somebody wrote them since. With no device yet, fixes' loop
asks again each second.

Each draw goes to the category the game's thread is in, set by three entry
hooks (5 bytes each, C wrappers around the game's functions, integers only):

- `drawroads` (0x553A10) `CRenderer::RenderRoads`: the whole pass is `roads`.
- `drawnonroad` (0x553260) `CRenderer::RenderOneNonRoad`: the entity's type
  (`+0x36`, low 3 bits): `buildings`, `vehicles` (driver and passengers
  included), `peds`, `objects`, `dummies`. A building with LOD children
  (`+0x34`) is `lods`.
- `drawentity` (0x732B40) `CVisibilityPlugins::RenderEntity`: the alpha
  lists' entities, by the same types; its inner `RenderOneNonRoad` stays the
  outer one's.

Anything else is `other`: sky, water, effects, shadows, the HUD, SA-MP, the
overlays. Each category's scopes are timed with QueryPerformanceCounter;
`other` has no time of its own (the frame's ms less the rest).

The frame's phases are timed the same way, around the functions `Idle`
(0x53E920) calls. Three are entry hooks; six are the calls themselves
(5 bytes, `E8`), because urp-client.dll holds those functions' entries. A
call's wrapper runs the function at its entry, so that mod's hook runs inside
the phase and its time is the phase's.

| phase | hook | function |
|---|---|---|
| `process` | `phprocess`, the call at 0x53E981 | `CGame::Process` |
| `process.world` | `phworld` 0x5684A0 | `CWorld::Process`, inside it |
| `process.streaming` | `phstreaming`, the call at 0x53BF0B | `CStreaming::Update`, inside it |
| `list` | `phlist`, the call at 0x53E9F9 | `CRenderer::ConstructRenderList` |
| `prerender` | `phprerender` 0x553910 | `CRenderer::PreRender` |
| `scene` | `phscene`, the call at 0x53EABF | `RenderScene` (the categories' scopes are inside it; the mirror's call at 0x7271DE is not timed) |
| `effects` | `pheffects`, the call at 0x53EAD3 | `RenderEffects` |
| `hud` | `phhud`, the call at 0x53EB12 | `Render2dStuff` |
| `present` | `phpresent` 0x7EE370 | `RwCameraShowRaster`: the present and whatever hooks it |

Inside `CWorld::Process`, its passes over the moving entities are timed a
call each. The sites are `mov reg, [esi]; mov ecx, esi; call [reg+slot]`
(7 bytes, replaced by a call and two NOPs) and one direct call:

| phase | hook | call |
|---|---|---|
| `world.anim` | `wanim` 0x5685BB | `CEntity::UpdateAnim` (0x535F00), the moving list's (the entity is in `ecx` there) |
| `world.control.peds`, `.vehicles`, `.objects` | `wcontrol1` 0x56863B, `wcontrol2` 0x5686ED (postponed) | `ProcessControl` (vtable +0x28), by the entity's type |
| `world.collision.peds`, `.vehicles`, `.objects` | `wcollision1` 0x56876F, `wcollision2` 0x5687CF (the four further rounds) | `ProcessCollision` (+0x2C), by type |
| `world.shift` | `wshift1` 0x568894, `wshift2` 0x568908 | `ProcessShift` (+0x30) |

The stuck pass' `ProcessCollision` (0x56882E) and the cutscene's calls are
not timed. Two clock reads a call cost some tens of microseconds a frame.

A ped's or vehicle's `ProcessControl` and `ProcessCollision` are also summed
by what it is doing as the call starts: its distance from the player's ped
(0xB6F5F0; under 30 m, 30-100 m, over 100 m), moving or still
(`m_vecMoveSpeed` at +0x44), and a vehicle driven or empty (`m_pDriver` at
+0x460) or a ped in a vehicle or on foot (`bInVehicle`, bit 8 at +0x46C).
With `drift` on, each 30 s line's buckets go to `logs\world.log` under a
header with the line's fps, ms, draws and world time:

A vehicle's collision is also summed by its status (`+0x36` bits 3-7:
physics, abandoned, wrecked ...) and whether the game has put it on fake
physics (`m_nFakePhysics` at +0xB8 reached 10: `CAutomobile::ProcessControl`
resets its speeds each frame, so `ProcessCollision` skips `CheckCollision`).
Only abandoned and wrecked vehicles can get there; a vehicle never leaves the
moving list.

    19:22:01 fps=299 ms=3.34 draws=1588 world=1.051
      control vehicles under 30 m still empty: 14.20 a frame, 0.031000 ms
      collision peds 30-100 m moving on foot: 9.10 a frame, 0.120000 ms
      collision vehicles status abandoned fake physics: 20.10 a frame, 0.060000 ms

Every call `Idle` makes (32 sites from `CTimer::Update` at 0x53E968 to
`RsCameraShowRaster` at 0x53EC01, and `AppEventHandler`'s call of `Idle`
at 0x53ECBD), and every call `CGame::Process` makes (74 sites from
`CPad::UpdatePads` at 0x53BEE6 to `WaterCreatureManager_c::Update` at
0x53C22F, its clock reads left out) is also timed by chaining: the site's `E8` goes to a stub made
at run time that calls the target the site had, whoever's it is, through a
pointer. So SA-MP's hooks on `CGame::Process` and `Render2dStuff` are inside
those calls, and the module a call goes to is named. The stub keeps the
flags and every register but `eax`/`edx` around its timing; the return
address is kept in the site's slot (`Idle` does not recurse). Stopping puts a
site back only while it still calls the stub. With `drift` on, these go to
`logs\world.log` after the buckets, the longest first:

    idle Idle: 3.100 ms a frame
    idle CGame::Process: 1.400 ms a frame (through samp.dll)
    process CWorld::Process: 0.900 ms a frame

`rest` is the frame's ms less the undotted phases: the message loop, audio,
SA-MP's own work, the menus and fonts. A phase whose site another mod holds
is left out and its time falls into `rest`.

With `drift` also on, each 30 s line gains, after `lods`: `draws` and `ktris`
(a frame's draw calls and thousands of primitives), `d.<category>` (a frame's
draws) and `ms.<category>` (a frame's ms inside its scopes), then `t.<phase>` and `t.rest` (a frame's
ms). `/fixes drawcount
report` prints the same by category for the frames since the last report,
with each one's share.

## 55. Parked cars' collision (option parkedcol)

On by default. A car never leaves the moving list, so
`CPhysical::ProcessCollision` (0x54DFB0) runs for every parked car every
frame, about 6 microseconds each (drawcount's `logs\world.log`, 2026-11-14:
28 parked cars in LS, 20 of them on fake physics, about 0.17 ms a frame).
With its move and turn speeds zero the game's path checks nothing: it applies
the zero speeds, reorthogonalises the matrix, sets the flags, measures a
distance of zero and calls `RemoveAndAdd`, which re-links the car in the
sector lists where it already is.

`parkedcol` (8 bytes at the entry, `lib/fixes/native/parked.c`) takes a car
that the stock path would leave as it is and returns after setting what that
path sets: `m_fMovingSpeed` (+0xD4) 0, physical flags (+0x40) 0x800, 0x1000
and 0x8000 clear, not stuck and in a safe position (+0x1C: 0x10 clear, 0x20
set). The car must be a vehicle, abandoned or wrecked (status 4 or 5: never a
player's, a remote, simple or ghost car), use collision, have simple
collision on (0x20000 clear), not be flagged to process even if stationary
(0x800) or for `ApplySpeed`'s special path (0x80), not be a car park vehicle
(+0x42C bit 1), and have its move, turn and both friction speeds (+0x44 to
+0x73) all zero, compared as integers (-0 is zero). A car that is hit gets a
speed and takes the game's path the same frame. Peds and objects always take
it. `CrashFix_ParkedSkips` counts the skipped calls.

## 56. HighFpsFixes' frame-rate fixes (options timerfps, camerafps, weaponfps)

HighFpsFixes (sonochiwa's sa-high-fps-fixes, MIT, 1.9.0) fixes many
frame-rate bugs 41 and 43 do not. `lib/fixes/native/hfps.c` has the ones
SA-MP reaches, as crashfix.dll rows: stock bytes checked, another hook stood
down for, a line each. New options `timerfps`, `camerafps` and `weaponfps`
(on by default) and the existing `pedfps`, `vehiclefps`, `effectsfps`,
`turnfps` and `framecaps` switch them.

**Standing down for it.** When HighFpsFixes.asi is loaded, its INI beside it
(its file name with `.ini`; a key missing or not `0`/`1` is on, as its
`ReadSetting` has it, and `classicHandling=1` turns its handling keys off) is
read once, and every row or option of ours that fixes the same behaviour
stands down while its switch for it is on (`FPS.HFPS` in fixes.lua maps them):
where it patches our bytes the row would anyway, and the map also covers what
it fixes at another instruction (the swim speed it wraps at the caller, the
ped push, the parachute's literals, the door dampings it takes at their
shared `fmul`, the sleep counter's increment beside our threshold, the
particles it gates in `AddParticle`). Its guard rewrites a site holding
another mod's jump; ours never hold one it fixes, so they do not fight.

**Per 30 fps step.** `cf_due30` says whether a 30 fps step ends this frame
(every frame at or under 30 fps), once per frame for every caller, and counts
the steps in `cf_steps30`. The rows that call C with the game's values on the
x87 stack move them to memory first and use `pow3`, a pow in three registers:
msvcrt's `pow` (behind MinGW's `powf`) wants the stack empty and returned NaN
with one value on it.

| option | keys | what |
|---|---|---|
| `pedfps`, `vehiclefps`, `timerfps` | 103 rows (`aimtime1`, ... `hudtime46`, `drown`) | a `call _ftol` of a frame's time keeps its fraction per call site (`cf_ftol_carry`, by return address; a second call in the frame with the same value gets the same integer): the target lock's 1200 ms, nine more task timers, the police AI, `FlyingControl`, the plane and car bomb timers, the upside-down check, a damaged engine's sputter, drowning's damage (none above 150 fps), the idle camera, on-screen countdowns, every script's TIMERA/B (SilentPatch's call there stands it down), 21 stat counters, 13 stunt counters and 46 HUD timers. They ran long at high fps and stopped above 1000 |
| `vehiclefps` | `gearkick`, `gearkick2` | `JustFinishedAccelerationLoop` declares the acceleration loop ended after 10 frames, which kicks the nose up on an upshift: 9 ms at 1000 fps, where it kicked every eleventh frame. Both reads take `round(10 / ratio)` |
| `vehiclefps` | `carsteer1`-`2`, `bikesteer1`-`2`, `planesteer1`-`4` | the steering and control surfaces ease to the pad by `0.2 * timestep` a frame; `1 - (1 - 0.3333)^ratio` closes per second what a 30 fps step does |
| `turnfps` | `flyturn1`-`3` | `FlyingControl` raises each axis's turn resistance to `timestep²`, so an aircraft held far more turn per second at high fps (and with `turndamping`'s 0.99 turned faster than at 30). The first timestep is 1.6667 |
| `vehiclefps` | `springdamp`, `loadlean` | `ApplySpringDampening` takes `min(timestep * level, 0.25)` a frame: at 30 fps a level over 0.15 is capped, at high fps it is not (a third more damping). The level becomes the one that takes per second what the capped step does. `loadlean` keeps each entity's speeds at the start of `CPhysical::ProcessControl`, and the damper bites the speed the frame has added so far as a 30 fps step does, so the body leans as far under a cornering or braking load |
| `vehiclefps` | `carrest`, `bikerest`, `trailerrest`, `objsleep`, `trailersleep` | `m_fMovingSpeed` (a distance per frame) as a 30 fps step's against the rest limit; an object's and a trailer's still frames counted per 30 fps step (cars and bikes have `carsleep`, `bikesleep`) |
| `vehiclefps` | `movingparts` | `UpdateMovingCollision` truncated each frame's step of the forks, blade, bed and ramps: 300 units a second for 480 at 300 fps, nothing above 500. The 30 fps step per 30 fps step, its fraction kept per vehicle |
| `vehiclefps` | `sink1`-`2`, `boatwaves`, `boatengine`, `hydraise`, `hydlower` | a car under water and a burnt-out boat sink per 30 fps step; a boat's wave slam from the rise over a 30 fps step; a coasting propeller's 0.95; a hydraulic car's stance steps per 30 fps step |
| `vehiclefps` | `planeloss1`-`3` | a damaged part costs a slewed control `1 - 0.2 * status` a frame; each part's share settles where a 30 fps step does, under this frame's slew |
| `vehiclefps` | `wheelspin1`-`6`, `burnspeed`, `bikewheel1`-`2`, `bikepitch`, `bikerear1`-`2` | a wheel in the air spins up (-0.1), back (+0.05) and down (0.95) per 30 fps step; the burnout's 3000; a free bike wheel's decay and turn, the rear wheel's speed limit |
| `vehiclefps` | `bmxsprint`, `bmxlean1`-`4`, `headbop1`-`2`, `wheelsettle1`-`6` | the BMX rider's leans, the driver's head bop, a bike's or aircraft's wheel easing down (0.75 of the gap) per 30 fps step |
| `vehiclefps` | `attached`, `aisteer` | `PositionAttachedEntity` and the aircraft autopilot divide by `max(1, timestep)`, short by the ratio above 50 fps: the timestep itself |
| `pedfps` | `drunksteer`, `pickup1`-`2` | a drunk's 10-deep steering delay shifts per 30 fps step (9 frames was 4.5 ms at 2000 fps); the step onto a box takes a 30 fps step's share |
| `timerfps` | `fatcount`, `hudflash1`-`6`, `ambience1`-`2`, `mapwheel`, `mapzoomin`, `mapzoomout` | fat burns with the remainder kept (none under rate 5 at 500 fps); the HUD flashes on game ms / 40; distant gunfire and the foghorn every 1024 30 fps steps; a map wheel notch zooms past the 20 ms repeat on its first frame |
| `effectsfps` | `firecar`, `firespread`, `firemerge`, `blastframes`, `blastroll`, `blastfire`, `lightning1`-`4` | a fire's rolled events, an explosion's second blast and setting its victim alight, a lightning burst's start and end, per 30 fps step; an explosion's frame count stops at 255 (it wrapped at 0.85 s at 300 fps); a burst measured in 30 fps steps |
| `effectsfps` | `footprints`, `jetflame1`-`2`, `swimsplash1`-`4`, `enginerevs1`-`2` | bloody footprints fade, the jetpack's flame ramps, a sprinting swimmer splashes, the engine note follows the pedal, per 30 fps step |
| `effectsfps` | `samprot`, `sampmove` | SA-MP's `CObject::Process` turns a moving object by how far it has come and calls it arrived when the step would pass the rest: a server moving it by millimetres a frame left it under float precision at high fps, never turning or arriving. Both from SA-MP's own elapsed * speed. Found in samp.dll by the arrival test's 15 bytes (once), the turn 12Ch on |
| `camerafps` | `aimcam`, `aimweapon`, `aimzoom` + a game patch at 0x52381D | aiming on foot, `CCamera::Process` and `Process_AimWeapon` run with the timestep at least 1 (the aim camera shook at high fps), the zoom with the real one; the on-foot camera's `max(1, timestep)` clamp NOPed as the car camera's is |
| `weaponfps` | `weaponfx`, `weaponammo`, `weaponshot`, `weaponfire`, `weaponwater`, `chainsaw`, `teargas` | a spray's or flame's emitter keeps its particle fraction (none came out at high fps); a flamethrower, spraycan or extinguisher takes ammo and adds its shot 30 a second, the extinguisher's water a 30 fps step's worth; the chainsaw's rewind held to a strike every 2/30 s (45 a second at 144); tear gas's roll per 30 fps step |
| `framecaps` | `radiolock` | the main loop held the frame limiter while the player's car radio had a beat track: a car ride at 30 fps. Only the dance and lowrider minigames keep it |

Not taken: the gang war timers, the burglary and mission literals, the stunt
jump camera, the cop's car exit and the SWAT ropes (single player); the wheel
slip rate (41: the skid test is fps-neutral in a closed loop); the rifle's
aim-walk step (samp.dll gives `ControlGunMove` its own timestep); the
on-rails wheel spin; its buoyancy threshold (`floatfps` takes the same span);
the climb's clamp (`climb1`-`2`); the bike lean target, the abandoned bike's
substeps and the takeoff pitch (experiments there); `AddParticle`'s general
gate (`effectsfps` gates the per-frame emitters at their callers); the frame
limits and the footprints' height.

Tests: fixes_test checks the INI (a missing key on, `0`, `1`, a comment that
makes it neither, `classicHandling`) and that `wheelfps` stands down while
`swimfps` stays; crashfix_dll_test checks every row on the stock bytes, the
carry (1000 ms in 600 frames at 600 fps, the same integer twice, `ecx` and the
x87 stack as `_ftol` leaves them), the steering, the kick count, an
aircraft's turn per second, the damper, the wave, the step thunks, the 30 fps
steps, fat, ammo, shots, the chainsaw, the HUD flash, footprints, the aim
hold and samp.dll's object site.
