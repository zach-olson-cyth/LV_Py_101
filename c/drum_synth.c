/*
 * drum_synth.c
 * NI Linux RT CLFN - Drum hit synthesizer
 *
 * Each drum sound uses a physically-motivated synthesis model:
 *   Kick  : pitched sine sweep (80→40 Hz) + noise transient
 *   Snare : short pitched tone + filtered white noise ("crack")
 *   Hi-hat: band-pass filtered white noise, fast decay
 *   Tom   : sine pitch sweep, medium decay
 *   Crash : filtered noise, long cymbal decay
 *   Ride  : filtered noise with slight pitch, medium decay
 *
 * Function: render_drum_hit
 *   Return type: void
 *   Same call pattern as render_note for easy Case Structure swap.
 *
 * CLFN terminals:
 *   1  drum_id      I32   Value  (0=kick,1=snare,2=hihat,3=tom-hi,
 *                                 4=tom-mid,5=tom-lo,6=crash,7=ride)
 *   2  velocity     DBL   Value  (0.0 - 1.0)
 *   3  n_samples    I32   Value  (2205 - 44100)
 *   4  sample_rate  I32   Value  (44100)
 *   5  out          DBL   Pointer to Value  <- Initialize Array DBL n_samples
 *
 * NOTE: drum_id maps directly to the DRUM[] scale indices in melody_learn_v2.c
 *   DRUM[0]=kick  DRUM[1]=snare  DRUM[2]=hihat  DRUM[3]=tom-hi
 *   DRUM[4]=tom-mid  DRUM[5]=tom-lo  DRUM[6]=crash  DRUM[7]=ride
 *
 * In LabVIEW: when style=5, pass the note index from your melody loop
 * through: drum_id = round((freq - 65.41) * 7 / (261.63 - 65.41))
 * OR simpler: store drum_id directly using a lookup on the freq value.
 */

#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
  #define EXPORT __declspec(dllexport)
#else
  #define EXPORT __attribute__((visibility("default")))
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ── Simple LCG pseudo-random (no stdlib rand dependency issues on RT) ── */
static uint32_t lcg_state = 0x12345678u;
static double lcg_rand(void) {
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return ((double)(lcg_state >> 1)) / (double)0x7FFFFFFFu - 1.0; /* -1..+1 */
}

/* ── One-pole low-pass filter (in-place) ────────────────────────────── */
static void lpf(double *buf, int n, double cutoff_hz, int sr) {
    double dt = 1.0 / sr;
    double rc = 1.0 / (2.0 * M_PI * cutoff_hz);
    double alpha = dt / (rc + dt);
    double y = 0.0;
    for (int i = 0; i < n; i++) {
        y += alpha * (buf[i] - y);
        buf[i] = y;
    }
}

/* ── One-pole high-pass filter (in-place) ───────────────────────────── */
static void hpf(double *buf, int n, double cutoff_hz, int sr) {
    double dt = 1.0 / sr;
    double rc = 1.0 / (2.0 * M_PI * cutoff_hz);
    double alpha = rc / (rc + dt);
    double y = 0.0, x_prev = 0.0;
    for (int i = 0; i < n; i++) {
        double x = buf[i];
        y = alpha * (y + x - x_prev);
        x_prev = x;
        buf[i] = y;
    }
}

/* ── Exponential decay envelope ─────────────────────────────────────── */
static double env_exp(int i, int n, double decay_s, int sr) {
    double t = (double)i / sr;
    return exp(-t / decay_s);
}

/* ── Drum synthesis models ───────────────────────────────────────────── */

static void synth_kick(double *out, int n, int sr, double vel) {
    /* Pitched sine sweep: 80 Hz → 40 Hz over first 80ms, then tail */
    double f0 = 80.0, f1 = 40.0;
    double sweep_time = 0.08; /* seconds */
    double decay_body = 0.35;
    double phase = 0.0;
    for (int i = 0; i < n; i++) {
        double t = (double)i / sr;
        /* Exponential pitch sweep */
        double frac = (t < sweep_time) ? (1.0 - t/sweep_time) : 0.0;
        double f = f1 + (f0 - f1) * frac;
        phase += 2.0 * M_PI * f / sr;
        double tone = sin(phase);
        /* Noise transient: only first 15ms */
        double noise = (t < 0.015) ? lcg_rand() * 0.4 : 0.0;
        double env = env_exp(i, n, decay_body, sr);
        /* Extra punch: short attack click */
        double punch = (t < 0.003) ? (1.0 - t/0.003) * 0.5 : 0.0;
        out[i] = vel * env * (tone + noise + punch);
    }
}

