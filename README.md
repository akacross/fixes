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
- Server teleports (player or vehicle) to a NaN position or far off the
  world: dropped before SA-MP moves anything.
- A NaN, infinite or off-map position from anywhere (a server, another
  player's sync, a script) sending you to an endless white loading screen:
  the game itself puts it back before a ped, car or object is placed, and
  your own ped and car stay where they were. No SA-MP version or library
  involved. Only x and y are held to the map (20000); height may go to
  1000000, so objects a server keeps underground or in the sky are left
  alone.
- A camera gone NaN (after a fall or a teleport): the world stopped drawing
  and the screen flashed until a respawn. It is set up again behind you on
  the next frame.
- A null GXT string crashing `CMessages::GetStringLength`
  (gxt_crash_fix's fix).
- SA-MP's removed-building table overflowing past 1000 entries (samp.dll).
- A ped's animations being wiped while its tasks still point at them (the
  crash after a reconnect or a skin change mid-fall).
- More than 1000 visible entities overwriting the renderer's counts. It
  stands down when a limit adjuster raises that limit.
- The entity-reference pool running out at 3000, which leaves dangling
  pointers. It grows in blocks of 4096.
- The hospital restart point buffer overflowing.
- A vehicle or object shadow mesh over 2048 vertices (with volumetric
  shadows on) and an IK chain through a bone the ped's skin lacks (San
  Andreas Online's fixes).

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
- A server's car vanishes after you get out of it, its upgrade parts left
  behind (a script marking it no longer needed): the car is kept.
- A ped with broken bone data draws its triangles across the screen: the bad
  bone shrinks to nothing, or the ped is not drawn (MTA's checks).
- A car's glass and other see-through parts past 20 draw unsorted: the list
  grows (San Andreas Online's fix).
- The sky shows lines across its gradient (colour bands): it is drawn
  dithered, so the steps blend.
- Switching the Frame Limiter on could crash: the game reset the graphics
  device to turn vsync on, and a failed reset crashed it. The device is no
  longer reset for that; the Frame Limiter still caps the frame rate.

Frame rate (each as at 30 fps, the rate the game was tuned for):
- Off the throttle a car stops dead at high fps and rolls at low: the
  coasting slow-down is the same every second at any fps (Car Speed FPS Fix).
- Swimming is slow at high fps: it is as fast at any fps (Swim FPS Fix).
- Buoyancy as Swim FPS Fix scales it (less lift above 30 fps, boats too).
- MTA's frame-rate fixes: a spin slows as fast at any fps; climbing over
  objects no longer kills; moving while aiming; boats, helicopter takeoff and
  parked cars settling; particles, rain, splashes and the water cannon at 30
  a second; camera shake, fog and falling glass by the timestep; a ped
  walking into a parked car no longer shoves it away.
- FramerateVigilante's: doors, bonnets and boots swing and settle as at 30
  fps; a burnout no longer spins faster; the dive and holding at the surface
  when swimming; a quick horn tap switches the siren at any fps.
- The parachute: turning, rolling and speeding up as at 30 fps instead of
  spinning at high fps (its script's per-frame values follow the timestep).

If Car Speed FPS Fix (CLEO) is running, fixes leaves it and its file alone,
turns its own coasting fix off so nothing is applied twice, and asks in chat
(`/fixes cleo`, below). Swim FPS Fix.asi and gxt_crash_fix.asi, when
installed, are left to do their part and named in chat; remove them if you
want fixes' own.

Speed:
- SA-MP's text (chat, nametags, labels) is laid out once and replayed, not
  laid out again every frame: `lib/fixes/text_cache.dll`. It works with
  Microsoft's d3dx9_25.dll and with Wine's own (a prefix without Microsoft's).
- SA-MP's and GTA's frame caps come off while the in-game Frame Limiter is
  off, over 0.3.DL's own 2 ms wait too (given back with `framecaps` off).

Also: every animation block is preloaded and pinned, SA-MP's F1 help dialog
is disabled, the smoking animation is fixed, and crashes.asi's visual and
display switches are options (windowed mode, running in the background,
no heat haze, no clouds and the rest).

The full list, with each patch's disassembly and reasoning, is in
[docs/MEMORY_FIXES.md](docs/MEMORY_FIXES.md);
[docs/MTA_CRASHFIX_UPDATES.md](docs/MTA_CRASHFIX_UPDATES.md) compares the
crash fixes with MTA's current ones,
[docs/MTA_FRAMERATE_FIXES.md](docs/MTA_FRAMERATE_FIXES.md) the frame-rate
fixes, and [docs/SAO_REVIEW.md](docs/SAO_REVIEW.md) what San Andreas Online
adds.

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

It works on its own; the log is `moonloader/logs/fixes.log`, written anew
at every launch. Every patch and hook logs a line as it goes in, and a crash fix
that averted a crash says so.

- `/fixes` lists the options and what each fix is doing.
- `/fixes patches` counts the patches and hooks: applied, owned by another
  mod, refused, off.
- `/fixes <option> on|off` switches an option at once.
- `/fixes drift on` writes a line every 30 s to `moonloader/logs/drift.log`:
  the frame rate and frame times, and every count that could grow over a
  session (the text cache, the game's pools, memory). `/fixes drift report`
  compares now with the start, for a frame rate that falls the longer you
  play.
- `/fixes deathtrace on` writes the last 3 s before each death to
  `moonloader/logs/death.log`: health, vehicle, what the game blamed, what
  came in from the network and what SA-MP reported.
- `/fixes cleo` when Car Speed FPS Fix (CLEO) runs: `stop` stops it for this
  session and uses fixes' own, `keep` keeps it and turns `wheelfps` off,
  `remove` explains first, and only `/fixes cleo remove yes` within 30
  seconds renames its file to `.cs.disabled` (nothing is deleted).

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
| `fogrestore` | on | the game's fog sent again after SA-MP leaves it stale |
| `focusmenu` | off | the pause menu opens when the window gets focus back, as in single-player (SA-MP turns this off) |
| `positionguard` | on | NaN, inf and off-map positions put back in the game itself |
| `rpcpositions` | on | drop server teleports to NaN or off the world (off: only the position guard) |
| `keepcars` | on | a car marked no longer needed is kept, not deleted with its parts left behind |
| `wheelfps` | on | the coasting slow-down off the throttle is the same at any fps |
| `swimfps` | on | swimming is as fast at any fps |
| `floatfps` | on | buoyancy scaled as Swim FPS Fix does (boats too) |
| `turnfps`, `pedfps`, `vehiclefps`, `effectsfps`, `pushfps` | on | MTA's frame-rate fixes (FramerateVigilante's doors and burnout under `vehiclefps`) |
| `sirenfps` | on | a horn tap under 150 ms switches the siren at any fps |
| `chutefps` | on | the parachute steers as at 30 fps |
| `pedmatrix` | on | a ped with broken bone matrices is not drawn stretched across the screen |
| `glasslist` | on | a car's see-through parts drawn in order past 20 |
| `skygrad` | on | the sky drawn dithered, without colour bands |
| `novsyncreset` | on | the Frame Limiter and loading screens do not reset the graphics device for vsync |
| `drift` | off | the frame-rate log above |
| `deathtrace` | off | the death log above |
| `noheathaze`, `noclouds` | on | no heat haze, no clouds |
| `noflashes`, `nosound`, `novehiclelighting`, `nospecular`, `notargetblip`, `nointeriorreflections`, `nopostfx` | off | each takes that effect away |

Options are saved in `moonloader/config/fixes.json`.

## Build the DLLs

`lib/fixes/native/build.sh` builds `lib/fixes/text_cache.dll` and
`lib/fixes/crashfix.dll` with mingw (i686-w64-mingw32). Under Wine,
`lib/fixes/test/text_cache_dll_test.sh` tests the text cache against the real
d3dx9_25.dll, and `lib/fixes/test/crashfix_dll_test.sh` runs the crash fixes
against gta_sa.exe's own code, and `position_guard_test.sh` and
`fps_fixes_test.sh` run the position guard and the frame-rate code.
`luajit lib/fixes/test/fixes_test.lua` tests the script.

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
- The coasting fix follows Car Speed FPS Fix by **Junior_Djjr**; the swim and
  buoyancy fixes follow Swim FPS Fix.asi; the GXT fix follows gxt_crash_fix.
- The door, burnout, swim and siren frame-rate fixes are from
  FramerateVigilante by [Junior_Djjr](https://github.com/JuniorDjjr) (MIT).
- The dithered sky follows skygrad.asi by **FraGae**; the code and shaders
  are fixes' own.
- The shadow-mesh and IK-chain crash fixes and the glass list are from
  [San Andreas Online](https://github.com/Dryxio/san-andreas-online)
  (GPL-3.0), an MTA fork.

## License

GPL-3.0, see [LICENSE](LICENSE).
