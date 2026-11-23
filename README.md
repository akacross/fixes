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
- Spectating a ped that has no task: the camera crashed every frame (SA-MP
  swallowed it as "Warning(s007) ... at 0x523E6B") and was left half
  updated.
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
- Dragging the scrollbar of a SA-MP list dialog that fits on one page
  divided by zero and froze the game (samp.dll): the divisors are tested.

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
- Cars lose their glass and the tinted covers of their lights, and are lit
  differently, as the camera moves. A car that found the 200-entry alpha
  entity list full was drawn at once with a stricter alpha test, which throws
  the glass away. That list grows too (`glasslist`).
- A window, or the door behind it, vanishes depending on the angle. A car's
  see-through parts are now drawn in two passes: the solid parts first, then
  the glass on top without writing depth (`glassdepth`). Each part is also
  sorted by its own distance from the camera (`glasssort`).
- A car further than 20 m away skips the windows and doors on its far side,
  so you could see through it. They are always drawn now (`farsideparts`).
- The sky's sides show at a wide FOV: the sky widens with the FOV
  (`widesky`, San Andreas Online's).
- Mirrors show nothing with anti-aliasing on: their buffer is made without
  multisampling (`mirroraa`, MTA's).
- Headlights and other world lights don't light a car's paint: the paint's
  own light moves out of their slot (`lightslot`, MTA's; SilentPatch does the
  same and fixes leaves it to SilentPatch).
- The sky shows lines across its gradient (colour bands): it is drawn
  dithered, so the steps blend.
- Switching the Frame Limiter on could crash: the game reset the graphics
  device to turn vsync on, and a failed reset crashed it. The device is no
  longer reset for that; the Frame Limiter still caps the frame rate.
- An unfocused window pauses the game: with `unfocused` on it keeps running
  behind another window, and the cursor stays where you put it.
- Textures are filtered anisotropically, 16x or the most the device offers.
  Up close they magnify linearly: anisotropic magnification is a feature many
  Direct3D 9 drivers lack, and it could leave close-up textures such as wheels
  white.
- A big server map fills the game's pools: the stock 2500 dummies (4000 under
  SA-MP R1) and 13000 buildings run out and the game crashes at 0x538103.
  `poollimits.asi`, which fixes copies beside `gta_sa.exe`, raises 23 pool
  sizes before the game builds them (90000 buildings, 25000 dummies, 101500
  collisions...). It raises them again after SA-MP writes its own, and never
  lowers a bigger one. It also raises the list of entities to draw from 1000
  to 7500, so objects keep drawing on object-heavy servers
  (SAMPGraphicRestore.asi's and flickr.asi's patch: neither is needed for
  it). The game's code has to be patched before any script runs, so this one
  part is an ASI. It takes effect from the next launch.

From SA-MixSets (Junior_Djjr), the fixes fixes did not have and SA-MP allows:
- A car's shadow and its headlight glow on the ground move in steps: they
  follow the car smoothly (`smoothshadows`).
- The HUD fist and weapon icons are stretched, 58 wide against 47 high: they
  are drawn square (`weaponicon`, off by default).
- The mouse wheel cancels a reload (`scrollreload`, off by default: it
  changes weapon switching).
- A truck with double rear wheels lays one skidmark and one dust trail per
  side: each rear wheel gets its pair's too (`doubleskid`).
- Changing the radio breaks a boat driver's pose (`boatradio`).
- A boat's wake is as bright at night as by day: it dims with the water's
  light (`boatfoam`).

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
- Timers that count whole milliseconds a frame: a knock-down, a burning car,
  plane, bike or boat, the car alarm, a choke, the climb, fight and stance
  timers and an explosion's fuel last as long at any fps (they ran long at
  144 fps and never ended above 1000).
- Cars are quicker at high fps and donuts turn too far (12% quicker to
  20 m/s at 850 fps, donuts turning twice as far): the gearbox's engine
  inertia is taken per 30 fps frame, and a burnout's whole turn is scaled, not
  only its steering. The tyre's skid test was measured and is the same at any
  fps, so it is left alone.
