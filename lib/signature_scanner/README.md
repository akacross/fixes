# signature_scanner

Signature (pattern) scanner for **samp.dll** — 0.3.7 R1–R5 and 0.3.DL — for MoonLoader/LuaJIT.

It exists so scripts stop hardcoding per-version offsets: point it at a verified byte
pattern, and it finds the address on whatever build is actually running.

```lua
local sig = require('signature_scanner').init()   -- logs to logs\akacross.log (area sigscan)

local va  = sig.resolve('F1KeyHandler')   -- absolute address, ready for memory.*/ffi
local rva = sig.resolveRva('SAMP_INFO')   -- offset from samp.dll's base
sig.flush()                               -- persist what was resolved
```

Call `init()` **after** `while not isSampAvailable() do wait(0) end`.

## Layout

| file | role |
| --- | --- |
| `init.lua` | wiring + `resolve` / `resolveRva` / `resolveAll` / `flush` |
| `patterns.lua` | the signature database — **verified entries only** |
| `version.lua` | build identification + known per-version RVAs |
| `scanner.lua` | pattern compilation, section scanning, operand decoding |
| `memory.lua` | module lookup, PE/section parsing, bounded reads |
| `resolver.lua` | cache → scan → known-offset resolution |
| `cache.lua` | persistence, keyed by build identity |
| `fields.lua` | **generated** per-version struct member offsets (see below) |
| `seed.lua` | derive a candidate pattern at runtime from a known offset |
| `self_test.lua` | in-game verification (`require(...).run()`) |
| `example_usage.lua` | worked examples |
| `test/host_test.lua` | runs the real scanner against a samp.dll file, on the desktop |
| `tools/gen_pattern.py` | offline pattern derivation/verification across builds |
| `tools/gen_fields.py` | regenerates `fields.lua` from the SAMPFUNCS SFAPI headers |

## RVA vs absolute address

Everything stored — patterns, `version.KNOWN`, the cache — is an **RVA**, an offset from
`getModuleHandle('samp.dll')`. Absolute addresses only exist at the API boundary
(`resolve()`), because samp.dll can be relocated and an absolute address from a previous
launch is not valid in this one.

`gta_offsets.lua` is the opposite case: those are **absolute** gta_sa.exe addresses. They
are not samp offsets and must never be fed to this scanner.

## Adding a signature

Do not hand-write patterns. Derive them from a real DLL and record the match counts:

```bash
# what have I got?
lib/signature_scanner/tools/gen_pattern.py identify /path/to/*/samp.dll

# derive from a known offset, verify against every build you have
lib/signature_scanner/tools/gen_pattern.py derive R1/samp.dll 0x713D6 --length 33 \
    --verify R1/samp.dll R5/samp.dll --extract rel32 --offset 28
```

Then add a descriptor to `patterns.lua`:

```lua
MySymbol = {
    pattern  = '8B 0D ?? ?? ?? ?? 85 C9 74 ??',
    sections = { '.text' },      -- default: executable, non-discardable sections
    offset   = 0,                -- added to the match before extraction
    extract  = nil,              -- nil | 'rel32' | 'imm32va' | 'imm32'
    expect   = { R1 = 0x..., },  -- optional cross-check, logged on mismatch
}
```

`extract` is what makes a match useful: a pattern match is the address of an
*instruction*, so `rel32` gets you the function a `call` targets, and `imm32va` gets you
the global a `mov` reads.

Every entry must match **exactly once**. An ambiguous pattern is refused rather than
guessed at — `MAX_MATCHES` in `scanner.lua` caps the search.

## Verifying

Desktop, against real DLLs (no game needed) — pass every build at once:

```bash
luajit lib/signature_scanner/test/host_test.lua "/mnt/data/Projects/New Folder/"samp{r1,r3-1,r5,dl}.dll
```

It maps each DLL the way the Windows loader would and runs the production code path, then
checks each result against the `expect` value for that build. The reference set lives in
`/mnt/data/Projects/New Folder/` (`sampr1.dll`, `sampr3-1.dll`, `sampr5.dll`, `sampdl.dll`)
— keep it, since the game folder's `samp.dll` gets swapped between builds.

`cache is for a different samp.dll build; discarding it` in the output is the
foreign-build assertion passing, not a failure.

In game:

```lua
require('signature_scanner.self_test').run()
```

## Per-build notes

* **R1 and R2 are packed** — extra `.poop0/.poop1/.poop2` sections, entry point inside the
  packer stub. Their `.text` is plain x86 on disk, so file-derived patterns work, but
  `.poop2` is `DISCARDABLE` and is excluded from runtime scanning.
* R3/R4/R5 and DL are ordinary builds.
* Version detection is by `AddressOfEntryPoint`, with `SizeOfImage` + `TimeDateStamp` as a
  fallback. An unrecognised build is not fatal: scanning still works, only the
  `version.KNOWN` fallbacks and per-version overrides are unavailable.

## Verified data

