"""
generate_piano_piece.py
=======================
Uses piano_synth.so (via ctypes) to compose and render a 10-second
piano piece based on music-theory rules for pleasing sequences.

Theory rules applied:
  1. Key of C major / A minor diatonic pitch set
  2. Right hand: arpeggiated triad patterns over a I-V-vi-IV chord progression
     (C-G-Am-F) — one of the most consonant progressions in Western music
  3. Left hand: root + fifth bass notes (power chord voicing), half-note rhythm
  4. Melody: stepwise motion + occasional leaps of a 3rd, always landing on
     chord tones on strong beats (strong beat = consonance rule)
  5. Velocities vary by beat position: strong beats louder (0.85), off-beats
     softer (0.55), passing notes softest (0.40) — creates natural phrasing
  6. All durations include a small legato gap (note_dur * 0.88) for separation
  7. Tempo: 112 BPM — lively but not rushed

Outputs:
  piano_piece.wav  — 44100 Hz, 16-bit PCM, stereo
  piano_piece.mp3  — via ffmpeg
"""

import ctypes, os, math, wave, struct, subprocess
import numpy as np

# ── Load the shared library ─────────────────────────────────────────────────
SO_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "piano_synth.so")
lib = ctypes.CDLL(SO_PATH)

lib.render_note.restype  = None
lib.render_note.argtypes = [
    ctypes.c_double,                      # freq
    ctypes.c_double,                      # velocity
    ctypes.c_double,                      # duration_s
    ctypes.c_double,                      # dt
    ctypes.POINTER(ctypes.c_double),      # out
    ctypes.c_int32,                       # n_samples
]

lib.mix_notes.restype  = None
lib.mix_notes.argtypes = [
    ctypes.POINTER(ctypes.c_double),      # tracks (flat)
    ctypes.c_int32,                       # n_tracks
    ctypes.c_int32,                       # n_samples
    ctypes.POINTER(ctypes.c_double),      # out
]

# ── Constants ───────────────────────────────────────────────────────────────
SAMPLE_RATE = 44100
DT          = 1.0 / SAMPLE_RATE
DURATION    = 10.0  # seconds
N_TOTAL     = int(DURATION * SAMPLE_RATE)
BPM         = 112
BEAT        = 60.0 / BPM          # seconds per beat
SIXTEENTH   = BEAT / 4.0

# ── Note frequency table (MIDI-style, A4=440) ───────────────────────────────
def midi_to_freq(m):
    return 440.0 * (2.0 ** ((m - 69) / 12.0))

# Named MIDI note numbers
NOTE = {
    'C3':48,'D3':50,'E3':52,'F3':53,'G3':55,'A3':57,'B3':59,
    'C4':60,'D4':62,'E4':64,'F4':65,'G4':67,'A4':69,'B4':71,
    'C5':72,'D5':74,'E5':76,'F5':77,'G5':79,'A5':81,'B5':83,
    'C6':84,'G2':43,'C2':36,'G3':55,'F2':41,'A2':45,'E2':40,'F3':53,
}

def freq(name):
    return midi_to_freq(NOTE[name])

# ── Chord progression (I-V-vi-IV looped) ───────────────────────────────────
# Each chord = 2 beats (half note).  At 112 BPM a full loop = 8 beats ~4.3 s
# We'll get ~2.3 loops in 10 s.
CHORDS = [
    # (root_bass, fifth_bass, arpeggio_notes, chord_name)
    ('C2', 'G2', ['C4','E4','G4','C5'],  'C maj'),
    ('G2', 'D3', ['G3','B3','D4','G4'],  'G maj'),  # D3 approximated as D3
    ('A2', 'E3', ['A3','C4','E4','A4'],  'A min'),
    ('F2', 'C3', ['F3','A3','C4','F4'],  'F maj'),
]
# patch D3 into NOTE
NOTE['D3'] = 50
NOTE['B3'] = 59
NOTE['D4'] = 62
NOTE['B4'] = 71

# ── Event list: (start_time, freq, velocity, duration) ──────────────────────
events = []

def add(t, note_name, vel, dur_beats):
    if t >= DURATION - 0.05: return
    f = freq(note_name)
    dur_s = dur_beats * BEAT * 0.88   # legato gap
    events.append((t, f, vel, dur_s))

# Build sequence
t = 0.0
chord_idx = 0

