# Audio system plan

Status: not started. Phase 1 starts with [`sync-primitives.md`](sync-primitives.md).

## Context
Fling has no audio. We use the Godot model: the engine owns the mixer and talks to
each platform's own audio API. No miniaudio, FMOD, OpenAL, or Wwise. v1 scope:
- sound effects
- spatial (3D) sound
- background music

Anything more complex comes later. The work also needs:
- a Fling audio asset type that reads WAV, MP3, and Ogg
- the Core primitives Fling lacks. Today only the threading files use `std::mutex`,
  `std::condition_variable`, and `std::atomic`, directly. There are no engine sync types.

Decisions:
- **Decoders:** dr_wav and dr_mp3 from github.com/mackron/dr_libs (public domain or MIT-0),
  through a `flingengine/` fork submodule. Ogg uses the stb_vorbis that is already vendored.
- **Linux:** PulseAudio (`libpulse-simple.so.0`) first, then ALSA (`libasound.so.2`), then the
  null device. Both libraries load at runtime through `Fling::DynamicLibrary`.
- **Sync types:** thin Core wrappers over std types, with Clang thread-safety annotations.
- **Windows:** WASAPI.
- **macOS:** not in v1. `Core/inc/Platform.h` stops the build with `#error` on Apple.

This file is the design. The phases are in the order of the PRs.
Code comments must not mention other engines.

## Existing code to reuse
- Threads: `Core/inc/Threading/Thread.h`, `Runnable.h`, `ThreadTypes.h` (`ThreadPriority::TimeCritical`).
- Queues: `ThreadQueue.h` (many producers, one consumer).
- Fixed-rate pacing: `TickLimiter.h`. The null device uses it.
- Threading rules: `Skills/threading.md`.
  - Requests are data, not lambdas.
  - Gameplay appends to a queue singleton, and one resolver drains it.
  - No-thread mode for tests.
- Runtime library loading:
  - `Core/inc/DynamicLibrary.h`: `OpenFirst` and `SymbolAs<Fn>`.
  - The X-macro function table pattern in `Graphics/src/VulkanFunctions.inl` and `Graphics/src/VulkanLoader.cpp:90-115`.
- Assets: the `Resource` base (`Resources/inc/Resource.h`) and `ResourceManager::LoadResource<T>`.
  Also the `Create(Guid)` plus constructor pattern in `Resources/inc/File.h` and `src/File.cpp`.
- Third-party libraries: one `#define ..._IMPLEMENTATION` per library, in one `.cpp`
  (see `Graphics/src/Texture.cpp:3`). Externals are declared in `CMake/FlingExternals.cmake:8-32`.
- Spatial data:
  - `Engine/inc/Components/Transform.h` (`m_Pos`, `GetWorldMat()`).
  - `Engine/inc/Camera.h` (`GetPosition`, `GetRotation`) as the fallback listener.
- Engine startup and loop: `Application/src/Engine.cpp`.
  - `ResourceManager::Init` at :73.
  - `m_World->Update` at :211.
  - Shutdown at :247-305.
  - The `--null-render` branches.

## Phase 1: Core primitives (CoreTests)
1. **Sync types** (Mutex, ScopedLock, SharedMutex, SpinLock, ConditionVariable,
   Semaphore, Event, with Clang thread-safety annotations): see
   [`sync-primitives.md`](sync-primitives.md). It is a separate brief, so another agent can do it.
2. `Core/inc/FileSystem.h`: `ReadFileBytes(path) -> std::optional<std::vector<std::byte>>`.
   The decoders read from memory. Moving `File` and `Shader::LoadRawBytes` onto it can be a later cleanup.
   Test it in CoreTests with a missing file and with `Assets/TestFile.txt`.

Not added now, with reasons:
- SPSC ring buffer: the push-model backends below do not need it. Add it with a callback
  backend such as CoreAudio.
- `PlatformTime`: this is phase T1 of [`improve-time.md`](improve-time.md). The null device uses `TickLimiter`.
- Atomic wrappers: `std::atomic` is enough.

## Phase 2: dr_libs and the AudioClip asset (ResourcesTests)
- Submodule: `external/dr_libs` → `flingengine/dr_libs` fork.
  - `fling_add_external(dr_libs ...)` in `FlingExternals.cmake`.
  - An entry in `THIRD_PARTY_NOTICES.md`.
  - Note it in `Plans/submodule-bumps.md` if that doc lists submodules.
- `Resources/CMakeLists.txt`: add `FlingExt::dr_libs` and `FlingExt::stb` as **PRIVATE_LIBS**.
  Public headers include no decoder headers.