Target builds are the four SAMPFUNCS supports: **R1, R3-1, R5, 0.3.DL**. The `R3` tag means
the **R3-1** build (entry point `0xCC4D0`) — `version.lua`'s `M.LABELS` spells out each tag.
R2/R4 are detected but have no measured offsets.

| tag | build | md5 | entry | F1KeyHandler | ShowHelpDialogCall | ShowHelpDialogFunc |
| --- | --- | --- | --- | --- | --- | --- |
| `DLR1` | 0.3.DL R1 | `449e4f98…` | `0xFDB60` | `0x75456` | `0x75472` | `0x6B570` |
| `R1` | 0.3.7 R1 | `1d22eaa2…` | `0x31DF13` | `0x713D6` | `0x713F2` | `0x67450` |
| `R3` | 0.3.7 R3-1 | `08cf4166…` | `0xCC4D0` | `0x752C6` | `0x752E2` | `0x6B3C0` |
| `R5` | 0.3.7 R5 | `5ba5f0be…` | `0xCBC90` | `0x759D5` | `0x759F1` | `0x6BB30` |

| tag | SAMP_INFO / CNetGame | CNetGameCtor | CNetGameDtor | FrameLimiter | FrameSleepStub |
| --- | --- | --- | --- | --- | --- |
| `DLR1` | `0x2ACA24` | `0xB610` | `0x9570` | `0xA1A30` | `0xA2201` |
| `R1` | `0x21A0F8` | — ¹ | `0x9380` | `0x9D170` | `0x9D8F1` |
| `R3` | `0x26E8DC` | `0xB5F0` | `0x9510` | `0xA14E0` | `0xA1CB1` |
| `R5` | `0x26EB94` | `0xB930` | `0x9880` | `0xA1C10` | `0xA23E1` |

| tag | RadarToggleBranch | DebugLabelFormat (`.rdata`) | ConnectDelayCmp |
| --- | --- | --- | --- |
| `DLR1` | `0xA1BD8` | `0x11C800` | `0x8643` |
| `R1` | `0x9D318` | `0xD83A8` | `0x2D3C45` ² |
| `R3` | `0xA1688` | `0xEA780` | `0x85E3` |
| `R5` | `0xA1DB8` | `0xEA7D8` | `0x8953` |

² R1 keeps this code in its packed `.poop1` section with the surrounding instructions
mutated, so it gets its own signature (a `byVersion` override) anchored on the junk either
side of the compare. Both signatures put the imm32 at `+7`.

`ConnectDelayCmp` resolves the **imm32 of `cmp eax, 0xBB8`** — the 3000 ms throttle between
connection attempts, in the `CNetGame` method that logs `Connecting to %s:%d...`:

```
sub eax, [esi+lastConnectAttempt]
cmp eax, 0xBB8                      <- the dword this points at
jbe skip
```

Write a dword `0` over it and the compare becomes `cmp eax, 0`, so the wait is always
already satisfied. It is the long-circulated `sampGetBase() + 2964549` connection-delay
patch, which was only ever published as an R1 offset.

`RadarToggleBranch` and `DebugLabelFormat` were recovered from a commented-out block of
R1-only patches (`sampGetBase() + 643864` and `+ 885672`, in decimal); the signatures found
their equivalents on the other three builds. `DebugLabelFormat` is the module's first
**data** signature — the string's own bytes, looked up in `.rdata` rather than the default
executable sections.

¹ R1 constructs `CNetGame` from code in its packed `.poop1` section, so there is nothing to
match in the file image. It is a `byVersion` entry for the other three rather than a
`common` one, so R1 reports no symbol instead of a failed lookup for one that can't exist.

### CNetGame == SAMP_INFO

They are the same pointer slot — `SAMP_INFO` is the SAMPFUNCS name, `CNetGame` the sampapi
name, for one object. (The original `patterns.lua` listed them as two separate symbols.)
Both are kept, resolved by **signatures that share no bytes**: `SAMP_INFO` from the F1
handler, `CNetGame` from the sole `.text` site that writes the global (its teardown path).
They agree on all four builds, and `host_test.lua` asserts the equality — so each is a
running check on the other.

Evidence it is `CNetGame`, from the binary rather than from lore: on R5 the constructor does
`strncpy(this+0x30, host, 0x101)`, stores the port at `this+0x235`, stores a freshly created
RakClient interface into the object, and allocates `serverPresets` into `this+0x3D5`; the
routine that logs `Connecting to %s:%d...` reads the host and port back from those same
offsets and calls RakNet through the stored interface. An object owning the host, port and
RakClient is `CNetGame`.

The SAMPFUNCS SFAPI headers then confirmed it field-for-field — they call the same struct
`stSAMP`, which is simply the third name for it. Six of six offsets I had measured on R5
matched the header exactly (`+0x000` rakClient, `+0x030` serverIp, `+0x131` serverName,
`+0x235` serverPort, `+0x3D1` lastConnectAttempt, `+0x3D5` serverPresets, `+0x3DE` pools).