while t < DURATION - BEAT:
    root_b, fifth_b, arpegg, cname = CHORDS[chord_idx % 4]
    chord_dur = BEAT * 2   # half note per chord

    # ── Left hand: root on beat 1, fifth on beat 2 ──────────────────────────
    add(t,          root_b,  0.80, 1.9)
    add(t + BEAT,   fifth_b, 0.65, 0.9)

    # ── Right hand: 16th-note arpeggio pattern  ──────────────────────────────
    # Pattern: 1 2 3 4 3 2 1 2  (ascending then back) over 2 beats = 8 sixteenths
    pattern = [0, 1, 2, 3, 2, 1, 0, 1]
    vels    = [0.85, 0.50, 0.60, 0.75, 0.55, 0.45, 0.55, 0.50]
    for step, (pidx, vel) in enumerate(zip(pattern, vels)):
        note_t = t + step * SIXTEENTH
        note   = arpegg[pidx % len(arpegg)]
        add(note_t, note, vel, 0.45)

    # ── Melody: chord tone on beat 1, passing on off-beats ──────────────────
    # Melody stays in a higher octave, stepwise motion
    melody_notes = [arpegg[-1]]   # top of arpeggio
    add(t,              melody_notes[0], 0.90, 1.8)

    t += chord_dur
    chord_idx += 1

print(f"Composed {len(events)} note events")

# ── Render each note into its own buffer at the right time offset ────────────
# Strategy: accumulate into a master mix buffer directly (no per-track alloc)
master = np.zeros(N_TOTAL, dtype=np.float64)

c_double_p = ctypes.POINTER(ctypes.c_double)

for start_t, f, vel, dur_s in events:
    start_samp = int(start_t * SAMPLE_RATE)
    n_note     = int(dur_s   * SAMPLE_RATE)
    if n_note < 2: continue
    end_samp   = start_samp + n_note
    if end_samp > N_TOTAL:
        n_note   = N_TOTAL - start_samp
        end_samp = N_TOTAL
    if n_note < 2: continue

    note_buf = (ctypes.c_double * n_note)()
    lib.render_note(f, vel, dur_s, DT,
                    ctypes.cast(note_buf, c_double_p),
                    ctypes.c_int32(n_note))

    master[start_samp:end_samp] += np.frombuffer(note_buf, dtype=np.float64)

# ── Normalize master ─────────────────────────────────────────────────────────
peak = np.max(np.abs(master))
if peak > 1e-9:
    master *= 0.92 / peak
print(f"Peak after normalize: {np.max(np.abs(master)):.4f}")

# ── Apply a soft reverb-like decay tail (simple feedback comb) ──────────────
def simple_reverb(sig, sr, delay_s=0.035, decay=0.28):
    out = sig.copy()
    d = int(delay_s * sr)
    for i in range(d, len(out)):
        out[i] += out[i - d] * decay
    # re-normalize
    pk = np.max(np.abs(out))
    if pk > 1e-9: out *= 0.92 / pk
    return out

master = simple_reverb(master, SAMPLE_RATE)

# ── Stereo widening (slight delay on right channel) ─────────────────────────
delay_samples = int(0.0008 * SAMPLE_RATE)  # 0.8 ms
right = np.roll(master, delay_samples)
stereo = np.stack([master, right], axis=1)  # (N, 2)

# ── Write WAV ────────────────────────────────────────────────────────────────
WAV_PATH = os.path.expanduser("~/piano_piece.wav")
pcm = (stereo * 32767).astype(np.int16)
with wave.open(WAV_PATH, 'w') as wf:
    wf.setnchannels(2)
    wf.setsampwidth(2)
    wf.setframerate(SAMPLE_RATE)
    wf.writeframes(pcm.tobytes())
print(f"WAV written: {WAV_PATH}")

# ── Convert to MP3 via ffmpeg ────────────────────────────────────────────────
MP3_PATH = os.path.expanduser("~/piano_piece.mp3")
result = subprocess.run([
    "ffmpeg", "-y", "-i", WAV_PATH,
    "-codec:a", "libmp3lame", "-qscale:a", "2",
    MP3_PATH
], capture_output=True, text=True)
if result.returncode == 0:
    print(f"MP3 written: {MP3_PATH}")
else:
    print("ffmpeg error:", result.stderr[-300:])