- A ridden bike at rest no longer rocks at high fps.
- From HighFpsFixes (sonochiwa), what fixes did not have:
  - Timers that drop a frame's fraction of a millisecond keep it: the target
    lock and other ped tasks, police, plane and car-bomb timers, drowning
    (no damage above 150 fps), the idle camera, script and on-screen
    timers, skill and stunt counters and every HUD timer.
  - Vehicles: steering and plane controls ease as at 30 fps; an upshift kicks
    the nose once, not every eleventh frame; aircraft turn and settle, the
    suspension damps and the body leans as at 30 fps; forklift forks, the
    dozer blade and ramps move (they stopped above 500 fps); sinking, a
    damaged plane's controls, hydraulics, boat waves, wheels spinning in the
    air, bike and BMX leans and wheels settling as at 30 fps.
  - The world: fire spread, explosions (cut short at high fps), lightning,
    the engine note, ambient gunfire, the jetpack flame, swim splashes and
    bloody footprints as at 30 fps; SA-MP's slowly moving objects turn and
    arrive.
  - The on-foot camera turns and the aim camera holds still and zooms as at
    30 fps; HUD flashes keep their rate; a wheel notch zooms the pause map.
  - A spray, flame or extinguisher shows its particles, uses ammo and hurts
    30 times a second; the chainsaw strikes 15 times a second; tear gas as
    at 30 fps.
  - A car radio no longer holds the game at 30 fps.

If Car Speed FPS Fix (CLEO) is running, fixes leaves it and its file alone,
turns its own coasting fix off so nothing is applied twice, and asks in chat
(`/fixes cleo`, below). Swim FPS Fix.asi and gxt_crash_fix.asi, when
installed, are left to do their part and named in chat; remove them if you
want fixes' own. With HighFpsFixes.asi installed, fixes reads its INI and
stands down every fix of its own that does what one of its switches turned
on does, so nothing is applied twice.

