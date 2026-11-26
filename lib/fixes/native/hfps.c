/* HighFpsFixes' frame-rate fixes that fixes lacked (sonochiwa's
 * sa-high-fps-fixes, MIT), for gta_sa.exe 1.0 US; included by crashfix.c
 * after framerate.c, so its SCALED, DUE and cf_k30 are here, each switched by
 * its own fixes option. Where they differ from HighFpsFixes':
 *   - a truncated time keeps its fraction per call site as HighFpsFixes'
 *     does, but a site fixes already carries (the burns, the fall, the
 *     choke, the alarm, the explosion fuel) stays on cf_ms_step.
 *   - not taken: the gang war timers, the burglary and mission script
 *     literals, the stunt jump camera, the cop's car exit and the SWAT ropes
 *     (single player); the wheel slip rate (the skid test is fps-neutral in a
 *     closed loop: docs/MEMORY_FIXES.md 41); the rifle's aim-walk step
 *     (samp.dll gives ControlGunMove its own timestep); the on-rails wheel
 *     spin (SA-MP has no simple cars); the buoyancy threshold and the climb's
 *     clamp (floatfps and climb1-2 take them); the bike lean target, the
 *     abandoned bike's substeps and the takeoff pitch (experiments there);
 *     AddParticle's gate (effectsfps gates the emitters at their callers);
 *     the fps limits.
 */

/* ------------------------------------------------------------------------ */
/* The rows: key, option, what, site, bytes, stock                          */
/* ------------------------------------------------------------------------ */
void cf_ftol_carry(void);
void cf_sink_step(void);
void cf_hyd_raise(void);
void cf_hyd_lower(void);
void cf_wave_rise(void);
void cf_decay95(void);
void cf_sub10(void);
void cf_add05(void);
void cf_sub05(void);
void cf_settle(void);
extern int32_t cf_loop_frames;
void cf_pickup_step(void);
void cf_fat_add(void);
void cf_add10(void);
void cf_count30(void);
void cf_revs_up(void);
void cf_revs_down(void);
void cf_ambience(void);
void cf_map_wheel(void);
void cf_real_timestep(void);
void cf_chainsaw(void);
int32_t __cdecl cf_roll_range(int32_t low, int32_t high);
int32_t __cdecl cf_roll_fire(void);
int32_t __cdecl cf_roll_byte(void);
int32_t __cdecl cf_roll_word(void);
void *__attribute__((thiscall)) cf_fx_gate(void *manager, const char *name, const void *at, void *matrix, uint8_t bounds);
uint8_t __cdecl cf_add_shot(void *creator, int32_t type, float ox, float oy, float oz, float tx, float ty, float tz);
uint8_t __cdecl cf_creeping_fire(float x, float y, float z, int32_t generations, uint8_t spread, uint8_t script, float dz);
uint8_t __attribute__((thiscall)) cf_extinguish(void *manager, float x, float y, float z, float radius, float strength);
uint8_t __attribute__((thiscall)) cf_beat_limit(void *audio);

