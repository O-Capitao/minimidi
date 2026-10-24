# MiniMIDI profiling report

Date: 2026-09-08  
Revision: `33c2416` (`bring back the organic codez`)

Follow-up: recommendation 1 was implemented after this baseline profile. Each
runtime MIDI-cache entry now owns one project-scaled event-tick array, and all
clips that reuse that MIDI file share it. Audio scheduling and TUI note
rendering consume the cached values instead of rescaling event ticks. A
follow-up instrumented test rendered 1,936,200 playing frames without calling
an event-scaling function from the audio loop; `scale_event_tick` ran only 348
times while the test projects were loaded.

## Executive summary

No memory leak, invalid memory access, or undefined behavior was detected in
the exercised project, MIDI, offline-audio, and TUI lifecycle paths. The
current engine also has substantial CPU headroom: with the Makefile's default
unoptimized build, a synthetic four-track continuously active arrangement
used about 1.0% of one CPU core while rendering 512-frame blocks offline.

The main opportunities are nevertheless in the real-time audio callback. It
currently converts sample positions to ticks, repeatedly rescales the same
future MIDI event, evaluates oscillator frequency with `pow()`, wraps phase
with `floor()`, and publishes an atomic tick at sample rate. These operations
are not causing trouble with the current four-track limit and small fixtures,
but they are avoidable work on the latency-sensitive thread.

The most important limitation of this report is that the container has no
working ALSA, PulseAudio, or JACK output. PortAudio device startup, callback
jitter, underruns, and device-library cleanup could not be measured. The
audio results below use the same `MM_AudioEngine_render` function through the
offline engine.

## Test environment and method

| Item | Value |
| --- | --- |
| CPU | Intel Core i5-9400, 6 cores, 2.90 GHz base |
| OS | x86-64 Linux 6.12.107, glibc 2.41 |
| Compiler | GCC 14.2.0 |
| Libraries | PortAudio 19, ncurses/panel 6.5, libyaml 0.2.5 |
| Project rate | 44,000 Hz |
| Render block | 512 mono frames; 11.64 ms of audio per block |
| Normal fixture | `test_proj.yaml`: two tracks, two sequences, three distinct MIDI files, 112 retained note events |
| Maximum-track fixture | Synthetic four-track project with all four looping tracks continuously active |

The following checks were performed:

- Warning-clean normal application build and automated test run.
- AddressSanitizer and UndefinedBehaviorSanitizer build of the automated suite.
- LeakSanitizer with `detect_leaks=1` for the automated suite. It was run
  outside the filesystem sandbox because LeakSanitizer cannot operate under
  that sandbox's `ptrace` setup.
- A separate sanitizer-instrumented pseudo-terminal test covering project
  load, offline audio initialization, ncurses initialization/rendering, track
  selection, the track-properties panel, and complete teardown.
- GCC `-fanalyzer` over the complete application.
- `gprof` instrumentation of project loading and continuous two-track audio
  rendering.
- Five timed runs of each audio benchmark with the current default `-O0`
  behavior and with `-O2` for comparison.
- A 3,000-frame ncurses render benchmark in an 80x24 pseudo-terminal.

`valgrind`, `perf`, and `heaptrack` were not installed in the environment.
Temporary benchmark programs and fixtures were kept outside the repository
and removed after collecting the results.

## Memory-safety results

### Dynamic checks

| Workload | ASan | UBSan | LSan |
| --- | --- | --- | --- |
| Automated parser/project/sequencer suite | clean | clean | no leaks reported |
| Offline audio plus ncurses lifecycle | clean | clean | no leaks reported |

The automated suite includes successful load/free and YAML round-trip paths,
format-1 MIDI parsing, natural clip length, full sequencer completion and
restart, an infinite sequence, malformed MIDI, unknown tracks, the track
limit, and an overlapping-assignment failure. Cleanup after partially built
projects is therefore exercised as well as the normal path.

The benchmark repeatedly loaded and released the normal project 1,000 times
and initialized/destroyed the offline engine 10,000 times. Peak resident size
remained approximately 5.5-5.7 MiB across runs. Peak RSS is not itself a leak
proof, but it is consistent with the sanitizer result.

### Static ownership review

GCC `-fanalyzer` emitted one `-Wanalyzer-malloc-leak` warning for the
`target->tracks` allocation in `compile_sequence` at
`minimidi-proj.c:100-101`. This does not appear to be a real leak:

- the pointer immediately becomes owned by an element of
  `project->sequence_arr`;
- `MM_Project_free` traverses every sequence and frees its track and clip
  arrays, including partially compiled sequences; and
- LeakSanitizer remained clean on successful initialization and the tested
  runtime compilation failure.

The warning should be monitored if ownership is reorganized, but no code
change is currently justified by it.