Speed:
- SA-MP's text (chat, nametags, labels) is laid out once and replayed, not
  laid out again every frame: `lib/fixes/text_cache.dll`. It works with
  Microsoft's d3dx9_25.dll and with Wine's own (a prefix without Microsoft's).
- SA-MP's and GTA's frame caps come off while the in-game Frame Limiter is
  off, over 0.3.DL's own 2 ms wait too (given back with `framecaps` off).
- A parked car that is standing still skips the game's collision step, which
  changed nothing for it but still cost about 6 microseconds a car every
  frame (0.17 ms a frame for the 28 parked cars around Los Santos).

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
moonloader/lib/fixes/poollimits.asi        (fixes copies it beside gta_sa.exe)
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
- Every crash-type exception goes to `moonloader/logs/faults.log` as it
  happens, with its registers, the return addresses on its stack and, inside
  Lua, the Lua frames (`faultlog`, on by default). SA-MP swallows crashes in
  the game's update, which can freeze the world with nothing logged; this
  only watches, from a thread of its own.
- `/fixes streamtime on` times model loading on the game's thread into
  `moonloader/logs/stream.log`: a line per file converted (type, id, ms, size)
  and per `LoadAllRequestedModels` drain with who called it. `/fixes
  streamtime report` prints a summary, and with `drift` on each drift line
  shows the ms a frame. A diagnostic for
  [docs/ASYNC_STREAMING_PLAN.md](docs/ASYNC_STREAMING_PLAN.md), not a fix.
- `/fixes drawcount on` counts the frame's draw calls by what is drawn
  (roads, buildings, vehicles, peds and the rest) and writes a world detail
  to `moonloader/logs/world.log`: peds' and vehicles' update and collision by
  distance, movement and use. A diagnostic, not a fix.
- `/fixes hook <key> off` takes one hook out for this session, to test it
  alone; `on` or a restart puts it back.
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
| `turnfps`, `pedfps`, `vehiclefps`, `effectsfps`, `pushfps` | on | MTA's frame-rate fixes (FramerateVigilante's and HighFpsFixes' under them too) |
| `sirenfps` | on | a horn tap under 150 ms switches the siren at any fps |
| `chutefps` | on | the parachute steers as at 30 fps |
| `timerfps` | on | HUD timers and flashes, skills, stunts, ambient sounds and script timers in real time (HighFpsFixes) |
| `camerafps` | on | the on-foot and aim cameras as at 30 fps (HighFpsFixes) |
| `weaponfps` | on | spray, flame and extinguisher particles, ammo and damage, the chainsaw and tear gas as at 30 fps (HighFpsFixes) |
| `pedmatrix` | on | a ped with broken bone matrices is not drawn stretched across the screen |
| `glasslist` | on | a car's see-through parts drawn in order past 20, and the alpha entity list grows past 200 |
| `glassdepth` | on | a car's glass drawn after its solid parts without writing depth |
| `glasssort` | on | a car's see-through parts sorted by their own distance from the camera |
| `farsideparts` | on | a car's far-side parts drawn at any distance |
| `parkedcol` | on | a parked car standing still skips the collision step |
| `skygrad` | on | the sky drawn dithered, without colour bands |
| `widesky` | on | the sky widens with the FOV |
| `mirroraa` | on | mirrors work with anti-aliasing on |
| `lightslot` | on | headlights and other world lights reach a car's paint |
| `novsyncreset` | on | the Frame Limiter and loading screens do not reset the graphics device for vsync |
| `anisotropic` | on | anisotropic texture filtering, 16x or the device's most |
| `poollimits` | on | bigger pools through poollimits.asi beside gta_sa.exe, from the next launch |
| `widemodes` | on | 16:9 and the other resolutions the game hides are listed |
| `unfocused` | off | the game keeps running while another window has focus |
| `smoothshadows`, `doubleskid`, `boatradio`, `boatfoam` | on | SA-MixSets' fixes above |
| `weaponicon` | off | the HUD weapon icon drawn square (SA-MixSets) |
| `scrollreload` | off | the mouse wheel does not cancel a reload (changes weapon switching) |
| `faultlog` | on | the fault log above |
| `streamtime` | off | the model loading timer above |
| `drawcount` | off | the draw-call counter above |
| `drift` | off | the frame-rate log above |
| `deathtrace` | off | the death log above |
| `noheathaze`, `noclouds` | on | no heat haze, no clouds |
| `noflashes`, `nosound`, `novehiclelighting`, `nospecular`, `notargetblip`, `nointeriorreflections`, `nopostfx` | off | each takes that effect away |

Options are saved in `moonloader/config/fixes.json`.

## Build the DLLs

`lib/fixes/native/build.sh` builds `lib/fixes/text_cache.dll`,
`lib/fixes/crashfix.dll` and `lib/fixes/poollimits.asi` with mingw
(i686-w64-mingw32). Under Wine,
`lib/fixes/test/text_cache_dll_test.sh` tests the text cache against the real
d3dx9_25.dll, and `lib/fixes/test/crashfix_dll_test.sh` runs the crash fixes
against gta_sa.exe's own code, and `position_guard_test.sh` and
`fps_fixes_test.sh` run the position guard and the frame-rate code, and
`poollimits_test.sh` loads the ASI in a copy of gta_sa.exe.
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
- The timer, vehicle, world, camera, weapon and HUD frame-rate fixes under
  `timerfps`, `camerafps`, `weaponfps` and the other frame-rate options are
  from [HighFpsFixes](https://github.com/sonochiwa/sa-high-fps-fixes) by
  **sonochiwa** (MIT).
- The shadow, weapon icon, reload, skidmark, boat radio and wake fixes are
  from [SA-MixSets](https://github.com/CanerKaraca23/SA-MixSets) by
  **Junior_Djjr** (MIT).
- The dithered sky follows skygrad.asi by **FraGae**; the code and shaders
  are fixes' own.
- The shadow-mesh and IK-chain crash fixes, the glass list and the wide sky
  are from [San Andreas Online](https://github.com/Dryxio/san-andreas-online)
  (GPL-3.0), an MTA fork.
- The mirror buffer and the car paint light slot follow
  [MTA:SA](https://github.com/multitheftauto/mtasa-blue)'s.

## License

GPL-3.0, see [LICENSE](LICENSE).