#define HFPS(FJ, FP) \
    FP(drown, "pedfps", KIND_CALL, "CPlayerPed::HandlePlayerBreath: drowning's damage keeps its fraction (none above 150 fps)", \
        0x60A939, 5, "\xE8\x02\x72\x21\x00", cf_ftol_carry) \
    FJ(drunksteer, "pedfps", "CPad::Update: a drunk's steering delay shifts per 30 fps step", 0x541D2D, 8, \
        "\x8D\x43\x72\xB9\x09\x00\x00\x00") \
    FP(pickup1, "pedfps", KIND_CALL, "CTaskSimpleHoldEntity::ProcessPed: the step onto a box per 30 fps step", 0x693F4F, 6, \
        "\xD8\x35\x5C\xCB\xB7\x00", cf_pickup_step) \
    FP(pickup2, "pedfps", KIND_CALL, "CTaskSimpleHoldEntity::ProcessPed: the step onto a box per 30 fps step", 0x693F80, 6, \
        "\xD8\x35\x5C\xCB\xB7\x00", cf_pickup_step) \
    FP(fatcount, "timerfps", KIND_CALL, "CStats::UpdateFatAndMuscleStats: exercise burns fat with the remainder kept", \
        0x55C5C8, 29, "\x8B\xD0\x0F\xAF\x54\x24\x08\xB8\xCD\xCC\xCC\xCC\xF7\xE2\xA1\xFC\x94\xB7\x00\xC1\xEA\x03\x03\xC2\xA3" \
        "\xFC\x94\xB7\x00", cf_fat_add) \
    FJ(footprints, "effectsfps", "CPed::PlayFootSteps: bloody footprints fade per 30 fps step", 0x5E5877, 9, \
        "\x49\x85\xC9\x89\x8E\x50\x07\x00\x00") \
    FP(jetflame1, "effectsfps", KIND_CALL, "CTaskSimpleJetPack::DoJetPackEffect: the flame ramps per 30 fps step", 0x67B99C, 6, \
        "\xD8\x05\x1C\x8B\x85\x00", cf_add10) \
    FP(jetflame2, "effectsfps", KIND_CALL, "CTaskSimpleJetPack::DoJetPackEffect: the flame ramps per 30 fps step", 0x67B9C8, 6, \
        "\xD8\x25\x1C\x8B\x85\x00", cf_sub10) \
    FP(swimsplash1, "effectsfps", KIND_CALL, "CTaskSimpleSwim::ProcessEffects: a sprinting swimmer's splash per 30 fps step", \
        0x68AEBA, 5, "\xE8\x21\xED\xE1\xFF", cf_fx_gate) \
    FP(swimsplash2, "effectsfps", KIND_CALL, "CTaskSimpleSwim::ProcessEffects: a sprinting swimmer's splash per 30 fps step", \
        0x68AF15, 5, "\xE8\xC6\xEC\xE1\xFF", cf_fx_gate) \
    FP(swimsplash3, "effectsfps", KIND_CALL, "CTaskSimpleSwim::ProcessEffects: a sprinting swimmer's splash per 30 fps step", \
        0x68AF66, 5, "\xE8\x75\xEC\xE1\xFF", cf_fx_gate) \
    FP(swimsplash4, "effectsfps", KIND_CALL, "CTaskSimpleSwim::ProcessEffects: a sprinting swimmer's splash per 30 fps step", \
        0x68AFB3, 5, "\xE8\x28\xEC\xE1\xFF", cf_fx_gate) \
    FJ(firecar, "effectsfps", "CFire::ProcessFire: setting cars alight rolled per 30 fps step", 0x53A8CA, 8, \
        "\xA8\x1F\x0F\x85\x33\x01\x00\x00") \
    FJ(firespread, "effectsfps", "CFire::ProcessFire: a new fire nearby rolled per 30 fps step", 0x53AABB, 8, \
        "\xA8\x7F\x0F\x85\x5F\x01\x00\x00") \
    FJ(firemerge, "effectsfps", "CFire::ProcessFire: merging with a weak fire rolled per 30 fps step", 0x53AC46, 8, \
        "\xA8\x0F\x0F\x85\x24\x01\x00\x00") \
    FJ(blastframes, "effectsfps", "CExplosion::Update: its frame count stops at 255, not wraps (cut at 0.85 s at 300 fps)", \
        0x7379EE, 5, "\xFE\x46\xF8\xEB\x04") \
    FP(blastroll, "effectsfps", KIND_CALL, "CExplosion::Update: an aircraft's second blast rolled per 30 fps step", 0x7378D7, 5, \
        "\xE8\xA4\xF8\xCC\xFF", cf_roll_range) \
    FP(blastfire, "effectsfps", KIND_CALL, "CExplosion::Update: setting the victim alight rolled per 30 fps step", 0x737717, 5, \
        "\xE8\x02\xA4\x0E\x00", cf_roll_fire) \
    FP(lightning1, "effectsfps", KIND_CALL, "CWeather::Update: a lightning burst ends, rolled per 30 fps step", 0x72B9DB, 5, \
        "\xE8\x3E\x61\x0F\x00", cf_roll_byte) \
    FP(lightning2, "effectsfps", KIND_CALL, "CWeather::Update: a lightning burst starts, rolled per 30 fps step", 0x72BA88, 5, \
        "\xE8\x91\x60\x0F\x00", cf_roll_word) \
    FP(lightning3, "effectsfps", KIND_CALL, "CWeather::Update: a burst measured in 30 fps steps (the thunder's delay)", \
        0x72B9EA, 5, "\xA1\x4C\xCB\xB7\x00", cf_count30) \
    FP(lightning4, "effectsfps", KIND_CALL, "CWeather::Update: a burst measured in 30 fps steps (the thunder's delay)", \
        0x72BAA6, 5, "\xA1\x4C\xCB\xB7\x00", cf_count30) \
    FP(enginerevs1, "effectsfps", KIND_CALL, "CAEVehicleAudioEntity::UpdateGasPedalAudio: the note follows the pedal per 30 fps step", \
        0x4F5EE8, 6, "\xD9\x05\x24\xBC\x8C\x00", cf_revs_up) \
    FP(enginerevs2, "effectsfps", KIND_CALL, "CAEVehicleAudioEntity::UpdateGasPedalAudio: the note follows the pedal per 30 fps step", \
        0x4F5F08, 6, "\xD8\x25\x28\xBC\x8C\x00", cf_revs_down) \
    FP(ambience1, "timerfps", KIND_CALL, "CAEGlobalWeaponAudioEntity::ServiceAmbientGunFire: every 1024 30 fps steps, not frames", \
        0x4DF360, 10, "\xF7\x05\x4C\xCB\xB7\x00\xFF\x03\x00\x00", cf_ambience) \
    FP(ambience2, "timerfps", KIND_CALL, "CAEGlobalWeaponAudioEntity::ServiceAmbientGunFire: every 1024 30 fps steps, not frames", \
        0x4DF479, 10, "\xF7\x05\x4C\xCB\xB7\x00\xFF\x03\x00\x00", cf_ambience) \
    FJ(hudflash1, "timerfps", "CHud::RenderArmorBar: flashes on a 25 fps clock", 0x5890AD, 7, "\xF6\x05\x4C\xCB\xB7\x00\x08") \
    FJ(hudflash2, "timerfps", "CHud::RenderBreathBar: flashes on a 25 fps clock", 0x58919D, 7, "\xF6\x05\x4C\xCB\xB7\x00\x08") \
    FJ(hudflash3, "timerfps", "CHud::RenderHealthBar: flashes on a 25 fps clock", 0x58927C, 6, "\x8A\x1D\x4C\xCB\xB7\x00") \
    FJ(hudflash4, "timerfps", "CHud::DrawRadar: flashes on a 25 fps clock", 0x58A361, 7, "\xF6\x05\x4C\xCB\xB7\x00\x08") \
    FJ(hudflash5, "timerfps", "CHud::DrawWanted: the stars flash on a 25 fps clock", 0x58DDBA, 7, "\xF6\x05\x4C\xCB\xB7\x00\x04") \
    FJ(hudflash6, "timerfps", "CHud::DrawWanted: the stars flash on a 25 fps clock", 0x58DE67, 7, "\xF6\x05\x4C\xCB\xB7\x00\x04") \
    FP(mapwheel, "timerfps", KIND_CALL, "CMenuManager: the map's mouse wheel, each notch seen once", 0x577445, 6, \
        "\xD9\x05\xF4\x53\x86\x00", cf_map_wheel) \
    FJ(mapzoomin, "timerfps", "CMenuManager: a wheel notch zooms the map in past the 20 ms repeat", 0x577656, 9, \
        "\x83\xF8\x14\x0F\x86\xB9\x02\x00\x00") \
    FJ(mapzoomout, "timerfps", "CMenuManager: a wheel notch zooms the map out past the 20 ms repeat", 0x577989, 5, \
        "\x83\xF8\x14\x76\x70") \
    FJ(aimcam, "camerafps", "CCamera::Process: aiming on foot, the timestep at least 1 (the aim camera shook at high fps)", \
        0x52B730, 6, "\x81\xEC\xA0\x00\x00\x00") \
    FJ(aimweapon, "camerafps", "CCam::Process_AimWeapon: the timestep at least 1 (the aim camera shook at high fps)", \
        0x521500, 5, "\xA0\x10\x01\xB7\x00") \
    FP(aimzoom, "camerafps", KIND_CALL, "CCam::Process_AimWeapon: the zoom steps by the real timestep", 0x52167A, 6, \
        "\xD9\x05\x5C\xCB\xB7\x00", cf_real_timestep) \
    FJ(weaponfx, "weaponfps", "FxEmitter_c::CreateParticles: a spray's or flame's particle fraction kept", 0x4A41E0, 6, \
        "\x81\xEC\x8C\x00\x00\x00") \
    FJ(weaponammo, "weaponfps", "CWeapon::Fire: a flamethrower, spraycan or extinguisher uses 30 a second", 0x7428A8, 5, \
        "\x8B\x46\x08\x85\xC0") \
    FP(weaponshot, "weaponfps", KIND_CALL, "CWeapon::FireAreaEffect: its shot (damage, paint, water) 30 a second", 0x73EB20, 5, \
        "\xE8\x0B\xB1\xFF\xFF", cf_add_shot) \
    FP(weaponfire, "weaponfps", KIND_CALL, "CWeapon::FireAreaEffect: the flame's fire with its shot", 0x73EBFE, 5, \
        "\xE8\x4D\xB8\xDF\xFF", cf_creeping_fire) \
    FP(weaponwater, "weaponfps", KIND_CALL, "CShotInfo::Update: an extinguisher's shot puts out a 30 fps step's worth", \
        0x73A1E1, 5, "\xE8\xDA\xF2\xDF\xFF", cf_extinguish) \
    FP(chainsaw, "weaponfps", KIND_CALL, "CTaskSimpleFight::ProcessPed: the chainsaw strikes 15 times a second", 0x629F83, 6, \
        "\xD8\x25\x58\x8C\x85\x00", cf_chainsaw) \
    FP(teargas, "weaponfps", KIND_CALL, "CProjectileInfo::Update: tear gas chokes, rolled per 30 fps step", 0x738C19, 5, \
        "\xE8\x62\xE5\xCC\xFF", cf_roll_range) \
    FP(radiolock, "framecaps", KIND_CALL, "the main loop: a car radio's beat no longer holds the game at 30 fps", 0x748D78, 5, \
        "\xE8\x53\xE4\xDB\xFF", cf_beat_limit) \
    FJ(samprot, "effectsfps", "SA-MP's CObject::Process: a moving object turns as far as its time says", 0, 10, \
        "\xD9\x44\x24\x28\xD8\xB3\x5B\x01\x00\x00") \
    FJ(sampmove, "effectsfps", "SA-MP's CObject::Process: a moving object arrives when its time is up", 0, 13, \
        "\xD9\x44\x24\x24\xD8\x5C\x24\x20\xDF\xE0\xF6\xC4\x01") \
    FJ(gearkick, "vehiclefps", "CAEVehicleAudioEntity::JustFinishedAccelerationLoop: an upshift's 10 frames as 10 at 30 fps", \
        0x4F5E59, 7, "\x66\x3B\x05\xC0\xBC\x8C\x00") \
    FP(gearkick2, "vehiclefps", KIND_PTR, "CAEVehicleAudioEntity: the loop's frame count, as gearkick has it", 0x4F7E2B, 4, \
        "\xC0\xBC\x8C\x00", &cf_loop_frames) \
    FJ(carsteer1, "vehiclefps", "CAutomobile::ProcessControlInputs: the steering eases to the pad as at 30 fps", 0x6AD864, 10, \
        "\xD8\x4C\x24\x18\xD8\x0D\x58\x10\x87\x00") \
    FJ(carsteer2, "vehiclefps", "CAutomobile::ProcessControlInputs: the steering eases to the pad as at 30 fps", 0x6AD925, 10, \
        "\xD8\x0D\x58\x10\x87\x00\xD8\x4C\x24\x18") \
    FJ(bikesteer1, "vehiclefps", "CBike::ProcessControlInputs: the steering eases to the pad as at 30 fps", 0x6BE4A7, 10, \
        "\xD8\x4C\x24\x18\xD8\x0D\x00\x13\x87\x00") \
    FJ(bikesteer2, "vehiclefps", "CBike::ProcessControlInputs: the steering eases to the pad as at 30 fps", 0x6BE5F8, 10, \
        "\xD8\x4C\x24\x18\xD8\x0D\x00\x13\x87\x00") \
    FJ(planesteer1, "vehiclefps", "CPlane::ProcessControlInputs: a control surface eases to the stick as at 30 fps", 0x6CAF2C, \
        10, "\xD8\x4C\x24\x18\xD8\x0D\xC4\x8C\x85\x00") \
    FJ(planesteer2, "vehiclefps", "CPlane::ProcessControlInputs: a control surface eases to the stick as at 30 fps", 0x6CB06C, \
        10, "\xD8\x4C\x24\x18\xD8\x0D\xC4\x8C\x85\x00") \
    FJ(planesteer3, "vehiclefps", "CPlane::ProcessControlInputs: a control surface eases to the stick as at 30 fps", 0x6CB0B1, \
        10, "\xD8\x4C\x24\x14\xD8\x0D\xC4\x8C\x85\x00") \
    FJ(planesteer4, "vehiclefps", "CPlane::ProcessControlInputs: the rudder eases to the pad as at 30 fps", 0x6CB1B1, 6, \
        "\xD8\x0D\xC4\x8C\x85\x00") \
    FJ(flyturn1, "turnfps", "CVehicle::FlyingControl: the turn resistance once per 30 fps step, not timestep squared", \
        0x6DAA16, 10, "\xD8\x05\x24\x86\x85\x00\xD8\x7C\x24\x1C") \
    FJ(flyturn2, "turnfps", "CVehicle::FlyingControl: the turn resistance once per 30 fps step, not timestep squared", \
        0x6DAA69, 10, "\xD8\x05\x24\x86\x85\x00\xD8\x7C\x24\x20") \
    FJ(flyturn3, "turnfps", "CVehicle::FlyingControl: the turn resistance once per 30 fps step, not timestep squared", \
        0x6DAABC, 10, "\xD8\x05\x24\x86\x85\x00\xD8\x7C\x24\x24") \
    FJ(springdamp, "vehiclefps", "CPhysical::ApplySpringDampening: the damping a 30 fps step takes, capped as there", 0x543E90, \
        7, "\x83\xEC\x28\x8B\x44\x24\x3C") \
    FJ(loadlean, "vehiclefps", "CPhysical::ProcessControl: the speeds a frame starts with, so the body leans as at 30 fps", \
        0x5485E0, 8, "\x56\x8B\xF1\x8A\x46\x36\x8A\xC8") \
    FJ(carrest, "vehiclefps", "CAutomobile::ProcessControl: the distance moved, per 30 fps step, against the rest limit", \
        0x6B1C9C, 6, "\xD9\x86\xD4\x00\x00\x00") \
    FJ(bikerest, "vehiclefps", "CBike::ProcessControl: the distance moved, per 30 fps step, against the rest limit", 0x6B9955, \
        6, "\xD9\x86\xD4\x00\x00\x00") \
    FJ(trainrest, "vehiclefps", "CTrain::ProcessControl: the distance moved, per 30 fps step, against the rest limit", \
        0x6F9B92, 6, "\xD9\x86\xD4\x00\x00\x00") \
    FJ(objsleep, "vehiclefps", "CObject::ProcessControl: still frames counted per 30 fps step", 0x5A241F, 8, \
        "\x8A\x8E\xB8\x00\x00\x00\xFE\xC1") \
    FJ(trainsleep, "vehiclefps", "CTrain::ProcessControl: still frames counted per 30 fps step", 0x6F9BD1, 8, \
        "\x8A\x9E\xB8\x00\x00\x00\xFE\xC3") \
    FJ(movingparts, "vehiclefps", "CAutomobile::UpdateMovingCollision: forks, blade, bed and ramps move with the fraction kept", \
        0x6A1691, 24, "\x8B\x0D\x5C\xCB\xB7\x00\x89\x4C\x24\x14\x8B\xCB\xE8\x6E\xE5\xE9\xFF\x0F\xBF\xD0\x89\x54\x24\x10") \
    FP(sink1, "vehiclefps", KIND_CALL, "CAutomobile::ProcessBuoyancy: a car under water sinks per 30 fps step", 0x6A9098, 6, \
        "\xD8\x0D\x44\x12\x87\x00", cf_sink_step) \
    FP(sink2, "vehiclefps", KIND_CALL, "CBoat::ProcessControl: a burnt-out boat sinks per 30 fps step", 0x6F17CC, 6, \
        "\xD8\x0D\x44\x12\x87\x00", cf_sink_step) \
    FJ(planeloss1, "vehiclefps", "CPlane::ProcessFlyingCarStuff: a damaged rudder's loss of control as at 30 fps", 0x6CBADC, 6, \
        "\xD8\x8E\x88\x09\x00\x00") \
    FJ(planeloss2, "vehiclefps", "CPlane::ProcessFlyingCarStuff: a damaged elevator's loss of control as at 30 fps", 0x6CBBF5, \
        6, "\xD8\x8E\x8C\x09\x00\x00") \
    FJ(planeloss3, "vehiclefps", "CPlane::ProcessFlyingCarStuff: a damaged aileron's loss of control as at 30 fps", 0x6CBD1A, \
        6, "\xD8\x8E\x90\x09\x00\x00") \
    FP(hydraise, "vehiclefps", KIND_CALL, "CAutomobile::HydraulicControl: the raised stance steps per 30 fps step", 0x6A0AF6, 8, \
        "\x41\x66\x89\x8D\x6C\x08\x00\x00", cf_hyd_raise) \
    FP(hydlower, "vehiclefps", KIND_CALL, "CAutomobile::HydraulicControl: the stance drops per 30 fps step", 0x6A0B1E, 6, \
        "\x8D\x41\xFF\x66\x85\xC0", cf_hyd_lower) \
    FP(boatwaves, "vehiclefps", KIND_CALL, "CVehicle::ProcessBoatControl: a wave's slam from the rise over a 30 fps step", \
        0x6DCEE3, 5, "\xE8\x58\x4C\x14\x00", cf_wave_rise) \
    FP(boatengine, "vehiclefps", KIND_CALL, "CBoat::ProcessControl: a coasting propeller's 0.95 per 30 fps step", 0x6F1900, 6, \
        "\xD8\x0D\xF0\x8E\x85\x00", cf_decay95) \
    FP(wheelspin1, "vehiclefps", KIND_CALL, "CAutomobile::ProcessCarWheelPair: a wheel in the air spins up per 30 fps step", \
        0x6A5DB2, 6, "\xD8\x25\x1C\x8B\x85\x00", cf_sub10) \
    FP(wheelspin2, "vehiclefps", KIND_CALL, "CAutomobile::ProcessCarWheelPair: a wheel in the air spins up per 30 fps step", \
        0x6A5F1D, 6, "\xD8\x25\x1C\x8B\x85\x00", cf_sub10) \
    FP(wheelspin3, "vehiclefps", KIND_CALL, "CAutomobile::ProcessCarWheelPair: a wheel in the air spins back per 30 fps step", \
        0x6A5DF7, 6, "\xD8\x05\x28\x8C\x85\x00", cf_add05) \
    FP(wheelspin4, "vehiclefps", KIND_CALL, "CAutomobile::ProcessCarWheelPair: a wheel in the air spins back per 30 fps step", \
        0x6A5F62, 6, "\xD8\x05\x28\x8C\x85\x00", cf_add05) \
    FP(wheelspin5, "vehiclefps", KIND_CALL, "CAutomobile::ProcessCarWheelPair: a free wheel's 0.95 per 30 fps step", 0x6A5E54, 6, \
        "\xD8\x0D\xF0\x8E\x85\x00", cf_decay95) \
    FP(wheelspin6, "vehiclefps", KIND_CALL, "CAutomobile::ProcessCarWheelPair: a free wheel's 0.95 per 30 fps step", 0x6A5FBF, 6, \
        "\xD8\x0D\xF0\x8E\x85\x00", cf_decay95) \
    FJ(burnspeed, "vehiclefps", "CAutomobile::ProcessCarWheelPair: a burnout's 3000 wheel speed per 30 fps step", 0x6A4FE6, 6, \
        "\xD9\x05\x94\x9A\x85\x00") \
    FP(bikewheel1, "vehiclefps", KIND_CALL, "CBike::ProcessControl: a free front wheel's 0.95 per 30 fps step", 0x6BAC7D, 6, \
        "\xD8\x0D\xF0\x8E\x85\x00", cf_decay95) \
    FP(bikewheel2, "vehiclefps", KIND_CALL, "CBike::ProcessControl: a free front wheel's 0.95 per 30 fps step", 0x6BB5A1, 6, \
        "\xD8\x0D\xF0\x8E\x85\x00", cf_decay95) \
    FJ(bikepitch, "vehiclefps", "CBike::ProcessControl: a free front wheel turns by its speed per 30 fps step", 0x6BAC89, 6, \
        "\xD8\x86\x50\x07\x00\x00") \
    FP(bikerear1, "vehiclefps", KIND_CALL, "CBike::ProcessControl: the rear wheel's speed limit steps per 30 fps step", \
        0x6BAFF4, 6, "\xD8\x25\x1C\x8B\x85\x00", cf_sub10) \
    FP(bikerear2, "vehiclefps", KIND_CALL, "CBike::ProcessControl: the rear wheel's speed limit steps per 30 fps step", \
        0x6BB00F, 6, "\xD8\x05\x28\x8C\x85\x00", cf_add05) \
    FP(bmxsprint, "vehiclefps", KIND_CALL, "CBmx::ProcessControl: the sprint lean's 0.95 per 30 fps step", 0x6BFB3B, 6, \
        "\xD8\x0D\xF0\x8E\x85\x00", cf_decay95) \
    FJ(bmxlean1, "vehiclefps", "CBmx::ProcessDrivingAnims: the rider's lean settles per 30 fps step", 0x6C0067, 6, \
        "\xD8\x8E\x54\x06\x00\x00") \
    FJ(bmxlean2, "vehiclefps", "CBmx::ProcessDrivingAnims: the rider's lean settles per 30 fps step", 0x6C0079, 6, \
        "\xD8\x8E\x58\x06\x00\x00") \
    FJ(bmxlean3, "vehiclefps", "CBmx::ProcessDrivingAnims: the rider's lean settles per 30 fps step", 0x6C00FA, 6, \
        "\xD8\x8E\x54\x06\x00\x00") \
    FJ(bmxlean4, "vehiclefps", "CBmx::ProcessDrivingAnims: the rider's lean settles per 30 fps step", 0x6C010C, 6, \
        "\xD8\x8E\x58\x06\x00\x00") \
    FP(headbop1, "vehiclefps", KIND_CALL, "CTaskSimpleCarDrive::ProcessHeadBopping: the bop ramps per 30 fps step", 0x6429C6, \
        6, "\xD8\x05\x28\x8C\x85\x00", cf_add05) \
    FP(headbop2, "vehiclefps", KIND_CALL, "CTaskSimpleCarDrive::ProcessHeadBopping: the bop ramps per 30 fps step", 0x642A02, \
        6, "\xD8\x25\x28\x8C\x85\x00", cf_sub05) \
    FP(wheelsettle1, "vehiclefps", KIND_CALL, "CBike::PreRender: a wheel eases down per 30 fps step", 0x6BD150, 6, \
        "\xD8\x0D\x34\x8F\x85\x00", cf_settle) \
    FP(wheelsettle2, "vehiclefps", KIND_CALL, "CBike::PreRender: a wheel eases down per 30 fps step", 0x6BD1D0, 6, \
        "\xD8\x0D\x34\x8F\x85\x00", cf_settle) \
    FP(wheelsettle3, "vehiclefps", KIND_CALL, "CBmx::PreRender: a wheel eases down per 30 fps step", 0x6C08D0, 6, \
        "\xD8\x0D\x34\x8F\x85\x00", cf_settle) \
    FP(wheelsettle4, "vehiclefps", KIND_CALL, "CBmx::PreRender: a wheel eases down per 30 fps step", 0x6C0950, 6, \
        "\xD8\x0D\x34\x8F\x85\x00", cf_settle) \
    FP(wheelsettle5, "vehiclefps", KIND_CALL, "CHeli::PreRender: a wheel eases down per 30 fps step", 0x6C559E, 6, \
        "\xD8\x0D\x34\x8F\x85\x00", cf_settle) \
    FP(wheelsettle6, "vehiclefps", KIND_CALL, "CPlane::PreRender: a wheel eases down per 30 fps step", 0x6C95AE, 6, \
        "\xD8\x0D\x34\x8F\x85\x00", cf_settle) \
    FJ(attached, "vehiclefps", "CPhysical::PositionAttachedEntity: the move speed by the real timestep, not max(1, it)", \
        0x5477F4, 17, "\xD9\x05\x24\x86\x85\x00\xD8\x1D\x5C\xCB\xB7\x00\xDF\xE0\xF6\xC4\x41") \
    FJ(aisteer, "vehiclefps", "an aircraft's autopilot: its turn rate by the real timestep, not max(1, it)", 0x4235D2, 17, \
        "\xD9\x05\x24\x86\x85\x00\xD8\x1D\x5C\xCB\xB7\x00\xDF\xE0\xF6\xC4\x41") \
    FP(aimtime1, "pedfps", KIND_CALL, "CPlayerPed::ProcessGroupBehaviour: how long the group-command up button is held (+8Ah)", 0x60D3AD, 5, "\xE8\x8E\x47\x21\x00", cf_ftol_carry) \
    FP(aimtime2, "pedfps", KIND_CALL, "CPlayerPed::ProcessGroupBehaviour: how long the group-command down button is held (+88h)", 0x60D47C, 5, "\xE8\xBF\x46\x21\x00", cf_ftol_carry) \
    FP(inairstep, "pedfps", KIND_CALL, "CTaskSimpleInAir::ProcessPed: its timer (+28h)", 0x6809A2, 5, "\xE8\x99\x11\x1A\x00", cf_ftol_carry) \
    FP(fightmove, "pedfps", KIND_CALL, "CTaskSimpleFightingControl::CalcMoveCommand: its timer (+1Ch)", 0x624C06, 5, "\xE8\x35\xCF\x1F\x00", cf_ftol_carry) \
    FP(stealthstep, "pedfps", KIND_CALL, "CTaskSimpleStealthKill::ManageAnim: its 10 s timer (+1Ch)", 0x6298AA, 5, "\xE8\x91\x82\x1F\x00", cf_ftol_carry) \
    FP(killstep, "pedfps", KIND_CALL, "CTaskSimpleGangDriveBy::ProcessPed: its attack timer (+16h)", 0x62D855, 5, "\xE8\xE6\x42\x1F\x00", cf_ftol_carry) \
    FP(drivestep, "pedfps", KIND_CALL, "CTaskSimpleCarDrive::ProcessPed: its 2 s timer (+4Ch)", 0x644595, 5, "\xE8\xA6\xD5\x1D\x00", cf_ftol_carry) \
    FP(duckstep, "pedfps", KIND_CALL, "CTaskSimpleDuck::ProcessPed: its timer (+0Eh)", 0x6944DA, 5, "\xE8\x61\xD6\x18\x00", cf_ftol_carry) \
    FP(fallstep2, "pedfps", KIND_CALL, "CTaskSimpleFall::ProcessPed: the down time, its second branch", 0x67FBB3, 5, "\xE8\x88\x1F\x1A\x00", cf_ftol_carry) \
    FP(gunstep, "pedfps", KIND_CALL, "CTaskSimpleGunControl::ProcessPed: its timer (+28h)", 0x625664, 5, "\xE8\xD7\xC4\x1F\x00", cf_ftol_carry) \
    FP(aistep1, "vehiclefps", KIND_CALL, "CCarAI::UpdateCarAI: a police car's wait (+4DCh)", 0x41F28D, 5, "\xE8\xAE\x28\x40\x00", cf_ftol_carry) \
    FP(aistep2, "vehiclefps", KIND_CALL, "CCarAI::UpdateCarAI: a police car's wait, another branch", 0x41DE6A, 5, "\xE8\xD1\x3C\x40\x00", cf_ftol_carry) \
    FP(aistep3, "vehiclefps", KIND_CALL, "CCarAI::UpdateCarAI: a police car's wait, a third branch", 0x41E1D5, 5, "\xE8\x66\x39\x40\x00", cf_ftol_carry) \
    FP(flystep, "vehiclefps", KIND_CALL, "CVehicle::FlyingControl: its timer (+9A0h)", 0x6D9702, 5, "\xE8\x39\x84\x14\x00", cf_ftol_carry) \
    FP(planestep, "vehiclefps", KIND_CALL, "CPlane::ProcessControl: its timer (+9FCh)", 0x6C938F, 5, "\xE8\xAC\x87\x15\x00", cf_ftol_carry) \
    FP(bombstep, "vehiclefps", KIND_CALL, "CVehicle::ProcessDelayedExplosion: a car bomb's fuse", 0x6D135C, 5, "\xE8\xDF\x07\x15\x00", cf_ftol_carry) \
    FP(upsidestep, "vehiclefps", KIND_CALL, "CUpsideDownCarCheck::UpdateTimers: a car on its roof", 0x4655F9, 5, "\xE8\x42\xC5\x3B\x00", cf_ftol_carry) \
    FP(sputter1, "vehiclefps", KIND_CALL, "CPlane::ProcessFlyingCarStuff: a damaged engine's sputter phase", 0x6CB833, 5, "\xE8\x08\x63\x15\x00", cf_ftol_carry) \
    FP(sputter2, "vehiclefps", KIND_CALL, "CPlane::ProcessFlyingCarStuff: a damaged engine's sputter phase, rounded again", 0x6CB84C, 5, "\xE8\xEF\x62\x15\x00", cf_ftol_carry) \
    FP(idlecam, "timerfps", KIND_CALL, "CIdleCam::ProcessIdleCamTicker: the idle time before the camera drifts", 0x50A22F, 5, "\xE8\x0C\x79\x31\x00", cf_ftol_carry) \
    FP(onscreen, "timerfps", KIND_CALL, "COnscreenTimerEntry::Process: an on-screen countdown", 0x44CB31, 5, "\xE8\x0A\x50\x3D\x00", cf_ftol_carry) \
    FP(scripttimer, "timerfps", KIND_CALL, "CTheScripts::Process: every script's TIMERA and TIMERB", 0x46A036, 5, "\xE8\x05\x7B\x3B\x00", cf_ftol_carry) \
    FP(fatstep, "timerfps", KIND_CALL, "CStats: m_FatCounter", 0x55C5C3, 5, "\xE8\x78\x55\x2C\x00", cf_ftol_carry) \
    FP(healthstep, "timerfps", KIND_CALL, "CStats: m_MaxHealthCounter", 0x55C64E, 5, "\xE8\xED\x54\x2C\x00", cf_ftol_carry) \
    FP(sprintstep, "timerfps", KIND_CALL, "CStats: m_SprintStaminaCounter", 0x55C6DE, 5, "\xE8\x5D\x54\x2C\x00", cf_ftol_carry) \
    FP(runstep, "timerfps", KIND_CALL, "CStats: m_RunningCounter", 0x55C76E, 5, "\xE8\xCD\x53\x2C\x00", cf_ftol_carry) \
    FP(cyclestam, "timerfps", KIND_CALL, "CStats: m_CycleStaminaCounter", 0x55C838, 5, "\xE8\x03\x53\x2C\x00", cf_ftol_carry) \
    FP(cycleskill1, "timerfps", KIND_CALL, "CStats: m_CycleSkillCounter", 0x55C94B, 5, "\xE8\xF0\x51\x2C\x00", cf_ftol_carry) \
    FP(cycleskill2, "timerfps", KIND_CALL, "CStats: m_CycleSkillCounter, the increment", 0x55C972, 5, "\xE8\xC9\x51\x2C\x00", cf_ftol_carry) \
    FP(swimstam, "timerfps", KIND_CALL, "CStats: m_SwimStaminaCounter", 0x55CA26, 5, "\xE8\x15\x51\x2C\x00", cf_ftol_carry) \
    FP(swimunder, "timerfps", KIND_CALL, "CStats: m_SwimUnderWaterCounter", 0x55CAA8, 5, "\xE8\x93\x50\x2C\x00", cf_ftol_carry) \
    FP(drive1, "timerfps", KIND_CALL, "CStats: m_DrivingCounter", 0x55CB91, 5, "\xE8\xAA\x4F\x2C\x00", cf_ftol_carry) \
    FP(drive2, "timerfps", KIND_CALL, "CStats: m_DrivingCounter, the store", 0x55CBB2, 5, "\xE8\x89\x4F\x2C\x00", cf_ftol_carry) \
    FP(drive3, "timerfps", KIND_CALL, "CStats: m_DrivingCounter, second branch", 0x55CBD2, 5, "\xE8\x69\x4F\x2C\x00", cf_ftol_carry) \
    FP(drive4, "timerfps", KIND_CALL, "CStats: m_DrivingCounter, the store, second branch", 0x55CBF3, 5, "\xE8\x48\x4F\x2C\x00", cf_ftol_carry) \
    FP(fly1, "timerfps", KIND_CALL, "CStats: m_FlyingCounter", 0x55CCE4, 5, "\xE8\x57\x4E\x2C\x00", cf_ftol_carry) \
    FP(fly2, "timerfps", KIND_CALL, "CStats: m_FlyingCounter, the store", 0x55CD05, 5, "\xE8\x36\x4E\x2C\x00", cf_ftol_carry) \
    FP(fly3, "timerfps", KIND_CALL, "CStats: m_FlyingCounter, second branch", 0x55CD25, 5, "\xE8\x16\x4E\x2C\x00", cf_ftol_carry) \
    FP(fly4, "timerfps", KIND_CALL, "CStats: m_FlyingCounter, the store, second branch", 0x55CD46, 5, "\xE8\xF5\x4D\x2C\x00", cf_ftol_carry) \
    FP(bike1, "timerfps", KIND_CALL, "CStats: m_BikeCounter", 0x55CE33, 5, "\xE8\x08\x4D\x2C\x00", cf_ftol_carry) \
    FP(bike2, "timerfps", KIND_CALL, "CStats: m_BikeCounter, the store", 0x55CE54, 5, "\xE8\xE7\x4C\x2C\x00", cf_ftol_carry) \
    FP(bike3, "timerfps", KIND_CALL, "CStats: m_BikeCounter, second branch", 0x55CE76, 5, "\xE8\xC5\x4C\x2C\x00", cf_ftol_carry) \
    FP(bike4, "timerfps", KIND_CALL, "CStats: m_BikeCounter, the store, second branch", 0x55CE97, 5, "\xE8\xA4\x4C\x2C\x00", cf_ftol_carry) \
    FP(stunt1, "timerfps", KIND_CALL, "CPlayerInfo::Process: the stunt counter of less than three wheels", 0x56F9E7, 5, "\xE8\x54\x21\x2B\x00", cf_ftol_carry) \
    FP(stunt2, "timerfps", KIND_CALL, "CPlayerInfo::Process: the stunt counter of two wheels", 0x56FA7B, 5, "\xE8\xC0\x20\x2B\x00", cf_ftol_carry) \
    FP(stunt3, "timerfps", KIND_CALL, "CPlayerInfo::Process: the stunt counter of two wheels, its grace", 0x56FAC8, 5, "\xE8\x73\x20\x2B\x00", cf_ftol_carry) \
    FP(stunt4, "timerfps", KIND_CALL, "CPlayerInfo::Process: the stunt counter of two wheels, another branch", 0x56FBAA, 5, "\xE8\x91\x1F\x2B\x00", cf_ftol_carry) \
    FP(stunt5, "timerfps", KIND_CALL, "CPlayerInfo::Process: the stunt counter of two wheels, its grace, another branch", 0x56FBF7, 5, "\xE8\x44\x1F\x2B\x00", cf_ftol_carry) \
    FP(stunt6, "timerfps", KIND_CALL, "CPlayerInfo::Process: the stunt counter of two wheels, the grace stored", 0x56FC2D, 5, "\xE8\x0E\x1F\x2B\x00", cf_ftol_carry) \
    FP(stunt7, "timerfps", KIND_CALL, "CPlayerInfo::Process: the stunt counter of two wheels, the grace refilled", 0x56FC67, 5, "\xE8\xD4\x1E\x2B\x00", cf_ftol_carry) \
    FP(stunt8, "timerfps", KIND_CALL, "CPlayerInfo::Process: the stunt counter of a wheelie", 0x56FE0D, 5, "\xE8\x2E\x1D\x2B\x00", cf_ftol_carry) \
    FP(stunt9, "timerfps", KIND_CALL, "CPlayerInfo::Process: the stunt counter of a wheelie, its grace", 0x56FE5A, 5, "\xE8\xE1\x1C\x2B\x00", cf_ftol_carry) \
    FP(stunt10, "timerfps", KIND_CALL, "CPlayerInfo::Process: the stunt counter of a wheelie, the grace stored", 0x56FE90, 5, "\xE8\xAB\x1C\x2B\x00", cf_ftol_carry) \
    FP(stunt11, "timerfps", KIND_CALL, "CPlayerInfo::Process: the stunt counter of a wheelie, the grace refilled", 0x56FFA4, 5, "\xE8\x97\x1B\x2B\x00", cf_ftol_carry) \
    FP(stunt12, "timerfps", KIND_CALL, "CPlayerInfo::Process: the stunt counter of a stoppie", 0x570003, 5, "\xE8\x38\x1B\x2B\x00", cf_ftol_carry) \
    FP(stunt13, "timerfps", KIND_CALL, "CPlayerInfo::Process: the stunt counter of a stoppie, its grace", 0x570050, 5, "\xE8\xEB\x1A\x2B\x00", cf_ftol_carry) \
    FP(hudtime1, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58AB42, 5, "\xE8\xF9\x6F\x29\x00", cf_ftol_carry) \
    FP(hudtime2, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58ABC4, 5, "\xE8\x77\x6F\x29\x00", cf_ftol_carry) \
    FP(hudtime3, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58AC26, 5, "\xE8\x15\x6F\x29\x00", cf_ftol_carry) \
    FP(hudtime4, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58ACFA, 5, "\xE8\x41\x6E\x29\x00", cf_ftol_carry) \
    FP(hudtime5, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58AF5F, 5, "\xE8\xDC\x6B\x29\x00", cf_ftol_carry) \
    FP(hudtime6, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58AFAD, 5, "\xE8\x8E\x6B\x29\x00", cf_ftol_carry) \
    FP(hudtime7, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58AFD4, 5, "\xE8\x67\x6B\x29\x00", cf_ftol_carry) \
    FP(hudtime8, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58B05F, 5, "\xE8\xDC\x6A\x29\x00", cf_ftol_carry) \
    FP(hudtime9, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58B894, 5, "\xE8\xA7\x62\x29\x00", cf_ftol_carry) \
    FP(hudtime10, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58B8E6, 5, "\xE8\x55\x62\x29\x00", cf_ftol_carry) \
    FP(hudtime11, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58B938, 5, "\xE8\x03\x62\x29\x00", cf_ftol_carry) \
    FP(hudtime12, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58BA22, 5, "\xE8\x19\x61\x29\x00", cf_ftol_carry) \
    FP(hudtime13, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58C938, 5, "\xE8\x03\x52\x29\x00", cf_ftol_carry) \
    FP(hudtime14, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58C9C4, 5, "\xE8\x77\x51\x29\x00", cf_ftol_carry) \
    FP(hudtime15, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58CB5C, 5, "\xE8\xDF\x4F\x29\x00", cf_ftol_carry) \
    FP(hudtime16, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58CFCD, 5, "\xE8\x6E\x4B\x29\x00", cf_ftol_carry) \
    FP(hudtime17, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58D0B5, 5, "\xE8\x86\x4A\x29\x00", cf_ftol_carry) \
    FP(hudtime18, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58D31B, 5, "\xE8\x20\x48\x29\x00", cf_ftol_carry) \
    FP(hudtime19, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58D388, 5, "\xE8\xB3\x47\x29\x00", cf_ftol_carry) \
    FP(hudtime20, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58D655, 5, "\xE8\xE6\x44\x29\x00", cf_ftol_carry) \
    FP(hudtime21, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58D6B1, 5, "\xE8\x8A\x44\x29\x00", cf_ftol_carry) \
    FP(hudtime22, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58D713, 5, "\xE8\x28\x44\x29\x00", cf_ftol_carry) \
    FP(hudtime23, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58DA32, 5, "\xE8\x09\x41\x29\x00", cf_ftol_carry) \
    FP(hudtime24, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58DA7A, 5, "\xE8\xC1\x40\x29\x00", cf_ftol_carry) \
    FP(hudtime25, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58DAF0, 5, "\xE8\x4B\x40\x29\x00", cf_ftol_carry) \
    FP(hudtime26, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58DB8F, 5, "\xE8\xAC\x3F\x29\x00", cf_ftol_carry) \
    FP(hudtime27, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58DBC5, 5, "\xE8\x76\x3F\x29\x00", cf_ftol_carry) \
    FP(hudtime28, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58DC3B, 5, "\xE8\x00\x3F\x29\x00", cf_ftol_carry) \
    FP(hudtime29, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58ECB2, 5, "\xE8\x89\x2E\x29\x00", cf_ftol_carry) \
    FP(hudtime30, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58ECF8, 5, "\xE8\x43\x2E\x29\x00", cf_ftol_carry) \
    FP(hudtime31, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58ED52, 5, "\xE8\xE9\x2D\x29\x00", cf_ftol_carry) \
    FP(hudtime32, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58ED96, 5, "\xE8\xA5\x2D\x29\x00", cf_ftol_carry) \
    FP(hudtime33, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58EDCA, 5, "\xE8\x71\x2D\x29\x00", cf_ftol_carry) \
    FP(hudtime34, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58EE24, 5, "\xE8\x17\x2D\x29\x00", cf_ftol_carry) \
    FP(hudtime35, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58F237, 5, "\xE8\x04\x29\x29\x00", cf_ftol_carry) \
    FP(hudtime36, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58F293, 5, "\xE8\xA8\x28\x29\x00", cf_ftol_carry) \
    FP(hudtime37, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58F2F5, 5, "\xE8\x46\x28\x29\x00", cf_ftol_carry) \
    FP(hudtime38, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58F38A, 5, "\xE8\xB1\x27\x29\x00", cf_ftol_carry) \
    FP(hudtime39, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58F3D4, 5, "\xE8\x67\x27\x29\x00", cf_ftol_carry) \
    FP(hudtime40, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58F436, 5, "\xE8\x05\x27\x29\x00", cf_ftol_carry) \
    FP(hudtime41, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58F69F, 5, "\xE8\x9C\x24\x29\x00", cf_ftol_carry) \
    FP(hudtime42, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58F6E7, 5, "\xE8\x54\x24\x29\x00", cf_ftol_carry) \
    FP(hudtime43, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58F75D, 5, "\xE8\xDE\x23\x29\x00", cf_ftol_carry) \
    FP(hudtime44, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58F7FD, 5, "\xE8\x3E\x23\x29\x00", cf_ftol_carry) \
    FP(hudtime45, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58F833, 5, "\xE8\x08\x23\x29\x00", cf_ftol_carry) \
    FP(hudtime46, "timerfps", KIND_CALL, "CHud: a timed text's or bar's time", 0x58F8A9, 5, "\xE8\x92\x22\x29\x00", cf_ftol_carry)