The MIDI event linked-list nodes allocated at `minimidi.c:205-215` are also
released correctly by `list_clear` at `minimidi.c:192-203`. The audio engine,
ring buffer, project-file tree, MIDI cache, ncurses windows, and panel all have
matching teardown in the tested paths.

### Leak-test gap in the Makefile

The existing `make sanitize` target explicitly sets
`ASAN_OPTIONS=detect_leaks=0`. It checks invalid access and undefined behavior,
but despite its name it does not check leaks. A separate Linux leak target, or
enabling `detect_leaks=1` where supported, would prevent that distinction from
being missed during routine testing.

## CPU measurements

Each audio result is the mean of five runs. "Real-time CPU" compares the CPU
time required to synthesize 30 seconds of audio with those 30 seconds; it is
the approximate share of one core. The benchmark forces every listed track to
remain active and looping, making it heavier than the current example project.

| Build | Active tracks | Throughput | CPU per 512 frames | Real-time CPU | Approx. callback headroom |
| --- | ---: | ---: | ---: | ---: | ---: |
| Makefile default (`-O0`) | 2 | 7.98 Mframes/s | 64.2 us | 0.55% | 181x |
| Makefile default (`-O0`) | 4 | 4.39 Mframes/s | 116.6 us | 1.00% | 100x |
| Optimized (`-O2`) | 2 | 14.33 Mframes/s | 35.7 us | 0.31% | 326x |
| Optimized (`-O2`) | 4 | 7.70 Mframes/s | 66.5 us | 0.57% | 175x |

The headroom ratio compares average offline render time with the 11.64 ms
PortAudio block duration. It is not a worst-case deadline guarantee: an audio
callback can still underrun because of scheduling delays, page faults, device
behavior, or a rare slow block.

Stopped rendering is effectively free in this workload: the default build
filled about 705 million stopped frames per second. Keeping the PortAudio
stream alive after the arrangement finishes therefore wastes little CPU,
although it does keep the device active.

Project startup is also small for the current fixture. Loading and fully
freeing its YAML plus three distinct MIDI files averaged 73.7 us at `-O0` and
68.1 us at `-O2`. Offline engine initialization and destruction were below a
microsecond after allocator warm-up.

The TUI benchmark averaged 18.9 us for an unchanged stopped frame and 19.9 us
for a playing frame. At the configured 30 FPS this is roughly 0.06% of one
core for the current fixture. This measurement benefits from ncurses' virtual
screen diffing and does not represent a terminal with slow I/O or a large MIDI
file.

## CPU profile

The `gprof` run used the default unoptimized build and simulated 30 seconds of
continuous two-track playback. Its 10 ms sampling interval makes individual
percentages approximate, but the call counts clearly identify repeated work.

| Function | Self time | Calls | Interpretation |
| --- | ---: | ---: | --- |
| `audio_track_sample` | 40.63% | 2,992,256 | Per-track scheduling and dispatch is the main in-program cost. |
| audio `scale_ticks` | 18.75% | 2,695,504 | The same next event tick is rescaled again on nearly every track/sample until it becomes due. |
| `samples_to_ticks` | 18.75% | 1,496,128 | A long-double conversion/division runs once per playing sample. |
| `synth_sample` | 12.50% | 2,772,256 | Oscillator generation; shared-library math time is not fully attributed here. |

These four functions account for about 90.6% of sampled time in the program
itself. Shared `libm` routines are not instrumented by this `gprof` build, so
the table understates the cost of `pow`, `sin`, and `floor` called by
`synth_sample`.

All `_Atomic` fields used between the UI and callback (`double`, `size_t`, and
`bool`) reported lock-free on this x86-64 host. That is good for this target,
but C does not guarantee the same result on every architecture.

## Findings and recommendations

### 1. Cache project-scaled event ticks (implemented)

Original priority: medium; best measured callback improvement.

`audio_track_sample` recalculates the next event's project tick at
`minimidi-audio.c:150-154` every sample while waiting for that event. The
profile recorded 2.70 million calls during 1.50 million playing frames.

The runtime MIDI cache now converts each event once when it first loads a
distinct MIDI file. Clips retain a pointer to that shared array. This preserves
the previous nearest-tick rounding, avoids duplicate caches for repeated MIDI
assignments, and removes `scale_ticks` from the audio sample loop. The TUI uses
the same values for note starts and paired note ends. In the follow-up `gprof`
run, event scaling fell from millions of callback-path calls to 348 load-time
calls across the full test suite.

### 2. Cache oscillator phase increments on note-on

Priority: medium.

`synth_sample` calculates
`440 * pow(2, (note - 69) / 12)` for every audible track/sample at
`minimidi-audio.c:53-74`, even though frequency changes only on note-on.
Store `phase_increment = frequency / sample_rate` in `MM_Synth` when the note
changes. A 128-entry MIDI frequency or phase-increment table would remove
`pow()` entirely from the callback. Phase wrapping can remain correct while
using a cheap common path for increments below one.