static void synth_snare(double *out, int n, int sr, double vel) {
    /* Short pitched tone (190 Hz) + filtered white noise */
    double f_tone = 190.0;
    double decay_tone  = 0.06;
    double decay_noise = 0.12;
    double phase = 0.0;
    for (int i = 0; i < n; i++) {
        double t = (double)i / sr;
        phase += 2.0 * M_PI * f_tone / sr;
        double tone  = sin(phase) * env_exp(i, n, decay_tone,  sr);
        double noise = lcg_rand()  * env_exp(i, n, decay_noise, sr);
        /* Attack transient */
        double attack = (t < 0.002) ? 1.5 : 1.0;
        out[i] = vel * attack * (tone * 0.4 + noise * 0.9);
    }
    /* Band-pass the noise component: HP at 800 Hz, LP at 8000 Hz */
    /* We approximate by filtering the whole buffer */
    hpf(out, n, 600.0,  sr);
    lpf(out, n, 9000.0, sr);
    /* Restore levels after filter attenuation */
    for (int i = 0; i < n; i++) out[i] *= 3.5;
}

static void synth_hihat(double *out, int n, int sr, double vel, double decay_s) {
    /* Band-pass filtered white noise — very short */
    for (int i = 0; i < n; i++) {
        out[i] = lcg_rand() * env_exp(i, n, decay_s, sr);
    }
    hpf(out, n, 5000.0, sr);
    lpf(out, n, 12000.0, sr);
    for (int i = 0; i < n; i++) out[i] *= vel * 4.0;
}

static void synth_tom(double *out, int n, int sr, double vel,
                      double f0, double f1, double decay_s) {
    /* Sine sweep from f0 to f1 over first 40ms */
    double sweep_time = 0.04;
    double phase = 0.0;
    for (int i = 0; i < n; i++) {
        double t = (double)i / sr;
        double frac = (t < sweep_time) ? (1.0 - t/sweep_time) : 0.0;
        double f = f1 + (f0 - f1) * frac;
        phase += 2.0 * M_PI * f / sr;
        double env = env_exp(i, n, decay_s, sr);
        /* Small noise burst at attack */
        double noise = (t < 0.008) ? lcg_rand() * 0.15 : 0.0;
        out[i] = vel * env * (sin(phase) + noise);
    }
}

static void synth_cymbal(double *out, int n, int sr, double vel, double decay_s) {
    /* Metallic: sum of inharmonic partials + filtered noise */
    /* Inharmonic ratios characteristic of cymbals */
    static const double ratios[] = {1.0, 1.48, 1.72, 2.0, 2.55, 3.15};
    double f0 = 440.0;
    double phases[6] = {0};
    for (int i = 0; i < n; i++) {
        double tone = 0.0;
        for (int k = 0; k < 6; k++) {
            phases[k] += 2.0 * M_PI * f0 * ratios[k] / sr;
            tone += sin(phases[k]) / (k + 1);
        }
        double noise = lcg_rand() * 0.5;
        double env = env_exp(i, n, decay_s, sr);
        out[i] = vel * env * (tone * 0.3 + noise * 0.7);
    }
    hpf(out, n, 3000.0, sr);
    lpf(out, n, 14000.0, sr);
    for (int i = 0; i < n; i++) out[i] *= 2.0;
}

/* ── Normalize output to [-1, 1] ────────────────────────────────────── */
static void normalize(double *out, int n) {
    double peak = 0.0;
    for (int i = 0; i < n; i++) {
        double a = fabs(out[i]);
        if (a > peak) peak = a;
    }
    if (peak > 1e-9) {
        double scale = 0.95 / peak;
        for (int i = 0; i < n; i++) out[i] *= scale;
    }
}