HFPS(FDECLARE_J, FDECLARE_P)

/* ------------------------------------------------------------------------ */
/* A truncated time with its fraction carried                               */
/* ------------------------------------------------------------------------ */
/* _ftol's integer for one call site (its return address), the fraction kept
 * for the site's next call. A site called again in the same frame with the
 * same value (one timer per ped or per car) gets the same integer. */
#define FRAME_COUNTER (*(const uint32_t *)0xB7CB4C)    /* CTimer::m_FrameCounter */
#define CARRY_SITES 256

typedef struct { uint32_t site, frame; double value; float carry; int32_t result; } Carry;
static Carry carries[CARRY_SITES];

int32_t __cdecl cf_ftol_carried(uint32_t site, double value)
{
    if (!(fabs(value) < 2147483000.0)) return (int32_t)0x80000000;
    unsigned i = ((site >> 2) * 2654435761u) >> 24;
    for (int n = 0; carries[i].site && carries[i].site != site; n++, i = (i + 1) % CARRY_SITES)
        if (n == CARRY_SITES) return (int32_t)value;
    Carry *c = &carries[i];
    uint32_t frame = FRAME_COUNTER;
    if (c->site != site) c->site = site, c->carry = 0, c->frame = frame - 1;
    else if (c->frame == frame && c->value == value) return c->result;
    double total = value + c->carry, whole = (double)(int64_t)total;
    c->carry = (float)(total - whole);
    c->frame = frame, c->value = value, c->result = (int32_t)whole;
    return c->result;
}

