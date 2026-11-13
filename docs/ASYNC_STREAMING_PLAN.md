# Async model streaming plan

Goal: fewer hitches when SA-MP streams models in (teleports, driving fast,
object-heavy maps), without moving RenderWare calls off the main thread.

## What the game already does

GTA SA already uses the "memory stream" pattern:

- `CdStreamThread` (0x406560) reads IMG sectors into a buffer on its own thread.
- `CStreaming::ConvertBufferToObject` (0x40C6B0) runs on the main thread and
  opens that buffer with `RwStreamOpen(rwSTREAMMEMORY, ...)`,
  then parses the DFF / TXD / COL / IFP.

So a custom `rwSTREAMCUSTOM` reader gains nothing: the read is already async.
The hitches come from main-thread work:

1. Parsing and creating D3D textures in `ConvertBufferToObject`.
2. Big files (`ms_bLoadingBigModel`): the second half is read and converted in
   `CStreaming::FinishLoadingLargeFile` (0x408CB0), a stall.
3. `CStreaming::LoadAllRequestedModels` (0x40EA10): a synchronous drain, called
   on teleports, interiors, spawns and by SA-MP itself.

The four addresses above match gta-reversed (`Streaming.cpp`,
`CdStreamInfo.cpp`) and this exe's code (checked 2026-11-13). In this exe
0x40EA10 is a jump to 0x40A45E, which holds the function's first instruction
(`mov al, [0x965538]`) and jumps back to 0x40EA15: a hook at the entry replaces
that 5-byte jump. Verify any further address the same way before patching.

## Steps

1. **Measure.** Hook `ConvertBufferToObject`, `FinishLoadingLargeFile` and
   `LoadAllRequestedModels` in `crashfix.c` (or a new `lib/fixes/native/stream.c`)
   with QPC timings per model id and type. Log to `logs\stream.log` and add
   `stream ms/frame`, worst model, and count to the perf line. Drive through
   LS/SF, teleport, enter interiors.
   Done: option `streamtime` (off by default), `lib/fixes/native/stream.c`,
   `MEMORY_FIXES.md` 53. Numbers not collected yet.
2. **Pick from the numbers:**
   - Conversion cost dominates: cap conversions per frame (time budget in
     `CStreaming::Update`), keeping the rest queued as `LOADSTATE_READ`.
   - Big-file stalls dominate: raise the streaming buffer
     (`ms_streamingBufferSize`) so fewer models take the two-part path.
   - `LoadAllRequestedModels` dominates: find SA-MP's callers (samp.dll R1/R5
     offsets) and see which can be dropped or replaced by plain requests.
3. **Optional, risky:** decode TXD texture data on a worker into
   system memory, then only `CreateTexture` + copy on the main thread. Needs a
   prepass over the TXD chunks; DXVK device calls stay on the main thread.
4. **Option + tests.** A `fixes` option (default off until measured), a test in
   `lib/fixes/test/`, entry in `MEMORY_FIXES.md`.

## Risks

- The `txdconvert` crashfix hook sits at 0x40C8CE inside
  `ConvertBufferToObject`; a new hook must not overlap it.
- Deferring conversions can leave a model `LOADSTATE_READ` while scripts or
  SA-MP expect it loaded after `LoadAllRequestedModels`; never defer inside it.
- Streaming buffer size changes interact with stream.ini and other mods that
  write `ms_streamingBufferSize`.
