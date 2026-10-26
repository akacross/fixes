# fixes

Crash fixes, bug fixes and speed-ups for GTA San Andreas 1.0 US under SA-MP,
as one MoonLoader script. Each patch checks the bytes it expects first, so
nothing is written blind, and it stands aside when another mod already
handles the same thing.

## What it fixes

Crashes:
- MTA:SA's crash-fix hooks, about 80 of them: missing models, collision and
  animation data, null clumps and matrices, sound bank data out of range,
  bad texture dictionaries, vertex buffers that fail when video memory runs
  out, a train that loops forever on a bad track, and more
  (`lib/fixes/crashfix.dll`). They are gtasa_crashfix's (crashes.asi) hooks
  brought up to MTA's current code, with crashes.asi's and some of MTA's own
  mistakes fixed. With crashes.asi installed they stand aside, so it can go.
- MTA's 73 multiplayer patches, as crashes.asi applies them with SA-MP.
- SA-MP's exception filter crashing on the eleventh exception: every one is
  swallowed, the first ten still printed.
- Server RPCs whose sizes SA-MP trusts, a GameText style past 6 and a texture
  dictionary's struct size (MiTurboFix's fixes).
- SA-MP's removed-building table overflowing past 1000 entries (samp.dll).
- A ped's animations being wiped while its tasks still point at them (the
  crash after a reconnect or a skin change mid-fall).
- More than 1000 visible entities overwriting the renderer's counts. It
  stands down when a limit adjuster raises that limit.
- The entity-reference pool running out at 3000, which leaves dangling
  pointers. It grows in blocks of 4096.
- The hospital restart point buffer overflowing.

Game bugs:
- Tail light damage reads the wrong light.
- Wet-road reflections sit at the wrong depth.
- The player loses its real-time shadow slot under SA-MP.
- Shadows skip entities flagged not to receive them.
- Headlight beams leave backface culling off.
- Boat and aircraft explosions last 3 seconds too long.
- 16:9 and other non-4:3 video modes are filtered out.
- A live driver gets dragged out on entry.
- The chase camera lags above 50 fps.
- Exterior occluders hide objects a server maps behind them.
- Camera moves, shakes and zooms and the weapon spread follow a clock the
  server can set; they read a smooth one instead.
- The game's fog stops reaching the screen once SA-MP has set the device's
  fog: it is sent again twice a frame (fogrestore.asi's fix). With
  fogrestore.asi installed it stands aside, so the ASI can go; ENB and
  SkyGFX stand it down too.

Speed:
- SA-MP's text (chat, nametags, labels) is laid out once and replayed, not
  laid out again every frame: `lib/fixes/text_cache.dll`.
- SA-MP's and GTA's frame caps come off while the in-game Frame Limiter is
  off.

Also: every animation block is preloaded and pinned, SA-MP's F1 help dialog
is disabled, the smoking animation is fixed, and crashes.asi's visual and
display switches are options (windowed mode, running in the background,
no heat haze, no clouds and the rest).

The full list, with each patch's disassembly and reasoning, is in
[docs/MEMORY_FIXES.md](docs/MEMORY_FIXES.md);
[docs/MTA_CRASHFIX_UPDATES.md](docs/MTA_CRASHFIX_UPDATES.md) compares the
crash fixes with MTA's current ones.

## Install

Requires GTA San Andreas 1.0 US, SA-MP 0.3.7 (R1-R5) or 0.3.DL, SAMPFUNCS
and MoonLoader 0.26 or newer. Copy the files into your `moonloader` folder,
keeping the paths:

```
moonloader/fixes.lua
moonloader/lib/fixes/text_cache.dll
moonloader/lib/fixes/crashfix.dll
moonloader/lib/signature_scanner/        (the whole folder)
```

`lib/fixes/native` (the DLLs' source) and `lib/fixes/test` are not needed in
game.

## Use

It works on its own; the log is `moonloader/moonloader.log` (lines start with
`[fixes]`). Every patch and hook logs a line as it goes in, and a crash fix
that averted a crash says so.

- `/fixes` lists the options and what each fix is doing.
- `/fixes patches` counts the patches and hooks: applied, owned by another
  mod, refused, off.
- `/fixes <option> on|off` switches an option at once.

| option | default | what it does |
|---|---|---|
| `crashfix` | on | the crash-fix hooks |
| `mtapatches` | on | MTA's multiplayer patches |
| `frameclone` | off | crash fix 19 (RwFrameCloneHierarchy); breaks SkyGFX |
| `oldcrashfixes` | off | crash fixes 12 and 17, which MTA no longer installs |
| `volshadows` | off | volumetric vehicle shadows (off: cheap blob shadows) |
| `textcache` | on | the text cache |
| `framecaps` | on | the frame caps off while the Frame Limiter is off |
| `floatclock` | on | the smooth clock for the camera and the weapon spread |
| `dialoglines` | on | cut list dialog lines past 128 characters |
| `occluders` | on | no exterior occlusion |
| `plates` | off | custom plates on bikes, boats, planes and trailers |
| `windowed` | on | Alt+Enter switches to windowed mode |
| `background` | on | the game keeps running in the background and the pause menu |
| `noheathaze`, `noclouds` | on | no heat haze, no clouds |
| `noflashes`, `nosound`, `novehiclelighting`, `nospecular`, `notargetblip`, `nointeriorreflections`, `nopostfx` | off | each takes that effect away |

Options are saved in `moonloader/config/fixes.json`.

## Build the DLLs

`lib/fixes/native/build.sh` builds `lib/fixes/text_cache.dll` and
`lib/fixes/crashfix.dll` with mingw (i686-w64-mingw32). Under Wine,
`lib/fixes/test/text_cache_dll_test.sh` tests the text cache against the real
d3dx9_25.dll, and `lib/fixes/test/crashfix_dll_test.sh` runs the crash fixes
against gta_sa.exe's own code. `luajit lib/fixes/test/fixes_test.lua` tests
the script.

## Credits

- The text cache (`lib/fixes/native/text_cache.cpp`) and the entity-reference
  pool fix are based on TextCacheFix and RefPoolFix by **Софтик Инквизитор**
  ([blast.hk](https://www.blast.hk/)), redistributed and modified with his
  permission.
- The crash-fix hooks and the multiplayer patches are
  [MTA:SA](https://github.com/multitheftauto/mtasa-blue)'s (GPL-3.0), by way
  of gtasa_crashfix (crashes.asi) by **Whitetigerswt** (GPL-3.0).
- The RPC, GameText and texture-dictionary checks are from
  [MiTurboFix](https://github.com/povargek/MiTurboFix) (public domain).
- The exception filter follows the anticrashers by **Export** and
  **DarkP1xel**.

## License

GPL-3.0, see [LICENSE](LICENSE).