/* ------------------------------------------------------------------------ */
/* Per 30 fps step                                                          */
/* ------------------------------------------------------------------------ */
int cf_installed(const char *key);

/* timestep / 1.6667: 1 at 30 fps */
static float ratio30(void)
{
    float t = TIMESTEP;
    return t > 0.0001f ? t / cf_k30 : 1.0f;
}

/* b^e on the x87 stack in at most three registers: these run with the game's
 * values still on it, where msvcrt's pow (which wants it empty) is not safe */
static float pow3(float b, float e)
{
    if (!(b > 0.0f)) return b == 0.0f && e > 0.0f ? 0.0f : 1.0f;
    float r;
    __asm__("fyl2x\n\tfld %%st(0)\n\tfrndint\n\tfxch\n\tfsub %%st(1), %%st\n\tf2xm1\n\t"
            "fld1\n\tfaddp\n\tfscale\n\tfstp %%st(1)" : "=t"(r) : "0"(b), "u"(e) : "st(1)");
    return r;
}

/* rate^(timestep / 1.6667): a factor applied once a frame, per second as at 30 fps */
float __cdecl cf_per30(float rate) { return pow3(rate, ratio30()); }

/* 30 fps steps counted, for code that measures time in frames */
uint32_t cf_steps30;

/* Whether a 30 fps step ends this frame: every frame at or under 30 fps, the
 * same answer for every caller in a frame */