This cost is not fully visible in `gprof` because `pow` is in shared `libm`,
but it scales directly with active tracks and samples.

### 3. Advance time incrementally and publish UI state per block

Priority: medium.

`samples_to_ticks` performs a long-double conversion and division for every
playing sample at `minimidi-audio.c:224-225`; it represented 18.75% of sampled
program time. Maintain an integer/fixed-point tick phase, or precompute a
sample-to-tick ratio and advance it incrementally.

The callback also stores `posted_sequence_tick` atomically on every sample at
`minimidi-audio.c:234`, although the UI reads at 30 FPS. Publishing the final
tick once per callback would reduce cross-thread cache traffic from 44,000
stores per second to about 86 stores per second with a 512-frame block.
Sequence transitions within a block need a clearly defined final snapshot,
but do not require sample-rate publication.

### 4. Add a real release build

Priority: low for four tracks, useful and simple.

The Makefile always uses debug symbols without an optimization level. `-O2`
improved active rendering by about 1.8x in both the two- and four-track tests.
Keep the current warning-rich debug build, but add a separate release target
using `-O2` (or `-O3` only after output-equivalence testing).

### 5. Bound TUI work by visible events

Priority: low for current fixtures; potentially high for dense or short-looped
MIDI files.

`render_midi` at `minimidi-tui.c:315-375` scans every event for every visible
loop cycle on every frame. A short source loop viewed across a long time span
can therefore cost `visible cycles x all MIDI events`, even when few notes are
on screen. The current fixture is fast, but this scaling was not stress-tested
with a large MIDI file.

Precompute project-tick note spans per clip, binary-search the first visible
span, or retain a viewport cache until the track, sequence, zoom, or scroll
position changes. The stopped TUI also redraws every 33 ms even when neither
input nor playback state changed; dirty-state rendering would remove that
idle work, although its measured cost is currently negligible.

### 6. Avoid per-item YAML growth and duplicate MIDI list nodes if scale grows

Priority: low.

The YAML helper at `minimidi-proj-file.c:147-156` grows arrays by exactly one
element, causing repeated reallocations and potentially quadratic copying for
projects with many sequences or assignments. Use a count/capacity vector like
the MIDI event parser already does.

The MIDI loader stores events in a contiguous array and additionally allocates
one linked-list node per event at `minimidi.c:304-305`. The runtime audio and
current TUI use the array, so the linked representation adds allocation time
and memory for a compatibility API that the application does not currently
consume. Retain it only if external callers need it, or build temporary range
lists on demand.

### 7. Add real-time diagnostics

Priority: medium for diagnosing real machines.

The PortAudio callback ignores `PaStreamCallbackFlags` at
`minimidi-audio.c:204-214`. That means output underflows are not observable,
so a field report cannot distinguish synthesis load from device/scheduler
problems. Increment lock-free counters for underflow/overflow flags in the
callback and display or log them from the main thread. Do not log directly
from the callback.

### 8. Make the `realpath` declaration independent of ncurses flags

Priority: medium build-portability issue, not a runtime hotspot.

A direct build without ncurses' pkg-config feature macros failed because
`realpath(path, NULL)` in `minimidi-proj.c` had no visible declaration. The
normal build happens to receive `_DEFAULT_SOURCE`/`_XOPEN_SOURCE` from
ncurses' pkg-config output. If pkg-config discovery fails, the Makefile uses
fallback libraries but loses those macros and can fail to compile.

Declare the intended feature-test level explicitly for the project, or avoid
the GNU allocated-buffer form of `realpath`. Project compilation should not
depend on a transitive ncurses compiler flag.

## Positive real-time properties

Several important properties are already sound:

- Project and MIDI data are parsed and allocated before PortAudio starts.
- `MM_AudioEngine_render` performs no allocation, file I/O, logging, or
  blocking synchronization.
- The callback honors PortAudio's actual `frames_per_buffer` value.
- Commands travel through a bounded SPSC ring buffer.
- UI-facing state uses relaxed atomics rather than locks.
- Clips are sorted and overlap-checked during project compilation.
- MIDI files are cached by canonical path and freed once with the project.

## Suggested order of work

1. Add a permanent offline benchmark fixture and record callback time for two
   and four continuously active tracks.
2. Cache event project ticks and oscillator phase increments, then compare
   audio output and rerun the benchmark.
3. Replace per-sample tick division and per-sample UI atomics with incremental
   timing and per-block publication.
4. Add a release target and enable LeakSanitizer in a supported test target.
5. Add PortAudio underflow counters and profile on a real output device under
   normal system load.
6. Optimize TUI event lookup only after testing a realistically large/dense
   MIDI file; current viewport rendering is already inexpensive.