- `Resources/src/AudioDecoders.cpp` is the only file that defines
  `DR_WAV_IMPLEMENTATION` and `DR_MP3_IMPLEMENTATION` and includes `stb_vorbis.c`.
  - Graphics defines only `STB_IMAGE_IMPLEMENTATION`, so there is no clash.
  - Do not define `STB_VORBIS_HEADER_ONLY` here.
- `Resources/inc/AudioClip.h`: `class AudioClip : public Resource`, immutable after load.
  - `static std::shared_ptr<AudioClip> Create(Guid, AudioLoadMode = AudioLoadMode::Decode)`.
  - `AudioFormat { uint32 SampleRate; uint16 Channels; uint64 FrameCount; }` (Channels is 1 or 2 in v1).
  - The format is chosen from the file extension (.wav, .mp3, .ogg). An unknown type or a
    failed decode logs `F_LOG_ERROR` and leaves `IsLoaded()` false.
  - `Decode` mode: the whole file becomes `std::vector<float>` (interleaved float32), for sound effects.
  - `Stream` mode: keeps the encoded bytes from `ReadFileBytes`, for music.
  - `std::unique_ptr<AudioDecoder> CreateDecoder() const`. `AudioDecoder` is an abstract class
    with `Read(float* Out, uint64 Frames) -> uint64`, `Seek(uint64 Frame)`, and `GetFormat()`.
    Its concrete wav, mp3, and vorbis classes are private in `src/`. They decode from the clip's memory.
  - Why it is safe on the audio thread: the clip does not change after construction, and the
    audio thread never calls `ResourceManager`. `ResourceManager` is not thread-safe.
- Test assets in `Assets/Audio/Test/`: a short mono WAV, a stereo WAV, an MP3, and an Ogg, each a
  few KB of a known sine wave. Commit small binaries only.
- Tests in `FlingTests/Resources/AudioClipTests.cpp`:
  - each format: sample rate, channel count, and frame count
  - Decode mode and the streamed decoder give the same samples
  - `Seek`
  - a missing file and a bad file fail without a crash

## Phase 3: The Audio module, mixer, and null device (new AudioTests)
New module `FlingEngine/Audio`: `fling_add_module(Audio PUBLIC_DEPS Core Resources Engine)`.
- Follow `Skills/adding-modules.md`.
- It depends on Engine for Transform and the registry, as Graphics does.
- Add it to `Fling::GameStack`, to Application's PUBLIC_DEPS, and to the isolation tests in
  `FlingTests/CMakeLists.txt`. Engine must not see Audio headers.

Files under `Audio/inc`:
- **`AudioTypes.h`**:
  - `VoiceHandle { uint16 Index; uint16 Generation; }`
  - `AudioBus { Master, Music, Effects }`
  - `SpatialParams { vec3 Position; float MinDistance, MaxDistance, Rolloff; }`
  - `ListenerState { vec3 Position, Forward, Up; }`
- **`AudioRequest.h`**: data requests only. Each one copies values, and none captures World or
  a component (threading rule).
  - `PlayVoice { VoiceHandle; shared_ptr<const AudioClip>; AudioBus; float Volume, Pitch; bool Loop; std::optional<SpatialParams>; float FadeInSec; }`
  - `StopVoice { VoiceHandle; float FadeOutSec; }`
  - `SetVoiceParams`
  - `SetListener`
  - `SetBusVolume`
  - The type is a `std::variant`, so a later recorder can record and replay it.
- **`AudioMixer.h`**: pure DSP with no OS calls. It is the core you can test.
  - Output: float32 interleaved stereo at the device rate. 48 kHz is preferred; the device's rate is accepted.
  - A fixed pool of 64 voices. A handle is an index plus a generation. When the pool is full,
    the play request is dropped and a warning is logged once each second.
  - Each voice keeps a frame position as a fixed-point or double value. It resamples with
    linear interpolation to cover clip rate → device rate and pitch.
  - A decoded clip is read from memory. A streamed clip has its own `AudioDecoder`, which
    fills a small ring of about 4096 frames on the mixer thread.
  - Each bus applies a gain, and Master is applied last.
  - Gain changes ramp over one period, so a change does not click.
  - **Spatial:**
    - Volume: clamped inverse-distance attenuation, `Min / (Min + Rolloff * (d - Min))` for d between Min and Max.
    - Pan: equal-power, from `dot(ListenerRight, normalize(Pos - ListenerPos))`.
    - A stereo clip is mixed to mono first.
    - Not in v1: Doppler and HRTF.
  - `Render(float* Out, uint32 Frames)` runs one period:
    1. Apply the pending requests.
    2. Mix.
    3. Clamp to -1…1.
  - When a voice ends, the mixer pushes `VoiceEnded{ VoiceHandle, shared_ptr<const AudioClip> }`
    to a result `ThreadQueue`. The main thread drains that queue, so the last reference to a clip
    is released on the main thread and the audio thread never frees memory. This takes the place
    of a command fence.