static uint32_t due_frame = 0xFFFFFFFF;
static int due_now = 1;
static float due_carry;
int __cdecl cf_due30(void)
{
    uint32_t frame = FRAME_COUNTER;
    if (frame == due_frame) return due_now;
    due_frame = frame;
    float r = ratio30();
    if (r >= 1.0f) {
        due_carry = 0, due_now = 1;
    } else {
        due_carry += r;
        due_now = due_carry >= 1.0f;
        if (due_now) due_carry = due_carry - 1.0f < 1.0f ? due_carry - 1.0f : 0;
    }
    cf_steps30 += due_now;
    return due_now;
}

/* A share of a gap closed once a frame, k * timestep, as the share a 30 fps
 * step closes per second: 1 - (1 - k * 1.6667)^(timestep / 1.6667) */
float __cdecl cf_slew(float k)
{
    float base = 1.0f - k * cf_k30, r = ratio30();
    if (!(base > 0.0f && base < 1.0f)) return k * cf_k30 * r;
    return 1.0f - pow3(base, r);
}

/* The same over the timestep, for a gap already times the timestep */
float __cdecl cf_slew_per_step(float k)
{
    float t = TIMESTEP;
    return t > 0.0001f ? cf_slew(k) / t : k;
}

/* A weight on a gap once a frame (a wheel easing down) as the same per second */
float __cdecl cf_settle_weight(void)
{
    float w = *(const float *)0x858F34;
    if (!(w > 0.0f && w < 1.0f)) return w;
    return 1.0f - pow3(1.0f - w, ratio30());
}

/* ------------------------------------------------------------------------ */
/* The gearbox's kick                                                       */
/* ------------------------------------------------------------------------ */
/* JustFinishedAccelerationLoop raises bAudioChangingGear, which kicks the
 * car's nose up once a frame, after 10 frames: 0.33 s at 30 fps, 9 ms at
 * 1000, where it kicked every eleventh frame. The count is 10 at 30 fps and
 * the same time at any other. */
int32_t cf_loop_frames = 10;
void __cdecl cf_loop_frames_update(void)
{
    float r = ratio30(), n = roundf(*(const int32_t *)0x8CBCC0 / r);
    cf_loop_frames = r >= 1.0f ? *(const int32_t *)0x8CBCC0 : n > 30000.0f ? 30000 : (int32_t)n;
}

/* ------------------------------------------------------------------------ */
/* An aircraft's turn resistance                                            */
/* ------------------------------------------------------------------------ */
/* FlyingControl damps each turn axis by (pow(resistance, timestep) / (1 +
 * speed term))^timestep: the resistance to the timestep squared, so per
 * second it held far more turn at high fps (with turndamping's 0.99 it then
 * turned faster than at 30). The resistance takes 1.6667 in place of the
 * first timestep. */
float __cdecl cf_fly_turn(float resistance, float stock, float speed_term)
{
    float t = TIMESTEP;
    if (!(t > 0.0001f) || t >= cf_k30 || !(resistance > 0.0f) || !isfinite(resistance)) return stock / (1.0f + speed_term);
    return pow3(resistance, cf_k30) / (1.0f + speed_term);
}

/* ------------------------------------------------------------------------ */
/* The suspension's damping and the body's lean                             */
/* ------------------------------------------------------------------------ */
/* ApplySpringDampening takes min(timestep * level, 0.25) of the contact's
 * speed along the spring a frame: at 30 fps any car over level 0.15 is at the
 * 0.25 cap (7.5 a second), at high fps under it (level 0.2: 10 a second). The
 * level becomes the one that takes, per second, what the capped 30 fps step
 * takes. The damper also bites the speed the frame's forces have added so
 * far, which at 30 fps (a quarter of it) leans the body a third further
 * under a cornering or braking load; that bite is put back. */
#define DAMPING_LIMIT (*(const float *)0x8CD7A0)
#define DAMPING_FORCE_LIMIT (*(const float *)0x8CD7A4)

static struct { const uint8_t *physical; uint32_t frame; float move[3], turn[3]; } frame_start;

void __cdecl cf_frame_start(const uint8_t *physical)
{
    frame_start.physical = physical, frame_start.frame = FRAME_COUNTER;
    memcpy(frame_start.move, physical + 0x44, sizeof frame_start.move);
    memcpy(frame_start.turn, physical + 0x50, sizeof frame_start.turn);
}

