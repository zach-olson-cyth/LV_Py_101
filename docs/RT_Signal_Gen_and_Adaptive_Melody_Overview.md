# LabVIEW RT Signal Generator & Adaptive Melody System

This document summarizes the code and .so libraries used in the LV_Py_101 project, including the sweep waveform generator, piano and drum synths, and the adaptive melody learner (melody_learn_v3).

## Components

- `waveform_gen.c` / `waveform_gen.so`: fixed-frequency sine and sine-sweep generator for function generator behavior in LabVIEW.
- `piano_synth.c` / `piano_synth.so`: physically-inspired piano note renderer and mixer.
- `drum_synth.c` / `drum_synth.so`: synthesized drum kit (kick, snare, hi-hat, toms, crash, ride).
- `melody_learn_v3.c` / `melody_learn_v3.so`: 6-style adaptive melody and drum pattern generator using UCB1 bandit learning.
- `melody_learn_v2_coeffs.csv`: coefficient bank for melody_learn_v3 (initialized rewards/visits per arm and style).

## Data Flow Overview

1. **Sweep / Signal Generation**
   - LabVIEW calls `generatewaveform(amp, freq_start, freq_stop, dt, length_pts, out_array)` from `waveform_gen.so` to generate:
     - Fixed sine when `freq_start == freq_stop`.
     - Linear sine sweep when `freq_start != freq_stop`.
   - Output is a 1D DBL array used as a function generator or audio test stimulus.

2. **Piano / Drum Rendering**
   - For melodic audio, LabVIEW uses:
     - `render_note(freq, velocity, duration_s, dt, out, n_samples)` from `piano_synth.so`.
     - `mix_notes(tracks, n_tracks, n_samples, out)` to sum multiple note buffers.
   - For drum audio, LabVIEW uses:
     - `render_drum_hit(drum_id, velocity, n_samples, sample_rate, out)` from `drum_synth.so`.
     - `freq_to_drum_id(freq)` to map melody_learn_v3 drum-style frequencies back to drum IDs.

3. **Adaptive Melody & Style Learning**
   - LabVIEW runs `melody_learn_v3.so` each session to propose a sequence of notes or drum hits.
   - The VI passes in the previous arm indices and a like/dislike value from the user, then logs the new arms and coefficients.
   - Coefficients are persisted in `melody_learn_v2_coeffs.csv` on RT and reloaded at startup.

## melody_learn_v3 Function

```c
EXPORT void generate_melody_learn(
    int32_t  n_notes,
    int32_t  sample_rate,
    int32_t  style,
    int32_t  like_dislike,
    int32_t  last_arm_contour,
    int32_t  last_arm_rhythm,
    double  *coeffs_in,
    double  *freqs_out,
    int32_t *durs_out,
    double  *coeffs_out,
    int32_t *arm_contour_out,
    int32_t *arm_rhythm_out
);
```

### Styles

The learner supports six styles:

0. **Lyrical** (C major, 112 BPM) – mostly quarter and half notes.
1. **Upbeat** (C major, 112 BPM) – more eighth-note motion.
2. **Mixed** (C major, 112 BPM) – varied rhythms, bridge between styles.
3. **Jazz** (C Dorian, 112 BPM) – swing-feel durations and ii–V–I–IV chord tones.
4. **Rock Band** (A pentatonic minor, 132 BPM) – 16th-heavy riff patterns.
5. **Beat** (drum kit, 120 BPM) – kick/snare/hat/tom/crash/ride grooves.

### Arms and Coefficient Layout

- `N_ARMS = 8` per style.
- Coefficient array length is 256 doubles. Layout:
  - `CONT_REW(style, arm)`  at indices   0–47  (6 styles × 8 arms).
  - `CONT_VIS(style, arm)`  at indices  48–95.
  - `RHYT_REW(style, arm)`  at indices  96–143.
  - `RHYT_VIS(style, arm)`  at indices 144–191.
  - `192–255` reserved for future use.
- Macros in C:

```c
#define CONT_REW(c,s,a) ((c)[(s)*N_ARMS+(a)])
#define CONT_VIS(c,s,a) ((c)[48+(s)*N_ARMS+(a)])
#define RHYT_REW(c,s,a) ((c)[96+(s)*N_ARMS+(a)])
#define RHYT_VIS(c,s,a) ((c)[144+(s)*N_ARMS+(a)])
```

### Arms

- **Contour arms** (CONTOUR_ARMS[8][4]) – stepwise scale deltas over 4-note cells.
- **Drum contour arms** (DRUM_ARMS[8][4]) – absolute drum indices into DRUM[8].
- **Rhythm arms** (RHYTHM_ARMS[8][4]) – beat durations for lyrical/upbeat/mixed/jazz.
- **Rock rhythm arms** (ROCK_RHYTHM[8][4]) – 16th-note centric durations.
- **Beat rhythm arms** (BEAT_RHYTHM[8][4]) – drum-groove durations at 120 BPM.

### UCB1 Learning

- For both contour and rhythm arms, the code uses UCB1 multi-armed bandit selection:
  - Each arm tracks cumulative reward and visit count.
  - `like_dislike > 0` gives reward 1.0 on the previous arms.
  - `ucb1_select` picks the arm with the best trade-off between mean reward and exploration bonus.
  - Each new arm gets +0.5 pending credit in its visit counter to encourage exploration.

### Note Generation Rules

Per note index `n`:

- Compute chord index `chord_idx = (n/4) % 4` for a 4-chord loop.
- Depending on `style`:
  - **Beat (5)**: use DRUM_ARMS and BEAT_RHYTHM to pick drum MIDI and duration.
  - **Rock (4)**: use APEN (A pentatonic minor), ROCK_CT chord tones, CONTOUR_ARMS, and ROCK_RHYTHM.
  - **Jazz (3)**: use CDOR (C Dorian), JAZZ_CT chord tones, CONTOUR_ARMS, and RHYTHM_ARMS with swing via `jazz_dur`.
  - **Lyrical/Upbeat/Mixed (0–2)**: use CMAJ (C major), CHORD_CT chord tones, CONTOUR_ARMS, and RHYTHM_ARMS.
- Frequencies are converted from MIDI via `midi_freq(m)`.
- Durations are converted from beats to samples with `beat_samples(sample_rate, beats, style)`.

## LabVIEW Integration Notes

- All .so exports use C calling convention and plain C types (I32, DBL, arrays by pointer).
- For array outputs (waveforms, note buffers):
  - Pre-allocate arrays in LabVIEW with `Initialize Array` and pass as Array Data Pointer.
- Coefficients are stored as CSV on RT (`melody_learn_v2_coeffs.csv`) and loaded into a DBL array for each session, then written back on shutdown.

## Future Extensions

- Add guitar/bass synths following the same CLFN pattern.
- Add more styles by allocating additional coefficient banks in the 192–255 region.
- Extend UCB1 to contextual bandits using tempo or user state as context.