- **`AudioDevice.h`**: the backend interface. These are private headers in `Audio/src/Backends/`,
  in the same pattern as `src/Threading/NativeThread.h`.
  - `Open(const AudioDeviceDesc&) -> bool`
  - `GetFormat()`
  - `GetPeriodFrames()`
  - `Write(const float*, Frames)`: blocks until the device has room. This is the push model,
    as in Godot's drivers.
  - `Close()`
  - `NullAudioDevice`: `Write` waits on a `TickLimiter` at the period rate and drops the samples.
- **`AudioEngine.h`**: the subsystem object. `Application::Engine` keeps it in a `std::shared_ptr`.
  It is not a singleton.
  - `Init(const AudioSettings&)` picks the backend and starts `AudioThread`.
  - `AudioThread : Runnable` is created with `{.Name = "Audio Thread", .Priority = TimeCritical}`.
    `Run` repeats: `Requests.Drain` → `Mixer.Render(period)` → `Device->Write` (blocks).
  - Main-thread API: `Push(AudioRequest)`, `AllocateVoice() -> VoiceHandle`, `DrainResults()`, and `Shutdown()`.
  - The handle counter lives on the main thread, so the main thread can return handles before the mixer runs.
  - Shutdown: stop the thread, close the device, and drain the results.
  - **No-thread mode** (threading rule 5): `AudioSettings::Threaded = false`. Tests then call
    `AudioEngine::RenderOffline(Frames, Out)`, which renders exactly the same samples on every run.
- Settings:
  - `EngineConf.ini` `[Audio]`: `Backend=auto|wasapi|pulse|alsa|null`, `MasterVolume`, `MusicVolume`, `EffectsVolume`.
  - New `--null-audio` launch option in `Core/inc/LaunchOptions.h`.
  - `--null-render` also turns on null audio, so headless runs and CI need no sound device.
- Tests in `FlingTests/Audio/`, all offline, with a generated sine-wave `AudioClip`:
  - gain and bus volume
  - pan law: left, right, and center
  - attenuation at Min, at Max, and between them
  - loop wraparound
  - resample length: a 44.1 kHz clip played at 48 kHz has the right number of output frames
  - pool exhaustion
  - stop with a fade
  - streamed music gives the same output as the decoded clip
  - `VoiceEnded` returns the clip
  - an engine with the threaded null device starts and shuts down cleanly

## Phase 4: Platform backends
All backends are in `Audio/src/Backends/`. Each `.cpp` is wrapped in a `#if FLING_WINDOWS` or
`#if FLING_LINUX` guard, because the module globs all its sources. `auto` tries them in this order:
- Windows: WASAPI → Null
- Linux: Pulse → ALSA → Null

A failed open logs a warning and tries the next backend.

- **WASAPI** (`WasapiAudioDevice.cpp`):
  - The audio thread calls `CoInitializeEx` (MTA).
  - `IMMDeviceEnumerator` → default render endpoint → `IAudioClient` in shared mode,
    event-driven, with a 10 ms buffer. `Write` waits on the event, then calls
    `GetCurrentPadding` and `IAudioRenderClient` `GetBuffer`/`ReleaseBuffer`.
  - Ask for float32 through `AUTOCONVERTPCM`. Otherwise use the mix format, and convert if it is int16.
  - MMCSS: `AvSetMmThreadCharacteristicsW(L"Pro Audio")`.
  - When the default device is lost or changes (`AUDCLNT_E_DEVICE_INVALIDATED`), reopen the default device.
  - Link `ole32` and `avrt` PRIVATE on WIN32.
  - Check that it builds with MinGW: the `KSDATAFORMAT_SUBTYPE_IEEE_FLOAT` GUID may need a local definition.
- **PulseAudio** (`PulseAudioDevice.cpp`):
  - Open `libpulse-simple.so.0` with `DynamicLibrary::OpenFirst`.
  - The function table is an X-macro `PulseFunctions.inl`: `pa_simple_new`, `pa_simple_write`,
    `pa_simple_drain`, `pa_simple_free`, `pa_simple_get_latency`, `pa_strerror`. `pa_strerror` is
    found through the dependency of the same handle.
  - Declare the few stable ABI types locally (opaque `pa_simple`, `pa_sample_spec`, `pa_buffer_attr`,
    and the enums). The build then needs no Pulse headers or `-dev` package, as with Vulkan.
  - Use `PA_SAMPLE_FLOAT32LE`, stereo, and `tlength` set to about 2 periods. `pa_simple_write` blocks.