/* ── Main exported function ─────────────────────────────────────────── */
EXPORT void render_drum_hit(
    int32_t  drum_id,
    double   velocity,
    int32_t  n_samples,
    int32_t  sample_rate,
    double  *out
) {
    if (!out || n_samples <= 0) return;

    int n  = (int)n_samples;
    int sr = (sample_rate > 0) ? (int)sample_rate : 44100;

    if (velocity < 0.0) velocity = 0.0;
    if (velocity > 1.0) velocity = 1.0;
    if (drum_id < 0 || drum_id > 7) drum_id = 0;

    memset(out, 0, n * sizeof(double));

    switch (drum_id) {
        case 0: synth_kick  (out, n, sr, velocity); break;
        case 1: synth_snare (out, n, sr, velocity); break;
        case 2: synth_hihat (out, n, sr, velocity, 0.04); break;  /* closed */
        case 3: synth_tom   (out, n, sr, velocity, 320.0, 200.0, 0.18); break; /* tom-hi */
        case 4: synth_tom   (out, n, sr, velocity, 220.0, 140.0, 0.22); break; /* tom-mid */
        case 5: synth_tom   (out, n, sr, velocity, 150.0,  90.0, 0.28); break; /* tom-lo */
        case 6: synth_cymbal(out, n, sr, velocity, 0.80); break;  /* crash */
        case 7: synth_cymbal(out, n, sr, velocity, 0.30); break;  /* ride */
    }

    normalize(out, n);
}

/* ── freq_to_drum_id helper ─────────────────────────────────────────── */
/* Converts melody_learn_v2 drum frequencies back to drum_id 0-7        */
/* Use this in LabVIEW to bridge the two .so files                       */
EXPORT int32_t freq_to_drum_id(double freq) {
    /* DRUM[] = {36,40,43,47,50,53,57,60} in MIDI */
    static const double drum_freqs[] = {
        65.41,  /* 0: kick  C2  */
        82.41,  /* 1: snare E2  */
        98.00,  /* 2: hihat G2  */
       123.47,  /* 3: tom-hi B2 */
       146.83,  /* 4: tom-mid D3*/
       174.61,  /* 5: tom-lo F3 */
       220.00,  /* 6: crash A3  */
       261.63,  /* 7: ride  C4  */
    };
    int best = 0;
    double best_dist = 1e9;
    for (int i = 0; i < 8; i++) {
        double d = fabs(freq - drum_freqs[i]);
        if (d < best_dist) { best_dist = d; best = i; }
    }
    return (int32_t)best;
}


/* ── Smoke test ─────────────────────────────────────────────────────── */
#ifdef DRUM_SMOKE_TEST
#include <stdio.h>

int main(void) {
    int n = 4410; /* 100ms */
    double *buf = (double*)calloc(n, sizeof(double));
    const char *names[] = {"kick","snare","hihat","tom-hi",
                           "tom-mid","tom-lo","crash","ride"};
    int all_ok = 1;
    for (int d = 0; d < 8; d++) {
        memset(buf, 0, n * sizeof(double));
        render_drum_hit(d, 0.8, n, 44100, buf);
        double peak = 0.0;
        for (int i = 0; i < n; i++) if (fabs(buf[i]) > peak) peak = fabs(buf[i]);
        int ok = (peak > 0.01 && peak <= 1.01);
        printf("drum %d (%s): peak=%.4f  %s\n", d, names[d], peak, ok?"PASS":"FAIL");
        if (!ok) all_ok = 0;
    }

    /* freq_to_drum_id */
    int id = freq_to_drum_id(82.41);
    printf("freq_to_drum_id(82.41Hz snare): %d  %s\n", id, id==1?"PASS":"FAIL");
    if (id != 1) all_ok = 0;

    /* NULL guard */
    render_drum_hit(0, 0.8, n, 44100, NULL);
    printf("NULL guard: PASS\n");

    printf("OVERALL: %s\n", all_ok ? "PASS" : "FAIL");
    free(buf);
    return all_ok ? 0 : 1;
}
#endif
