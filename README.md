# fixes

Crash fixes, bug fixes and speed-ups for GTA San Andreas 1.0 US under SA-MP,
as one MoonLoader script. Each patch checks the bytes it expects first, so
nothing is written blind, and it stands aside when another mod already
handles the same thing.

## What it fixes

Crashes:
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

Speed:
- SA-MP's text (chat, nametags, labels) is laid out once and replayed, not
  laid out again every frame: `lib/fixes/text_cache.dll`.

Also: every animation block is preloaded and pinned, SA-MP's F1 help dialog
is disabled, and the smoking animation is fixed.

The full list, with each patch's disassembly and reasoning, is in
[docs/MEMORY_FIXES.md](docs/MEMORY_FIXES.md).

## Install

Requires GTA San Andreas 1.0 US, SA-MP 0.3.7 (R1-R5) or 0.3.DL, SAMPFUNCS
and MoonLoader 0.26 or newer. Copy the files into your `moonloader` folder,
keeping the paths:

```
moonloader/fixes.lua
moonloader/lib/fixes/text_cache.dll
moonloader/lib/signature_scanner/        (the whole folder)
```

`lib/fixes/native` (the DLL's source) and `lib/fixes/test` are not needed in
game.

## Use

It works on its own; the log is `moonloader/moonloader.log` (lines start with
`[fixes]`).

- `/fixes` lists the options and what each fix is doing.
- `/fixes <option> on|off` switches an option at once:
  - `plates`: custom plates on bikes, boats, planes and trailers. Off by
    default.
  - `textcache`: the text cache. On by default.

Options are saved in `moonloader/config/fixes.json`.

## Build the DLL

`lib/fixes/native/build.sh` builds `lib/fixes/text_cache.dll` with
i686-w64-mingw32-g++. `lib/fixes/test/text_cache_dll_test.sh` tests it under
Wine against the real d3dx9_25.dll.

## Credits

The text cache (`lib/fixes/native/text_cache.cpp`) and the entity-reference
pool fix are based on TextCacheFix and RefPoolFix by **Софтик Инквизитор**
([blast.hk](https://www.blast.hk/)), redistributed and modified with his
permission.

## License

GPL-3.0, see [LICENSE](LICENSE).