- **ALSA** (`AlsaAudioDevice.cpp`):
  - Open `libasound.so.2` with the same pattern.
  - Functions: `snd_pcm_open("default")`, `snd_pcm_set_params(FLOAT_LE, RW_INTERLEAVED, 2, 48000, soft_resample=1, 20000us)`,
    `snd_pcm_writei`, `snd_pcm_recover`, `snd_pcm_close`, `snd_strerror`.
- Tests: the backends need hardware, so CI does not test them. Test the selection logic with a
  fake device factory: a failed open falls back to the next backend, and `Backend=null` is respected.
  Check by hand on Linux, and on Windows if one is available.

## Phase 5: Engine integration (ECS-ready, before ECS stages exist)
`ResolveDeferred` and `SimTick` do not exist yet. The engine still runs `World::Update`.
- Components in `Audio/inc/Components/` are registered with the component type registry and
  serialized with cereal, like Transform:
  - `AudioEmitter { Guid Clip; AudioBus Bus; float Volume, Pitch; bool Loop, Spatial, PlayOnStart; float MinDistance, MaxDistance; }`
    has a transient `VoiceHandle` proxy. The proxy is not serialized.
  - `AudioListener {}` is a tag on an entity that has a Transform. With no listener entity, the Camera is the listener.
- `AudioCommands` is a queue singleton in `registry.ctx()`. Gameplay appends to it:
  - `SoundCue { Guid Clip; AudioBus; float Volume, Pitch; std::optional<vec3> Position; }`
  - `MusicCommand { Play(Guid, FadeSec) | Stop(FadeSec) }`
  - Gameplay code never talks to `AudioEngine` directly (threading rule 2).
- `AudioSync::Update(entt::registry&, AudioEngine&)` is called from `Engine.cpp` right after `m_World->Update`:
  1. Drain `AudioCommands`. Load clips with `AudioClip::Create`; they are cached.
  2. Start or stop `AudioEmitter` voices.
  3. Push emitter positions from `Transform` and the listener state.
  4. Drain results.

  When the ECS stages arrive, this becomes the `ResolveDeferred` resolver plus an `Extract` step.
  Audio is presentation only, and nothing feeds back into the sim.
- Music: `AudioEngine` keeps the current music voice. `Play` with a fade starts the new voice
  fading in and fades the old one out. The music clip uses `AudioLoadMode::Stream`.
  Do not use the name "Player" (memory rule).
- Sandbox demo:
  - Looping music.
  - An `AudioEmitter` on a moving or placed entity, so you hear the pan and attenuation.
  - A key that plays a one-shot `SoundCue`.
  - Assets in `Assets/Audio/`, with a license that lets them be redistributed, recorded in `THIRD_PARTY_NOTICES.md` if needed.

## Later (not v1)
- tick-scheduled `AudioEvent`s (`improve-time.md`), resim dedupe, a CoreAudio callback backend with an SPSC ring, HRTF, and effects buses.

## Docs to update when phases are done
- `Skills/architecture.md`: the Audio module and the Core additions.
- Move the rules that hold (backend order, threading, asset type) into a new `Skills/audio.md`.

## Suggested PR order
1. Phase 1 (`sync-primitives.md`, then `ReadFileBytes`)
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5

Each PR: build Debug and Release, run `ctest --test-dir build`,
`python3 scripts/check_comment_style.py`, and clang-format on the changed files.

## Verification
- `ctest --test-dir build`: CoreTests (sync, ReadFileBytes), ResourcesTests (AudioClip decode),
  AudioTests (offline mixer), and the isolation tests all pass.
- Clang build with `-Wthread-safety` has no warnings.
- `./build/Sandbox --null-render --unattended --frames 120 --seed 1` exits `0` and logs `Audio backend: null`.
- With `--null-audio` and no `--null-render`, the window opens and there is no sound.
- By hand on Linux: Sandbox plays the music and the one-shot. The emitter pans as the camera turns.
  - `Backend=alsa` in `EngineConf.ini` uses ALSA.
  - Renaming `libpulse-simple` away, or running in a container without it, falls back to ALSA, then to null, with a warning and no crash.
- By hand on Windows: the same, through WASAPI. Changing the default output device mid-run reopens the device.
- A TSan build (`Skills/threading.md`) runs AudioTests and the threaded null-device test with no races.
