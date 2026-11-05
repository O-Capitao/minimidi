# Configurable wave shapes

## Goal

Allow project files to define reusable oscillator wave shapes by composing the
existing primitive waveforms. Tracks select either a built-in primitive or a
named definition. The audio callback should read precomputed wavetable data and
must not evaluate the recursive wave expression for every output sample.

## Project configuration

Add an optional top-level `wave_shapes` mapping. Each key is a user-defined
wave name and each value is a wave expression. Names are case-sensitive and
must not collide with built-in names (`SQUARE`, `TRIANGLE`, `SIN`). If the
mapping is absent, existing project files continue to work unchanged.

Track `wave` values accept the three existing built-ins or a name defined in
`wave_shapes`. A reference to an unknown name is a project load error. Keep the
existing built-in names and behavior compatible with current project files.

Example:

```yaml
name: "Pocket Circuit Example"
tempo: 148
sample_rate: 44000
ppqn: 960
beat_per_bar: 4

wave_shapes:
  SQUARE_1:
    sum:
      fundamental:
        type: SQUARE
        amplitude: 1
        freq_multiplier: 1
        phase: 0
      second_harmonic:
        type: TRIANGLE
        amplitude: 0.1
        freq_multiplier: 2
        phase: 0

track-config:
  bass:
    wave: SQUARE_1
    gain: 0.58
```

## Wave expression format

An expression is exactly one of:

- A primitive node, with required `type`, `amplitude`, `freq_multiplier`, and
  `phase` fields. It may also provide the primitive-specific fields below.
- An operation node with exactly one operation key, currently `sum` or
  `multiply`. Its value is a mapping of named child expressions. It must have
  at least one child. Child names are labels and do not affect evaluation.

Operation nodes may contain other operation nodes to any supported nesting
depth. Primitive and operation nodes may not contain unknown fields. Invalid
types, missing fields, invalid numbers, empty operations, and excessive
nesting must produce a useful project load error (never a crash or unbounded
recursion).

Example of nested composition:

```yaml
wave_shapes:
  SQUARE_EX:
    multiply:
      fundamental:
        type: SQUARE
        amplitude: 1
        freq_multiplier: 1
        phase: 0
      harmonics:
        sum:
          harmonic_1:
            type: TRIANGLE
            amplitude: 0.1
            freq_multiplier: 2
            phase: 0
          harmonic_2:
            type: TRIANGLE
            amplitude: 0.05
            freq_multiplier: 3
            phase: 30
```

`sum` adds child samples. `multiply` multiplies child samples. A primitive
sample is its waveform value multiplied by `amplitude`. Do not normalize or
clamp intermediate composite values; existing track gain and output handling
remain responsible for level management.

`freq_multiplier` is an integer from 1 through 64. Its primitive frequency is
the note frequency times this multiplier. `phase` is a finite angle in
degrees; positive values advance the primitive's phase, with values wrapped
modulo 360 degrees. `amplitude` is finite and may be positive, zero, or
negative.

## Primitive waveforms

The supported `type` values are `SQUARE`, `TRIANGLE`, and `SIN`.

- `SIN` uses the existing sine shape. It accepts no additional fields.
- `TRIANGLE` accepts optional `tilt`, from -1 through +1, defaulting to 0.
  Tilt moves the cycle's peak from phase 0.5: -1 places it at phase 0 and +1
  places it at phase 1. The waveform rises linearly from the cycle start to
  that peak, then falls linearly to the next cycle start. At either endpoint
  tilt, this is a ramp with the discontinuity at the cycle boundary.
- `SQUARE` accepts optional `split`, from 0 through 1, defaulting to 0.5.
  The output is +1 before `split` in the cycle and -1 from `split` to the end.
  Values 0 and 1 are valid constant -1 and +1 shapes respectively.
  `SQUARE` also accepts optional `lean`, from 0 through 1, defaulting to 0.
  It linearly ramps from the start-of-cycle value toward the high level over
  the first `lean` fraction of the cycle, then stays high until `split`, and
  changes immediately to the low level. `lean` must not exceed `split`.
  `lean: 0` is the ordinary square wave. With `split: 1` and `lean: 1`, the
  result is a rising ramp from -1 to +1 over the cycle.

All numeric fields must reject NaN and infinity in addition to out-of-range
values. Primitive-specific fields on the wrong primitive are invalid.

## Wavetable and playback behavior

Compile each configured expression during project initialization into one
single-cycle table of 2048 samples and
uses linear interpolation during oscillator playback. Evaluate each
expression at normalized phase positions over `[0, 1)`, using each primitive's
frequency multiplier and phase offset. Integer frequency multipliers ensure
the composite repeats after one base cycle, so phase offsets do not require a
longer table.

Build tables before audio playback starts. The audio callback performs only
table lookup/interpolation and phase advancement for custom shapes; it must
not allocate memory, parse YAML, or recurse through expression trees. Keep
tables alive as long as any track can reference them, and release them during
project/audio teardown. Built-in waves may use the same table path or retain
their current oscillator implementation, provided their existing output
behavior remains compatible.

The oscillator phase advances by `note_frequency / sample_rate` per output
sample. Composite partials are represented inside the compiled table; do not
advance a separate oscillator phase for each primitive at playback time.

## Compatibility and errors

- Existing YAML without `wave_shapes` remains valid.
- Existing tracks using `SQUARE`, `TRIANGLE`, or `SIN` retain their current
  defaults and playback behavior.
- A project fails to load for duplicate/invalid definitions, undefined track
  references, malformed expression structure, unsupported operations or
  primitive types, invalid numbers, or invalid primitive parameters.
- Serialization must preserve configured wave definitions and track
  references if the project format supports round-tripping project files.

## Acceptance checks

- Load an old project without `wave_shapes` and verify its tracks still use
  the existing built-in waves.
- Load a project containing a primitive definition, a `sum`, a `multiply`,
  and a nested expression; verify tracks resolve each named shape.
- Reject unknown wave references and malformed or out-of-range definitions
  with a project load error.
- Verify the generated table for a single primitive matches its documented
  shape and the existing built-in output where compatibility is required.
- Render a note using a custom shape through the offline audio renderer and
  confirm output is finite and nonzero for a nonzero-amplitude definition.
- Confirm project teardown releases expression and table memory.
