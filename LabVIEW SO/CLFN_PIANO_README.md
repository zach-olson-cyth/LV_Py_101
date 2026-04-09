# piano_synth.so — LabVIEW CLFN Wiring Guide

## Deploy
Copy piano_synth.so to the cRIO-9040:
  /home/lvuser/natinst/piano_synth.so

---

## Function 1: render_note
Renders a single piano tone into a pre-allocated DBL array.

### CLFN Settings
- Library path: /home/lvuser/natinst/piano_synth.so
- Function name: render_note   (case-sensitive, lowercase)
- Calling convention: C
- Return type: void

### Terminals

| # | Name        | LV Type      | Direction | Pass Mode        | C Type   |
|---|-------------|--------------|-----------|------------------|----------|
| 1 | freq        | DBL          | Input     | Value            | double   |
| 2 | velocity    | DBL (0..1)   | Input     | Value            | double   |
| 3 | duration_s  | DBL          | Input     | Value            | double   |
| 4 | dt          | DBL          | Input     | Value            | double   |
| 5 | out         | 1D DBL Array | Output    | Array Data Ptr   | double * |
| 6 | n_samples   | I32          | Input     | Value            | int32    |

Pre-allocate out[] with Initialize Array (DBL, size = n_samples) before CLFN.

### Piano model
- 16 additive harmonics with inharmonicity (stiff-string approximation)
- ADSR envelope: 0.5% attack, 8% decay, 70% sustain, 15% release
- Velocity scales harmonic brightness (louder = more upper partials)

---

## Function 2: mix_notes
Mixes N pre-rendered note tracks into one output buffer. Peak-normalizes to 0.92.

### CLFN Settings
- Function name: mix_notes
- Return type: void

### Terminals

| # | Name      | LV Type          | Direction | Pass Mode       | C Type   |
|---|-----------|------------------|-----------|-----------------|----------|
| 1 | tracks    | 2D DBL Array     | Input     | Array Data Ptr  | double * |
| 2 | n_tracks  | I32              | Input     | Value           | int32    |
| 3 | n_samples | I32              | Input     | Value           | int32    |
| 4 | out       | 1D DBL Array     | Output    | Array Data Ptr  | double * |

tracks[] is a flat row-major 2D array: row t = note track t, each row n_samples long.

---

## Chord progression used in generate_piano_piece.py
I – V – vi – IV  (C maj, G maj, A min, F maj) at 112 BPM
- Arguably the most universally "pleasing" progression in Western music
- Right hand: 16th-note ascending/descending arpeggio
- Left hand: root + fifth bass notes (half-note rhythm)
- Melody: chord-tone on strong beats, stepwise passing notes between
- Velocity curve: strong beats 0.85, off-beats 0.50–0.60

## Harmonic content
Inharmonicity B = 0.00015 (matches mid-range acoustic upright piano)
Harmonic roll-off: exp(-(n-1) * 0.18 * (1 - vel*0.7))