## Struct members: `fields.lua`

The scanner resolves *pointers*; `fields.lua` describes the *members* behind them. It is
**generated** by `tools/gen_fields.py` from the SAMPFUNCS SFAPI per-version headers
(`SFAPI/samp/v0.3.7-R1|v0.3.7-R3-1|v0.3.7-R5|v0.3.DL/SFSAMP.h`), which SAMPFUNCS ships for
exactly our four builds. Regenerate rather than hand-edit:

```bash
lib/signature_scanner/tools/gen_fields.py -o lib/signature_scanner/fields.lua
```

```lua
local fields = require('signature_scanner.fields')
local off = fields.offset('stSAMP', 'pools', sig.detected.version)
```

### The layouts differ per build — this is the trap

Headers are a claim, so every layout was cross-checked against the four DLLs: the field the
code dereferences most often right after loading `g_SAMP` must be `pools`, and on each
build it is (R1 `+0x3CD` ×86, R3-1/R5/DL `+0x3DE` ×128–130). `host_test.lua` asserts this,
so a stale `fields.lua` fails loudly.

`stSAMP` members that move:

| member | R1 | R3-1 | R5 | DL |
| --- | --- | --- | --- | --- |
| `rakClientInterface` | `+0x3C9` | `+0x2C` | `+0x000` | `+0x2C` |
| `serverIp` | `+0x020` | `+0x030` | `+0x030` | `+0x030` |
| `serverName` | `+0x121` | `+0x131` | `+0x131` | `+0x131` |
| `serverPort` | `+0x225` | `+0x235` | `+0x235` | `+0x235` |
| `pools` | `+0x3CD` | `+0x3DE` | `+0x3DE` | `+0x3DE` |

**R1 is a different struct**, not a shifted one — `sizeof` is `0x3D1` against `0x3E2`, and it
orders its tail differently (`lanMode` before `gameState`, `rakClientInterface` near the
end). An earlier revision of these notes guessed R1 was "very likely the same 0.3.7
struct". It is not. R1's connect routine is in the packed `.poop1` section, so that guess
could not have been checked against the file — which is exactly why it stayed a guess and
not a recorded offset.

And `stSAMPPools` reorders **every** pointer between builds:

| pool | R1 | R3-1 | R5 | DL |
| --- | --- | --- | --- | --- |
| `playerPool` | `+0x18` | `+0x08` | `+0x04` | `+0x08` |
| `vehiclePool` | `+0x1C` | `+0x0C` | `+0x000` | `+0x0C` |
| `gangzonePool` | `+0x08` | `+0x18` | `+0x14` | `+0x18` |

Reading `pools->playerPool` with a hardcoded index gets you a different pool on three of
four builds. (MoonLoader's own `sampGet*PoolPtr()` handles this internally, which is why
autobind's gangzone code is unaffected.)

## FrameLimiter — the FPS unlock

`FrameLimiter` is SA-MP's per-frame timing update: it measures its own FPS and writes the
result over GTA's `CTimer::ms_fTimeStep` / `ms_fTimeStepNonClipped` (gta_sa.exe `0xB7CB5C`
/ `0xB7CB58`, per gta-reversed `game_sa/Timer.h`), then throttles. Writing `0xC3` at the
entry stubs it out, so GTA keeps its own timestep and the cap never applies — that is what
the circulated `samp.dll + 0x9D170` patch does. `fixes.lua:removeSampFpsLimit()` uses it.

Those two `CTimer` addresses are **gta_sa.exe** globals, identical in every samp build, so
they sit in the pattern as literal bytes and one signature covers all four. That is also
how the function was located on the other three builds: R1's `0x9D170` was the only known
value, and every build has exactly 7 `.text` references to those globals in the same
arrangement.

Worth knowing before using it: `ret` at the entry disables the whole function, not just the
throttle — the FPS bookkeeping and the timestep override go too. That is what the patch has
always done, but it is a bigger hammer than "unlock the frame rate" implies.

Every row was produced by scanning the DLL. Each signature matches **exactly once**
on all four. R5 and DL were additionally confirmed in game — `fixes.lua` resolved both by
signature, passed its opcode sanity check, and verified the NOPs after writing.

Two rows are worth calling out:

* **DL** — those offsets were in no hardcoded table anywhere in this repo. The scan found
  them on a build nobody had measured, which is the reason to prefer a signature.
* **R3-1** — the scan lands on `0x752C6`, exactly the constant `fixes.lua` has carried for
  years. A signature and a hand-measured offset agreeing independently is the best
  evidence either is right.

### Don't identify a build by `SizeOfImage` alone

R3-1 and R5 are both 1,204,224 bytes with `SizeOfImage` `0x27E000`; only the
`TimeDateStamp` separates them (`0x5C0B4243` vs `0x6372C39E`). The fallback in
`version.lua` requires **both** to match and deliberately has no size-only path — calling
an R3-1 image "R5" would hand out offsets ~1 KB off and write NOPs into unrelated code.