static float dot3(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

static void cross3(const float *a, const float *b, float *out)
{
    out[0] = a[1] * b[2] - a[2] * b[1], out[1] = a[2] * b[0] - a[0] * b[2], out[2] = a[0] * b[1] - a[1] * b[0];
}

typedef void (__attribute__((thiscall)) *ApplyForceFn)(void *, float, float, float, float, float, float, uint8_t);

static void lean_bite(uint8_t *physical, float extra, float spring_limit, const float *dir, const float *point)
{
    if (!(extra > 0.0f) || physical != frame_start.physical || frame_start.frame != FRAME_COUNTER) return;
    const float *move = (const float *)(physical + 0x44), *turn = (const float *)(physical + 0x50);
    float mass = *(const float *)(physical + 0x8C), turn_mass = *(const float *)(physical + 0x90);
    const float *centre = (const float *)(physical + 0xA4), *m = *(const float **)(physical + 0x14);
    if (!m || !(mass > 0.0f) || !(turn_mass > 0.0f)) return;
    float d[3], turned[3], delta[3], lever[3];
    for (int i = 0; i < 3; i++) d[i] = point[i] - (m[i] * centre[0] + m[4 + i] * centre[1] + m[8 + i] * centre[2]);
    for (int i = 0; i < 3; i++) turned[i] = turn[i] - frame_start.turn[i];
    cross3(turned, d, delta);
    for (int i = 0; i < 3; i++) delta[i] += move[i] - frame_start.move[i];
    float removal = -extra * dot3(delta, dir);
    if (removal == 0.0f || !isfinite(removal)) return;
    cross3(d, dir, lever);
    float force = removal / (dot3(lever, lever) / turn_mass + 1.0f / mass);
    float cap = fabsf(spring_limit) * DAMPING_FORCE_LIMIT;
    force = force > cap ? cap : force < -cap ? -cap : force;
    ((ApplyForceFn)0x542B50)(physical, force * dir[0], force * dir[1], force * dir[2], point[0], point[1], point[2], 1);
}

/* args: the damping level (rewritten), the spring force limit, the
 * direction, the contact point */
void __cdecl cf_spring_level(uint8_t *physical, float *args)
{
    float t = TIMESTEP, level = args[0], limit = DAMPING_LIMIT;
    if (!physical || !(t > 0.0001f) || t >= cf_k30 || !(level > 0.0f) || !isfinite(level) || !(limit > 0.0f)) return;
    float twice = physical[0x40] & 1 ? 2.0f : 1.0f;
    float stock = cf_k30 * level * twice < limit ? cf_k30 * level * twice : limit;
    if (!(stock > 0.0f && stock < 1.0f)) return;
    float fraction = 1.0f - pow3(1.0f - stock, t / cf_k30);
    args[0] = fraction / (t * twice);
    lean_bite(physical, stock - fraction, args[1], *(const float **)(args + 2), *(const float **)(args + 3));
}

/* ------------------------------------------------------------------------ */
/* Moving parts                                                             */
/* ------------------------------------------------------------------------ */
/* UpdateMovingCollision steps m_wMiscComponentAngle by upDown / 128 *
 * timestep * rate a frame, truncated: at 300 fps the forks rose at 300 units
 * a second for 480, above 500 fps not at all. The 30 fps step is taken per
 * 30 fps step, its fraction kept per vehicle. */
typedef int16_t (__attribute__((thiscall)) *GunUpDownFn)(void *pad);
static struct { const void *vehicle; uint32_t frame; float carry; } part_carry[8];

void __cdecl cf_moving_part(uint8_t *vehicle, void *pad, float rate)
{
    int16_t updown = ((GunUpDownFn)0x53FC10)(pad);
    float t = TIMESTEP, r = ratio30();
    uint32_t frame = FRAME_COUNTER;
    int oldest = 0, k = -1;
    for (int i = 0; i < 8 && k < 0; i++) {
        if (part_carry[i].vehicle == vehicle) k = i;
        else if (frame - part_carry[i].frame > frame - part_carry[oldest].frame) oldest = i;
    }
    if (k < 0) k = oldest, part_carry[k].vehicle = vehicle, part_carry[k].carry = 0;
    part_carry[k].frame = frame;
    float *carry = &part_carry[k].carry;
    int32_t step;
    if (r >= 1.0f) {
        step = (int16_t)(updown * t * rate / 128.0f);
        *carry = 0;
    } else {
        int16_t full = (int16_t)(updown * cf_k30 * rate / 128.0f);
        if (!full || (full > 0) != (*carry > 0.0f)) *carry = 0;
        float total = full * r + *carry;
        step = (int32_t)total;
        *carry = total - step;
    }
    uint16_t *angle = (uint16_t *)(vehicle + 0x86C);
    int32_t next = *angle + step, most = *(const int16_t *)0x8D314C;
    if (next < 0) next = 0, *carry = 0;
    if (next > most) next = most, *carry = 0;
    *angle = (uint16_t)next;
}

/* ------------------------------------------------------------------------ */
/* A damaged plane                                                          */
/* ------------------------------------------------------------------------ */
/* ProcessFlyingCarStuff multiplies a slewed control input by 1 - 0.2 *
 * status for each damaged part every frame, after ProcessControlInputs eased
 * it to the stick: per second the loss grew with the frame rate and a damaged
 * plane barely answered. The input settles where the slew and the loss meet;
 * each part's share is the one that settles where a 30 fps step does, under
 * the slew this frame has (planesteer's or the stock one). */
typedef uint8_t (__attribute__((thiscall)) *PartStatusFn)(void *damage, uint8_t part);
static const uint8_t plane_parts[3][3] = {{1, 16}, {2, 17, 18}, {2, 19, 20}};   /* count, parts */

float __cdecl cf_plane_loss(float share, const uint8_t *plane, int axis)
{
    float r = ratio30();
    if (!(r > 0.0f && r < 1.0f) || axis < 0 || axis > 2) return share;
    float step = *(const float *)0x858CC4, kept = 1.0f;
    int damaged = 0;
    for (int i = 1; i <= plane_parts[axis][0]; i++) {
        uint8_t part = plane_parts[axis][i];
        if (!*(const uint32_t *)(plane + 0x648 + part * 4)) continue;
        uint8_t status = ((PartStatusFn)0x6C2300)((void *)(plane + 0x5A0), part);
        if (status) kept *= 1.0f - step * status, damaged++;
    }
    float closed = step * cf_k30, now = cf_installed("planesteer1") ? cf_slew(step) : step * TIMESTEP;
    if (!damaged || !(kept > 0.0f && kept < 1.0f) || !(closed > 0.0f && closed < 1.0f) || !(now > 0.0f && now < 1.0f))
        return share;
    float settled = kept * closed / (1.0f - kept * (1.0f - closed));
    return pow3(settled / (now + settled * (1.0f - now)), 1.0f / damaged);
}

/* ------------------------------------------------------------------------ */
/* A boat's slam into a wave                                                */
/* ------------------------------------------------------------------------ */
/* ProcessBoatControl slams a boat when its immersion rose by 0.02 since the
 * last frame: a rise per frame, so at high fps the slams faded out. The rise
 * is taken over a 30 fps step, on the frame the step ends. */
int32_t __cdecl cf_wave_rise_c(double rise)
{
    float r = ratio30();
    if (r < 1.0f) {
        if (!cf_due30()) return 0;
        rise /= r;
    }
    if (!isfinite(rise)) return 0;
    return (int32_t)(rise > 1.0e9 ? 1.0e9 : rise < -1.0e9 ? -1.0e9 : rise);
}

/* ------------------------------------------------------------------------ */
/* Rolls and counts per 30 fps step                                          */
/* ------------------------------------------------------------------------ */
uint32_t __cdecl cf_count30_now(void)
{
    cf_due30();
    return cf_steps30;
}

typedef int32_t (__cdecl *RangeFn)(int32_t, int32_t);
typedef int32_t (__cdecl *RandFn)(void);
#define RANDOM_IN_RANGE ((RangeFn)0x407180)     /* CGeneral::GetRandomNumberInRange */
#define RAND ((RandFn)0x821B1E)

/* A chance rolled once a frame, rolled on the frames a 30 fps step ends;
 * on the others a value that fails it */
int32_t __cdecl cf_roll_range(int32_t low, int32_t high) { return cf_due30() ? RANDOM_IN_RANGE(low, high) : high; }
int32_t __cdecl cf_roll_fire(void) { return cf_due30() ? RAND() : 1; }
int32_t __cdecl cf_roll_byte(void) { return cf_due30() ? RAND() : 0xFF; }
int32_t __cdecl cf_roll_word(void) { return cf_due30() ? RAND() : 0xFFFF; }

/* An effect started once a frame (a swimmer's splash), started per 30 fps step */
typedef void *(__attribute__((thiscall)) *CreateFxFn)(void *, const char *, const void *, void *, uint8_t);
void *__attribute__((thiscall)) cf_fx_gate(void *manager, const char *name, const void *at, void *matrix, uint8_t bounds)
{
    return cf_due30() ? ((CreateFxFn)0x4A9BE0)(manager, name, at, matrix, bounds) : NULL;
}

/* ------------------------------------------------------------------------ */
/* Peds                                                                     */
/* ------------------------------------------------------------------------ */
/* CTaskSimpleHoldEntity walks the ped onto a box by offset / timestep * 0.1
 * a frame: 6% of the gap at 30 fps, at 300 fps a hundred times as fast. As
 * the share a 30 fps step takes, for the * 0.1 after. */
float __cdecl cf_pickup_share(float offset)
{
    float r = ratio30();
    if (r >= 1.0f) return offset / TIMESTEP;
    float share = 1.0f - pow3(1.0f - 0.1f / cf_k30, r);
    return offset * share / 0.1f;
}

/* UpdateFatAndMuscleStats adds ms * rate / 10 to m_FatCounter as integers:
 * at 500 fps every rate under 5 rounds to nothing */
static double fat_carry;
void __cdecl cf_fat_add_c(uint32_t ms, uint32_t rate)
{
    double value = (double)((uint64_t)ms * rate) / 10.0 + fat_carry;
    if (!(value > 0.0) || !isfinite(value)) return;
    uint32_t whole = (uint32_t)value;
    fat_carry = value - whole;
    *(uint32_t *)0xB794FC += whole;
}

/* CPed::PlayFootSteps counts a bloody footprint's life down by one a frame */
static struct { const void *ped; uint32_t frame; float carry; } prints[16];
uint32_t __cdecl cf_footprint(const void *ped, uint32_t counter)
{
    if (!counter) return 0;
    float r = ratio30();
    if (r >= 1.0f) return counter - 1;
    uint32_t frame = FRAME_COUNTER;
    int k = -1, oldest = 0;
    for (int i = 0; i < 16 && k < 0; i++) {
        if (prints[i].ped == ped) k = i;
        else if (frame - prints[i].frame > frame - prints[oldest].frame) oldest = i;
    }
    if (k < 0 || frame - prints[k].frame > 2) {
        if (k < 0) k = oldest;
        prints[k].ped = ped, prints[k].carry = 0;
    }
    prints[k].frame = frame;
    float total = prints[k].carry + r;
    uint32_t steps = (uint32_t)total;
    prints[k].carry = total - steps;
    return counter > steps ? counter - steps : 0;
}

/* ------------------------------------------------------------------------ */
/* The map's mouse wheel                                                    */
/* ------------------------------------------------------------------------ */
/* The pause map zooms on a 20 ms repeat, but a wheel notch is up for one
 * frame: at 2000 fps one notch in forty landed. A notch's first frame passes. */
uint8_t cf_wheel_up, cf_wheel_down;
static uint8_t wheel_up_was, wheel_down_was;
void __cdecl cf_map_wheel_sample(void)
{
    uint8_t up = *(volatile const uint8_t *)0xB7341B, down = *(volatile const uint8_t *)0xB7341C;
    cf_wheel_up = up && !wheel_up_was, cf_wheel_down = down && !wheel_down_was;
    wheel_up_was = up, wheel_down_was = down;
}

/* ------------------------------------------------------------------------ */
/* The aim camera                                                           */
/* ------------------------------------------------------------------------ */
/* Aiming on foot, the camera shook at high fps; with the timestep held at
 * least 1 through CCamera::Process and Process_AimWeapon (50 fps) it does
 * not. The zoom keeps the real timestep. */
#define TIMESTEP_RW (*(float *)0xB7CB5C)
#define TIMESTEP_NC (*(float *)0xB7CB58)        /* ms_fTimeStepNonClipped */
static int aim_depth;
static float aim_saved, aim_saved_nc;

static int aim_mode(int16_t mode) { return mode == 53 || mode == 65; }

int __cdecl cf_aiming(const uint8_t *camera)
{
    const uint8_t *ped = *(const uint8_t **)0xB6F5F0;
    if (camera != (const uint8_t *)0xB6F028 || !ped || (ped[0x46D] & 1)) return 0;
    if (aim_mode(*(const int16_t *)(camera + 0x830))) return 1;
    uint8_t active = camera[0x59];
    if (active > 2) return 0;
    for (int i = 0; i < 3; i++) if (aim_mode(*(const int16_t *)(camera + 0x174 + i * 0x238 + 0x0C))) return 1;
    return 0;
}

void __cdecl cf_aim_begin(void)
{
    if (aim_depth++) return;
    aim_saved = TIMESTEP_RW, aim_saved_nc = TIMESTEP_NC;
    if (aim_saved > 0.00001f && aim_saved < 1.0f) TIMESTEP_RW = 1.0f;
    if (aim_saved_nc > 0.00001f && aim_saved_nc < 1.0f) TIMESTEP_NC = 1.0f;
}

void __cdecl cf_aim_end(void)
{
    if (aim_depth <= 0 || --aim_depth) return;
    TIMESTEP_RW = aim_saved, TIMESTEP_NC = aim_saved_nc;
}

float __cdecl cf_real_timestep_c(void) { return aim_depth > 0 ? aim_saved : TIMESTEP; }

/* ------------------------------------------------------------------------ */
/* Spray, flame and extinguisher                                            */
/* ------------------------------------------------------------------------ */
static int continuous(int32_t type) { return type == 37 || type == 41 || type == 42; }

/* CreateParticles turns the time since its last call into whole particles
 * and drops the fraction, and a weapon's emitter starts again each frame: at
 * high fps no particle came out. The fraction is kept per blueprint. */
static struct { const void *blueprint; float intensity; } fx_carry[16];

float *__cdecl cf_weapon_fx_pre(const uint8_t *emitter)
{
    const uint8_t *system = *(const uint8_t **)(emitter + 8);
    if (!system || !(system[0x62] & 0x20)) return NULL;
    const void *blueprint = *(const void **)(emitter + 4);
    int k = -1;
    for (int i = 0; i < 16 && k < 0; i++) if (fx_carry[i].blueprint == blueprint) k = i;
    for (int i = 0; i < 16 && k < 0; i++) if (!fx_carry[i].blueprint) k = i, fx_carry[i].blueprint = blueprint;
    if (k < 0) return NULL;
    float *intensity = (float *)(emitter + 0x10);
    if (*intensity == 0.0f && fx_carry[k].intensity > 0.0f && fx_carry[k].intensity < 1.0f) *intensity = fx_carry[k].intensity;
    return &fx_carry[k].intensity;
}

void __cdecl cf_weapon_fx_post(const uint8_t *emitter, float *carry)
{
    if (carry) *carry = *(const float *)(emitter + 0x10);
}

/* CWeapon::Fire takes a unit of ammo a frame from these: 30 a second */
static struct { const void *weapon; int32_t type; uint32_t at; float credit; } ammo[16];

int __cdecl cf_take_ammo(const int32_t *weapon)
{
    if (!weapon || !continuous(*weapon) || !(TIMESTEP < cf_k30)) return 1;
    uint32_t now = GAME_MS;
    int k = 0;
    for (int i = 0; i < 16; i++) {
        if (ammo[i].weapon == weapon || !ammo[i].weapon) { k = i; break; }
        if ((int32_t)(ammo[i].at - ammo[k].at) < 0) k = i;
    }
    if (ammo[k].weapon != weapon || ammo[k].type != *weapon) ammo[k].weapon = weapon, ammo[k].type = *weapon, ammo[k].at = now, ammo[k].credit = 0;
    uint32_t since = now - ammo[k].at;
    ammo[k].at = now;
    if (since > 200) ammo[k].credit = 0;
    ammo[k].credit += since * 0.03f;
    if (ammo[k].credit < 1.0f) return 0;
    ammo[k].credit -= (float)(int)ammo[k].credit;
    return 1;
}

/* FireAreaEffect adds a shot (damage, paint, water, and the flame's fire) a
 * frame: 30 a second, per firer */
static struct { const void *creator; uint32_t at, frame; float credit; } shots[16];
static int shot_taken = 1;

static int take_shot(const void *creator)
{
    float r = ratio30();
    if (!(r > 0.0f) || r >= 1.0f) return 1;
    uint32_t now = GAME_MS, frame = FRAME_COUNTER;
    int k = 0;
    for (int i = 0; i < 16; i++) {
        if (shots[i].creator == creator || !shots[i].creator) { k = i; break; }
        if ((int32_t)(shots[i].at - shots[k].at) < 0) k = i;
    }
    if (shots[k].creator != creator || now - shots[k].at > 200)
        shots[k].creator = creator, shots[k].at = now, shots[k].frame = frame, shots[k].credit = 0;
    uint32_t most = (uint32_t)(1.0f / r) + 1, frames = frame - shots[k].frame < most ? frame - shots[k].frame : most;
    shots[k].at = now, shots[k].frame = frame;
    shots[k].credit += r * frames;
    if (shots[k].credit < 1.0f) return 0;
    shots[k].credit -= 1.0f;
    return 1;
}

typedef uint8_t (__cdecl *AddShotFn)(void *, int32_t, float, float, float, float, float, float);
uint8_t __cdecl cf_add_shot(void *creator, int32_t type, float ox, float oy, float oz, float tx, float ty, float tz)
{
    shot_taken = take_shot(creator);
    return shot_taken ? ((AddShotFn)0x739C30)(creator, type, ox, oy, oz, tx, ty, tz) : 0;
}

typedef uint8_t (__cdecl *CreepingFireFn)(float, float, float, int32_t, uint8_t, uint8_t, float);
uint8_t __cdecl cf_creeping_fire(float x, float y, float z, int32_t generations, uint8_t spread, uint8_t script, float dz)
{
    return shot_taken ? ((CreepingFireFn)0x53A450)(x, y, z, generations, spread, script, dz) : 0;
}

/* a shot let through stands for a 30 fps step: its water puts out that much */
typedef uint8_t (__attribute__((thiscall)) *ExtinguishFn)(void *, float, float, float, float, float);
uint8_t __attribute__((thiscall)) cf_extinguish(void *manager, float x, float y, float z, float radius, float strength)
{
    float r = ratio30();
    if (r > 0.0f && r < 1.0f) strength /= r;
    return ((ExtinguishFn)0x5394C0)(manager, x, y, z, radius, strength);
}

/* The chainsaw: its moving-attack animation is rewound past `hit` each time
 * it passes `chain`, a loop shorter than a frame, so it struck on every
 * second frame: 15 a second at 30 fps, 45 at 144. The rewind is held back
 * until 2/30 s has passed since the last strike. */
static struct { const void *anim; uint32_t at; float credit; } saws[8];
float cf_saw_rewind = 0.01f;

void __cdecl cf_chainsaw_c(const void *anim)
{
    cf_saw_rewind = 0.01f;
    if (!anim || !(TIMESTEP < cf_k30)) return;
    uint32_t now = GAME_MS;
    int k = -1, oldest = 0;
    for (int i = 0; i < 8 && k < 0; i++) {
        if (saws[i].anim == anim) k = i;
        else if (now - saws[i].at > now - saws[oldest].at) oldest = i;
    }
    if (k < 0) k = oldest, saws[k].anim = anim, saws[k].at = now - 301, saws[k].credit = 0;
    int armed = 1;
    if (now - saws[k].at > 300) saws[k].credit = 0;
    else {
        saws[k].credit += now - saws[k].at;
        if (saws[k].credit >= 2000.0f / 30.0f) {
            saws[k].credit -= 2000.0f / 30.0f;
            if (saws[k].credit > 2000.0f / 30.0f) saws[k].credit = 2000.0f / 30.0f;
        } else armed = 0;
    }
    saws[k].at = now;
    cf_saw_rewind = armed ? 0.01f : -0.001f;
}

/* ------------------------------------------------------------------------ */
/* The radio's 30 fps                                                       */
/* ------------------------------------------------------------------------ */
/* The main loop runs the frame limiter while the music has a beat track,
 * which the radio of the player's car has: a car ride held at 30 fps. Only
 * the minigames danced or bounced to the beat keep it. */
typedef uint8_t (__attribute__((thiscall)) *BeatFn)(void *);

static int script_running(const char *name)
{
    const uint8_t *script = *(const uint8_t *const *)0xA8B42C;           /* CTheScripts::pActiveScripts */
    for (int guard = 0; script && guard < 1000; guard++, script = *(const uint8_t *const *)script)
        if (!strncmp((const char *)script + 8, name, 8)) return 1;
    return 0;
}

uint8_t __attribute__((thiscall)) cf_beat_limit(void *audio)
{
    if (!((BeatFn)0x5071D0)(audio)) return 0;
    return script_running("dance") || script_running("lowgame") || script_running("bdisply");
}

/* ------------------------------------------------------------------------ */
/* SA-MP's moving objects                                                   */
/* ------------------------------------------------------------------------ */
/* SA-MP moves an object with a move speed and turns it by how far it has
 * come: a server moving it by millimetres a frame left it short of float
 * precision at high fps, so it never turned and never arrived. Both are
 * taken from the time it has moved (SA-MP's own elapsed * speed). */
uint32_t cf_samp_rot_back, cf_samp_arrived, cf_samp_moving;

float __cdecl cf_samp_rot_left(float elapsed, float total)
{
    if (!isfinite(total) || total <= 0.0f) return 0.0f;
    if (!isfinite(elapsed) || elapsed <= 0.0f) return 1.0f;
    return elapsed >= total ? 0.0f : 1.0f - elapsed / total;
}

int __cdecl cf_samp_expired(float elapsed, float total)
{
    return isfinite(total) && total > 0.0f && isfinite(elapsed) && elapsed >= total;
}

/* ------------------------------------------------------------------------ */
/* The hook bodies                                                          */
/* ------------------------------------------------------------------------ */
__asm__(
".intel_syntax noprefix\n"
/* For call _ftol: st0 popped, the integer in edx:eax, ecx kept */
".globl _cf_ftol_carry\n"
"_cf_ftol_carry:\n"
"    push ecx\n"
"    sub esp, 8\n"
"    fstp qword ptr [esp]\n"
"    push dword ptr [esp+12]\n"         /* the return address names the site */
"    call _cf_ftol_carried\n"
"    add esp, 12\n"
"    pop ecx\n"
"    cdq\n"
"    ret\n"
/* The constant steps of 6 bytes, called: a step per 30 fps step */
".globl _cf_sub10\n"
"_cf_sub10:\n"                          /* fsub [0.1] */
"    fld dword ptr ds:0x858B1C\n"
"    SCALED\n"
"    fsubp st(1), st\n"
"    ret\n"
".globl _cf_add05\n"
"_cf_add05:\n"                          /* fadd [0.05] */
"    fld dword ptr ds:0x858C28\n"
"    SCALED\n"
"    faddp st(1), st\n"
"    ret\n"
".globl _cf_sub05\n"
"_cf_sub05:\n"                          /* fsub [0.05] */
"    fld dword ptr ds:0x858C28\n"
"    SCALED\n"
"    fsubp st(1), st\n"
"    ret\n"
".globl _cf_decay95\n"
"_cf_decay95:\n"                        /* fmul [0.95] */
"    sub esp, 4\n"
"    fstp dword ptr [esp]\n"
"    pushfd\n"
"    pushad\n"
"    push dword ptr ds:0x858EF0\n"
"    call _cf_per30\n"
"    add esp, 4\n"
"    popad\n"
"    popfd\n"
"    fmul dword ptr [esp]\n"
"    add esp, 4\n"
"    ret\n"
".globl _cf_sink_step\n"
"_cf_sink_step:\n"                      /* fmul [8e-6] */
"    fmul dword ptr ds:0x871244\n"
"    SCALED\n"
"    ret\n"
".globl _cf_settle\n"
"_cf_settle:\n"                         /* fmul [0.75] */
"    sub esp, 4\n"
"    fstp dword ptr [esp]\n"
"    pushfd\n"
"    pushad\n"
"    call _cf_settle_weight\n"
"    popad\n"
"    popfd\n"
"    fmul dword ptr [esp]\n"
"    add esp, 4\n"
"    ret\n"

/* CAutomobile::HydraulicControl: inc ecx / mov [ebp+86Ch], cx on a step */
".globl _cf_hyd_raise\n"
"_cf_hyd_raise:\n"
"    push eax\n"
"    push ecx\n"
"    push edx\n"
"    call _cf_due30\n"
"    test eax, eax\n"
"    pop edx\n"
"    pop ecx\n"
"    pop eax\n"
"    je 1f\n"
"    inc ecx\n"
"    mov word ptr [ebp+0x86C], cx\n"
"1:  ret\n"
/* lea eax, [ecx-1] / test ax, ax on a step; ecx itself (not zero) on others */
".globl _cf_hyd_lower\n"
"_cf_hyd_lower:\n"
"    push ecx\n"
"    push edx\n"
"    call _cf_due30\n"
"    pop edx\n"
"    pop ecx\n"
"    test eax, eax\n"
"    lea eax, [ecx-1]\n"
"    jne 1f\n"
"    mov eax, ecx\n"
"1:  test ax, ax\n"
"    ret\n"

/* For call _ftol at the wave's rise */
".globl _cf_wave_rise\n"
"_cf_wave_rise:\n"
"    push ecx\n"
"    sub esp, 8\n"
"    fstp qword ptr [esp]\n"
"    call _cf_wave_rise_c\n"
"    add esp, 8\n"
"    pop ecx\n"
"    cdq\n"
"    ret\n"

/* JustFinishedAccelerationLoop: cmp ax, [the loop's frame count] */
"HOOK gearkick\n"
"    pushad\n"
"    call _cf_loop_frames_update\n"
"    popad\n"
"    cmp ax, word ptr [_cf_loop_frames]\n"
"    GO 0x4F5E60\n"

/* ProcessControlInputs: st0 is the gap to the pad, times timestep * k */
".macro SLEW k, back\n"
"    sub esp, 4\n"
"    fstp dword ptr [esp]\n"
"    pushfd\n"
"    pushad\n"
"    push dword ptr ds:\\k\n"
"    call _cf_slew\n"
"    add esp, 4\n"
"    popad\n"
"    popfd\n"
"    fmul dword ptr [esp]\n"
"    add esp, 4\n"
"    GO \\back\n"
".endm\n"
"HOOK carsteer1\n"
"    SLEW 0x871058, 0x6AD86E\n"
"HOOK carsteer2\n"
"    SLEW 0x871058, 0x6AD92F\n"
"HOOK bikesteer1\n"
"    SLEW 0x871300, 0x6BE4B1\n"
"HOOK bikesteer2\n"
"    SLEW 0x871300, 0x6BE602\n"
"HOOK planesteer1\n"
"    SLEW 0x858CC4, 0x6CAF36\n"
"HOOK planesteer2\n"
"    SLEW 0x858CC4, 0x6CB076\n"
"HOOK planesteer3\n"
"    SLEW 0x858CC4, 0x6CB0BB\n"
/* the rudder's fmul [0.2], which the plane's paths jump to (0x6CB238,
 * 0x6CB24D) with gap * timestep on st0: the share per timestep */
"HOOK planesteer4\n"
"    sub esp, 4\n"
"    fstp dword ptr [esp]\n"
"    pushfd\n"
"    pushad\n"
"    push dword ptr ds:0x858CC4\n"
"    call _cf_slew_per_step\n"
"    add esp, 4\n"
"    popad\n"
"    popfd\n"
"    fmul dword ptr [esp]\n"
"    add esp, 4\n"
"    GO 0x6CB1B7\n"

/* FlyingControl: st0 the speed term, [esp+N] pow(resistance, timestep), edi
 * the flying handling (the resistances at +40h, +44h, +48h) */
".macro FLYTURN at, res, back\n"
"    sub esp, 4\n"
"    fstp dword ptr [esp]\n"
"    push dword ptr [esp+\\at+4]\n"
"    push dword ptr [edi+\\res]\n"
"    call _cf_fly_turn\n"
"    add esp, 12\n"
"    GO \\back\n"
".endm\n"
"HOOK flyturn1\n"
"    FLYTURN 0x1C, 0x40, 0x6DAA20\n"
"HOOK flyturn2\n"
"    FLYTURN 0x20, 0x44, 0x6DAA73\n"
"HOOK flyturn3\n"
"    FLYTURN 0x24, 0x48, 0x6DAAC6\n"

/* CPhysical::ApplySpringDampening(level, limit, direction, point, speed),
 * this in ecx: the level rewritten where the caller pushed it */
"HOOK springdamp\n"
"    pushad\n"
"    lea eax, [esp+36]\n"
"    push eax\n"
"    push ecx\n"
"    call _cf_spring_level\n"
"    add esp, 8\n"
"    popad\n"
"    sub esp, 0x28\n"
"    mov eax, dword ptr [esp+0x3C]\n"
"    GO 0x543E97\n"
/* CPhysical::ProcessControl, this in ecx */
"HOOK loadlean\n"
"    pushad\n"
"    push ecx\n"
"    call _cf_frame_start\n"
"    add esp, 4\n"
"    popad\n"
"    push esi\n"
"    mov esi, ecx\n"
"    mov al, byte ptr [esi+0x36]\n"
"    mov cl, al\n"
"    GO 0x5485E8\n"

/* m_fMovingSpeed (+D4h), how far the entity moved this frame, as a 30 fps step's */
".macro REST back\n"
"    fld dword ptr [esi+0xD4]\n"
"    fmul dword ptr [_cf_k30]\n"
"    fdiv dword ptr ds:0xB7CB5C\n"
"    GO \\back\n"
".endm\n"
"HOOK carrest\n"
"    REST 0x6B1CA2\n"
"HOOK bikerest\n"
"    REST 0x6B995B\n"
"HOOK trainrest\n"
"    REST 0x6F9B98\n"

/* m_nFakePhysics (+B8h) counts on a 30 fps step, and past 10 on every frame */
".macro SLEEP reg, back\n"
"    pushfd\n"
"    pushad\n"
"    call _cf_due30\n"
"    mov dword ptr [esp+28], eax\n"
"    popad\n"
"    popfd\n"
"    mov \\reg, byte ptr [esi+0xB8]\n"
"    test eax, eax\n"
"    jnz 1f\n"
"    cmp \\reg, 10\n"
"    jb 2f\n"
"1:  inc \\reg\n"
"2:  GO \\back\n"
".endm\n"
"HOOK objsleep\n"
"    SLEEP cl, 0x5A2427\n"
"HOOK trainsleep\n"
"    SLEEP bl, 0x6F9BD9\n"

/* UpdateMovingCollision: edi the vehicle, ebx its driver's pad, [esp+1Ch] the rate */
"HOOK movingparts\n"
"    push dword ptr [esp+0x1C]\n"
"    push ebx\n"
"    push edi\n"
"    call _cf_moving_part\n"
"    add esp, 12\n"
"    GO 0x6A172B\n"

/* ProcessFlyingCarStuff: st0 is 1 - 0.2 * status, for the input at esi+off */
".macro LOSS axis, off, back\n"
"    sub esp, 4\n"
"    fstp dword ptr [esp]\n"
"    pushfd\n"
"    pushad\n"
"    push \\axis\n"
"    push esi\n"
"    push dword ptr [esp+44]\n"
"    call _cf_plane_loss\n"
"    add esp, 12\n"
"    popad\n"
"    popfd\n"
"    add esp, 4\n"
"    fmul dword ptr [esi+\\off]\n"
"    GO \\back\n"
".endm\n"
"HOOK planeloss1\n"
"    LOSS 0, 0x988, 0x6CBAE2\n"
"HOOK planeloss2\n"
"    LOSS 1, 0x98C, 0x6CBBFB\n"
"HOOK planeloss3\n"
"    LOSS 2, 0x990, 0x6CBD20\n"

/* ProcessCarWheelPair: the burnout's 3000 */
"HOOK burnspeed\n"
"    fld dword ptr ds:0x859A94\n"
"    SCALED\n"
"    GO 0x6A4FEC\n"
/* CBike::ProcessControl: st0 the front wheel's speed, added to its turn */
"HOOK bikepitch\n"
"    SCALED\n"
"    fadd dword ptr [esi+0x750]\n"
"    GO 0x6BAC8F\n"

/* CBmx::ProcessDrivingAnims: st0 the 0.95, for the lean at esi+off */
".macro BMXLEAN off, back\n"
"    pushfd\n"
"    pushad\n"
"    sub esp, 4\n"
"    fstp dword ptr [esp]\n"
"    call _cf_per30\n"
"    add esp, 4\n"
"    popad\n"
"    popfd\n"
"    fmul dword ptr [esi+\\off]\n"
"    GO \\back\n"
".endm\n"
"HOOK bmxlean1\n"
"    BMXLEAN 0x654, 0x6C006D\n"
"HOOK bmxlean2\n"
"    BMXLEAN 0x658, 0x6C007F\n"
"HOOK bmxlean3\n"
"    BMXLEAN 0x654, 0x6C0100\n"
"HOOK bmxlean4\n"
"    BMXLEAN 0x658, 0x6C0112\n"

/* PositionAttachedEntity: [esp+0Ch] = 1 / timestep, st0 kept */
"HOOK attached\n"
"    fld dword ptr ds:0xB7CB5C\n"
"    fdivr dword ptr ds:0x858624\n"
"    fstp dword ptr [esp+0x0C]\n"
"    GO 0x547823\n"
/* the autopilot: the divisor the timestep itself */
"HOOK aisteer\n"
"    fld dword ptr ds:0xB7CB5C\n"
"    GO 0x4235F3\n"

/* Peds */
".globl _cf_add10\n"
"_cf_add10:\n"                          /* fadd [0.1] */
"    fld dword ptr ds:0x858B1C\n"
"    SCALED\n"
"    faddp st(1), st\n"
"    ret\n"
/* fdiv [timestep] for the * 0.1 after: the share a 30 fps step takes */
".globl _cf_pickup_step\n"
"_cf_pickup_step:\n"
"    pushfd\n"
"    pushad\n"
"    sub esp, 4\n"
"    fstp dword ptr [esp]\n"
"    call _cf_pickup_share\n"
"    add esp, 4\n"
"    popad\n"
"    popfd\n"
"    ret\n"
/* m_FatCounter += eax * [the caller's esp+8] / 10, the remainder kept */
".globl _cf_fat_add\n"
"_cf_fat_add:\n"
"    push ecx\n"
"    push edx\n"
"    push dword ptr [esp+0x14]\n"
"    push eax\n"
"    call _cf_fat_add_c\n"
"    add esp, 8\n"
"    pop edx\n"
"    pop ecx\n"
"    mov eax, dword ptr ds:0xB794FC\n"
"    ret\n"
/* dec ecx / test ecx, ecx / mov [esi+750h], ecx */
"HOOK footprints\n"
"    pushad\n"
"    push ecx\n"
"    push esi\n"
"    call _cf_footprint\n"
"    add esp, 8\n"
"    mov dword ptr [esp+24], eax\n"
"    popad\n"
"    test ecx, ecx\n"
"    mov dword ptr [esi+0x750], ecx\n"
"    GO 0x5E5880\n"
/* CPad::Update: the drunk steering buffer shifts on a 30 fps step */
"HOOK drunksteer\n"
"    push edx\n"
"    call _cf_due30\n"
"    pop edx\n"
"    test eax, eax\n"
"    je 1f\n"
"    lea eax, [ebx+0x72]\n"
"    mov ecx, 9\n"
"    GO 0x541D35\n"
"1:  GO 0x541D42\n"

/* World */
/* CFire::ProcessFire: test al, mask / jne skip, on a 30 fps step */
".macro FIRE mask, go, skip\n"
"    push eax\n"
"    push ecx\n"
"    push edx\n"
"    call _cf_due30\n"
"    test eax, eax\n"
"    pop edx\n"
"    pop ecx\n"
"    pop eax\n"
"    je 1f\n"
"    test al, \\mask\n"
"    jne 1f\n"
"    GO \\go\n"
"1:  GO \\skip\n"
".endm\n"
"HOOK firecar\n"
"    FIRE 0x1F, 0x53A8D2, 0x53AA05\n"
"HOOK firespread\n"
"    FIRE 0x7F, 0x53AAC3, 0x53AC22\n"
"HOOK firemerge\n"
"    FIRE 0x0F, 0x53AC4E, 0x53AD72\n"
/* CExplosion::Update: inc byte [esi-8], stopping at 255 */
"HOOK blastframes\n"
"    cmp byte ptr [esi-8], 0xFF\n"
"    je 1f\n"
"    inc byte ptr [esi-8]\n"
"1:  GO 0x7379F7\n"
/* mov eax, [CTimer::m_FrameCounter]: the 30 fps steps counted */
".globl _cf_count30\n"
"_cf_count30:\n"
"    pushfd\n"
"    push ecx\n"
"    push edx\n"
"    call _cf_count30_now\n"
"    pop edx\n"
"    pop ecx\n"
"    popfd\n"
"    ret\n"
/* the engine note's step to the pedal: fld [0.09], fsub [0.07] */
".globl _cf_revs_up\n"
"_cf_revs_up:\n"
"    fld dword ptr ds:0x8CBC24\n"
"    SCALED\n"
"    ret\n"
".globl _cf_revs_down\n"
"_cf_revs_down:\n"
"    fld dword ptr ds:0x8CBC28\n"
"    SCALED\n"
"    fsubp st(1), st\n"
"    ret\n"
/* test [CTimer::m_FrameCounter], 3FFh: zero once every 1024 30 fps steps */
".globl _cf_ambience\n"
"_cf_ambience:\n"
"    push eax\n"
"    push ecx\n"
"    push edx\n"
"    call _cf_due30\n"
"    test eax, eax\n"
"    je 1f\n"
"    mov eax, dword ptr [_cf_steps30]\n"
"    test eax, 0x3FF\n"
"    jmp 2f\n"
"1:  or eax, 1\n"
"2:  pop edx\n"
"    pop ecx\n"
"    pop eax\n"
"    ret\n"

/* The HUD: the frame counter's bits on a 25 fps clock (game ms / 40) */
".macro CLOCK25\n"
"    push edx\n"
"    push ecx\n"
"    mov eax, dword ptr ds:0xB7CB84\n"
"    xor edx, edx\n"
"    mov ecx, 40\n"
"    div ecx\n"
"    pop ecx\n"
"    pop edx\n"
".endm\n"
".macro FLASH mask, back\n"
"    push eax\n"
"    CLOCK25\n"
"    test al, \\mask\n"
"    pop eax\n"
"    GO \\back\n"
".endm\n"
"HOOK hudflash1\n"
"    FLASH 8, 0x5890B4\n"
"HOOK hudflash2\n"
"    FLASH 8, 0x5891A4\n"
"HOOK hudflash3\n"
"    pushfd\n"
"    push eax\n"
"    CLOCK25\n"
"    mov bl, al\n"
"    pop eax\n"
"    popfd\n"
"    GO 0x589282\n"
"HOOK hudflash4\n"
"    FLASH 8, 0x58A368\n"
"HOOK hudflash5\n"
"    FLASH 4, 0x58DDC1\n"
"HOOK hudflash6\n"
"    FLASH 4, 0x58DE6E\n"
/* the map: the wheel sampled, then fld [8653F4h] */
".globl _cf_map_wheel\n"
"_cf_map_wheel:\n"
"    pushfd\n"
"    pushad\n"
"    call _cf_map_wheel_sample\n"
"    popad\n"
"    popfd\n"
"    fld dword ptr ds:0x8653F4\n"
"    ret\n"
"HOOK mapzoomin\n"
"    cmp eax, 0x14\n"
"    ja 1f\n"
"    cmp byte ptr [_cf_wheel_up], 0\n"
"    jne 1f\n"
"    GO 0x577918\n"
"1:  GO 0x57765F\n"
"HOOK mapzoomout\n"
"    cmp eax, 0x14\n"
"    ja 1f\n"
"    cmp byte ptr [_cf_wheel_down], 0\n"
"    jne 1f\n"
"    GO 0x5779FE\n"
"1:  GO 0x57798E\n"

/* The aim camera: CCamera::Process (this in ecx) with the timestep held */
"HOOK aimcam\n"
"    push ecx\n"
"    push ecx\n"
"    call _cf_aiming\n"
"    add esp, 4\n"
"    pop ecx\n"
"    test eax, eax\n"
"    jz 2f\n"
"    push ecx\n"
"    call _cf_aim_begin\n"
"    pop ecx\n"
"    call 1f\n"
"    jmp _cf_aim_end\n"                 /* returns for us */
"1:  sub esp, 0xA0\n"
"    GO 0x52B736\n"
"2:  sub esp, 0xA0\n"
"    GO 0x52B736\n"
/* CCam::Process_AimWeapon(target, orientation, speed, wanted), callee pops 16 */
"HOOK aimweapon\n"
"    push ecx\n"
"    call _cf_aim_begin\n"
"    pop ecx\n"
"    push dword ptr [esp+16]\n"
"    push dword ptr [esp+16]\n"
"    push dword ptr [esp+16]\n"
"    push dword ptr [esp+16]\n"
"    call 1f\n"
"    call _cf_aim_end\n"
"    ret 16\n"
"1:  mov al, byte ptr ds:0xB70110\n"
"    GO 0x521505\n"
/* fld [timestep]: the one before the hold */
".globl _cf_real_timestep\n"
"_cf_real_timestep:\n"
"    pushfd\n"
"    push eax\n"
"    push ecx\n"
"    push edx\n"
"    call _cf_real_timestep_c\n"
"    pop edx\n"
"    pop ecx\n"
"    pop eax\n"
"    popfd\n"
"    ret\n"

/* Weapons */
/* FxEmitter_c::CreateParticles(time, delta), this in ecx, callee pops 8:
 * a weapon emitter's fraction put back before, kept after */
"HOOK weaponfx\n"
"    push ecx\n"
"    push ecx\n"
"    call _cf_weapon_fx_pre\n"
"    add esp, 4\n"
"    pop ecx\n"
"    push eax\n"
"    push ecx\n"
"    push dword ptr [esp+16]\n"
"    push dword ptr [esp+16]\n"
"    call 1f\n"
"    call _cf_weapon_fx_post\n"
"    add esp, 8\n"
"    ret 8\n"
"1:  sub esp, 0x8C\n"
"    GO 0x4A41E6\n"
/* CWeapon::Fire (the weapon in esi): mov eax, [esi+8] / test eax, eax on a unit's turn */
"HOOK weaponammo\n"
"    pushfd\n"
"    pushad\n"
"    push esi\n"
"    call _cf_take_ammo\n"
"    add esp, 4\n"
"    mov dword ptr [esp+28], eax\n"
"    popad\n"
"    popfd\n"
"    test eax, eax\n"
"    jz 1f\n"
"    mov eax, dword ptr [esi+8]\n"
"    test eax, eax\n"
"    GO 0x7428AD\n"
"1:  GO 0x7428E9\n"
/* fsub [0.01]: the chainsaw's rewind, held back between strikes (the anim in ecx) */
".globl _cf_chainsaw\n"
"_cf_chainsaw:\n"
"    pushfd\n"
"    push eax\n"
"    push edx\n"
"    push ecx\n"
"    push ecx\n"
"    call _cf_chainsaw_c\n"
"    add esp, 4\n"
"    pop ecx\n"
"    pop edx\n"
"    pop eax\n"
"    popfd\n"
"    fsub dword ptr [_cf_saw_rewind]\n"
"    ret\n"

/* SA-MP's CObject::Process, the object in ebx: the turn left from the time
 * moved ([esp+28h]), over the distance (+15Bh) */
"HOOK samprot\n"
"    pushfd\n"
"    push eax\n"
"    push ecx\n"
"    push edx\n"
"    push dword ptr [ebx+0x15B]\n"
"    push dword ptr [esp+0x38]\n"
"    call _cf_samp_rot_left\n"
"    add esp, 8\n"
"    pop edx\n"
"    pop ecx\n"
"    pop eax\n"
"    popfd\n"
"    jmp dword ptr [_cf_samp_rot_back]\n"
/* arrived when the time moved ([esp+20h]) covers the distance, or as SA-MP tests */
"HOOK sampmove\n"
"    pushfd\n"
"    push eax\n"
"    push ecx\n"
"    push edx\n"
"    push dword ptr [ebx+0x15B]\n"
"    push dword ptr [esp+0x30]\n"
"    call _cf_samp_expired\n"
"    add esp, 8\n"
"    test eax, eax\n"
"    pop edx\n"
"    pop ecx\n"
"    pop eax\n"
"    jnz 1f\n"
"    popfd\n"
"    fld dword ptr [esp+0x24]\n"
"    fcomp dword ptr [esp+0x20]\n"
"    fnstsw ax\n"
"    test ah, 1\n"
"    jne 2f\n"
"    jmp dword ptr [_cf_samp_arrived]\n"
"1:  popfd\n"
"    jmp dword ptr [_cf_samp_arrived]\n"
"2:  jmp dword ptr [_cf_samp_moving]\n"
".att_syntax\n");
